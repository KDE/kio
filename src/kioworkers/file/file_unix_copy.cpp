/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Ehrlicher <ch.ehrlicher@gmx.de>
    SPDX-FileCopyrightText: 2021-2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

// The copying side of the file worker: FileProtocol::copy() and the helpers only it uses.

#include "file.h"
#include "kmountpoint_p.h"
#include "stat_unix.h"

#include "config-kioworker-file.h"

#if HAVE_POSIX_ACL
#include <../../aclhelpers_p.h>
#endif

#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QScopeGuard>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <qplatformdefs.h>

#include <chrono>

#include <KLocalizedString>
#include <QDebug>
#include <kmountpoint.h>

#include <QDataStream>

#include <cerrno>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <poll.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <thread>
#include <unistd.h>
#include <utime.h>

#ifdef Q_OS_LINUX

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

#endif // Q_OS_LINUX

#if HAVE_COPY_FILE_RANGE
// sys/types.h must be included before unistd.h,
// and it needs to be included explicitly for FreeBSD
#include <sys/types.h>
#include <unistd.h>
#endif

#if HAVE_SENDFILE
#include <sys/sendfile.h>
#endif

#if HAVE_LIBURING
#include <atomic>
#include <latch>
#include <liburing.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <vector>
// After sys/mman.h, whose macros it defines again with the same values.
#include <linux/mman.h> // struct cachestat, Linux 6.5
#endif

#if HAVE_SYS_XATTR_H
#include <sys/xattr.h>
// BSD uses a different include
#elif HAVE_SYS_EXTATTR_H
#include <sys/types.h> // For FreeBSD, this must be before sys/extattr.h

#include <sys/extattr.h>
#endif

using namespace KIO;
using namespace std::chrono_literals;

static bool mountIsCifs(const KMountPoint::Ptr &mount)
{
    return mount && (mount->mountType() == QLatin1String("cifs") || mount->mountType() == QLatin1String("smb3"));
}

static bool isOnCifsMount(const QString &filePath)
{
    return mountIsCifs(KMountPoint::currentMountPointForPath(filePath));
}

// Whether the file that was looked at sits on a CIFS/SMB mount. On Linux the unique mount id comes from
// the stat that was made of it anyway, and is resolved through the KMountPoint cache, so neither a statfs
// nor a parse of the mount table is needed. Elsewhere and on older kernels the mount is looked up by path.
static bool isOnCifs(const StatStruct &buf, const QString &fallbackPath)
{
#if HAVE_STATX_MNT_ID_UNIQUE
    if (buf.stx_mask & STATX_MNT_ID_UNIQUE) {
        return mountIsCifs(KMountPoint::currentMountPointForUniqueId(stat_mnt_id(buf)));
    }
#else
    Q_UNUSED(buf);
#endif
    return isOnCifsMount(fallbackPath);
}

#if HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H
bool FileProtocol::copyXattrs(const int src_fd, const int dest_fd, XattrNamespaces namespaces)
{
    // Get the list of keys
    ssize_t listlen = 0;
    QByteArray keylist;
    while (true) {
        keylist.resize(listlen);
#if HAVE_SYS_XATTR_H && !defined(__stub_getxattr) && !defined(Q_OS_MAC)
        listlen = flistxattr(src_fd, keylist.data(), listlen);
#elif defined(Q_OS_MAC)
        listlen = flistxattr(src_fd, keylist.data(), listlen, 0);
#elif HAVE_SYS_EXTATTR_H
        listlen = extattr_list_fd(src_fd, EXTATTR_NAMESPACE_USER, listlen == 0 ? nullptr : keylist.data(), listlen);
#endif
        if (listlen > 0 && keylist.size() == 0) {
            continue;
        }
        if (listlen > 0 && keylist.size() > 0) {
            break;
        }
        if (listlen == -1 && errno == ERANGE) {
            listlen = 0;
            continue;
        }
        if (listlen == 0) {
            // qCDebug(KIO_FILE) << "the file doesn't have any xattr";
            return true;
        }
        Q_ASSERT_X(listlen == -1, "copyXattrs", "unexpected return value from listxattr");
        if (listlen == -1 && errno == ENOTSUP) {
            qCDebug(KIO_FILE) << "source filesystem does not support xattrs";
        }
        return false;
    }

    keylist.resize(listlen);

    // Linux and MacOS return a list of null terminated strings, each string = [data,'\0']
    // BSDs return a list of items, each item consisting of the size byte
    // prepended to the key = [size, data]
    auto keyPtr = keylist.cbegin();
    size_t keyLen;
    QByteArray value;

    // For each key
    while (keyPtr != keylist.cend()) {
        // Get size of the key
#if HAVE_SYS_XATTR_H
        keyLen = strlen(keyPtr);
        auto next_key = [&]() {
            keyPtr += keyLen + 1;
        };
#elif HAVE_SYS_EXTATTR_H
        keyLen = static_cast<unsigned char>(*keyPtr);
        keyPtr++;
        auto next_key = [&]() {
            keyPtr += keyLen;
        };
#endif
        QByteArray key(keyPtr, keyLen);
#if HAVE_SYS_XATTR_H && !defined(Q_OS_MAC)
        if (namespaces == XattrNamespaces::User && !key.startsWith("user.")) {
            next_key();
            continue;
        }
#else
        Q_UNUSED(namespaces)
#endif

        // Get the value for key
        ssize_t valuelen = 0;
        do {
            value.resize(valuelen);
#if HAVE_SYS_XATTR_H && !defined(__stub_getxattr) && !defined(Q_OS_MAC)
            valuelen = fgetxattr(src_fd, key.constData(), value.data(), valuelen);
#elif defined(Q_OS_MAC)
            valuelen = fgetxattr(src_fd, key.constData(), value.data(), valuelen, 0, 0);
#elif HAVE_SYS_EXTATTR_H
            valuelen = extattr_get_fd(src_fd, EXTATTR_NAMESPACE_USER, key.constData(), valuelen == 0 ? nullptr : value.data(), valuelen);
#endif
            if (valuelen > 0 && value.size() == 0) {
                continue;
            }
            if (valuelen > 0 && value.size() > 0) {
                break;
            }
            if (valuelen == -1 && errno == ERANGE) {
                valuelen = 0;
                continue;
            }
            // happens when attr value is an empty string
            if (valuelen == 0) {
                break;
            }
            Q_ASSERT_X(valuelen == -1, "copyXattrs", "unexpected return value from getxattr");
            // Some other error, skip to the next attribute, most notably
            // - ENOTSUP: invalid (inaccassible) attribute namespace, e.g. with SELINUX
            break;
        } while (true);

        if (valuelen < 0) {
            // Skip to next attribute.
            next_key();
            continue;
        }

        // Write key:value pair on destination
#if HAVE_SYS_XATTR_H && !defined(__stub_getxattr) && !defined(Q_OS_MAC)
        ssize_t destlen = fsetxattr(dest_fd, key.constData(), value.constData(), valuelen, 0);
#elif defined(Q_OS_MAC)
        ssize_t destlen = fsetxattr(dest_fd, key.constData(), value.constData(), valuelen, 0, 0);
#elif HAVE_SYS_EXTATTR_H
        ssize_t destlen = extattr_set_fd(dest_fd, EXTATTR_NAMESPACE_USER, key.constData(), value.constData(), valuelen);
#endif
        if (destlen == -1 && errno == ENOTSUP) {
            qCDebug(KIO_FILE) << "Destination filesystem does not support xattrs";
            return false;
        }
        if (destlen == -1 && (errno == ENOSPC || errno == EDQUOT)) {
            return false;
        }

        next_key();
    }
    return true;
}
#endif // HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H

namespace
{
// A batch is a list of local files copied in one worker command, instead of one file_copy job per
// file, so the per-file job and IPC cost that dominates a many-small-files copy is paid once for
// the whole run. Each file goes through the same routine a single copy uses, with a request that
// only creates fresh files: an existing destination comes back as a conflict.
struct BatchCopyOp {
    int index; // position in the original request, used when reporting deferrals
    QByteArray src;
    QByteArray dest;
};

enum class ItemOutcome {
    Begin, // about to copy this file (drives the current-file report; emitted before the copy)
    Done,
    Conflict, // destination already exists and Overwrite was not requested
    Defer, // per-item recoverable failure: skip this file, keep going
    Abort, // batch-fatal failure: stop now
};

enum class CopyOutcome {
    Copied,
    Conflict,
    Failed, // errno is set
};

enum class CopyStage {
    OpenSource,
    OpenDest,
    Transfer,
    Publish, // renaming the finished ".part" over the destination
};

enum class ExistingDest {
    Refuse, // leave what is there alone and report a conflict
    Replace, // write a sibling ".part" and rename it over the destination once it is complete
    Truncate, // write over the file that is there, without a ".part"
};

enum class NewFilePermissions {
    FromSource, // the mode the source file carries
    SystemDefault, // what open() gives a new file, that is 0666 as the umask filters it
    Explicit, // the mode the request names
};

struct CopyRequest {
    ExistingDest existing = ExistingDest::Refuse;
    NewFilePermissions permissions = NewFilePermissions::FromSource;
    mode_t explicitMode = 0; // the mode to set where permissions names Explicit
    bool preserveOwner = true; // carry over the source's user and group
    bool preserveAcl = true; // carry over the source's access ACL where an extended attribute does not already
    bool tryReflink = false; // offer the filesystem the chance to share extents (FICLONE) instead of copying
    // The largest window of written data after which its writeback is started. 0 leaves writeback
    // to the kernel.
    KIO::filesize_t writebackWindow = 0;
    // A file larger than the window is on the device, with its name and its times, before the copy
    // reports it done.
    bool flushWindowedFile = false;
};

struct CopyReport {
    CopyOutcome outcome = CopyOutcome::Copied;
    CopyStage stage = CopyStage::Transfer;
    int err = 0;
    bool readFailed = false; // a read error on the source, as against a write error on the destination
};

// Tells apart errors that doom the rest of the batch from ones that only affect one file.
bool isFatalCopyError(int err)
{
    switch (err) {
    case ENOSPC: // out of space - every remaining file fails too
    case EDQUOT: // quota exhausted
    case EROFS: // destination became read-only
    case EIO: // I/O error - failing/disconnected device
    case ENXIO:
    case EMFILE: // out of file descriptors - won't recover within the batch
    case ENFILE:
        return true;
    default: // EACCES/EPERM/ENOENT/EISDIR/... affect this file only
        return false;
    }
}

// Smaller chunks measured ~25-30% slower on large cold copies.
static constexpr size_t s_copyChunk = 512 * 1024;
#if HAVE_COPY_FILE_RANGE
static constexpr size_t s_maxCopyChunk = 128 * 1024 * 1024;
static constexpr std::chrono::nanoseconds s_copyLatencyTargetLower = 200ms;
static constexpr std::chrono::nanoseconds s_copyLatencyTargetUpper = 250ms;
static constexpr std::chrono::nanoseconds s_copyLatencyTarget = 225ms;

// Larger chunks copy faster over the network, but a cancel or a progress update waits for the chunk being copied.
// Shrinking in one step to the speed of the slow call keeps a sudden slowdown to one slow call.
// A short call, such as the end of a file, says nothing about a larger chunk.
struct AdaptiveChunk {
    size_t size;
    const size_t min;
    const size_t max;

