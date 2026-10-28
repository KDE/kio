/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// The batch copy of the file worker through io_uring.

#include "file.h"
#include "file_unix_copy_p.h"

#include <QFileInfo>

#include <atomic>
#include <cerrno>
#include <latch>
#include <liburing.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <vector>
// After sys/mman.h, whose macros it defines again with the same values.
#include <linux/mman.h> // struct cachestat, Linux 6.5

CopyRing *CopyRing::create()
{
    if (qEnvironmentVariable("KIO_BATCH_URING") == QLatin1String("0")) {
        return nullptr;
    }
    auto copyRing = std::make_unique<CopyRing>();
    copyRing->metadataPool.setMaxThreadCount(16);

    // Three requests per file at most in each stage. Without SUBMIT_ALL (Linux 5.18), a request
    // that fails while it is read stops the submission there, which submitQueued handles.
    const unsigned entries = unsigned(3 * depth);
    if (io_uring_queue_init(entries, &copyRing->ring, IORING_SETUP_SUBMIT_ALL) != 0 && io_uring_queue_init(entries, &copyRing->ring, 0) != 0) {
        return nullptr;
    }
    // Every request of the copy has to be there. The opens, statx and close came in Linux 5.6,
    // together with the probe, so an older kernel has no probe and gets no ring.
    struct io_uring_probe *probe = io_uring_get_probe_ring(&copyRing->ring);
    bool supported = probe != nullptr;
    for (const int op : {IORING_OP_OPENAT, IORING_OP_STATX, IORING_OP_CLOSE, IORING_OP_READ, IORING_OP_WRITE, IORING_OP_FSYNC, IORING_OP_SYNC_FILE_RANGE}) {
        supported = supported && io_uring_opcode_supported(probe, op);
    }
    io_uring_free_probe(probe);
    if (!supported) {
        io_uring_queue_exit(&copyRing->ring);
        return nullptr;
    }
    copyRing->bufferSize = size_t(depth) * chunkSize;
    void *buffer = ::mmap(nullptr, copyRing->bufferSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer == MAP_FAILED) {
        io_uring_queue_exit(&copyRing->ring);
        return nullptr;
    }
    copyRing->buffer = static_cast<char *>(buffer);
    return copyRing.release();
}

CopyRing::~CopyRing()
{
    io_uring_queue_exit(&ring);
    if (buffer) {
        ::munmap(buffer, bufferSize);
    }
}

void CopyRingDeleter::operator()(CopyRing *ring) const
{
    delete ring;
}

namespace
{
// Copies the files of a batch through copyRing, a window at a time: up to depth files that share
// both folders. The stages of copyRun put the requests of the whole window in flight at once.
class UringCopyEngine : public BatchCopyEngine
{
public:
    UringCopyEngine(CopyRing *copyRing, PreserveFn preserve, gid_t egid, KilledFn isKilled, bool tryReflink)
        : BatchCopyEngine(std::move(preserve), egid, std::move(isKilled), tryReflink)
        , m_copyRing(copyRing)
    {
    }

protected:
    qsizetype runLength(const QList<BatchCopyOp> &ops, qsizetype first) const override;
    int copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, const ItemStartFn &onItemStart, const ItemResultFn &onItemResult) override;
    void srcDirOpened(int dirFd, const QString &folder) override
    {
        m_srcSlow = mountTraitsFor(dirFd, folder).slow;
    }
    void destDirOpened(int dirFd, const QString &folder) override
    {
        // KIO_BATCH_URING_FORCE_SLOW=1 treats a local folder as a network one, with the writeback
        // window of NFS, to test those stages.
        static const bool forceSlow = qEnvironmentVariableIntValue("KIO_BATCH_URING_FORCE_SLOW") == 1;
        m_destSlow = forceSlow || mountTraitsFor(dirFd, folder).slow;
        if (forceSlow && m_request.writebackWindow == 0) {
            m_request.writebackWindow = 16 * 1024 * 1024;
        }
    }

private:
    // Submits the queued requests and hands each completion to onCompletion with its user_data and
    // result. Returns 0, or the errno of a ring that failed.
    int submitAndReap(unsigned queued, const std::function<void(quint64 data, int res)> &onCompletion);
    // Submits the queued requests, 0 or an errno.
    int submitQueued(unsigned queued);
    // Waits for the next completion. A request on a stalled server never completes, so the wait
    // wakes every s_cancelCheck to ask whether the job was cancelled. On cancel, the requests in
    // flight are cancelled once and m_windowKilled is set. Returns 0 or an errno.
    int waitCompletion(struct io_uring_cqe **cqe);

