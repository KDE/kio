/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILE_UNIX_COPY_P_H
#define FILE_UNIX_COPY_P_H

// What the batch copy engines of the file worker share.

#include "config-kioworker-file.h"

#include <kio/global.h>

#include <QFile>
#include <QList>
#include <QString>

#include <chrono>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#if HAVE_LIBURING
#include <QThreadPool>
#include <liburing.h>
#endif

// One file of a batch copy command, which copies many local files in one worker command.
struct BatchCopyOp {
    int index; // position in the request
    QString src;
    QString dest;
};

// What became of one file of a batch.
enum class BatchItemResult {
    Copied,
    Conflict, // the destination exists, the job resolves it
    Deferred, // this file failed, the job copies it on its own
    BatchFailed, // this file failed in a way that stops the whole batch
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

struct DestDirGroup {
    gid_t createdGid = 0;
    bool imposedByDir = false;
};

using PreserveFn =
    std::function<void(int sourceFd, int destFd, const struct stat &st, const CopyRequest &request, bool destFreshlyCreated, DestDirGroup destDirGroup)>;

using KilledFn = std::function<bool()>;

// An error that every next file of the batch would hit as well.
bool isFatalBatchCopyError(int err);

// size, from the caller's fstat, bounds the loop so it stops at EOF without an extra probing call.
// On cancel errno is ECANCELED; on failure errno is left set and readFailed, if given, says which
// end failed.
bool copyFds(int sourceFd,
             int destFd,
             KIO::filesize_t size,
             KIO::filesize_t &bytesCopied,
             const std::function<bool()> &isKilled,
             const std::function<void(KIO::filesize_t)> &onProgress = {},
             bool *readFailed = nullptr,
             KIO::filesize_t writebackWindow = 0);

// What a copy to a folder needs to know about the mount that holds it.
struct MountTraits {
    bool removable = false;
    bool cifs = false;
    bool slow = false;
};

// The traits of the mount that holds the folder. The mount table gives them, so the server of a
// network filesystem is not asked. They are kept for each mount, whose unique id is never given to
// another mount, or else for each device. Linux only.
MountTraits mountTraitsFor(int dirFd, const QString &dir);

// How often a copy that waits for a window checks whether it was cancelled.
static constexpr std::chrono::milliseconds s_cancelCheck{100};

// A folder held open by its path, while the files in it are copied.
class HeldDir
{
public:
    enum class HoldResult {
        Failed, // errno is set
        AlreadyHeld,
        Opened, // in place of the folder held before
    };

    HeldDir() = default;
    ~HeldDir()
    {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }
    Q_DISABLE_COPY_MOVE(HeldDir)

    HoldResult hold(const QString &dir)
    {
        if (m_fd >= 0 && dir == m_path) {
            return HoldResult::AlreadyHeld;
        }
        const int fd = ::open(QFile::encodeName(dir).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            return HoldResult::Failed;
        }
        if (m_fd >= 0) {
            ::close(m_fd);
        }
        m_fd = fd;
        m_path = dir;
        return HoldResult::Opened;
    }
    int fd() const
    {
        return m_fd;
    }

private:
    int m_fd = -1;
    QString m_path;
};

// Copies the files of a batch one after the other, holding one source and one destination folder
// open at a time.
class BatchCopyEngine
{
public:
    using ItemStartFn = std::function<void(int index)>;
    using ItemResultFn = std::function<void(int index, BatchItemResult result)>;