    void update(std::chrono::nanoseconds elapsed, size_t copied)
    {
        if (elapsed < s_copyLatencyTargetLower) {
            if (copied == size) {
                size = qMin(size * 2, max);
            }
        } else if (elapsed > s_copyLatencyTargetUpper) {
            size = qMax(size_t(copied * s_copyLatencyTarget.count() / elapsed.count()), min);
        }
    }
};
#endif

// Keeps the written data that the kernel has not written back to at most two windows. A helper thread
// writes back each full window while the copy fills the next one, and the copy waits only when the
// window before the one it hands over is not written back yet. Without this the whole file can sit in
// the page cache, which a slow device or a network filesystem then writes back in one go, and the
// writeback holds up other requests to the same filesystem meanwhile.
// Only the helper starts writeback. CIFS sends the data from the thread that starts the writeback, and
// it runs one writeback of a file at a time: a start that finds one running is dropped.
// A window holds about s_windowWriteback of writeback at the speed of the window before, so a request
// that waits for the data of the file waits about twice that long, on a slow link as on a fast one. On
// a network filesystem, any other request to the server also waits behind the window being sent.
// The first window is s_minWindow, and a window at most doubles the one before it.
static constexpr off_t s_minWindow = 1024 * 1024;
static constexpr std::chrono::milliseconds s_windowWriteback = 200ms;

// How often a copy that waits for a window checks whether it was cancelled.
static constexpr std::chrono::milliseconds s_cancelCheck = 100ms;

struct WritebackWindow {
    int fd = -1;
    off_t maxSize = 0; // 0: the kernel writes back on its own
    off_t size = qMin(maxSize, s_minWindow);
    off_t windowStart = 0; // start of the window that the copy is writing
    off_t progressLimit = size; // the progress reported so far is at most this

    WritebackWindow(int destFd, off_t maxWindow, std::function<bool()> killed)
        : fd(destFd)
        , maxSize(maxWindow)
        , isKilled(std::move(killed))
    {
    }

    ~WritebackWindow()
    {
        if (!helper.joinable()) {
            return;
        }
        {
            std::lock_guard lock(shared->mutex);
            shared->stopping = true;
        }
        shared->changed.notify_all();
        // The writeback of a window cannot be interrupted. After a cancel, the helper finishes it on its
        // own instead of holding up the cancel.
        if (cancelled || (isKilled && isKilled())) {
            helper.detach();
        } else {
            helper.join();
        }
    }

    // written is the end of the data written so far. On failure errno is set, ECANCELED on cancel.
    bool advance([[maybe_unused]] off_t written)
    {
#if HAVE_SYNC_FILE_RANGE
        if (size == 0 || written - windowStart < size) {
            return true;
        }
        std::unique_lock lock(shared->mutex);
        if (!helper.joinable()) {
            // A descriptor of its own stays open when the helper outlives the copy after a cancel.
            const int helperFd = ::fcntl(fd, F_DUPFD_CLOEXEC, 0);
            if (helperFd < 0) {
                return false;
            }
            helper = std::thread(&WritebackWindow::writeBack, shared, helperFd);
        }
        shared->pending.push_back({windowStart, written});
        shared->changed.notify_all();
        while (!shared->changed.wait_for(lock, s_cancelCheck, [this] {
            return shared->writtenBack >= windowStart || shared->error != 0;
        })) {
            if (isKilled && isKilled()) {
                cancelled = true;
                errno = ECANCELED;
                return false;
            }
        }
        if (shared->error != 0) {
            errno = shared->error;
            return false;
        }
        if (shared->lastWindowNs > 0) {
            resize(shared->lastWindowBytes, shared->lastWindowNs);
            shared->lastWindowNs = 0;
        }
        windowStart = written;
        progressLimit = qMax(progressLimit, shared->writtenBack + size);
#endif
        return true;
    }

    // The progress to report for written bytes: at most one window ahead of the data written back.
    // It stays at the end of a window until that window is handed over, so it never goes back, also
    // when the window shrinks.
    off_t progress(off_t written) const
    {
        return maxSize == 0 ? written : qMin(written, progressLimit);
    }

private:
    struct Range {
        off_t start;
        off_t end;
    };

    // What the copy and the helper share. The helper keeps it alive when it outlives the copy.
    struct Shared {
        std::mutex mutex{};
        std::condition_variable changed{};
        std::deque<Range> pending{}; // windows handed over and not written back yet
        off_t writtenBack = 0; // the data before this offset is written back
        int error = 0; // errno of a writeback that failed
        bool stopping = false;
        off_t lastWindowBytes = 0;
        qint64 lastWindowNs = 0; // time the helper took to write back its last window, 0 once used
    };

    std::function<bool()> isKilled;
    std::shared_ptr<Shared> shared = std::make_shared<Shared>();
    std::thread helper{};
    bool cancelled = false;

    // Runs on the helper thread, which owns helperFd.
    static void writeBack([[maybe_unused]] std::shared_ptr<Shared> state, int helperFd)
    {
        const auto closeFd = qScopeGuard([helperFd] {
            ::close(helperFd);
        });
#if HAVE_SYNC_FILE_RANGE
        std::unique_lock lock(state->mutex);
        while (true) {
            state->changed.wait(lock, [&state] {
                return state->stopping || !state->pending.empty();
            });
            if (state->stopping) {
                return;
            }
            const Range range = state->pending.front();
            state->pending.pop_front();
            lock.unlock();

            QElapsedTimer timer;
            timer.start();
            constexpr unsigned writeAndWait = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER;
            const int failure = ::sync_file_range(helperFd, range.start, range.end - range.start, writeAndWait) == 0 ? 0 : errno;
#if HAVE_FADVISE
            if (failure == 0) {
                // Written back and not read again: the pages can go.
                ::posix_fadvise(helperFd, range.start, range.end - range.start, POSIX_FADV_DONTNEED);
            }
#endif
            const qint64 elapsedNs = qMax<qint64>(timer.nsecsElapsed(), 1);

            lock.lock();
            if (failure != 0) {
                state->error = failure;
            } else {
                state->writtenBack = range.end;
                state->lastWindowBytes = range.end - range.start;
                state->lastWindowNs = elapsedNs;
            }
            state->changed.notify_all();
            if (failure != 0) {
                return;
            }
        }
#endif
    }

    // writtenBackBytes took elapsedNs to write back.
    void resize(off_t writtenBackBytes, qint64 elapsedNs)
    {
        const double target = double(writtenBackBytes) * std::chrono::nanoseconds(s_windowWriteback).count() / double(elapsedNs);
        size = qBound(qMin(maxSize, s_minWindow), off_t(qMin(target, double(size) * 2)), maxSize);
    }
};

// Most files one command may carry. A job sends a few hundred at a time, and the limit keeps a
// request that claims more from reserving memory for it.
static constexpr qint32 s_maxBatchCopyFiles = 8192;

// size, from the caller's fstat, bounds the loop so it stops at EOF without an extra probing call.
// On cancel errno is ECANCELED; on failure errno is left set and readFailed, if given, says which
// end failed.
bool copyFds(int sourceFd,
             int destFd,
             [[maybe_unused]] KIO::filesize_t size,
             KIO::filesize_t &bytesCopied,
             const std::function<bool()> &isKilled,
             const std::function<void(KIO::filesize_t)> &onProgress = {},
             bool *readFailed = nullptr,
             KIO::filesize_t writebackWindow = 0)
{
    WritebackWindow window(destFd, off_t(writebackWindow), isKilled);
    const KIO::filesize_t progressBase = bytesCopied; // the running total before this file
#if HAVE_COPY_FILE_RANGE
    QElapsedTimer timer;
    timer.start();
    KIO::filesize_t copied = 0; // bytes copied for this file by copy_file_range or sendfile
    AdaptiveChunk cfrChunk{s_copyChunk, s_copyChunk, s_maxCopyChunk};
    // copy_file_range() fails with EXDEV between two filesystems, such as from the local disk to a
    // USB stick. sendfile() copies between any two files without the data going through user space.
    bool useSendfile = false;
    while (copied < size) {
        size_t want = size_t(qMin<KIO::filesize_t>(size - copied, cfrChunk.size));
        if (window.size > 0) {
            // A chunk larger than the window would leave more data than the window to write back.
            want = qMin(want, size_t(window.size));
        }
        const auto copyStart = timer.durationElapsed();
#if HAVE_SENDFILE
        const ssize_t n = useSendfile ? ::sendfile(destFd, sourceFd, nullptr, want) : ::copy_file_range(sourceFd, nullptr, destFd, nullptr, want, 0);
#else
        const ssize_t n = ::copy_file_range(sourceFd, nullptr, destFd, nullptr, want, 0);
#endif
        const auto elapsed = timer.durationElapsed() - copyStart;
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
#if HAVE_SENDFILE
            // copy_file_range() cannot copy between these two files. Any other error, such as EIO or
            // ENOSPC, would fail sendfile() the same way.
            if (!useSendfile && (errno == EXDEV || errno == ENOSYS || errno == EOPNOTSUPP || errno == EINVAL)) {
                useSendfile = true;
                continue;
            }
#endif
            break; // fall back to read/write for the remainder
        }
        if (n == 0) {
            break; // unexpected early EOF (source shrank underneath us); finish via read/write
        }
        cfrChunk.update(elapsed, size_t(n));
        copied += n;
        bytesCopied += n;
        if (!window.advance(off_t(copied))) {
            return false;
        }
        if (onProgress) {
            onProgress(progressBase + window.progress(off_t(copied)));
        }
        // Between chunks only, so a single-chunk file does no extra work.
        if (copied < size && isKilled()) {
            errno = ECANCELED;
            return false;
        }
    }
    if (copied >= size) {
        return true;
    }
#endif
    // The remainder copy_file_range did not handle, or the whole file where it is unavailable
    // (macOS, OpenBSD, older Linux). A short read or write is retried, not treated as a failure.
    QByteArray buffer(256 * 1024, Qt::Uninitialized);
#if HAVE_COPY_FILE_RANGE
    off_t fileEnd = off_t(copied);
#else
    off_t fileEnd = 0;
#endif
    while (true) {
        const ssize_t n = ::read(sourceFd, buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (readFailed) {
                *readFailed = true;
            }
            return false; // genuine read error
        }
        if (n == 0) {
            break; // end of file
        }
        if (isKilled()) {
            errno = ECANCELED;
            return false;
        }
        ssize_t written = 0;
        while (written < n) {
            const ssize_t w = ::write(destFd, buffer.data() + written, n - written);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                if (readFailed) {
                    *readFailed = false;
                }
                return false; // genuine write error
            }
            written += w;
        }
        bytesCopied += n;
        fileEnd += n;
        if (!window.advance(fileEnd)) {
            return false;
        }
        if (onProgress) {
            onProgress(progressBase + window.progress(fileEnd));
        }
    }
    return true;
}