    CopyRing *m_copyRing;
    bool m_windowKilled = false;
    unsigned m_ringOutstanding = 0; // requests submitted whose completion is not reaped yet
    bool m_destSlow = false; // m_destDir is on a network or other slow mount
    bool m_srcSlow = false; // m_srcDir is on a network or other slow mount
};

qsizetype UringCopyEngine::runLength(const QList<BatchCopyOp> &ops, qsizetype first) const
{
    const auto folderOf = [](const QString &path) {
        return QStringView(path).left(path.lastIndexOf(QLatin1Char('/')));
    };
    const QStringView srcDir = folderOf(ops.at(first).src);
    const QStringView destDir = folderOf(ops.at(first).dest);
    qsizetype count = 1;
    while (first + count < ops.size() && count < CopyRing::depth && folderOf(ops.at(first + count).src) == srcDir
           && folderOf(ops.at(first + count).dest) == destDir) {
        ++count;
    }
    return count;
}

// True when every page of the file is in the page cache. A kernel older than 6.5 has no cachestat
// and gets false, after which it is not asked again.
static bool isFullyCached(int fd, quint64 size)
{
#ifdef SYS_cachestat
    static std::atomic<bool> unsupported{false};
    if (unsupported) {
        return false;
    }
    struct cachestat_range range = {0, 0}; // to the end of the file
    struct cachestat cached = {};
    if (::syscall(SYS_cachestat, fd, &range, &cached, 0) != 0) {
        if (errno == ENOSYS) {
            unsupported = true;
        }
        return false;
    }
    static const quint64 pageSize = quint64(::sysconf(_SC_PAGESIZE));
    return cached.nr_cache >= (size + pageSize - 1) / pageSize;
#else
    Q_UNUSED(fd)
    Q_UNUSED(size)
    return false;
#endif
}

int UringCopyEngine::submitQueued(unsigned queued)
{
    // A request that fails while it is read, such as an open of an empty name, can stop the
    // submission there. The ones after it are still queued and go with the next call.
    unsigned submitted = 0;
    while (submitted < queued) {
        const int ret = io_uring_submit(&m_copyRing->ring);
        if (ret == -EINTR) {
            continue;
        }
        if (ret <= 0) {
            return ret < 0 ? -ret : EIO;
        }
        submitted += unsigned(ret);
        m_ringOutstanding += unsigned(ret);
    }
    return 0;
}

int UringCopyEngine::submitAndReap(unsigned queued, const std::function<void(quint64 data, int res)> &onCompletion)
{
    if (queued == 0) {
        return 0;
    }
    if (const int err = submitQueued(queued)) {
        return err;
    }
    for (unsigned n = 0; n < queued; ++n) {
        struct io_uring_cqe *cqe = nullptr;
        if (const int err = waitCompletion(&cqe)) {
            return err;
        }
        onCompletion(io_uring_cqe_get_data64(cqe), cqe->res);
        io_uring_cqe_seen(&m_copyRing->ring, cqe);
        --m_ringOutstanding;
    }
    return 0;
}

// The completion of the request that cancels the others carries this tag, and is not handed on.
// Not ~0, which liburing uses for the timeouts of its own waits on kernels before 5.11.
static constexpr quint64 s_cancelTag = ~quint64(0) - 1;

int UringCopyEngine::waitCompletion(struct io_uring_cqe **cqe)
{
    while (true) {
        struct __kernel_timespec timeout = {0, std::chrono::nanoseconds(s_cancelCheck).count()};
        const int ret = io_uring_wait_cqe_timeout(&m_copyRing->ring, cqe, &timeout);
        if (ret == -EINTR) {
            continue;
        }
        if (ret == -ETIME) {
            if (!m_windowKilled && m_isKilled()) {
                m_windowKilled = true;
                // IORING_ASYNC_CANCEL_ANY came in Linux 5.19. Without it the requests run to the end.
                if (struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring)) {
                    io_uring_prep_cancel64(sqe, 0, IORING_ASYNC_CANCEL_ANY);
                    io_uring_sqe_set_data64(sqe, s_cancelTag);
                    submitQueued(1);
                }
            }
            continue;
        }
        if (ret < 0) {
            return -ret;
        }
        if (io_uring_cqe_get_data64(*cqe) == s_cancelTag) {
            io_uring_cqe_seen(&m_copyRing->ring, *cqe);
            --m_ringOutstanding;
            continue;
        }
        return 0;
    }
}