    BatchCopyEngine(PreserveFn preserve, gid_t egid, KilledFn isKilled, bool tryReflink)
        : m_preserve(std::move(preserve))
        , m_egid(egid)
        , m_isKilled(std::move(isKilled))
        , m_request{.tryReflink = tryReflink}
    {
    }
    // Called with the running byte total while a file is copied.
    void setProgressHook(std::function<void(KIO::filesize_t)> onProgress)
    {
        m_onProgress = std::move(onProgress);
    }
    virtual ~BatchCopyEngine() = default;
    // Returns the errno that stopped the batch, or 0 once every file was handled.
    int copyFiles(const QList<BatchCopyOp> &ops, const ItemStartFn &onItemStart, const ItemResultFn &onItemResult);
    // The bytes of the files copied so far.
    KIO::filesize_t bytesCopied() const
    {
        return m_bytesCopied;
    }
    // True once a file went to a folder on a removable device.
    bool copiedToRemovable() const
    {
        return m_copiedToRemovable;
    }

protected:
    // How many ops from first on go to one copyRun. They all share the two folders of ops[first]
    // when there is more than one.
    virtual qsizetype runLength(const QList<BatchCopyOp> &ops, qsizetype first) const
    {
        Q_UNUSED(ops)
        Q_UNUSED(first)
        return 1;
    }
    // Copies ops[first] to ops[first + count - 1] and reports each file. Returns the errno that
    // stops the batch, or 0.
    virtual int copyRun(const QList<BatchCopyOp> &ops, qsizetype first, qsizetype count, const ItemStartFn &onItemStart, const ItemResultFn &onItemResult);
    // Called with a source or destination folder just opened, once per folder.
    virtual void srcDirOpened(int dirFd, const QString &dir)
    {
        Q_UNUSED(dirFd)
        Q_UNUSED(dir)
    }
    virtual void destDirOpened(int dirFd, const QString &dir)
    {
        Q_UNUSED(dirFd)
        Q_UNUSED(dir)
    }

    // The copy of one file, the way the serial loop does it. Returns the errno that stops the batch,
    // or 0.
    int copyOne(const BatchCopyOp &op, const ItemStartFn &onItemStart, const ItemResultFn &onItemResult);

    // False with errno set when dir cannot be opened.
    bool ensureSrcDir(const QString &dir)
    {
        const HeldDir::HoldResult result = m_srcDir.hold(dir);
        if (result == HeldDir::HoldResult::Opened) {
            srcDirOpened(m_srcDir.fd(), dir);
        }
        return result != HeldDir::HoldResult::Failed;
    }
    bool ensureDestDir(const QString &dir)
    {
        const HeldDir::HoldResult result = m_destDir.hold(dir);
        if (result == HeldDir::HoldResult::Opened) {
            settleDestDir(dir);
            destDirOpened(m_destDir.fd(), dir);
        }
        return result != HeldDir::HoldResult::Failed;
    }
    // What the files copied to the folder just opened have in common.
    void settleDestDir(const QString &dir);

    PreserveFn m_preserve; // applies the permissions, times, owner and ACL of the source to each copied file
    gid_t m_egid;
    KilledFn m_isKilled;
    std::function<void(KIO::filesize_t)> m_onProgress;
    KIO::filesize_t m_bytesCopied = 0;
    CopyRequest m_request; // the same for every file of the batch

    HeldDir m_srcDir;
    HeldDir m_destDir;
    bool m_copiedToRemovable = false;
    DestDirGroup m_destDirGroup; // the group of a file created in m_destDir
};

#if HAVE_LIBURING
// The ring of the batch copy. It lives as long as the worker, and only the worker's thread
// submits to it.
class CopyRing
{
public:
    // nullptr when KIO_BATCH_URING=0, or when the kernel or a seccomp filter refuses io_uring.
    static CopyRing *create();
    ~CopyRing();

    struct io_uring ring;
    static constexpr int depth = 32; // files of one window
    // Between two local folders, a source smaller than this whose data is all in the page cache is
    // copied with copy_file_range: once in the kernel, where a read and a write copy it twice. From
    // 1 MiB the ring is faster even then, since its worker threads copy the files of a window on
    // several CPUs where copy_file_range copies one file at a time. The check is one cachestat
    // call, about as costly as an fstat below 1 MiB. A slow destination keeps the ring, where the
    // writes of a window overlap.
    static constexpr qint64 cachedCopyMaxSize = 1024 * 1024;
    // One slot of chunkSize per file of a window. The data of a file is read into its slot and
    // written from it a chunk at a time.
    static constexpr size_t chunkSize = 64 * 1024;
    char *buffer = nullptr;
    size_t bufferSize = 0;
    // Sets the permissions, times, owner and extended attributes of the files of a window on a
    // slow destination at the same time. io_uring has no request for them, and on a network
    // filesystem each one waits for the server.
    QThreadPool metadataPool;
    // Set when a submission or a wait failed. The worker drops the ring after the batch, and the
    // next batch makes a fresh one.
    bool broken = false;
};

// Copies the files of a batch through copyRing, a window of files that share both folders at a time.
std::unique_ptr<BatchCopyEngine> makeUringCopyEngine(CopyRing *copyRing, PreserveFn preserve, gid_t egid, KilledFn isKilled, bool tryReflink);
#endif

#endif // FILE_UNIX_COPY_P_H