struct DestDirGroup {
    gid_t createdGid = 0;
    bool imposedByDir = false;
};

// Runs with both fds still open, so the changes land on the file just written rather than on
// whatever the path resolves to now. Every step is best-effort and only warns.
void preserveAttrs([[maybe_unused]] int sourceFd,
                   int destFd,
                   const struct stat &st,
                   const CopyRequest &request,
                   bool destFreshlyCreated,
                   uid_t euid,
                   DestDirGroup destDirGroup)
{
    // The created file is asked whether it already carries the mode wanted. That mode came back
    // with the create, after the umask, a default ACL or the server applied theirs, and writing
    // data does not change it, so AT_STATX_DONT_SYNC takes it from the attribute cache where a
    // network filesystem would ask the server again. An overwritten file kept its stale bits, and
    // open() does not reliably set the special bits, so both still go through fchmod.
    if (request.permissions != NewFilePermissions::SystemDefault) {
        const mode_t mode = (request.permissions == NewFilePermissions::Explicit ? request.explicitMode : mode_t(st.st_mode)) & 07777;
        bool createdModeAlreadyCorrect = false;
        if (destFreshlyCreated && (mode & mode_t(07000)) == 0) {
#if HAVE_STATX
            // KIO_COPY_MODE_SYNC=1 asks the server, to compare the two.
            static const int sync = qEnvironmentVariableIntValue("KIO_COPY_MODE_SYNC") == 1 ? AT_STATX_SYNC_AS_STAT : AT_STATX_DONT_SYNC;
            struct statx created;
            createdModeAlreadyCorrect = ::statx(destFd, "", AT_EMPTY_PATH | sync, STATX_MODE, &created) == 0 && (created.stx_mode & 07777) == mode;
#else
            struct stat created;
            createdModeAlreadyCorrect = ::fstat(destFd, &created) == 0 && (created.st_mode & mode_t(07777)) == mode;
#endif
        }
        if (!createdModeAlreadyCorrect && ::fchmod(destFd, mode) != 0) {
            qCWarning(KIO_FILE) << "copy: could not preserve permissions:" << strerror(errno);
        }
    }

    // Access and modification time, with the dest still open (futimes/futimens need the fd).
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_HAIKU)
    struct timespec ut[2] = {st.st_atim, st.st_mtim}; // nanosecond precision
    if (::futimens(destFd, ut) != 0) {
#else
    struct timeval ut[2];
    ut[0].tv_sec = st.st_atime;
    ut[0].tv_usec = 0;
    ut[1].tv_sec = st.st_mtime;
    ut[1].tv_usec = 0;
    if (::futimes(destFd, ut) != 0) {
#endif
        qCWarning(KIO_FILE) << "copy: could not preserve timestamps:" << strerror(errno);
    }

    // Group first, then owner, so a non-root worker still sets the group before a denied owner
    // change. A file open() has just created may already have both; one written over has the user
    // and group it had before, so it needs the calls whatever the source carries.
    if (request.preserveOwner) {
        const bool groupComesFromDir = destDirGroup.imposedByDir;
        const bool groupAlreadyRight = groupComesFromDir || (destFreshlyCreated && st.st_gid == destDirGroup.createdGid);
        if (!groupAlreadyRight && ::fchown(destFd, -1 /*keep user*/, st.st_gid) != 0) {
            qCWarning(KIO_FILE) << "copy: could not preserve group:" << strerror(errno);
        }
        const bool ownerAlreadyRight = destFreshlyCreated && st.st_uid == euid;
        if (!ownerAlreadyRight && ::fchown(destFd, st.st_uid, -1 /*keep group*/) != 0) {
            qCWarning(KIO_FILE) << "copy: could not preserve owner:" << strerror(errno);
        }
    }

#if HAVE_POSIX_ACL && !HAVE_SYS_XATTR_H
    // Only where copyXattrs cannot: BSD extattr lists the USER namespace alone, so the ACL, a
    // system attribute, would not be copied. Where listxattr returns system.posix_acl_access this
    // would be a redundant get/set on every file.
    if (request.preserveAcl) {
        if (acl_t acl = acl_get_fd(sourceFd)) {
            if (FileProtocol::isExtendedACL(acl) && acl_set_fd(destFd, acl) != 0) {
                qCWarning(KIO_FILE) << "copy: could not preserve ACL:" << strerror(errno);
            }
            acl_free(acl);
        }
    }
#endif
}

using PreserveFn =
    std::function<void(int sourceFd, int destFd, const struct stat &st, const CopyRequest &request, bool destFreshlyCreated, DestDirGroup destDirGroup)>;

// The extended attributes go through the worker, which knows the namespace and ENOTSUP quirks.
PreserveFn makePreserveFn(FileProtocol *worker, uid_t euid)
{
    return [worker, euid](int sourceFd, int destFd, const struct stat &st, const CopyRequest &request, bool destFreshlyCreated, DestDirGroup destDirGroup) {
        preserveAttrs(sourceFd, destFd, st, request, destFreshlyCreated, euid, destDirGroup);
#if HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H
        worker->copyXattrs(sourceFd, destFd);
#else
        Q_UNUSED(worker)
#endif
    };
}

// A directory with the set-group-ID bit hands its own group to whatever is made in it, and that
// group is the one a copy into it is meant to keep.
DestDirGroup destDirGroupFor(int dirFd, gid_t egid)
{
    struct stat ds;
    if (::fstat(dirFd, &ds) == 0 && (ds.st_mode & S_ISGID)) {
        return {ds.st_gid, true};
    }
    return {egid, false};
}

// Used to open each directory once and openat() the files within it.
std::pair<QByteArray, QByteArray> splitDir(const QByteArray &path)
{
    const int slash = path.lastIndexOf('/');
    if (slash <= 0) {
        return {QByteArrayLiteral("/"), path.mid(slash + 1)};
    }
    return {path.left(slash), path.mid(slash + 1)};
}