// The files of one window go through the ring a stage at a time: open the sources and read their
// status, create the destinations, move the data, then close everything. On a network filesystem
// the requests of a stage are in flight together, where the serial loop waits for each in turn.
// There is no opcode for fchmod, futimens or fchown, so the metadata is still set one file at a time.
int UringCopyEngine::copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, const ItemStartFn &onItemStart, const ItemResultFn &onItemResult)
{
    struct File {
        QByteArray srcName;
        QByteArray destName;
        int srcFd = -1;
        int destFd = -1;
        int err = 0; // the first failure, 0 when there is none
        bool readFailed = false; // err is a read error on the source
        bool conflict = false; // the destination exists
        bool counted = false; // its bytes are in m_bytesCopied
        bool created = false; // the window made the destination
        struct statx stx;
    };
    std::vector<File> files(count);
    for (qsizetype i = 0; i < count; ++i) {
        files[i].srcName = QFile::encodeName(QFileInfo(ops.at(first + i).src).fileName());
        files[i].destName = QFile::encodeName(QFileInfo(ops.at(first + i).dest).fileName());
    }

    const bool inlineOpens = !m_srcSlow && !m_destSlow;

    // After a cancel the files go no further, and are taken back after the close.
    m_windowKilled = false;
    auto markKilled = [&] {
        if (!m_windowKilled) {
            return;
        }
        for (File &f : files) {
            if (f.err == 0) {
                f.err = ECANCELED;
            }
        }
    };

    // A failed submission or wait leaves requests in flight that write into files, and a window half
    // done. They are cancelled and waited for, the descriptors closed, and the copies the window
    // made deleted. None of the files is reported, so the caller copies them one at a time, and
    // the ring is not used again. The requests still queued are submitted with the cancel, and the
    // opens among them complete through lateDone, so their descriptors and files are known.
    auto failWindow = [&](int err, const std::function<void(quint64 data, int res)> &lateDone = {}) {
        m_copyRing->broken = true;
        if (struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring)) {
            io_uring_prep_cancel64(sqe, 0, IORING_ASYNC_CANCEL_ANY);
            io_uring_sqe_set_data64(sqe, s_cancelTag);
        }
        const int submitted = io_uring_submit(&m_copyRing->ring);
        if (submitted > 0) {
            m_ringOutstanding += unsigned(submitted);
        }
        while (m_ringOutstanding > 0) {
            struct io_uring_cqe *cqe = nullptr;
            const int ret = io_uring_wait_cqe(&m_copyRing->ring, &cqe);
            if (ret == -EINTR) {
                continue;
            }
            if (ret < 0) {
                break; // the ring itself failed, nothing more completes
            }
            const quint64 data = io_uring_cqe_get_data64(cqe);
            if (data != s_cancelTag && lateDone) {
                lateDone(data, cqe->res);
            }
            io_uring_cqe_seen(&m_copyRing->ring, cqe);
            --m_ringOutstanding;
        }
        m_ringOutstanding = 0;
        for (File &f : files) {
            if (f.srcFd >= 0) {
                ::close(f.srcFd);
            }
            if (f.destFd >= 0) {
                ::close(f.destFd);
            }
            if (f.created) {
                ::unlinkat(m_destDir.fd(), f.destName.constData(), 0);
            }
            if (f.counted) {
                m_bytesCopied -= f.stx.stx_size;
            }
        }
        return err;
    };

    // Stage 1: the sources. statx by name follows a symlink the way open does.
    auto sourceDone = [&](quint64 data, int res) {
        File &f = files[data / 2];
        if (data % 2 == 0 && res >= 0) {
            f.srcFd = res;
        } else if (res < 0 && f.err == 0) {
            f.err = -res;
        }
    };
    unsigned queued = 0;
    for (qsizetype i = 0; i < count; ++i) {
        if (inlineOpens) {
            const int fd = ::openat(m_srcDir.fd(), files[i].srcName.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            sourceDone(quint64(i) * 2, fd >= 0 ? fd : -errno);
            if (fd >= 0) {
                sourceDone(quint64(i) * 2 + 1, ::statx(fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &files[i].stx) == 0 ? 0 : -errno);
            }
            continue;
        }
        struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_openat(sqe, m_srcDir.fd(), files[i].srcName.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC, 0);
        io_uring_sqe_set_data64(sqe, quint64(i) * 2);
        sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_statx(sqe, m_srcDir.fd(), files[i].srcName.constData(), 0, STATX_BASIC_STATS, &files[i].stx);
        io_uring_sqe_set_data64(sqe, quint64(i) * 2 + 1);
        queued += 2;
    }
    if (const int err = submitAndReap(queued, sourceDone)) {
        return failWindow(err, sourceDone);
    }
    markKilled();

    // Stage 2: the destinations, created with the mode of their source. O_EXCL is the conflict check.
    auto destDone = [&](quint64 data, int res) {
        File &f = files[data];
        if (res >= 0) {
            f.destFd = res;
            f.created = true;
        } else if (res == -EEXIST) {
            f.conflict = true;
        } else {
            f.err = -res;
        }
    };
    queued = 0;
    for (qsizetype i = 0; i < count; ++i) {
        File &f = files[i];
        if (f.err != 0) {
            continue;
        }
        if (!S_ISREG(f.stx.stx_mode)) {
            f.err = EINVAL;
            continue;
        }
        const int flags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_EXCL;
        if (inlineOpens) {
            const int fd = ::openat(m_destDir.fd(), f.destName.constData(), flags, f.stx.stx_mode & 07777);
            destDone(quint64(i), fd >= 0 ? fd : -errno);
            continue;
        }
        struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_openat(sqe, m_destDir.fd(), f.destName.constData(), flags, f.stx.stx_mode & 07777);
        io_uring_sqe_set_data64(sqe, quint64(i));
        ++queued;
    }
    if (const int err = submitAndReap(queued, destDone)) {
        return failWindow(err, destDone);
    }
    markKilled();

    // Stage 3: the data, a chunk at a time through the slot of each file: a read linked to the
    // write of the same chunk. The next chunk of a file is queued when its write completes, so the
    // files of a window move at the same time in depth * chunkSize of memory.
    //
    // Where the destination has a writeback window, the write that ends a window of a file is linked
    // to a sync_file_range of that window, and the next chunk waits for it. The window of the
    // destination is shared by the files of the ring window, so the data waiting for writeback stays
    // what one copyFileAt keeps.
    enum ChunkRequest : quint64 {
        ChunkRead,
        ChunkWrite,
        ChunkSync,
        ChunkRequests
    };
    queued = 0;
    unsigned inflight = 0;
    std::vector<bool> retry(count, false);
    std::vector<bool> syncing(count, false);
    std::vector<quint64> offset(count, 0);
    quint64 syncWindow = 0;
    if (m_request.writebackWindow > 0) {
        syncWindow = qMax<quint64>(CopyRing::chunkSize, m_request.writebackWindow / quint64(count) / CopyRing::chunkSize * CopyRing::chunkSize);
    }
    auto chunkLength = [&](qsizetype i) {
        return unsigned(qMin<quint64>(CopyRing::chunkSize, files[i].stx.stx_size - offset[i]));
    };
    auto queueChunk = [&](qsizetype i) {
        File &f = files[i];
        char *slot = m_copyRing->buffer + size_t(i) * CopyRing::chunkSize;
        const unsigned length = chunkLength(i);
        const quint64 end = offset[i] + length;
        struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_read(sqe, f.srcFd, slot, length, offset[i]);
        io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
        io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkRead);
        sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_write(sqe, f.destFd, slot, length, offset[i]);
        io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkWrite);
        queued += 2;
        inflight += 2;
        // The last window of a file is left to the fsync of stage 3b.
        if (syncWindow > 0 && end < f.stx.stx_size && end % syncWindow == 0) {
            io_uring_sqe_set_flags(sqe, sqe->flags | IOSQE_IO_LINK);
            sqe = io_uring_get_sqe(&m_copyRing->ring);
            constexpr unsigned writeAndWait = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER;
            io_uring_prep_sync_file_range(sqe, f.destFd, unsigned(syncWindow), end - syncWindow, writeAndWait);
            io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkSync);
            syncing[i] = true;
            ++queued;
            ++inflight;
        }
    };
    // Reported as the chunks are written, so a window of large files shows progress. Stage 4b
    // reports again only past what this reached.
    KIO::filesize_t windowWritten = 0;
    KIO::filesize_t reported = m_bytesCopied;
    auto queueNext = [&](qsizetype i) {
        File &f = files[i];
        if (offset[i] >= f.stx.stx_size) {
            return;
        }
        if (m_isKilled()) {
            f.err = ECANCELED;
            return;
        }
        queueChunk(i);
    };
    // A short read or write, or a request cancelled by the one it is linked to, stops the chunks of
    // the file, and stage 4 copies it again. A failed writeback fails the file.
    auto chunkDone = [&](quint64 data, int res) {
        const qsizetype i = qsizetype(data / ChunkRequests);
        File &f = files[i];
        if (retry[i] || f.err != 0) {
            return;
        }
        switch (data % ChunkRequests) {
        case ChunkRead:
            if (res < 0 || unsigned(res) != chunkLength(i)) {
                retry[i] = true;
            }
            return;
        case ChunkWrite:
            if (res < 0 || unsigned(res) != chunkLength(i)) {
                retry[i] = true;
                return;
            }
            offset[i] += quint64(res);
            windowWritten += KIO::filesize_t(res);
            if (m_onProgress && m_bytesCopied + windowWritten > reported) {
                reported = m_bytesCopied + windowWritten;
                m_onProgress(reported);
            }
            if (!syncing[i]) {
                queueNext(i);
            }
            return;
        case ChunkSync:
            syncing[i] = false;
            if (res < 0) {
                f.err = -res;
                return;
            }
            queueNext(i);
            return;
        }
    };
    const bool checkCached = !m_srcSlow && !m_destSlow;
    for (qsizetype i = 0; i < count; ++i) {
        File &f = files[i];
        if (f.destFd < 0 || f.stx.stx_size == 0 || f.err != 0) {
            continue;
        }
#ifdef FICLONE
        if (m_request.tryReflink && ::ioctl(f.destFd, FICLONE, f.srcFd) != -1) {
            continue;
        }
#endif
        if (checkCached && qint64(f.stx.stx_size) < CopyRing::cachedCopyMaxSize && isFullyCached(f.srcFd, f.stx.stx_size)) {
            KIO::filesize_t ignored = 0;
            if (!copyFds(f.srcFd, f.destFd, f.stx.stx_size, ignored, m_isKilled)) {
                f.err = errno;
            }
            continue;
        }
        queueChunk(i);
    }
    while (inflight > 0) {
        if (const int err = submitQueued(queued)) {
            return failWindow(err);
        }
        queued = 0;
        struct io_uring_cqe *cqe = nullptr;
        if (const int err = waitCompletion(&cqe)) {
            return failWindow(err);
        }
        // Every completion already posted, then one submission for the chunks they queued.
        unsigned head;
        unsigned seen = 0;
        io_uring_for_each_cqe(&m_copyRing->ring, head, cqe)
        {
            ++seen;
            --m_ringOutstanding;
            const quint64 data = io_uring_cqe_get_data64(cqe);
            if (data == s_cancelTag) {
                continue;
            }
            chunkDone(data, cqe->res);
            --inflight;
        }
        io_uring_cq_advance(&m_copyRing->ring, seen);
    }
    markKilled();

    // Stage 3b, on a network mount: write back the data of the window. The NFS client keeps the writes
    // in the page cache and sends them before a setattr, so futimens would otherwise send the data of
    // each file in turn. A failed write shows up here.
    if (m_destSlow) {
        queued = 0;
        for (qsizetype i = 0; i < count; ++i) {
            const File &f = files[i];
            if (f.destFd < 0 || f.stx.stx_size == 0 || retry[i] || f.err != 0) {
                continue;
            }
            struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
            io_uring_prep_fsync(sqe, f.destFd, IORING_FSYNC_DATASYNC);
            io_uring_sqe_set_data64(sqe, quint64(i));
            ++queued;
        }
        if (const int err = submitAndReap(queued, [&](quint64 data, int res) {
                if (res < 0 && files[data].err == 0) {
                    files[data].err = -res;
                }
            })) {
            return failWindow(err);
        }
    }
    markKilled();

    // Stage 4: what a short transfer left, one file at a time.
    std::vector<qsizetype> copied;
    copied.reserve(size_t(count));
    for (qsizetype i = 0; i < count; ++i) {
        File &f = files[i];
        if (f.destFd < 0 || f.err != 0) {
            continue;
        }
        if (retry[i]) {
            KIO::filesize_t ignored = 0;
            if (::lseek(f.srcFd, 0, SEEK_SET) != 0 || ::lseek(f.destFd, 0, SEEK_SET) != 0 || ::ftruncate(f.destFd, 0) != 0
                || !copyFds(f.srcFd, f.destFd, f.stx.stx_size, ignored, m_isKilled, {}, &f.readFailed)) {
                f.err = errno;
                continue;
            }
        }
        copied.push_back(i);
    }

    // Stage 4b: the metadata, on the pool of the ring for a slow destination, else in turn.
    auto preserveFile = [this, &files](qsizetype i) {
        const File &f = files[i];
        struct stat st = {};
        st.st_mode = f.stx.stx_mode;
        st.st_uid = f.stx.stx_uid;
        st.st_gid = f.stx.stx_gid;
        st.st_size = off_t(f.stx.stx_size);
        st.st_atim = {time_t(f.stx.stx_atime.tv_sec), long(f.stx.stx_atime.tv_nsec)};
        st.st_mtim = {time_t(f.stx.stx_mtime.tv_sec), long(f.stx.stx_mtime.tv_nsec)};
        m_preserve(f.srcFd, f.destFd, st, m_request, true, m_destDirGroup);
    };
    if (m_destSlow && copied.size() > 1) {
        // A latch rather than waitForDone(), which also stops the threads of the pool.
        std::latch done(std::ptrdiff_t(copied.size()));
        for (const qsizetype i : copied) {
            m_copyRing->metadataPool.start([&preserveFile, &done, i] {
                preserveFile(i);
                done.count_down();
            });
        }
        done.wait();
    } else {
        for (const qsizetype i : copied) {
            preserveFile(i);
        }
    }
    for (const qsizetype i : copied) {
        m_bytesCopied += files[i].stx.stx_size;
        files[i].counted = true;
        if (m_onProgress && m_bytesCopied > reported) {
            m_onProgress(m_bytesCopied);
        }
    }

    // Stage 5: close everything. A network filesystem can report a failed write only here.
    // A descriptor belongs to the ring once its close is queued, so a failed window does not close
    // it a second time.
    queued = 0;
    for (qsizetype i = 0; i < count; ++i) {
        for (int *fd : {&files[i].srcFd, &files[i].destFd}) {
            if (*fd >= 0) {
                struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
                io_uring_prep_close(sqe, *fd);
                io_uring_sqe_set_data64(sqe, quint64(i) * 2 + (fd == &files[i].destFd ? 1 : 0));
                *fd = -1;
                ++queued;
            }
        }
    }
    if (const int err = submitAndReap(queued, [&](quint64 data, int res) {
            File &f = files[data / 2];
            if (data % 2 == 1 && res < 0 && f.err == 0) {
                f.err = -res;
            }
        })) {
        return failWindow(err);
    }

    // A file that failed after its destination was made leaves nothing behind. From the file that
    // stops the batch on, the copies of the window are taken back too, since they are never reported.
    auto discard = [&](File &f) {
        if (f.created && ::unlinkat(m_destDir.fd(), f.destName.constData(), 0) != 0 && errno != ENOENT) {
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << QFile::decodeName(f.destName);
        }
        f.created = false;
        if (f.counted) {
            m_bytesCopied -= f.stx.stx_size;
            f.counted = false;
        }
    };
    for (qsizetype i = 0; i < count; ++i) {
        File &f = files[i];
        const BatchCopyOp &op = ops.at(first + i);
        int stopErr = 0;
        onItemStart(op.index);
        if (f.conflict) {
            onItemResult(op.index, BatchItemResult::Conflict);
        } else if (f.err != 0) {
            discard(f);
            // As in copyOne(): a read error only concerns the source file.
            if (f.err == ECANCELED || (!f.readFailed && isFatalBatchCopyError(f.err))) {
                if (f.err != ECANCELED) {
                    onItemResult(op.index, BatchItemResult::BatchFailed);
                }
                stopErr = f.err;
            } else {
                onItemResult(op.index, BatchItemResult::Deferred);
            }
        } else {
            onItemResult(op.index, BatchItemResult::Copied);
        }
        if (stopErr != 0) {
            for (qsizetype j = i + 1; j < count; ++j) {
                discard(files[j]);
            }
            return stopErr;
        }
    }
    return 0;
}
// NOTE: io_uring has no opcode for copy_file_range. UringCopyEngine moves the data of a file with
// reads and writes instead, so the whole file can go through the ring.
} // namespace

std::unique_ptr<BatchCopyEngine> makeUringCopyEngine(CopyRing *copyRing, PreserveFn preserve, gid_t egid, KilledFn isKilled, bool tryReflink)
{
    return std::make_unique<UringCopyEngine>(copyRing, std::move(preserve), egid, std::move(isKilled), tryReflink);
}
