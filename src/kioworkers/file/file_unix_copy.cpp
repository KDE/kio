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
#include "stat_unix.h"

#include "config-kioworker-file.h"

#if HAVE_POSIX_ACL
#include <../../aclhelpers_p.h>
#endif

#include <QFile>
#include <QScopeGuard>
#include <QThread>
#include <QUrl>
#include <qplatformdefs.h>

#include <KLocalizedString>
#include <QDebug>
#include <kmountpoint.h>

#include <QDataStream>
#include <QElapsedTimer>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/time.h>
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

#if HAVE_SYS_XATTR_H
#include <sys/xattr.h>
// BSD uses a different include
#elif HAVE_SYS_EXTATTR_H
#include <sys/types.h> // For FreeBSD, this must be before sys/extattr.h

#include <sys/extattr.h>
#endif

using namespace KIO;

/* 512 kB */
static constexpr int s_maxIPCSize = 1024 * 512;

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
bool FileProtocol::copyXattrs(const int src_fd, const int dest_fd)
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

// Also the unit of work between cancellation checks. Smaller chunks measured ~25-30% slower on
// large cold copies.
static constexpr size_t s_copyChunk = 512 * 1024;

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
             bool *readFailed = nullptr)
{
#if HAVE_COPY_FILE_RANGE
    const size_t chunk = s_copyChunk;
    KIO::filesize_t copied = 0; // bytes copied for this file by copy_file_range
    while (copied < size) {
        const size_t want = size_t(qMin<KIO::filesize_t>(size - copied, chunk));
        const ssize_t n = ::copy_file_range(sourceFd, nullptr, destFd, nullptr, want, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            break; // fall back to read/write for the remainder
        }
        if (n == 0) {
            break; // unexpected early EOF (source shrank underneath us); finish via read/write
        }
        copied += n;
        bytesCopied += n;
        if (onProgress) {
            onProgress(bytesCopied);
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
        if (onProgress) {
            onProgress(bytesCopied);
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
    // The created file is asked whether it already carries the mode wanted, rather than the
    // process for its umask, which costs a /proc read per file. An overwritten file kept its stale
    // bits, and open() does not reliably set the special bits, so both still go through fchmod.
    if (request.permissions != NewFilePermissions::SystemDefault) {
        const mode_t mode = (request.permissions == NewFilePermissions::Explicit ? request.explicitMode : mode_t(st.st_mode)) & 07777;
        bool createdModeAlreadyCorrect = false;
        if (destFreshlyCreated && (mode & mode_t(07000)) == 0) {
            struct stat created;
            createdModeAlreadyCorrect = ::fstat(destFd, &created) == 0 && (created.st_mode & mode_t(07777)) == mode;
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
    while (!cloned && !copyFds(sourceFd, destFd, st.st_size, bytesCopied, isKilled, onProgress, &readFailed)) {
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

    if (publishViaRename && ::renameat(destDirFd, outName.constData(), destDirFd, destName.constData()) != 0) {
        return {CopyOutcome::Failed, CopyStage::Publish, errno, false};
    }

    cleanupOutput.dismiss();
    return {CopyOutcome::Copied, CopyStage::Transfer, 0, false};
}

// Copies each op and reports its ItemOutcome, so the worker can stream progress, defer
// per-item failures, and stop on a batch-fatal one. Conflicts are pre-filtered by the caller
// (overwrite policy). copyBatch returns the errno that aborted the batch, or 0 if it ran to
// completion. One source and one destination directory are held open at a time.
class BatchCopyEngine
{
public:
    BatchCopyEngine(PreserveFn preserve, gid_t egid, std::function<bool()> isKilled)
        : m_preserve(std::move(preserve))
        , m_egid(egid)
        , m_isKilled(std::move(isKilled))
        , m_request{.existing = ExistingDest::Refuse, .permissions = NewFilePermissions::FromSource}
    {
    }
    // Called with the running byte total as each file is copied. Set separately from the
    // constructor, since the report gate it feeds is built after the engine.
    void setProgressHook(std::function<void(KIO::filesize_t)> onProgress)
    {
        m_onProgress = std::move(onProgress);
    }
    ~BatchCopyEngine()
    {
        if (m_srcDirFd >= 0) {
            ::close(m_srcDirFd);
        }
        if (m_destDirFd >= 0) {
            ::close(m_destDirFd);
        }
    }
    int copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int index, ItemOutcome outcome, int err)> &onItem);

private:
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
        return holdDir(dir, m_srcDirFd, m_srcDirPath) >= 0;
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
        return true;
    }

    PreserveFn m_preserve; // applied to each successfully copied file (perms/times/owner/ACL)
    gid_t m_egid; // effective gid: the group new files get outside set-group-ID directories
    std::function<bool()> m_isKilled; // polled to abort promptly on cancellation
    std::function<void(KIO::filesize_t)> m_onProgress; // called with the running byte total as a file is copied
    CopyRequest m_request; // the same for every file of a batch: fresh files carrying the source's own metadata

    int m_srcDirFd = -1;
    QByteArray m_srcDirPath;
    int m_destDirFd = -1;
    QByteArray m_destDirPath;
    DestDirGroup m_destDirGroup; // group files freshly created in m_destDirFd come out with
};

int BatchCopyEngine::copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int, ItemOutcome, int)> &onItem)
{
    for (const auto &op : ops) {
        if (m_isKilled()) {
            return ECANCELED; // cancelled between files
        }
        const auto [srcDir, srcName] = splitDir(op.src);
        const auto [destDir, destName] = splitDir(op.dest);

        errno = 0;
        if (!ensureSrcDir(srcDir) || !ensureDestDir(destDir)) {
            const int err = errno; // could not open a directory: treat as a per-item failure
            if (isFatalCopyError(err)) {
                onItem(op.index, ItemOutcome::Abort, err);
                return err;
            }
            onItem(op.index, ItemOutcome::Defer, err);
            continue;
        }

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
    }
    return 0;
}

// NOTE: io_uring has no opcode for copy_file_range, so it could only pipeline the metadata calls
// around a data move that still goes through copy_file_range. Measured that way it came out
// *slower* than copying serially for warm-cache local files, the metadata calls being cheap there
// already. It is worth another look once the data move itself can be submitted to a ring.
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
    // flags carries nothing yet. Existing destinations are always left to the caller (the engine
    // opens every destination O_EXCL), so overwrite is not a batch concern.
    Q_UNUSED(flags)

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
    auto engine = std::make_unique<BatchCopyEngine>(makePreserveFn(this, ::geteuid()), egid, [this] {
        return wasKilled();
    });

    // Progress + completion reporting, batched on one ~100ms gate so a large run does not flood the
    // app. Each tick sends one data packet holding "qint32 current, QList<qint32> done", where
    // current is the index of the file about to be copied, or -1 when there is none, and done holds
    // the indices of the files finished since the previous packet. The cumulative byte total goes
    // out on its own through processedSize. The final flush carries the remaining indices and the
    // exact total. Deferred and errored items are not reported here, they come back in the answer
    // packet.
    KIO::filesize_t bytesDone = 0;
    QElapsedTimer reportTimer;
    QList<qint32> doneSinceReport;
    qint32 inFlight = -1; // file about to be / being copied
    auto flushReport = [&]() {
        processedSize(bytesDone);
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
    engine->setProgressHook([&](KIO::filesize_t) {
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
    flushReport();

    // The answer goes out once, when the batch is over. Why a file was left is not in it: the
    // caller copies it on its own and gets the conflict or the error from that attempt.
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
    const bool dest_exists = (LSTATAT(dfd, destName.constData(), &buffDest, KIO::StatBasic | KIO::StatInode | KIO::StatMountId) == 0);
    ExistingDest existing = ExistingDest::Refuse; // nothing is there: the create has the name to itself
    if (dest_exists) {
        if (stat_ino(buffDest) == buffSrc.st_ino && stat_dev(buffDest) == buffSrc.st_dev) {
            return WorkerResult::fail(KIO::ERR_IDENTICAL_FILES, dest);
        }
        if (S_ISDIR(stat_mode(buffDest))) {
            return WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, dest);
        }
        if (!(_flags & KIO::Overwrite)) {
            return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest);
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
    return WorkerResult::pass();
}