// Names are relative to already-open directory fds, so a run of files in one directory does not
// have the kernel resolve the shared prefix again for each of them. A single copy passes AT_FDCWD
// and a full path.
CopyReport copyFileAt(int srcDirFd,
                      const QByteArray &srcName,
                      int destDirFd,
                      const QByteArray &destName,
                      DestDirGroup destDirGroup,
                      const CopyRequest &request,
                      KIO::filesize_t &bytesCopied,
                      const PreserveFn &preserve,
                      const std::function<bool()> &isKilled,
                      const std::function<void(KIO::filesize_t)> &onProgress = {})
{
    // O_NONBLOCK because opening a named pipe (mkfifo) blocks until another process opens the write
    // end, and a device node can block as well. This command is synchronous, so a blocked open
    // never reaches wasKilled() and the copy could not be cancelled.
    const int sourceFd = ::openat(srcDirFd, srcName.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (sourceFd < 0) {
        return {CopyOutcome::Failed, CopyStage::OpenSource, errno, false};
    }
    auto cleanupSourceFd = qScopeGuard([sourceFd] {
        const int e = errno; // close() may clobber errno, and the failure is what the caller reads
        ::close(sourceFd);
        errno = e;
    });
    struct stat st;
    if (::fstat(sourceFd, &st) != 0) {
        return {CopyOutcome::Failed, CopyStage::OpenSource, errno, false};
    }
    // What the listing found can have become a FIFO, a device or a directory since. The flag
    // stays set: it makes no difference to a regular file and clearing it costs an fcntl.
    if (!S_ISREG(st.st_mode)) {
        return {CopyOutcome::Failed, CopyStage::OpenSource, EINVAL, false};
    }
#if HAVE_FADVISE
    // Only for files large enough to gain from the read-ahead: measured cold on an SSD this saves
    // ~10-18% from ~16 MiB up and is noise below ~1 MiB. The same measurement showed the hint on
    // the destination makes no difference.
    if (KIO::filesize_t(st.st_size) > KIO::filesize_t(8 * s_copyChunk)) {
        ::posix_fadvise(sourceFd, 0, 0, POSIX_FADV_SEQUENTIAL);
    }
#endif
    const bool publishViaRename = (request.existing == ExistingDest::Replace);
    const QByteArray outName = publishViaRename ? (destName + ".part") : destName;

    // O_NOFOLLOW never writes through a symlink at the name, and O_EXCL is also the conflict check
    // for a copy that refuses to replace anything: one syscall, no race.
    const mode_t createMode = request.permissions == NewFilePermissions::Explicit ? (request.explicitMode & 07777)
        : request.permissions == NewFilePermissions::SystemDefault                ? mode_t(0666)
                                                                                  : mode_t(st.st_mode & 07777);
    const int oflags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | (request.existing == ExistingDest::Truncate ? O_TRUNC : O_EXCL);
    int destFd = ::openat(destDirFd, outName.constData(), oflags, createMode);
    if (destFd < 0 && errno == EEXIST && publishViaRename) {
        // A ".part" an earlier interrupted copy left behind: drop it and try once more.
        ::unlinkat(destDirFd, outName.constData(), 0);
        destFd = ::openat(destDirFd, outName.constData(), oflags, createMode);
    }
    if (destFd < 0) {
        const int err = errno;
        return {err == EEXIST ? CopyOutcome::Conflict : CopyOutcome::Failed, CopyStage::OpenDest, err, false};
    }
    auto cleanupDestFd = qScopeGuard([destFd] {
        const int e = errno;
        ::close(destFd);
        errno = e;
    });

    const KIO::filesize_t bytesBefore = bytesCopied; // where the running total goes back to

    // Until the copy is committed, any return from here on leaves no half-written file behind under
    // either name, and takes its bytes back off the total. Dismissed once the result is committed.
    auto cleanupOutput = qScopeGuard([&] {
        const int e = errno; // the failure is what the caller reads
        if (::unlinkat(destDirFd, outName.constData(), 0) != 0 && errno != ENOENT) {
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << QFile::decodeName(outName);
        }
        errno = e;
        bytesCopied = bytesBefore;
    });
    bool cloned = false;
#ifdef FICLONE
    // Share the data blocks where the filesystem can, on any failure copy them.
    if (request.tryReflink && ::ioctl(destFd, FICLONE, sourceFd) != -1) {
        bytesCopied += KIO::filesize_t(st.st_size);
        cloned = true;
        if (onProgress) {
            onProgress(bytesCopied);
        }
    }
#endif
    bool destDeleteAttempted = false;
    bool readFailed = false;
    while (!cloned && !copyFds(sourceFd, destFd, st.st_size, bytesCopied, isKilled, onProgress, &readFailed, request.writebackWindow)) {
        const int err = errno;
        const bool diskFull = (err == ENOSPC || err == EDQUOT);
        if (diskFull && !readFailed && publishViaRename && !destDeleteAttempted) {
            // The file being replaced is still taking up the room the copy needs: free it and start
            // over, the descriptors have advanced.
            ::unlinkat(destDirFd, destName.constData(), 0);
            destDeleteAttempted = true;
            if (::lseek(sourceFd, 0, SEEK_SET) == 0 && ::lseek(destFd, 0, SEEK_SET) == 0 && ::ftruncate(destFd, 0) == 0) {
                bytesCopied = bytesBefore;
                continue;
            }
        }
        return {CopyOutcome::Failed, CopyStage::Transfer, err, readFailed};
    }

    if (isKilled()) {
        // The cancellation landed between the last chunk and here. The written file goes, and a
        // destination this copy was replacing is left as it was.
        return {CopyOutcome::Failed, CopyStage::Transfer, ECANCELED, false};
    }

    preserve(sourceFd, destFd, st, request, request.existing != ExistingDest::Truncate, destDirGroup); // both fds still open

    // The window has written back all but its last two windows, so this waits for those only.
    const bool flush = request.flushWindowedFile && request.writebackWindow > 0 && KIO::filesize_t(st.st_size) > request.writebackWindow;
    if (flush && ::fsync(destFd) != 0) {
        return {CopyOutcome::Failed, CopyStage::Transfer, errno, false};
    }

    if (publishViaRename && ::renameat(destDirFd, outName.constData(), destDirFd, destName.constData()) != 0) {
        return {CopyOutcome::Failed, CopyStage::Publish, errno, false};
    }
    // The entry of the file in the folder, under its final name.
    if (flush && ::fsync(destDirFd) != 0) {
        qCWarning(KIO_FILE) << "Could not flush the folder of" << QFile::decodeName(destName) << ":" << strerror(errno);
    }

    cleanupOutput.dismiss();
    return {CopyOutcome::Copied, CopyStage::Transfer, 0, false};
}

#ifdef Q_OS_LINUX
// The device number of the filesystem that holds fd, 0 when unknown. What the client already knows is
// enough, so the server of a network filesystem is not asked.
static quint64 deviceOf(int fd)
{
#if HAVE_STATX
    struct statx buf;
    if (::statx(fd, "", AT_EMPTY_PATH | AT_STATX_DONT_SYNC, STATX_TYPE, &buf) == 0) {
        return makedev(buf.stx_dev_major, buf.stx_dev_minor);
    }
    return 0;
#else
    QT_STATBUF buf;
    return QT_FSTAT(fd, &buf) == 0 ? quint64(buf.st_dev) : 0;
#endif
}

// Whether a filesystem was mounted or unmounted since the last call. The kernel reports a change of
// /proc/self/mountinfo to poll() until the file is read again.
static bool mountTableChanged()
{
    static const int fd = ::open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return true;
    }
    pollfd event{.fd = fd, .events = POLLPRI, .revents = 0};
    if (::poll(&event, 1, 0) <= 0 || !(event.revents & (POLLPRI | POLLERR))) {
        return false;
    }
    char buffer[4096];
    ::lseek(fd, 0, SEEK_SET);
    while (::read(fd, buffer, sizeof(buffer)) > 0) { }
    return true;
}
#endif

// What a copy to a folder needs to know about the mount that holds it.
struct MountTraits {
    bool removable = false;
    bool cifs = false;
    bool slow = false;
};

// The traits of the mount that holds the folder. The mount table gives them, so the server of a
// network filesystem is not asked. They are kept for each mount, whose unique id is never given to
// another mount, or else for each device. Linux only.
static MountTraits mountTraitsFor([[maybe_unused]] int dirFd, [[maybe_unused]] const QString &dir)
{
#ifdef Q_OS_LINUX
    quint64 mountId = 0;
#if HAVE_STATX_MNT_ID_UNIQUE
    struct statx folder;
    if (::statx(dirFd, "", AT_EMPTY_PATH | AT_STATX_DONT_SYNC, STATX_MNT_ID_UNIQUE, &folder) == 0 && (folder.stx_mask & STATX_MNT_ID_UNIQUE)) {
        mountId = folder.stx_mnt_id;
    }
#endif
    static QMutex traitsMutex;
    static QHash<quint64, MountTraits> traits; // by unique mount id
    // Without a unique mount id, as before Linux 6.8, by device number. A later mount can get the same
    // device number, so these are dropped whenever the mount table changes.
    static QHash<quint64, MountTraits> traitsByDevice;
    const quint64 device = mountId == 0 ? deviceOf(dirFd) : 0;
    {
        QMutexLocker locker(&traitsMutex);
        if (mountId != 0) {
            if (const auto it = traits.constFind(mountId); it != traits.cend()) {
                return *it;
            }
        } else {
            if (mountTableChanged()) {
                traitsByDevice.clear();
            }
            if (const auto it = traitsByDevice.constFind(device); device != 0 && it != traitsByDevice.cend()) {
                return *it;
            }
        }
    }

    const KMountPoint::Ptr mount = mountId != 0 ? KMountPoint::currentMountPointForUniqueId(mountId) : KMountPoint::currentMountPointForPath(dir);
    MountTraits found;
    if (mount) {
        found.removable = KIOPrivate::isOnRemovableDevice(*mount);
        found.cifs = mountIsCifs(mount);
        found.slow = mount->probablySlow();
    }
    QMutexLocker locker(&traitsMutex);
    if (mountId != 0) {
        traits.insert(mountId, found);
    } else if (device != 0) {
        traitsByDevice.insert(device, found);
    }
    return found;
#else
    return {};
#endif
}

// Whether the folder is on a removable device, which can be unplugged once a copy reports it is done.
// Linux only, where syncfs() can flush the device.
bool isFolderOnRemovableDevice([[maybe_unused]] int dirFd, [[maybe_unused]] const QString &dir)
{
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsSet("KIO_FILE_FORCE_REMOVABLE")) {
        return true;
    }
    return mountTraitsFor(dirFd, dir).removable;
#else
    return false;
#endif
}

struct WritebackPolicy {
    KIO::filesize_t window = 0; // see CopyRequest::writebackWindow
    bool flushWindowedFile = false; // see CopyRequest::flushWindowedFile
};

// How a copy to the folder writes its data back. A removable device takes long to write back a large
// file, and can be unplugged once the copy reports it is done, so a large file is flushed there. A
// network filesystem holds up other requests to the share while it writes a large file back. A local
// fixed disk writes back fast enough on its own.
WritebackPolicy writebackPolicyFor([[maybe_unused]] int destDirFd, [[maybe_unused]] const QString &destDir)
{
#if HAVE_SYNC_FILE_RANGE
    // The largest windows. On a fast link each window waits for the server to confirm the one before
    // it, so a larger window keeps more data in flight: with 2 ms of latency, SMB copies took 20%
    // longer with 16 MiB windows.
    constexpr KIO::filesize_t localWindow = 16 * 1024 * 1024;
    constexpr KIO::filesize_t smbWindow = 128 * 1024 * 1024;

    if (isFolderOnRemovableDevice(destDirFd, destDir)) {
        return {localWindow, true};
    }
    const MountTraits mount = mountTraitsFor(destDirFd, destDir);
    if (mount.cifs) {
        return {smbWindow, false};
    }
    // Any other filesystem that KMountPoint takes for slow: NFS, other network filesystems, and FUSE
    // filesystems not on a device, such as rclone. A FUSE filesystem that writes through leaves
    // nothing in the page cache, and its windows cost a call that has nothing to write.
    if (mount.slow) {
        return {localWindow, false};
    }
    return {};
#else
    return {};
#endif
}

} // namespace

struct DestinationWriteback::Private {
    Private(int fd, const WritebackPolicy &writebackPolicy, const QString &destFolder, std::function<bool()> isKilled)
        : policy(writebackPolicy)
        , window(fd, off_t(writebackPolicy.window), std::move(isKilled))
        , folder(destFolder)
    {
    }

    WritebackPolicy policy;
    WritebackWindow window;
    QString folder;
    bool fileFlushed = false;
};

DestinationWriteback::DestinationWriteback(int fd, const QString &folder, std::function<bool()> isKilled)
{
    WritebackPolicy policy;
    const int dirFd = ::open(QFile::encodeName(folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd >= 0) {
        policy = writebackPolicyFor(dirFd, folder);
        ::close(dirFd);
    }
    d = std::make_unique<Private>(fd, policy, folder, std::move(isKilled));
}

DestinationWriteback::~DestinationWriteback() = default;

bool DestinationWriteback::isActive() const
{
    return d->policy.window > 0;
}

bool DestinationWriteback::isRemovable() const
{
    return d->policy.flushWindowedFile;
}

bool DestinationWriteback::advance(off_t written)
{
    return d->window.advance(written);
}

bool DestinationWriteback::flushLargeFile(off_t size)
{
    // The window has written back all but its last two windows, so this waits for those only.
    if (!d->policy.flushWindowedFile || d->policy.window == 0 || KIO::filesize_t(size) <= d->policy.window) {
        return true;
    }
    if (::fsync(d->window.fd) != 0) {
        return false;
    }
    d->fileFlushed = true;
    return true;
}

void DestinationWriteback::flushFolder()
{
    if (!d->fileFlushed) {
        return;
    }
    const int dirFd = ::open(QFile::encodeName(d->folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0 || ::fsync(dirFd) != 0) {
        qCWarning(KIO_FILE) << "Could not flush the folder" << d->folder << ":" << strerror(errno);
    }
    if (dirFd >= 0) {
        ::close(dirFd);
    }
}

WorkerResult FileProtocol::flushRemovableFolder(const QString &folder)
{
    const int fd = ::open(QFile::encodeName(folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) {
        // The folder that the copy just wrote to cannot be reached, as when the device is unplugged, so
        // the data may not be on it.
        return WorkerResult::fail(KIO::ERR_CANNOT_WRITE, folder);
    }
    const auto closeFd = qScopeGuard([fd] {
        ::close(fd);
    });
    if (!isFolderOnRemovableDevice(fd, folder)) {
        return WorkerResult::pass();
    }
#ifdef Q_OS_LINUX
    infoMessage(i18n("Writing the copied data to the device..."));
    if (::syncfs(fd) != 0) {
        const int err = errno;
        return WorkerResult::fail((err == ENOSPC || err == EDQUOT) ? KIO::ERR_DISK_FULL : KIO::ERR_CANNOT_WRITE, folder);
    }
#endif
    return WorkerResult::pass();
}

#if HAVE_LIBURING
// The ring of the batch copy. It lives as long as the worker and only the worker's thread submits to
// it, which IORING_SETUP_SINGLE_ISSUER and IORING_SETUP_DEFER_TASKRUN rely on: completions are
// processed when that thread waits, not by interrupting it.
class CopyRing
{
public:
    // nullptr when KIO_BATCH_URING=0, or when the kernel or a seccomp filter refuses io_uring.
    // KIO_BATCH_URING_INLINE_OPEN=0 opens local files through the ring as well.
    static CopyRing *create();
    ~CopyRing();

    struct io_uring ring;
    int depth = 32; // files of one window
    // Files up to this size go through the ring, and a larger file through copyFileAt after the
    // window. The ring reads the files of a window at the same time, which a cold source and a slow
    // destination gain from at any size, and it writes back as copyFileAt does. A fully cached local
    // source under 1 MiB still goes to copy_file_range, see cachedCopyMaxSize. KIO_BATCH_URING_MAX_FILE
    // sets it in bytes.
    qint64 maxFileSize = std::numeric_limits<qint64>::max();
    bool dataSync = true; // a slow destination writes back the data of a window in the ring
    // Between two local folders the files are opened and created with plain calls. The ring runs
    // statx and a creating open on its worker threads, which costs more than the call itself on
    // a local filesystem.
    bool inlineLocalOpens = true;
    // Between two local folders, a source smaller than this whose data is all in the page cache is
    // copied with copy_file_range: once in the kernel, where a read and a write copy it twice. From
    // 1 MiB the ring is faster even then, since its worker threads copy the files of a window on
    // several CPUs where copy_file_range copies one file at a time. The check is one cachestat
    // call, about as costly as an fstat below 1 MiB. A slow destination keeps the ring, where the
    // writes of a window overlap. -1 when it is off.
    qint64 cachedCopyMaxSize = 1024 * 1024;
    // One slot of chunkSize per file of a window. The data of a file is read into its slot and
    // written from it a chunk at a time. Registered as buffer 0 when bufferRegistered.
    static constexpr size_t chunkSize = 64 * 1024;
    char *buffer = nullptr;
    size_t bufferSize = 0;
    bool bufferRegistered = false;
    // A table of 2 * depth registered files: slot 2i holds the source of file i of the window,
    // 2i + 1 its destination.
    bool fixedFiles = false;
    // Sets the permissions, times, owner and extended attributes of the files of a window on a
    // slow destination at the same time. io_uring has no request for them, and on a network
    // filesystem each one waits for the server. nullptr when KIO_BATCH_URING_META_THREADS=0.
    std::unique_ptr<QThreadPool> metadataPool;
    // Set when a submission or a wait failed. The worker drops the ring after the batch, and the
    // next batch makes a fresh one.
    bool broken = false;
};

CopyRing *CopyRing::create()
{
    if (qEnvironmentVariable("KIO_BATCH_URING") == QLatin1String("0")) {
        return nullptr;
    }
    auto copyRing = std::make_unique<CopyRing>();
    bool ok = false;
    const int depth = qEnvironmentVariableIntValue("KIO_BATCH_URING_DEPTH", &ok);
    copyRing->depth = ok && depth > 0 ? qMin(depth, 256) : 32;
    copyRing->dataSync = qEnvironmentVariable("KIO_BATCH_URING_FSYNC") != QLatin1String("0");
    const qint64 maxFile = qEnvironmentVariable("KIO_BATCH_URING_MAX_FILE").toLongLong(&ok);
    if (ok && maxFile >= 0) {
        copyRing->maxFileSize = maxFile;
    }
    copyRing->inlineLocalOpens = qEnvironmentVariable("KIO_BATCH_URING_INLINE_OPEN") != QLatin1String("0");
    // KIO_BATCH_URING_CACHESTAT_MAX sets the size from which a file is not checked, -1 turns the
    // check off.
    const int cachedMax = qEnvironmentVariableIntValue("KIO_BATCH_URING_CACHESTAT_MAX", &ok);
    if (ok) {
        copyRing->cachedCopyMaxSize = cachedMax;
    }
    const int threads = qEnvironmentVariableIntValue("KIO_BATCH_URING_META_THREADS", &ok);
    if (!ok || threads > 0) {
        copyRing->metadataPool = std::make_unique<QThreadPool>();
        copyRing->metadataPool->setMaxThreadCount(ok ? qMin(threads, 64) : 16);
    }
    // KIO_BATCH_URING_PLAIN=1 sets up the ring without the flags and registrations below, to
    // measure what they bring.
    const bool plain = qEnvironmentVariableIntValue("KIO_BATCH_URING_PLAIN") == 1;

    // Three requests per file at most in each stage. The flags are tried newest first:
    // SINGLE_ISSUER and DEFER_TASKRUN came in Linux 6.1, SUBMIT_ALL in 5.18.
    const unsigned entries = unsigned(3 * copyRing->depth);
    const unsigned flagSets[] = {
        IORING_SETUP_SUBMIT_ALL | IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_DEFER_TASKRUN,
        IORING_SETUP_SUBMIT_ALL | IORING_SETUP_COOP_TASKRUN,
        IORING_SETUP_SUBMIT_ALL,
        0,
    };
    bool ready = false;
    for (const unsigned flags : flagSets) {
        if (plain && (flags & (IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_COOP_TASKRUN))) {
            continue;
        }
        if (io_uring_queue_init(entries, &copyRing->ring, flags) == 0) {
            ready = true;
            break;
        }
    }
    if (!ready) {
        return nullptr;
    }
    copyRing->bufferSize = size_t(copyRing->depth) * chunkSize;
    void *buffer = ::mmap(nullptr, copyRing->bufferSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buffer == MAP_FAILED) {
        io_uring_queue_exit(&copyRing->ring);
        return nullptr;
    }
    copyRing->buffer = static_cast<char *>(buffer);
    if (plain) {
        return copyRing.release();
    }

    // io_uring_enter then finds the ring without looking up its descriptor each time.
    io_uring_register_ring_fd(&copyRing->ring);

    // Registered memory counts against RLIMIT_MEMLOCK. When it is refused, the slots are used with
    // plain reads and writes.
    const struct iovec iov = {buffer, copyRing->bufferSize};
    copyRing->bufferRegistered = io_uring_register_buffers(&copyRing->ring, &iov, 1) == 0;

    copyRing->fixedFiles = io_uring_register_files_sparse(&copyRing->ring, entries) == 0;
    return copyRing.release();
}

CopyRing::~CopyRing()
{
    io_uring_queue_exit(&ring); // also drops the registered files and buffer
    if (buffer) {
        ::munmap(buffer, bufferSize);
    }
}

void CopyRingDeleter::operator()(CopyRing *ring) const
{
    delete ring;
}
#endif

namespace
{
// Copies each op and reports its ItemOutcome, so the worker can stream progress, defer
// per-item failures, and stop on a batch-fatal one. Conflicts are pre-filtered by the caller
// (overwrite policy). copyBatch returns the errno that aborted the batch, or 0 if it ran to
// completion. One source and one destination directory are held open at a time.
class BatchCopyEngine
{
public:
    BatchCopyEngine(PreserveFn preserve, gid_t egid, std::function<bool()> isKilled, bool tryReflink)
        : m_preserve(std::move(preserve))
        , m_egid(egid)
        , m_isKilled(std::move(isKilled))
        , m_request{.existing = ExistingDest::Refuse, .permissions = NewFilePermissions::FromSource, .tryReflink = tryReflink}
    {
    }
    // Called with the running byte total as each file is copied. Set separately from the
    // constructor, since the report gate it feeds is built after the engine.
    void setProgressHook(std::function<void(KIO::filesize_t)> onProgress)
    {
        m_onProgress = std::move(onProgress);
    }
    virtual ~BatchCopyEngine()
    {
        if (m_srcDirFd >= 0) {
            ::close(m_srcDirFd);
        }
        if (m_destDirFd >= 0) {
            ::close(m_destDirFd);
        }
    }
    int copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int index, ItemOutcome outcome, int err)> &onItem);
    // True once a file went to a folder on a removable device.
    bool copiedToRemovable() const
    {
        return m_copiedToRemovable;
    }

protected:
    using OnItem = std::function<void(int index, ItemOutcome outcome, int err)>;
    // How many ops from first on go to one copyRun. They all share the two folders of ops[first]
    // when there is more than one.
    virtual qsizetype runLength(const QList<BatchCopyOp> &ops, qsizetype first) const
    {
        Q_UNUSED(ops)
        Q_UNUSED(first)
        return 1;
    }
    // Copies ops[first] to ops[first + count - 1] and reports each with onItem. Returns the errno
    // that stops the batch, or 0.
    virtual int copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, KIO::filesize_t &bytesCopied, const OnItem &onItem);
    // Called with a source or destination folder just opened, once per folder.
    virtual void srcDirOpened(int dirFd, const QString &folder)
    {
        Q_UNUSED(dirFd)
        Q_UNUSED(folder)
    }
    virtual void destDirOpened(int dirFd, const QString &folder)
    {
        Q_UNUSED(dirFd)
        Q_UNUSED(folder)
    }

    // Keeps one source and one destination directory open at a time, so consecutive files in the
    // same directory (the common case) are reached with openat() instead of re-resolving the full
    // path. heldFd and heldPath are the directory currently open, and are replaced when another one
    // is opened. Returns the descriptor just opened, 0 when heldFd already pointed at dir, or -1
    // with errno set when dir cannot be opened. Descriptor 0 is the worker's standard input, so an
    // open() here always lands above it.
    static int holdDir(const QByteArray &dir, int &heldFd, QByteArray &heldPath)
    {
        if (heldFd >= 0 && dir == heldPath) {
            return 0;
        }
        const int fd = ::open(dir.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            return -1;
        }
        if (heldFd >= 0) {
            ::close(heldFd);
        }
        heldFd = fd;
        heldPath = dir;
        return fd;
    }
    bool ensureSrcDir(const QByteArray &dir)
    {
        const int openedFd = holdDir(dir, m_srcDirFd, m_srcDirPath);
        if (openedFd > 0) {
            srcDirOpened(openedFd, QFile::decodeName(dir.isEmpty() ? QByteArray(".") : dir));
        }
        return openedFd >= 0;
    }
    bool ensureDestDir(const QByteArray &dir)
    {
        const int openedFd = holdDir(dir, m_destDirFd, m_destDirPath);
        if (openedFd <= 0) {
            return openedFd == 0; // the directory was already open, and carries the group settled then
        }
        // Ask the dir fd just opened (rather than stat-ing the path) so the group a new file comes
        // out with is settled once per directory.
        m_destDirGroup = destDirGroupFor(openedFd, m_egid);
        // The same writeback as copy(): windows on a removable device or a network filesystem, and the
        // flush of the folder by the job when the folder is on a removable device.
        const QString folder = QFile::decodeName(dir.isEmpty() ? QByteArray(".") : dir);
        const WritebackPolicy writeback = writebackPolicyFor(openedFd, folder);
        m_request.writebackWindow = writeback.window;
        m_request.flushWindowedFile = writeback.flushWindowedFile;
        m_copiedToRemovable = m_copiedToRemovable || isFolderOnRemovableDevice(openedFd, folder);
        destDirOpened(openedFd, folder);
        return true;
    }

    // The copy of one file, the way the serial loop does it. Returns the errno that stops the batch, or 0.
    int copyOne(const BatchCopyOp &op, const QByteArray &srcName, const QByteArray &destName, KIO::filesize_t &bytesCopied, const OnItem &onItem);

    PreserveFn m_preserve; // applied to each successfully copied file (perms/times/owner/ACL)
    gid_t m_egid; // effective gid: the group new files get outside set-group-ID directories
    std::function<bool()> m_isKilled; // polled to abort promptly on cancellation
    std::function<void(KIO::filesize_t)> m_onProgress; // called with the running byte total as a file is copied
    CopyRequest m_request; // the same for every file of a batch: fresh files carrying the source's own metadata

    int m_srcDirFd = -1;
    QByteArray m_srcDirPath;
    int m_destDirFd = -1;
    QByteArray m_destDirPath;
    bool m_copiedToRemovable = false;
    DestDirGroup m_destDirGroup; // group files freshly created in m_destDirFd come out with
};

int BatchCopyEngine::copyOne(const BatchCopyOp &op, const QByteArray &srcName, const QByteArray &destName, KIO::filesize_t &bytesCopied, const OnItem &onItem)
{
    onItem(op.index, ItemOutcome::Begin, 0); // about to copy this file

    const CopyReport report =
        copyFileAt(m_srcDirFd, srcName, m_destDirFd, destName, m_destDirGroup, m_request, bytesCopied, m_preserve, m_isKilled, m_onProgress);
    switch (report.outcome) {
    case CopyOutcome::Copied:
        onItem(op.index, ItemOutcome::Done, 0);
        break;
    case CopyOutcome::Conflict:
        onItem(op.index, ItemOutcome::Conflict, 0);
        break;
    case CopyOutcome::Failed:
        if (report.err == ECANCELED) {
            return ECANCELED; // cancelled mid-file
        }
        if (isFatalCopyError(report.err)) {
            onItem(op.index, ItemOutcome::Abort, report.err);
            return report.err; // stop the batch
        }
        onItem(op.index, ItemOutcome::Defer, report.err);
        break;
    }
    return 0;
}

int BatchCopyEngine::copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int, ItemOutcome, int)> &onItem)
{
    for (qsizetype i = 0; i < ops.size();) {
        if (m_isKilled()) {
            return ECANCELED; // cancelled between files
        }
        const BatchCopyOp &op = ops.at(i);
        const QByteArray srcDir = splitDir(op.src).first;
        const QByteArray destDir = splitDir(op.dest).first;

        errno = 0;
        if (!ensureSrcDir(srcDir) || !ensureDestDir(destDir)) {
            const int err = errno; // could not open a directory: treat as a per-item failure
            if (isFatalCopyError(err)) {
                onItem(op.index, ItemOutcome::Abort, err);
                return err;
            }
            onItem(op.index, ItemOutcome::Defer, err);
            ++i;
            continue;
        }

        const qsizetype count = runLength(ops, i);
        if (const int err = copyRun(ops, i, count, bytesCopied, onItem)) {
            return err;
        }
        i += count;
    }
    return 0;
}

int BatchCopyEngine::copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, KIO::filesize_t &bytesCopied, const OnItem &onItem)
{
    for (qsizetype i = first; i < first + count; ++i) {
        const BatchCopyOp &op = ops.at(i);
        if (const int err = copyOne(op, splitDir(op.src).second, splitDir(op.dest).second, bytesCopied, onItem)) {
            return err;
        }
    }
    return 0;
}

#if HAVE_LIBURING
// Copies the files of a batch through copyRing, a window at a time: up to depth files that share
// both folders. The stages of copyRun put the requests of the whole window in flight at once.
class UringCopyEngine : public BatchCopyEngine
{
public:
    UringCopyEngine(CopyRing *copyRing, PreserveFn preserve, gid_t egid, std::function<bool()> isKilled, bool tryReflink)
        : BatchCopyEngine(std::move(preserve), egid, std::move(isKilled), tryReflink)
        , m_copyRing(copyRing)
    {
    }

protected:
    qsizetype runLength(const QList<BatchCopyOp> &ops, qsizetype first) const override;
    int copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, KIO::filesize_t &bytesCopied, const OnItem &onItem) override;
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
    bool m_destSlow = false; // m_destDirFd is on a network or other slow mount
    bool m_srcSlow = false; // m_srcDirFd is on a network or other slow mount
};

qsizetype UringCopyEngine::runLength(const QList<BatchCopyOp> &ops, qsizetype first) const
{
    const QByteArray srcDir = splitDir(ops.at(first).src).first;
    const QByteArray destDir = splitDir(ops.at(first).dest).first;
    qsizetype count = 1;
    while (first + count < ops.size() && count < m_copyRing->depth && splitDir(ops.at(first + count).src).first == srcDir
           && splitDir(ops.at(first + count).dest).first == destDir) {
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
int UringCopyEngine::copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, KIO::filesize_t &bytesCopied, const OnItem &onItem)
{
    struct File {
        QByteArray srcName;
        QByteArray destName;
        int srcFd = -1;
        int destFd = -1;
        int err = 0; // the first failure, 0 when there is none
        bool conflict = false; // the destination exists
        bool large = false; // left to copyFileAt
        bool counted = false; // its bytes are in bytesCopied
        bool created = false; // the window made the destination
        struct statx stx;
    };
    std::vector<File> files(count);
    for (qsizetype i = 0; i < count; ++i) {
        files[i].srcName = splitDir(ops.at(first + i).src).second;
        files[i].destName = splitDir(ops.at(first + i).dest).second;
    }

    const bool inlineOpens = m_copyRing->inlineLocalOpens && !m_srcSlow && !m_destSlow;

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
        if (m_copyRing->fixedFiles) {
            std::vector<int> cleared(size_t(2 * count), -1);
            io_uring_register_files_update(&m_copyRing->ring, 0, cleared.data(), unsigned(cleared.size()));
        }
        for (File &f : files) {
            if (f.srcFd >= 0) {
                ::close(f.srcFd);
            }
            if (f.destFd >= 0) {
                ::close(f.destFd);
            }
            if (f.created) {
                ::unlinkat(m_destDirFd, f.destName.constData(), 0);
            }
            if (f.counted) {
                bytesCopied -= f.stx.stx_size;
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
            const int fd = ::openat(m_srcDirFd, files[i].srcName.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            sourceDone(quint64(i) * 2, fd >= 0 ? fd : -errno);
            if (fd >= 0) {
                sourceDone(quint64(i) * 2 + 1, ::statx(fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, &files[i].stx) == 0 ? 0 : -errno);
            }
            continue;
        }
        struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_openat(sqe, m_srcDirFd, files[i].srcName.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC, 0);
        io_uring_sqe_set_data64(sqe, quint64(i) * 2);
        sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_statx(sqe, m_srcDirFd, files[i].srcName.constData(), 0, STATX_BASIC_STATS, &files[i].stx);
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
        if (qint64(f.stx.stx_size) > m_copyRing->maxFileSize) {
            f.large = true;
            continue;
        }
        const int flags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | O_EXCL;
        if (inlineOpens) {
            const int fd = ::openat(m_destDirFd, f.destName.constData(), flags, f.stx.stx_mode & 07777);
            destDone(quint64(i), fd >= 0 ? fd : -errno);
            continue;
        }
        struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
        io_uring_prep_openat(sqe, m_destDirFd, f.destName.constData(), flags, f.stx.stx_mode & 07777);
        io_uring_sqe_set_data64(sqe, quint64(i));
        ++queued;
    }
    if (const int err = submitAndReap(queued, destDone)) {
        return failWindow(err, destDone);
    }
    markKilled();

    // The sources and destinations of the window go in the registered file table, so the requests on
    // them below take a slot instead of a descriptor lookup each. The table holds a reference of its
    // own, which is dropped before the close of stage 5 so the close is the last one.
    std::vector<int> table;
    bool useTable = false;
    if (m_copyRing->fixedFiles) {
        table.resize(size_t(2 * count));
        for (qsizetype i = 0; i < count; ++i) {
            table[2 * i] = files[i].destFd >= 0 ? files[i].srcFd : -1;
            table[2 * i + 1] = files[i].destFd;
        }
        const int updated = io_uring_register_files_update(&m_copyRing->ring, 0, table.data(), unsigned(table.size()));
        useTable = updated == int(table.size());
        if (!useTable && updated > 0) {
            // Some slots took a reference, which would keep their files open past the close.
            std::fill(table.begin(), table.end(), -1);
            io_uring_register_files_update(&m_copyRing->ring, 0, table.data(), unsigned(table.size()));
        }
    }
    // The descriptor, or the table slot, a request on file i names.
    auto target = [&](qsizetype i, bool dest, struct io_uring_sqe *sqe) {
        if (useTable) {
            sqe->fd = int(2 * i + (dest ? 1 : 0));
            sqe->flags |= IOSQE_FIXED_FILE;
        }
    };

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
        if (m_copyRing->bufferRegistered) {
            io_uring_prep_read_fixed(sqe, f.srcFd, slot, length, offset[i], 0);
        } else {
            io_uring_prep_read(sqe, f.srcFd, slot, length, offset[i]);
        }
        io_uring_sqe_set_flags(sqe, IOSQE_IO_LINK);
        target(i, false, sqe);
        io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkRead);
        sqe = io_uring_get_sqe(&m_copyRing->ring);
        if (m_copyRing->bufferRegistered) {
            io_uring_prep_write_fixed(sqe, f.destFd, slot, length, offset[i], 0);
        } else {
            io_uring_prep_write(sqe, f.destFd, slot, length, offset[i]);
        }
        target(i, true, sqe);
        io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkWrite);
        queued += 2;
        inflight += 2;
        // The last window of a file is left to the fsync of stage 3b.
        if (syncWindow > 0 && end < f.stx.stx_size && end % syncWindow == 0) {
            io_uring_sqe_set_flags(sqe, sqe->flags | IOSQE_IO_LINK);
            sqe = io_uring_get_sqe(&m_copyRing->ring);
            constexpr unsigned writeAndWait = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE | SYNC_FILE_RANGE_WAIT_AFTER;
            io_uring_prep_sync_file_range(sqe, f.destFd, unsigned(syncWindow), end - syncWindow, writeAndWait);
            target(i, true, sqe);
            io_uring_sqe_set_data64(sqe, quint64(i) * ChunkRequests + ChunkSync);
            syncing[i] = true;
            ++queued;
            ++inflight;
        }
    };
    // Reported as the chunks are written, so a window of large files shows progress. Stage 4b
    // reports again only past what this reached.
    KIO::filesize_t windowWritten = 0;
    KIO::filesize_t reported = bytesCopied;
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
            if (m_onProgress && bytesCopied + windowWritten > reported) {
                reported = bytesCopied + windowWritten;
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
    const bool checkCached = m_copyRing->cachedCopyMaxSize >= 0 && !m_srcSlow && !m_destSlow;
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
        if (checkCached && qint64(f.stx.stx_size) < m_copyRing->cachedCopyMaxSize && isFullyCached(f.srcFd, f.stx.stx_size)) {
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
    if (m_destSlow && m_copyRing->dataSync) {
        queued = 0;
        for (qsizetype i = 0; i < count; ++i) {
            const File &f = files[i];
            if (f.destFd < 0 || f.stx.stx_size == 0 || retry[i] || f.err != 0) {
                continue;
            }
            struct io_uring_sqe *sqe = io_uring_get_sqe(&m_copyRing->ring);
            io_uring_prep_fsync(sqe, f.destFd, IORING_FSYNC_DATASYNC);
            target(i, true, sqe);
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
            bool readFailed = false;
            if (::lseek(f.srcFd, 0, SEEK_SET) != 0 || ::lseek(f.destFd, 0, SEEK_SET) != 0 || ::ftruncate(f.destFd, 0) != 0
                || !copyFds(f.srcFd, f.destFd, f.stx.stx_size, ignored, m_isKilled, {}, &readFailed)) {
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
    if (m_destSlow && m_copyRing->metadataPool && copied.size() > 1) {
        // A latch rather than waitForDone(), which also stops the threads of the pool.
        std::latch done(std::ptrdiff_t(copied.size()));
        for (const qsizetype i : copied) {
            m_copyRing->metadataPool->start([&preserveFile, &done, i] {
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
        bytesCopied += files[i].stx.stx_size;
        files[i].counted = true;
        if (m_onProgress && bytesCopied > reported) {
            m_onProgress(bytesCopied);
        }
    }

    // Stage 5: close everything. A network filesystem can report a failed write only here.
    if (useTable) {
        std::fill(table.begin(), table.end(), -1);
        io_uring_register_files_update(&m_copyRing->ring, 0, table.data(), unsigned(table.size()));
    }
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
        if (f.created && ::unlinkat(m_destDirFd, f.destName.constData(), 0) != 0 && errno != ENOENT) {
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << QFile::decodeName(f.destName);
        }
        f.created = false;
        if (f.counted) {
            bytesCopied -= f.stx.stx_size;
            f.counted = false;
        }
    };
    for (qsizetype i = 0; i < count; ++i) {
        File &f = files[i];
        const BatchCopyOp &op = ops.at(first + i);
        int stopErr = 0;
        if (f.large) {
            stopErr = copyOne(op, f.srcName, f.destName, bytesCopied, onItem);
        } else {
            onItem(op.index, ItemOutcome::Begin, 0);
            if (f.conflict) {
                onItem(op.index, ItemOutcome::Conflict, 0);
            } else if (f.err != 0) {
                discard(f);
                if (f.err == ECANCELED || isFatalCopyError(f.err)) {
                    if (f.err != ECANCELED) {
                        onItem(op.index, ItemOutcome::Abort, f.err);
                    }
                    stopErr = f.err;
                } else {
                    onItem(op.index, ItemOutcome::Defer, f.err);
                }
            } else {
                onItem(op.index, ItemOutcome::Done, 0);
            }
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
#endif

// NOTE: io_uring has no opcode for copy_file_range. UringCopyEngine moves the data of a file with
// reads and writes instead, so the whole file can go through the ring.
} // namespace

WorkerResult FileProtocol::batchCopy(QDataStream &stream)
{
    // Request: qint32 flags (reserved), qint32 count, count x (QString src, QString dest).
    // Reply: packets on the data channel, each starting with its kind, read by CopyJobPrivate.
    enum PacketKind : qint32 {
        PacketProgress = 0, // qint32 current, QList<qint32> finished since the previous packet
        PacketAnswer = 1, // QList<qint32>: the files the worker did not copy
    };
    qint32 flags = 0;
    qint32 count = 0;
    stream >> flags >> count;
    // The request arrives from the application, so its count decides a reservation and a loop here
    // and is checked before either. The cap is far above the largest batch a job sends.
    if (count < 0 || count > s_maxBatchCopyFiles) {
        return WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Malformed batch copy request"));
    }
    // bit0: the destination filesystem supports reflink (FICLONE). The Job sets it from the mount
    // it already looked up, so the worker never probes a filesystem that cannot clone. Existing
    // destinations are still always left to the caller (O_EXCL), so overwrite is not a concern here.
    const bool tryReflink = (flags & 0x1);

    QList<qint32> deferred; // files left for the caller: an existing destination, or one that failed

    // Existing destinations are detected by the engine's O_EXCL open and deferred, so no
    // destination access() pre-check is needed here.
    QList<BatchCopyOp> todo;
    todo.reserve(count);
    for (int i = 0; i < count; ++i) {
        QString src;
        QString dest;
        stream >> src >> dest;
        if (stream.status() != QDataStream::Ok) {
            return WorkerResult::fail(KIO::ERR_INTERNAL, i18n("Malformed batch copy request"));
        }
        todo.push_back({i, QFile::encodeName(src), QFile::encodeName(dest)});
    }

    // Group the batch by source directory so the engine's single-slot directory handles
    // (m_srcDirFd/m_destDirFd) stay warm: consecutive files then share a directory and are reached
    // with openat() instead of reopening it per file. The recursive caller already lists per
    // directory, but a flat copy of files gathered from several folders into one destination would
    // otherwise reopen the source directory on every file. Sorting on the source path keeps the
    // destination handle warm too (it is a single directory in the flat case, and mirrors the source
    // tree otherwise). op.index carries the original position, so deferrals still map back.
    std::sort(todo.begin(), todo.end(), [](const BatchCopyOp &a, const BatchCopyOp &b) {
        return a.src < b.src;
    });

    // Every copied file comes out carrying the source's metadata. Best-effort: a preservation
    // failure does not fail the copy. The umask and our euid are read once for the whole batch, and
    // the group a freshly created file already has is worked out per destination directory by the
    // engine and handed in.
    const gid_t egid = ::getegid();
    auto isKilled = [this] {
        return wasKilled();
    };
    std::unique_ptr<BatchCopyEngine> engine;
#if HAVE_LIBURING
    if (!m_copyRingTried) {
        m_copyRingTried = true;
        m_copyRing.reset(CopyRing::create());
    }
    if (m_copyRing) {
        engine = std::make_unique<UringCopyEngine>(m_copyRing.get(), makePreserveFn(this, ::geteuid()), egid, isKilled, tryReflink);
    }
#endif
    if (!engine) {
        engine = std::make_unique<BatchCopyEngine>(makePreserveFn(this, ::geteuid()), egid, isKilled, tryReflink);
    }

    // Progress + completion reporting, batched on one ~100ms gate so a large run does not flood the
    // app. Each tick sends one data packet holding "qint32 current, QList<qint32> done", where
    // current is the index of the file about to be copied, or -1 when there is none, and done holds
    // the indices of the files finished since the previous packet. The cumulative byte total goes
    // out on its own through processedSize. The final flush carries the remaining indices and the
    // exact total. Deferred and errored items are not reported here, they come back in the answer
    // packet.
    KIO::filesize_t bytesDone = 0;
    // The byte total the engine reported last. It runs ahead of bytesDone while the ring writes the
    // files of a window, which count in bytesDone only once their metadata is set.
    KIO::filesize_t bytesWritten = 0;
    QElapsedTimer reportTimer;
    QList<qint32> doneSinceReport;
    qint32 inFlight = -1; // file about to be / being copied
    auto flushReport = [&]() {
        processedSize(qMax(bytesDone, bytesWritten));
        if (inFlight < 0 && doneSinceReport.isEmpty()) {
            return;
        }
        QByteArray report;
        QDataStream out(&report, QIODevice::WriteOnly);
        out << qint32(PacketProgress) << inFlight << doneSinceReport;
        data(report);
        doneSinceReport.clear();
        reportTimer.restart();
    };
    // Bytes as they are copied, on the same gate: a file large enough to take seconds then advances
    // the byte total while it is being copied, instead of standing still until the next file starts.
    engine->setProgressHook([&](KIO::filesize_t total) {
        bytesWritten = qMax(bytesWritten, total);
        if (!reportTimer.isValid() || reportTimer.hasExpired(100)) {
            flushReport();
        }
    });

    const int abortErr = engine->copyBatch(todo, bytesDone, [&](int index, ItemOutcome outcome, int err) {
        switch (outcome) {
        case ItemOutcome::Begin:
            inFlight = index; // about to copy this file
            if (!reportTimer.isValid() || reportTimer.hasExpired(100)) {
                flushReport();
            }
            break;
        case ItemOutcome::Done:
            doneSinceReport.append(index);
            break;
        case ItemOutcome::Conflict:
            deferred.append(index); // dest exists: let the app resolve it
            break;
        case ItemOutcome::Defer:
        case ItemOutcome::Abort: // record the item that triggered the abort too
            deferred.append(index);
            break;
        }
    });
    inFlight = -1; // nothing in flight anymore; final flush carries the remaining completed indices
    bytesWritten = 0; // the exact total, without what was written for files then taken back
    flushReport();

    const bool copiedToRemovable = engine->copiedToRemovable();
    engine.reset();
#if HAVE_LIBURING
    // A ring that failed is dropped, and the next batch makes a fresh one.
    if (m_copyRing && m_copyRing->broken) {
        m_copyRing.reset();
        m_copyRingTried = false;
    }
#endif

    // The answer goes out once, when the batch is over. Why a file was left is not in it: the
    // caller copies it on its own and gets the conflict or the error from that attempt.
    if (copiedToRemovable) {
        // As in copy(): the job flushes the folder once its last file is copied. data() sends it along
        // with the answer, also when the batch fails.
        setMetaData(QStringLiteral("flush-destination"), QStringLiteral("true"));
    }
    QByteArray answer;
    QDataStream out(&answer, QIODevice::WriteOnly);
    out << qint32(PacketAnswer) << deferred;
    data(answer);

    if (abortErr == ECANCELED) {
        // Cancelled mid-batch (the app killed the job): stop, like copy() does on wasKilled().
        return WorkerResult::fail(KIO::ERR_USER_CANCELED, QString());
    }
    if (abortErr != 0) {
        // Batch-fatal error (disk full, read-only fs, ...): stop and fail the command, like
        // copy() does. Items already copied stay; the rest were not attempted.
        const int kioError = (abortErr == ENOSPC || abortErr == EDQUOT) ? KIO::ERR_DISK_FULL : KIO::ERR_CANNOT_WRITE;
        return WorkerResult::fail(kioError, QString::fromLocal8Bit(strerror(abortErr)));
    }
    return WorkerResult::pass();
}

WorkerResult FileProtocol::copy(const QUrl &_srcUrl, const QUrl &_destUrl, int _mode, JobFlags _flags)
{
    const QUrl srcUrl = localFileWithoutHostname(_srcUrl);
    const QUrl destUrl = localFileWithoutHostname(_destUrl);

    qCDebug(KIO_FILE) << "copy()" << srcUrl << "to" << destUrl << "mode=" << _mode;

    const QString src = srcUrl.toLocalFile();
    const QString dest = destUrl.toLocalFile();
    const QByteArray _src(QFile::encodeName(src));

    QT_STATBUF buffSrc;
    if (QT_STAT(_src.data(), &buffSrc) == -1) {
        if (errno == EACCES) {
            return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, src);
        } else {
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, src);
        }
    }

    if (S_ISDIR(buffSrc.st_mode)) {
        return WorkerResult::fail(KIO::ERR_IS_DIRECTORY, src);
    }
    if (S_ISFIFO(buffSrc.st_mode) || S_ISSOCK(buffSrc.st_mode)) {
        return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
    }

    // Pin the destination directory with an O_DIRECTORY descriptor and do every
    // following step (filesystem-type probe, existence check, create, publish)
    // relative to it. Combined with O_NOFOLLOW on the file itself, the copy then
    // operates on the exact directory and name we checked, instead of re-resolving
    // the path each time and possibly following a symlink swapped in meanwhile.
    const QByteArray _destDir = QFile::encodeName(destUrl.adjusted(QUrl::RemoveFilename).toLocalFile());
    const QByteArray destName = QFile::encodeName(destUrl.fileName());
    int dfd = ::open(_destDir.isEmpty() ? "." : _destDir.constData(), O_DIRECTORY | O_CLOEXEC);
    if (dfd < 0) {
        return WorkerResult::fail(errno == EACCES ? KIO::ERR_WRITE_ACCESS_DENIED : KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
    }
    const auto closeDfd = qScopeGuard([&dfd] {
        ::close(dfd);
    });

    // Look at the destination without following a final symlink. The mount it is on is asked for at the
    // same time, so that the filesystem-type probe below costs nothing of its own.
    StatStruct buffDest;
    const auto lookAtDest = [&] {
        return LSTATAT(dfd, destName.constData(), &buffDest, KIO::StatBasic | KIO::StatInode | KIO::StatMountId) == 0;
    };
    // The error for a destination that is there and is not replaced.
    const auto existingDestError = [&] {
        if (stat_ino(buffDest) == buffSrc.st_ino && stat_dev(buffDest) == buffSrc.st_dev) {
            return KIO::ERR_IDENTICAL_FILES;
        }
        if (S_ISDIR(stat_mode(buffDest))) {
            return KIO::ERR_DIR_ALREADY_EXIST;
        }
        return KIO::ERR_FILE_ALREADY_EXIST;
    };

    // Without Overwrite, the create refuses a name that is taken (O_EXCL), and the destination is
    // looked at only then. Each look is a round trip on a network filesystem.
    const bool overwrite = _flags & KIO::Overwrite;
    ExistingDest existing = ExistingDest::Refuse; // nothing is there: the create has the name to itself
    if (overwrite && lookAtDest()) {
        if (const auto error = existingDestError(); error != KIO::ERR_FILE_ALREADY_EXIST) {
            return WorkerResult::fail(error, dest);
        }
        if (S_ISLNK(stat_mode(buffDest))) {
            // Overwriting a symlink: unlink it so we never write through it to its
            // target. The create below refuses (O_EXCL) a symlink racing back in.
            if (::unlinkat(dfd, destName.constData(), 0) != 0 && errno != ENOENT) {
                return WorkerResult::fail(KIO::ERR_CANNOT_DELETE_ORIGINAL, dest);
            }
        } else if (S_ISREG(stat_mode(buffDest))) {
            // Write to a sibling ".part" and atomically rename it over the target so
            // an interrupted copy never leaves the destination truncated. CIFS keeps
            // the historical in-place overwrite.
            existing = isOnCifs(buffDest, dest) ? ExistingDest::Truncate : ExistingDest::Replace;
        }
    }

    const auto srcSize = buffSrc.st_size;
    totalSize(srcSize);

    const bool slowTestMode = testMode && dest.contains(QLatin1String("slow"));

    // KIO passes -1 to keep the system default permissions. Comparing in mode_t width
    // also matches FreeBSD, where a 16-bit mode_t delivers the sentinel as (mode_t)-1.
    const bool defaultPermissions = mode_t(_mode) == mode_t(-1);

    CopyRequest request;
    request.existing = existing;
    request.permissions = defaultPermissions ? NewFilePermissions::SystemDefault : NewFilePermissions::Explicit;
    request.explicitMode = mode_t(_mode);
    request.preserveOwner = !defaultPermissions;
    // The literal -1, not the mode_t-width test above: on FreeBSD the sentinel arrives as 65535,
    // and acl_equiv_mode_np() there does not report a plain access ACL as mode-equivalent, so a
    // copy asking for the system default would take its mode from the ACL of a source that has no
    // extended ACL at all.
    request.preserveAcl = (_mode == -1);
    request.tryReflink = !slowTestMode; // sharing the data is instant, and a slow copy is the point of the test mode
    const WritebackPolicy writeback = writebackPolicyFor(dfd, QFile::decodeName(_destDir.isEmpty() ? QByteArray(".") : _destDir));
    request.writebackWindow = writeback.window;
    request.flushWindowedFile = writeback.flushWindowedFile;

    const auto isKilled = [this] {
        return wasKilled();
    };
    // The test mode drags each chunk out, so a test has time to suspend or cancel a running copy.
    const auto onProgress = [this, slowTestMode](KIO::filesize_t copied) {
        processedSize(copied);
        if (slowTestMode) {
            QThread::msleep(50);
        }
    };

    KIO::filesize_t bytesCopied = 0;
    const CopyReport report = copyFileAt(AT_FDCWD,
                                         _src,
                                         dfd,
                                         destName,
                                         destDirGroupFor(dfd, ::getegid()),
                                         request,
                                         bytesCopied,
                                         makePreserveFn(this, ::geteuid()),
                                         isKilled,
                                         onProgress);

    switch (report.outcome) {
    case CopyOutcome::Copied:
        break;
    case CopyOutcome::Conflict:
        if (!overwrite && lookAtDest()) {
            return WorkerResult::fail(existingDestError(), dest);
        }
        // The name was free when the destination was looked at above, and is taken now.
        return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest);
    case CopyOutcome::Failed:
        // A cancel can land on any step, and it is the answer whatever step that was. Only a
        // transfer that had started leaves a destination file for copyFileAt to have removed.
        if (report.err == ECANCELED) {
            if (report.stage == CopyStage::Transfer) {
                qCDebug(KIO_FILE) << "Clean dest file after KIO worker was killed:" << dest;
            }
            return WorkerResult::fail(KIO::ERR_USER_CANCELED, dest);
        }
        switch (report.stage) {
        case CopyStage::OpenSource:
            return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
        case CopyStage::OpenDest:
            return WorkerResult::fail(report.err == EACCES ? KIO::ERR_WRITE_ACCESS_DENIED : KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
        case CopyStage::Transfer:
            if (report.readFailed) {
                return WorkerResult::fail(KIO::ERR_CANNOT_READ, src);
            }
            return WorkerResult::fail((report.err == ENOSPC || report.err == EDQUOT) ? KIO::ERR_DISK_FULL : KIO::ERR_CANNOT_WRITE, dest);
        case CopyStage::Publish:
            qCWarning(KIO_FILE) << "Couldn't rename the copy of" << src << "over" << dest << "(" << strerror(report.err) << ")";
            return WorkerResult::fail(KIO::ERR_CANNOT_WRITE, dest);
        }
        break;
    }

    processedSize(srcSize);
    if (isFolderOnRemovableDevice(dfd, QFile::decodeName(_destDir.isEmpty() ? QByteArray(".") : _destDir))) {
        // The data of the copy is still in the page cache. The job asks for the flush of the folder
        // once its last file is copied.
        setMetaData(QStringLiteral("flush-destination"), QStringLiteral("true"));
    }
    return WorkerResult::pass();
}
