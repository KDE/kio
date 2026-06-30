/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Ehrlicher <ch.ehrlicher@gmx.de>
    SPDX-FileCopyrightText: 2021-2024 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "file.h"
#include "stat_unix.h"

#include "config-kioworker-file.h"

#include "../utils_p.h"

#if HAVE_POSIX_ACL
#include <../../aclhelpers_p.h>
#endif

#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QThread>
#include <qplatformdefs.h>

#include <KConfigGroup>
#include <KFileSystemType>
#include <KLocalizedString>
#include <QDebug>
#include <kmountpoint.h>

#include <QDataStream>
#include <QElapsedTimer>

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <functional>
#include <memory>
#include <stdint.h>
#include <sys/stat.h>
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

static bool same_inode(const QT_STATBUF &src, const QT_STATBUF &dest)
{
    if (src.st_ino == dest.st_ino && src.st_dev == dest.st_dev) {
        return true;
    }

    return false;
}

#if HAVE_POSIX_ACL
bool FileProtocol::isExtendedACL(acl_t acl)
{
    return (ACLPortability::acl_equiv_mode(acl, nullptr) != 0);
}
#endif

static bool isOnCifsMount(const QString &filePath)
{
    const auto mount = KMountPoint::currentMountPoints().findByPath(filePath);
    if (!mount) {
        return false;
    }
    return mount->mountType() == QStringLiteral("cifs") || mount->mountType() == QStringLiteral("smb3");
}

#if HAVE_STATX
// statx syscall is available
using StatStruct = struct statx;
#else
using StatStruct = QT_STATBUF;
#endif

static QByteArray readlinkToBuffer(const StatStruct &buf, const QByteArray &path)
{
    // Use readlink on Unix because symLinkTarget turns relative targets into absolute (#352927)
    size_t size = stat_size(buf);
    if (size > SIZE_MAX) {
        qCWarning(KIO_FILE) << "file size bigger than SIZE_MAX, too big for readlink use!" << path;
        return {};
    }
    size_t lowerBound = 256;
    size_t higherBound = 1024;
    size_t bufferSize = qBound(lowerBound, size + 1, higherBound);
    QByteArray linkTargetBuffer(bufferSize, Qt::Initialization::Uninitialized);
    while (true) {
        ssize_t n = readlink(path.constData(), linkTargetBuffer.data(), bufferSize);
        if (n < 0 && errno != ERANGE) {
            /* On AIX 5L v5.3 and HP-UX 11i v2 04/09, readlink returns -1
               with errno == ERANGE if the buffer is too small.
               According to gnulib/lib/areadlink-with-size.c */
            qCWarning(KIO_FILE) << "readlink failed!" << path;
            return {};
        } else if (n > 0 && static_cast<size_t>(n) != bufferSize) {
            // the buffer was not filled in the last iteration
            // we are finished reading, break the loop
            linkTargetBuffer.truncate(n);
            break;
        }
        bufferSize *= 2;
        linkTargetBuffer.resize(bufferSize);
    }
    return linkTargetBuffer;
}

static bool createUDSEntry(const QString &filename, const QByteArray &path, UDSEntry &entry, KIO::StatDetails details, const QString &fullPath)
{
    assert(entry.count() == 0); // by contract :-)
    int entries = 0;
    if (details & KIO::StatBasic) {
        // filename, access, type, size, linkdest
        entries += 5;
    }
    if (details & KIO::StatUser) {
        // uid, gid
        entries += 2;
    }
    if (details & KIO::StatTime) {
        // atime, mtime, btime
        entries += 3;
        if ((details & KIO::StatTimeNsOffset) == KIO::StatTimeNsOffset) {
            // atime, mtime, btime ns offsets
            entries += 3;
        }
    }
    if (details & KIO::StatAcl) {
        // acl data
        entries += 3;
    }
    if (details & KIO::StatInode) {
        // dev, inode
        entries += 2;
    }
    if (details & KIO::StatMimeType) {
        // mimetype
        entries += 1;
    }
#if HAVE_STATX_SUBVOL
    if (details & KIO::StatSubVolId) {
        entries += 1;
    }
#endif
#if HAVE_STATX_MNT_ID_UNIQUE
    if (details & KIO::StatMountId) {
        entries += 1;
    }
#endif
    entry.reserve(entries);

    if (details & KIO::StatBasic) {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
    }

    bool isBrokenSymLink = false;
#if HAVE_POSIX_ACL
    QByteArray targetPath = path;
#endif

    StatStruct buff;

    if (LSTAT(path.constData(), &buff, details) == 0) {
        if (Utils::isLinkMask(stat_mode(buff))) {
            QByteArray linkTargetBuffer;
            if (details & (KIO::StatBasic | KIO::StatResolveSymlink)) {
                linkTargetBuffer = readlinkToBuffer(buff, path);
                if (linkTargetBuffer.isEmpty()) {
                    return false;
                }
                const QString linkTarget = QFile::decodeName(linkTargetBuffer);
                entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, linkTarget);
            }

            // A symlink
            if (details & KIO::StatResolveSymlink) {
                if (STAT(path.constData(), &buff, details) == -1) {
                    isBrokenSymLink = true;
                } else {
#if HAVE_POSIX_ACL
                    if (details & KIO::StatAcl) {
                        // valid symlink, will get the ACLs of the destination
                        targetPath = linkTargetBuffer;
                    }
#endif
                }
            }
        }
    } else {
        // qCWarning(KIO_FILE) << "lstat didn't work on " << path.data();
        return false;
    }

    mode_t type = 0;
    if (details & (KIO::StatBasic | KIO::StatAcl)) {
        mode_t access;
        signed long long size;
        if (isBrokenSymLink) {
            // It is a link pointing to nowhere
            type = S_IFMT - 1;
            access = S_IRWXU | S_IRWXG | S_IRWXO;
            size = 0LL;
        } else {
            type = stat_mode(buff) & S_IFMT; // extract file type
            access = stat_mode(buff) & 07777; // extract permissions
            size = stat_size(buff);
        }

        if (details & KIO::StatBasic) {
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, access);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, size);
        }

#if HAVE_POSIX_ACL
        if (details & KIO::StatAcl) {
            /* Append an atom indicating whether the file has extended acl information
             * and if withACL is specified also one with the acl itself. If it's a directory
             * and it has a default ACL, also append that. */
            appendACLAtoms(targetPath, entry, type);
        }
#endif
    }

    if (details & KIO::StatUser) {
        const auto uid = stat_uid(buff);
        const auto gid = stat_gid(buff);
        entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_USER_ID, uid);
        entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_GROUP_ID, gid);
    }

    if (details & KIO::StatTime) {
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, stat_mtime(buff));
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, stat_atime(buff));

        if ((details & KIO::StatTimeNsOffset) == KIO::StatTimeNsOffset) {
            entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME_NS_OFFSET, stat_mtime_ns(buff));
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME_NS_OFFSET, stat_atime_ns(buff));
        }

#ifdef st_birthtime
        /* For example FreeBSD's and NetBSD's stat contains a field for
         * the inode birth time: st_birthtime
         * This however only works on UFS and ZFS, and not, on say, NFS.
         * Instead of setting a bogus fallback like st_mtime, only use
         * it if it is greater than 0. */
        if (buff.st_birthtime > 0) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.st_birthtime);

            if ((details & KIO::StatTimeNsOffset) == KIO::StatTimeNsOffset) {
                entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME_NS_OFFSET, buff.st_birthtimespec.tv_nsec);
            }
        }

#elif defined __st_birthtime
        /* As above, but OpenBSD calls it slightly differently. */
        if (buff.__st_birthtime > 0) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.__st_birthtime);

            if ((details & KIO::StatTimeNsOffset) == KIO::StatTimeNsOffset) {
                entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME_NS_OFFSET, buff.__st_birthtimensec);
            }
        }

#elif HAVE_STATX
        /* And linux version using statx syscall */
        if (buff.stx_mask & STATX_BTIME) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.stx_btime.tv_sec);

            if ((details & KIO::StatTimeNsOffset) == KIO::StatTimeNsOffset) {
                entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME_NS_OFFSET, buff.stx_btime.tv_nsec);
            }
        }
#endif
    }

    if (details & KIO::StatInode) {
        entry.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, stat_dev(buff));
        entry.fastInsert(KIO::UDSEntry::UDS_INODE, stat_ino(buff));
    }

    if (details & KIO::StatMimeType) {
        if (type == 0 || type != S_IFDIR) {
            QMimeDatabase db;
            entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, db.mimeTypeForFile(fullPath).name());
        } else {
            // fast path for directories
            entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        }
    }

#if HAVE_STATX_SUBVOL
    if (details & KIO::StatSubVolId) {
        entry.fastInsert(KIO::UDSEntry::UDS_SUBVOL_ID, stat_subvol(buff));
    }
#endif

#if HAVE_STATX_MNT_ID_UNIQUE
    if (details & KIO::StatMountId) {
        entry.fastInsert(KIO::UDSEntry::UDS_MOUNT_ID, stat_mnt_id(buff));
    }
#endif

    return true;
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

// --- Batched copy ---------------------------------------------------------------------------
// Copies a list of local files in one worker command (the special() sub-command, see batchCopy()
// below) instead of one file_copy job per file, amortizing the per-file job/IPC cost that dominates
// many-small-files copies. Mirrors copy()'s preservation (permissions/mtime/ownership/ACL/xattrs)
// and read/write fallback, copies in cancellable chunks, and only creates fresh files - deferring
// any existing destination back to copy() for safe overwrite.
namespace
{
struct BatchCopyOp {
    int index; // position in the original request, used when reporting deferrals
    QByteArray src;
    QByteArray dest;
};

enum BatchDeferReason {
    DeferConflict = 1, // destination exists and Overwrite was not requested
    DeferError = 2, // copy failed for this item (errno reported alongside); rest of batch continues
};

enum class ItemOutcome {
    Begin, // about to copy this file (drives the current-file report; emitted before the copy)
    Done,
    Conflict, // destination already exists and Overwrite was not requested
    Defer, // per-item recoverable failure: skip this file, keep going
    Abort, // batch-fatal failure: stop now
};

// Result of copying one file. Conflict is split out so the worker can report it as DeferConflict
// (the app may resolve it) rather than a copy error.
enum class CopyOutcome {
    Copied,
    Conflict,
    Failed, // errno is set
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

// Transfer size limit per copy_file_range/read call, and the unit of work between wasKilled()
// checks. 512 KiB balances throughput against cancellation latency: a page-size chunk measured
// ~25-30% slower on large cold copies (the extra copy_file_range syscalls add up), while 512 KiB
// recovers that and still bounds a cancel to about one chunk - sub-millisecond on an SSD, ~10 ms on
// a hard disk. Files no larger than this (the common small-file case) are copied in a single call.
static constexpr size_t s_batchCopyChunk = 512 * 1024;

// Copy contents between two already-open fds: copy_file_range, with a read/write fallback for
// the cases it rejects (cross-filesystem -> EXDEV, unsupported -> ENOSYS, etc.). size is this
// file's size (from the caller's fstat), so the loop stops the moment the file is fully copied
// instead of issuing one more copy_file_range just to observe the 0-byte EOF return - and copies
// nothing at all for an empty file. The transfer is chunked and isKilled() is polled between
// chunks so a cancel takes effect quickly; on cancel, errno is set to ECANCELED and false is
// returned. bytesCopied is the caller's running total (across the whole batch, for progress); the
// loop is driven by a per-file counter, not by it. errno is left set on failure.
bool copyFds(int sourceFd, int destFd, [[maybe_unused]] KIO::filesize_t size, KIO::filesize_t &bytesCopied, const std::function<bool()> &isKilled)
{
#if HAVE_COPY_FILE_RANGE
    const size_t chunk = s_batchCopyChunk;
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
        // Poll for cancellation only between chunks, so a single-chunk file (the common small-file
        // case) does no extra work while a large file stays promptly interruptible.
        if (copied < size && isKilled && isKilled()) {
            errno = ECANCELED;
            return false;
        }
    }
    if (copied >= size) {
        return true;
    }
#endif
    // Read/write path: the remainder copy_file_range did not handle, or the whole file where
    // copy_file_range is unavailable (macOS, OpenBSD, older Linux). read() and write() may be cut
    // short by a signal (EINTR) or, for write(), a partial transfer, so retry them rather than
    // treating a short result as failure.
    QByteArray buffer(256 * 1024, Qt::Uninitialized);
    while (true) {
        const ssize_t n = ::read(sourceFd, buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false; // genuine read error
        }
        if (n == 0) {
            break; // end of file
        }
        if (isKilled && isKilled()) {
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
                return false; // genuine write error
            }
            written += w;
        }
        bytesCopied += n;
    }
    return true;
}

// Reads the process umask without the racy umask(x)/umask(old) toggle: the file worker can run as
// a thread inside the application's process, where momentarily clearing the umask would affect
// files other threads create at the same time. Linux exposes it in /proc/self/status since 4.7.
// Where it cannot be read we return 0777, which makes preserveAttrs treat every permission bit as
// possibly masked and always fchmod (the safe default, same as before this optimization).
mode_t currentUmask()
{
#ifdef Q_OS_LINUX
    QFile status(QStringLiteral("/proc/self/status"));
    if (status.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!status.atEnd()) {
            const QByteArray line = status.readLine();
            if (line.startsWith("Umask:")) {
                bool ok = false;
                const int bits = line.mid(6).trimmed().toInt(&ok, 8);
                if (ok) {
                    return mode_t(bits);
                }
                break; // malformed line: fall back to the safe 0777 default below
            }
        }
    }
#endif
    return 0777;
}

// Best-effort metadata preservation, mirroring copy() in file_unix.cpp: permissions, timestamps,
// ownership and (where supported) the POSIX ACL. Done with both fds still open so the changes land
// on the file just written, not on whatever the path resolves to now. Failures are non-fatal (a
// copy that loses its mtime is still a successful copy), matching copy()'s warn-and-continue
// behaviour. Extended attributes are copied by the caller via FileProtocol::copyXattrs(), which
// already handles the namespace/ENOTSUP quirks.
void preserveAttrs([[maybe_unused]] int sourceFd, int destFd, const struct stat &st, bool destFreshlyCreated, mode_t umaskBits, uid_t euid, gid_t createdGid)
{
    // Permissions. The open() that created the file already applied (mode & ~umask), so the fchmod
    // is redundant - and skippable - exactly when that result already equals the source mode: the
    // file was freshly created (an overwritten file kept its stale bits), the umask stripped none
    // of the source bits, and there are no special bits (setuid/setgid/sticky), which open() does
    // not reliably set from its mode argument.
    const mode_t mode = st.st_mode & 07777;
    const bool createdModeAlreadyCorrect = destFreshlyCreated && (mode & (umaskBits | mode_t(07000))) == 0;
    if (!createdModeAlreadyCorrect && ::fchmod(destFd, mode) != 0) {
        qCWarning(KIO_FILE) << "batch copy: could not preserve permissions:" << strerror(errno);
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
        qCWarning(KIO_FILE) << "batch copy: could not preserve timestamps:" << strerror(errno);
    }

    // Ownership. Like the fchmod above, open() already gave the new file an owner and group, so
    // each fchown is skippable when it already matches the source: the created owner is always our
    // euid, and the created group is createdGid (our egid, or the destination directory's group
    // when that directory is set-group-ID - the caller works that out). Group first, then owner,
    // so a non-root worker can still set the group before a (possibly denied) owner change. As with
    // copy(), preserving the owner is best-effort and just warns when not permitted.
    if (st.st_gid != createdGid && ::fchown(destFd, -1 /*keep user*/, st.st_gid) != 0) {
        qCWarning(KIO_FILE) << "batch copy: could not preserve group:" << strerror(errno);
    }
    if (st.st_uid != euid && ::fchown(destFd, st.st_uid, -1 /*keep group*/) != 0) {
        qCWarning(KIO_FILE) << "batch copy: could not preserve owner:" << strerror(errno);
    }

#if HAVE_POSIX_ACL && !HAVE_SYS_XATTR_H
    // Carry over the source's access ACL explicitly only where copyXattrs cannot: BSD extattr
    // lists just the USER namespace, so the ACL (a system attribute) would not be copied. On
    // Linux and other HAVE_SYS_XATTR_H platforms listxattr returns system.posix_acl_access, so
    // copyXattrs already replicates the ACL and doing it here would be a redundant get/set (plus
    // an acl_t allocation) on every file.
    if (acl_t acl = acl_get_fd(sourceFd)) {
        if (acl_set_fd(destFd, acl) != 0) {
            qCWarning(KIO_FILE) << "batch copy: could not preserve ACL:" << strerror(errno);
        }
        acl_free(acl);
    }
#endif
}

// preserve(sourceFd, destFd, st, destFreshlyCreated, createdGid) runs after a successful content copy, with
// both fds open. The engine is agnostic to what it does; the worker injects one that mirrors
// copy()'s preservation. destFreshlyCreated lets it skip a redundant fchmod, and createdGid is the
// group open() already gave the new file (worked out once per destination directory, see below).
using PreserveFn = std::function<void(int sourceFd, int destFd, const struct stat &st, bool destFreshlyCreated, gid_t createdGid)>;

// Splits an absolute encoded path into (parent directory, base name). The directory is "/" for a
// root-level entry. Used to open each directory once and openat() the files within it.
std::pair<QByteArray, QByteArray> splitDir(const QByteArray &path)
{
    const int slash = path.lastIndexOf('/');
    if (slash <= 0) {
        return {QByteArrayLiteral("/"), path.mid(slash + 1)};
    }
    return {path.left(slash), path.mid(slash + 1)};
}

// Copies one file by name, relative to already-open source and destination directory fds. Opening
// each directory once and using openat() for every file in it spares the kernel from re-resolving
// the shared directory prefix on each open. createdGid is the group a freshly created file already
// has in destDirFd (its parent directory), so preserveAttrs can skip a redundant fchown.
CopyOutcome copyOneFileAt(int srcDirFd,
                          const QByteArray &srcName,
                          int destDirFd,
                          const QByteArray &destName,
                          gid_t createdGid,
                          KIO::filesize_t &bytesCopied,
                          const PreserveFn &preserve,
                          const std::function<bool()> &isKilled,
                          bool tryReflink)
{
    const int sourceFd = ::openat(srcDirFd, srcName.constData(), O_RDONLY | O_CLOEXEC);
    if (sourceFd < 0) {
        return CopyOutcome::Failed;
    }
    // Close on every return path, preserving errno across close() so it cannot mask the error we return.
    auto cleanupSourceFd = qScopeGuard([sourceFd] {
        const int e = errno;
        ::close(sourceFd);
        errno = e;
    });
    struct stat st;
    if (::fstat(sourceFd, &st) != 0) {
        return CopyOutcome::Failed;
    }
#if HAVE_FADVISE
    // Hint sequential access so the kernel reads ahead, but only for files large enough to benefit:
    // measured cold on an SSD this saves ~10-18% from ~16 MiB up and is noise below ~1 MiB, so gate
    // it at several chunks (4 MiB). Worth the extra syscall only then. (Source only - the same
    // measurement showed the destination hint makes no difference for sequential writes.)
    if (KIO::filesize_t(st.st_size) > KIO::filesize_t(8 * s_batchCopyChunk)) {
        ::posix_fadvise(sourceFd, 0, 0, POSIX_FADV_SEQUENTIAL);
    }
#endif
    // Create the destination with O_CREAT|O_EXCL: the batch only ever creates *fresh* files. Any
    // existing destination - a regular file to overwrite, a symlink, or the source itself (same
    // inode via a hardlink) - is reported as a conflict and left to the per-file copy() path, which
    // handles overwrite atomically (via a .part backup), rejects identical files, and replaces a
    // symlink destination instead of writing through it. So the batch never truncates an existing
    // file, and the O_EXCL also doubles as the conflict check (one syscall, no race).
    const int destFd = ::openat(destDirFd, destName.constData(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, st.st_mode & 07777);
    if (destFd < 0) {
        return errno == EEXIST ? CopyOutcome::Conflict : CopyOutcome::Failed;
    }
    auto cleanupDestFd = qScopeGuard([destFd] {
        const int e = errno;
        ::close(destFd);
        errno = e;
    });
    bool cloned = false;
#ifdef FICLONE
    // Reflink (share extents) when tryReflink says the destination supports it; on any failure fall
    // through to the normal copy.
    if (tryReflink && ::ioctl(destFd, FICLONE, sourceFd) != -1) {
        bytesCopied += KIO::filesize_t(st.st_size);
        cloned = true;
    }
#endif
    const bool ok = cloned || copyFds(sourceFd, destFd, st.st_size, bytesCopied, isKilled);
    if (ok && preserve) {
        preserve(sourceFd, destFd, st, true /* always freshly created */, createdGid); // both fds still open; best-effort, never flips ok
    }
    if (!ok) {
        const int e = errno; // unlinkat() may clobber errno; keep the copy failure for the caller
        ::unlinkat(destDirFd, destName.constData(), 0); // don't leave a partially written destination behind
        errno = e;
        return CopyOutcome::Failed;
    }
    return CopyOutcome::Copied;
}

// Engine interface: copies each op and reports its ItemOutcome, so the worker can stream
// progress, defer per-item failures, and stop on a batch-fatal one. Conflicts are pre-filtered
// by the caller (overwrite policy). Returns the errno that aborted the batch, or 0 if it ran to
// completion.
class BatchCopyEngine
{
public:
    BatchCopyEngine(PreserveFn preserve, gid_t egid, std::function<bool()> isKilled, bool tryReflink)
        : m_preserve(std::move(preserve))
        , m_egid(egid)
        , m_isKilled(std::move(isKilled))
        , m_tryReflink(tryReflink)
    {
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
    virtual int
    copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int index, ItemOutcome outcome, int err)> &onItem) = 0;

protected:
    // Keeps one source and one destination directory open at a time, so consecutive files in the
    // same directory (the common case) are reached with openat() instead of re-resolving the full
    // path. Returns false (errno set) if the directory cannot be opened.
    bool ensureSrcDir(const QByteArray &dir)
    {
        if (m_srcDirFd >= 0 && dir == m_srcDirPath) {
            return true;
        }
        const int fd = ::open(dir.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        if (m_srcDirFd >= 0) {
            ::close(m_srcDirFd);
        }
        m_srcDirFd = fd;
        m_srcDirPath = dir;
        return true;
    }
    bool ensureDestDir(const QByteArray &dir)
    {
        if (m_destDirFd >= 0 && dir == m_destDirPath) {
            return true;
        }
        const int fd = ::open(dir.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            return false;
        }
        if (m_destDirFd >= 0) {
            ::close(m_destDirFd);
        }
        m_destDirFd = fd;
        m_destDirPath = dir;
        // A freshly created file's group is our egid, unless this directory is set-group-ID, in
        // which case it inherits the directory's group. fstat the dir fd we just opened (rather
        // than stat-ing the path) to settle that once per directory.
        struct stat ds;
        m_destDirCreatedGid = (::fstat(fd, &ds) == 0 && (ds.st_mode & S_ISGID)) ? ds.st_gid : m_egid;
        return true;
    }

    PreserveFn m_preserve; // applied to each successfully copied file (perms/times/owner/ACL)
    gid_t m_egid; // effective gid: the group new files get outside set-group-ID directories
    std::function<bool()> m_isKilled; // polled to abort promptly on cancellation
    bool m_tryReflink = false; // destination filesystem can reflink (FICLONE); set by the Job

    int m_srcDirFd = -1;
    QByteArray m_srcDirPath;
    int m_destDirFd = -1;
    QByteArray m_destDirPath;
    gid_t m_destDirCreatedGid = 0; // group of files freshly created in m_destDirFd
};

// Default engine, and the portable path wherever io_uring is unavailable (BSD, older Linux):
// serial copy_file_range.
class CopyFileRangeEngine : public BatchCopyEngine
{
public:
    using BatchCopyEngine::BatchCopyEngine;

    int copyBatch(const QList<BatchCopyOp> &ops, KIO::filesize_t &bytesCopied, const std::function<void(int, ItemOutcome, int)> &onItem) override
    {
        for (const auto &op : ops) {
            if (m_isKilled && m_isKilled()) {
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

            errno = 0;
            const CopyOutcome oc =
                copyOneFileAt(m_srcDirFd, srcName, m_destDirFd, destName, m_destDirCreatedGid, bytesCopied, m_preserve, m_isKilled, m_tryReflink);
            switch (oc) {
            case CopyOutcome::Copied:
                onItem(op.index, ItemOutcome::Done, 0);
                break;
            case CopyOutcome::Conflict:
                onItem(op.index, ItemOutcome::Conflict, 0);
                break;
            case CopyOutcome::Failed: {
                const int err = errno;
                if (err == ECANCELED) {
                    return ECANCELED; // cancelled mid-file
                }
                if (isFatalCopyError(err)) {
                    onItem(op.index, ItemOutcome::Abort, err);
                    return err; // stop the batch
                }
                onItem(op.index, ItemOutcome::Defer, err);
                break;
            }
            }
        }
        return 0;
    }
};

// NOTE: an io_uring engine would slot in here as another BatchCopyEngine. A naive wave-based
// version (pipelining openat/close across the batch, data move still copy_file_range since there
// is no io_uring opcode for it) measured *slower* than the serial copy_file_range engine for
// warm-cache local small files - the metadata syscalls it overlaps are already cheap there, and
// copy_file_range keeps data in-kernel (and reflinks). It would need a properly pipelined design
// (linked SQEs, registered files) and a cold-cache/high-latency benchmark to justify the liburing
// dependency, so it is intentionally left out for now.
} // namespace

WorkerResult FileProtocol::batchCopy(QDataStream &stream)
{
    // Request: qint32 flags (reserved), qint32 count, count x (QString src, QString dest).
    qint32 flags = 0;
    qint32 count = 0;
    stream >> flags >> count;
    // bit0: the destination filesystem supports reflink (FICLONE). The Job sets it from the mount
    // table so the worker never probes a filesystem that cannot clone. Existing destinations are
    // still always deferred to the per-file path (O_EXCL), so overwrite is not a batch concern.
    const bool tryReflink = (flags & 0x1);

    struct Deferral {
        qint32 index;
        qint32 reason;
        qint32 err;
    };
    QList<Deferral> deferrals;

    // Existing destinations are detected by the engine's O_EXCL open and deferred, so no
    // destination access() pre-check is needed here.
    QList<BatchCopyOp> todo;
    todo.reserve(count);
    for (int i = 0; i < count; ++i) {
        QString src;
        QString dest;
        stream >> src >> dest;
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

    // Preserve perms/times/owner/ACL (free helper) plus extended attributes (member fn that
    // knows the namespace quirks) on each copied file, like copy() does. Best-effort: a
    // preservation failure does not fail the copy. The umask and our euid are read once for the
    // whole batch and let the helper skip the fchmod/fchown calls open() already satisfied; the
    // group a freshly created file already has (createdGid) is worked out per destination
    // directory by the engine and handed in.
    const mode_t umaskBits = currentUmask();
    const uid_t euid = ::geteuid();
    const gid_t egid = ::getegid();
    PreserveFn preserve = [umaskBits, euid, this](int sourceFd, int destFd, const struct stat &st, bool destFreshlyCreated, gid_t createdGid) {
        preserveAttrs(sourceFd, destFd, st, destFreshlyCreated, umaskBits, euid, createdGid);
#if HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H
        copyXattrs(sourceFd, destFd);
#endif
    };
    auto engine = std::make_unique<CopyFileRangeEngine>(
        std::move(preserve),
        egid,
        [this] {
            return wasKilled();
        },
        tryReflink);

    // Progress + completion reporting, batched on one ~100ms gate so a large run does not flood the
    // app. Each tick sends one infoMessage "<current>;<done1>,<done2>,..." where <current> is the
    // index of the file about to be copied (drives copying(), so it names a not-yet-copied file) and
    // the list is the files completed since the last tick (replayed as copyingDone()), plus the
    // cumulative byte total via processedSize. The final flush carries the remainder and exact total.
    // Deferred/errored items are not reported here - they come back in the batchDeferred metadata.
    KIO::filesize_t bytesDone = 0;
    QElapsedTimer reportTimer;
    QList<qint32> doneSinceReport;
    qint32 inFlight = -1; // file about to be / being copied, for copying()
    auto flushReport = [&]() {
        processedSize(bytesDone);
        if (inFlight < 0 && doneSinceReport.isEmpty()) {
            return;
        }
        QString msg = (inFlight >= 0 ? QString::number(inFlight) : QString()) + QLatin1Char(';');
        QStringList parts;
        parts.reserve(doneSinceReport.size());
        for (qint32 i : std::as_const(doneSinceReport)) {
            parts << QString::number(i);
        }
        infoMessage(msg + parts.join(QLatin1Char(',')));
        doneSinceReport.clear();
        reportTimer.restart();
    };
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
            deferrals.push_back({index, DeferConflict, 0}); // dest exists: let the app resolve it
            break;
        case ItemOutcome::Defer:
        case ItemOutcome::Abort: // record the item that triggered the abort too
            deferrals.push_back({index, DeferError, qint32(err)});
            break;
        }
    });
    inFlight = -1; // nothing in flight anymore; final flush carries the remaining completed indices
    flushReport();

    // Return the deferral list as metadata (KIO::special creates a SimpleJob, which delivers
    // metadata but not data()): "index:reason:errno;" per deferred item.
    QString deferredStr;
    for (const Deferral &d : std::as_const(deferrals)) {
        deferredStr += QStringLiteral("%1:%2:%3;").arg(d.index).arg(d.reason).arg(d.err);
    }
    setMetaData(QStringLiteral("batchDeferred"), deferredStr);

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

WorkerResult FileProtocol::copy(const QUrl &srcUrl, const QUrl &destUrl, int _mode, JobFlags _flags)
{
    qCDebug(KIO_FILE) << "copy()" << srcUrl << "to" << destUrl << "mode=" << _mode;

    const QString src = srcUrl.toLocalFile();
    QString dest = destUrl.toLocalFile();
    QByteArray _src(QFile::encodeName(src));
    QByteArray _dest(QFile::encodeName(dest));
    QByteArray _destBackup;

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

    QT_STATBUF buffDest;
    bool dest_exists = (QT_LSTAT(_dest.data(), &buffDest) != -1);
    if (dest_exists) {
        if (same_inode(buffDest, buffSrc)) {
            return WorkerResult::fail(KIO::ERR_IDENTICAL_FILES, dest);
        }

        if (S_ISDIR(buffDest.st_mode)) {
            return WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, dest);
        }

        if (_flags & KIO::Overwrite) {
            // If the destination is a symlink and overwrite is TRUE,
            // remove the symlink first to prevent the scenario where
            // the symlink actually points to current source!
            if (S_ISLNK(buffDest.st_mode)) {
                // qDebug() << "copy(): LINK DESTINATION";
                if (!QFile::remove(dest)) {
                    return WorkerResult::fail(KIO::ERR_CANNOT_DELETE_ORIGINAL, dest);
                }
            } else if (S_ISREG(buffDest.st_mode) && !isOnCifsMount(dest)) {
                _destBackup = _dest;
                dest.append(QStringLiteral(".part"));
                _dest = QFile::encodeName(dest);
            }
        } else {
            return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest);
        }
    }

    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
    }

#if HAVE_FADVISE
    posix_fadvise(srcFile.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    QFile destFile(dest);
    if (!destFile.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        if (errno == EACCES) {
            return WorkerResult::fail(KIO::ERR_WRITE_ACCESS_DENIED, dest);
        } else {
            return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
        }
    }

    // _mode == -1 means don't touch dest permissions, leave it with the system default ones
    if (_mode != -1) {
        // Change permissions through the open descriptor so they land on the
        // file just opened, not on whatever the path resolves to now.
        if (::fchmod(destFile.handle(), _mode) == -1) {
            qCWarning(KIO_FILE) << "Could not change permissions for" << dest;
        }
    }

#if HAVE_FADVISE
    posix_fadvise(destFile.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    const auto srcSize = buffSrc.st_size;
    totalSize(srcSize);

    off_t sizeProcessed = 0;

    const bool slowTestMode = testMode && destFile.fileName().contains(QLatin1String("slow"));

#ifdef FICLONE
    if (!slowTestMode) {
        // Share data blocks ("reflink") on supporting filesystems, like brfs and XFS
        int ret = ::ioctl(destFile.handle(), FICLONE, srcFile.handle());
        if (ret != -1) {
            sizeProcessed = srcSize;
            processedSize(srcSize);
        }
    }
    // if fs does not support reflinking, files are on different devices...
#endif

    bool existingDestDeleteAttempted = false;

    processedSize(sizeProcessed);

#if HAVE_COPY_FILE_RANGE
    while (!wasKilled() && sizeProcessed < srcSize) {
        if (slowTestMode) {
            QThread::msleep(50);
        }

        const ssize_t copiedBytes = ::copy_file_range(srcFile.handle(), nullptr, destFile.handle(), nullptr, s_maxIPCSize, 0);

        if (copiedBytes == -1) {
            // ENOENT is returned on cifs in some cases, probably a kernel bug
            // (s.a. https://git.savannah.gnu.org/cgit/coreutils.git/commit/?id=7fc84d1c0f6b35231b0b4577b70aaa26bf548a7c)
            if (errno == EINVAL || errno == EXDEV || errno == ENOENT) {
                break; // will continue with next copy mechanism
            }

            if (errno == EINTR) { // Interrupted
                continue;
            }

            if (errno == ENOSPC) { // disk full
                // attempt to free disk space occupied by file being overwritten
                if (!_destBackup.isEmpty() && !existingDestDeleteAttempted) {
                    ::unlink(_destBackup.constData());
                    existingDestDeleteAttempted = true;
                    continue;
                }

                if (!QFile::remove(dest)) { // don't keep partly copied file
                    qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
                }

                return WorkerResult::fail(KIO::ERR_DISK_FULL, dest);
            }

            if (!QFile::remove(dest)) { // don't keep partly copied file
                qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
            }

            return WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Cannot copy file from %1 to %2. (Errno: %3)", src, dest, errno));
        }

        sizeProcessed += copiedBytes;
        processedSize(sizeProcessed);
    }
#endif

    /* standard read/write fallback */
    if (sizeProcessed < srcSize) {
        QByteArray buffer(s_maxIPCSize, Qt::Uninitialized);
        while (!wasKilled() && sizeProcessed < srcSize) {
            if (testMode && destFile.fileName().contains(QLatin1String("slow"))) {
                QThread::msleep(50);
            }

            const ssize_t readBytes = ::read(srcFile.handle(), buffer.data(), s_maxIPCSize);

            if (readBytes == -1) {
                if (errno == EINTR) { // Interrupted
                    continue;
                } else {
                    qCWarning(KIO_FILE) << "Couldn't read[2]. Error:" << srcFile.errorString();
                }

                if (!QFile::remove(dest)) { // don't keep partly copied file
                    qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
                }
                return WorkerResult::fail(KIO::ERR_CANNOT_READ, src);
            }

            if (destFile.write(buffer.data(), readBytes) != readBytes) {
                int error = KIO::ERR_CANNOT_WRITE;
                if (destFile.error() == QFileDevice::ResourceError) { // disk full
                    // attempt to free disk space occupied by file being overwritten
                    if (!_destBackup.isEmpty() && !existingDestDeleteAttempted) {
                        ::unlink(_destBackup.constData());
                        existingDestDeleteAttempted = true;
                        if (destFile.write(buffer.data(), readBytes) == readBytes) { // retry
                            continue;
                        }
                    }
                    error = KIO::ERR_DISK_FULL;
                } else {
                    qCWarning(KIO_FILE) << "Couldn't write[2]. Error:" << destFile.errorString();
                }

                if (!QFile::remove(dest)) { // don't keep partly copied file
                    qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
                }
                return WorkerResult::fail(error, dest);
            }
            sizeProcessed += readBytes;
            processedSize(sizeProcessed);
        }
    }

    // Copy Extended attributes
#if HAVE_SYS_XATTR_H || HAVE_SYS_EXTATTR_H
    if (!copyXattrs(srcFile.handle(), destFile.handle())) {
        qCDebug(KIO_FILE) << "can't copy Extended attributes";
    }
#endif

    srcFile.close();

    destFile.flush(); // so the writes complete before the timestamp and ownership changes

    // copy access and modification time
    if (!wasKilled()) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_HAIKU)
        // with nano secs precision
        struct timespec ut[2];
        ut[0] = buffSrc.st_atim;
        ut[1] = buffSrc.st_mtim;
        // need to do this with the dest file still opened, or this fails
        if (::futimens(destFile.handle(), ut) != 0) {
#else
        struct timeval ut[2];
        ut[0].tv_sec = buffSrc.st_atime;
        ut[0].tv_usec = 0;
        ut[1].tv_sec = buffSrc.st_mtime;
        ut[1].tv_usec = 0;
        if (::futimes(destFile.handle(), ut) != 0) {
#endif
            qCWarning(KIO_FILE) << "Couldn't preserve access and modification time for" << dest;
        }
    }

    // preserve ownership through the open descriptor, so the change lands on
    // the file just written
    if (_mode != -1) {
        if (::fchown(destFile.handle(), -1 /*keep user*/, buffSrc.st_gid) == 0) {
            // as we are the owner of the new file, we can always change the group, but
            // we might not be allowed to change the owner
            if (::fchown(destFile.handle(), buffSrc.st_uid, -1 /*keep group*/) < 0) {
                qCWarning(KIO_FILE) << "Couldn't chown destFile" << _dest << "(" << strerror(errno) << ")";
            }
        } else {
            qCWarning(KIO_FILE) << "Couldn't preserve group for" << dest;
        }
    }

    destFile.close();

    if (wasKilled()) {
        qCDebug(KIO_FILE) << "Clean dest file after KIO worker was killed:" << dest;
        if (!QFile::remove(dest)) { // don't keep partly copied file
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
        }
        return WorkerResult::fail(KIO::ERR_USER_CANCELED, dest);
    }

    if (destFile.error() != QFile::NoError) {
        qCWarning(KIO_FILE) << "Error when closing file descriptor[2]:" << destFile.errorString();

        if (!QFile::remove(dest)) { // don't keep partly copied file
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << dest;
        }

        return WorkerResult::fail(KIO::ERR_CANNOT_WRITE, dest);
    }

#if HAVE_POSIX_ACL
    // If no special mode is given, preserve the ACL attributes from the source file
    if (_mode == -1) {
        acl_t acl = acl_get_fd(srcFile.handle());
        if (acl && acl_set_file(_dest.data(), ACL_TYPE_ACCESS, acl) != 0) {
            qCWarning(KIO_FILE) << "Could not set ACL permissions for" << dest;
        }
    }
#endif

    if (!_destBackup.isEmpty()) { // Overwrite final dest file with new file
        if (::unlink(_destBackup.constData()) == -1) {
            qCWarning(KIO_FILE) << "Couldn't remove original dest" << _destBackup << "(" << strerror(errno) << ")";
        }

        if (::rename(_dest.constData(), _destBackup.constData()) == -1) {
            qCWarning(KIO_FILE) << "Couldn't rename" << _dest << "to" << _destBackup << "(" << strerror(errno) << ")";
        }
    }

    processedSize(srcSize);
    return WorkerResult::pass();
}

static bool isLocalFileSameHost(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return false;
    }

    if (url.host().isEmpty() || (url.host() == QLatin1String("localhost"))) {
        return true;
    }

    char hostname[256];
    hostname[0] = '\0';
    if (!gethostname(hostname, 255)) {
        hostname[sizeof(hostname) - 1] = '\0';
    }

    return (QString::compare(url.host(), QLatin1String(hostname), Qt::CaseInsensitive) == 0);
}

#if HAVE_SYS_XATTR_H
static bool isNtfsHidden(const QString &filename)
{
    constexpr auto attrName = "system.ntfs_attrib_be";
    const auto filenameEncoded = QFile::encodeName(filename);

    uint32_t intAttr = 0;
    constexpr size_t xattr_size = sizeof(intAttr);
    char strAttr[xattr_size];
#ifdef Q_OS_MACOS
    auto length = getxattr(filenameEncoded.data(), attrName, strAttr, xattr_size, 0, XATTR_NOFOLLOW);
#else
    auto length = getxattr(filenameEncoded.data(), attrName, strAttr, xattr_size);
#endif
    if (length <= 0) {
        return false;
    }

    char *c = strAttr;
    for (decltype(length) n = 0; n < length; ++n, ++c) {
        intAttr <<= 8;
        intAttr |= static_cast<uchar>(*c);
    }

    constexpr auto FILE_ATTRIBUTE_HIDDEN = 0x2u;
    return static_cast<bool>(intAttr & FILE_ATTRIBUTE_HIDDEN);
}
#endif

WorkerResult FileProtocol::listDir(const QUrl &url)
{
    if (!isLocalFileSameHost(url)) {
        QUrl redir(url);
        redir.setScheme(configValue(QStringLiteral("DefaultRemoteProtocol"), QStringLiteral("smb")));
        redirection(redir);
        // qDebug() << "redirecting to " << redir;
        return WorkerResult::pass();
    }
    const QString path(url.toLocalFile());
    const QByteArray _path(QFile::encodeName(path));
    DIR *dp = opendir(_path.data());
    if (dp == nullptr) {
        switch (errno) {
        case ENOENT:
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, path);
        case ENOTDIR:
            return WorkerResult::fail(KIO::ERR_IS_FILE, path);
#ifdef ENOMEDIUM
        case ENOMEDIUM:
            return WorkerResult::fail(ERR_WORKER_DEFINED, i18n("No media in device for %1", path));
#endif
        default:
            return WorkerResult::fail(KIO::ERR_CANNOT_ENTER_DIRECTORY, path);
            break;
        }
    }

    const QByteArray encodedBasePath = _path + '/';

    const KIO::StatDetails details = getStatDetails();
    // qDebug() << "========= LIST " << url << "details=" << details << " =========";
    UDSEntry entry;

#if !(HAVE_DIRENT_D_TYPE)
    QT_STATBUF st;
#endif
    QT_DIRENT *ep;
    while ((ep = QT_READDIR(dp)) != nullptr) {
        if (wasKilled()) {
            closedir(dp);
            return WorkerResult::pass();
        }

        entry.clear();

        const QString filename = QFile::decodeName(ep->d_name);

        /*
         * details == 0 (if statement) is the fast code path.
         * We only get the file name and type. After that we emit
         * the result.
         *
         * The else statement is the slow path that requests all
         * file information in file.cpp. It executes a stat call
         * for every entry thus becoming slower.
         *
         */
        if (details == KIO::StatBasic) {
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
#if HAVE_DIRENT_D_TYPE
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, (ep->d_type == DT_DIR) ? S_IFDIR : S_IFREG);
            const bool isSymLink = (ep->d_type == DT_LNK);
#else
            // oops, no fast way, we need to stat (e.g. on Solaris)
            if (QT_LSTAT(ep->d_name, &st) == -1) {
                continue; // how can stat fail?
            }
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_ISDIR(st.st_mode) ? S_IFDIR : S_IFREG);
            const bool isSymLink = S_ISLNK(st.st_mode);
#endif
            if (isSymLink) {
                // for symlinks obey the UDSEntry contract and provide UDS_LINK_DEST
                // even if we don't know the link dest (and DeleteJob doesn't care...)
                entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QStringLiteral("Dummy Link Target"));
            }
            listEntry(entry);

        } else {
            QString fullPath = Utils::slashAppended(path);
            fullPath += filename;

            if (createUDSEntry(filename, encodedBasePath + QByteArray(ep->d_name), entry, details, fullPath)) {
#if HAVE_SYS_XATTR_H && HAVE_DIRENT_D_TYPE
                if (isNtfsHidden(filename)) {
                    bool ntfsHidden = true;

                    // Bug 392913: NTFS root volume is always "hidden", ignore this
                    if (ep->d_type == DT_DIR || ep->d_type == DT_UNKNOWN || ep->d_type == DT_LNK) {
                        const QString fullFilePath = QDir(filename).canonicalPath();
                        auto mountPoint = KMountPoint::currentMountPoints().findByPath(fullFilePath);
                        if (mountPoint && mountPoint->mountPoint() == fullFilePath) {
                            ntfsHidden = false;
                        }
                    }

                    if (ntfsHidden) {
                        entry.fastInsert(KIO::UDSEntry::UDS_HIDDEN, 1);
                    }
                }
#endif
                listEntry(entry);
            }
        }
    }

    closedir(dp);

    return WorkerResult::pass();
}

WorkerResult FileProtocol::rename(const QUrl &srcUrl, const QUrl &destUrl, KIO::JobFlags _flags)
{
    char off_t_should_be_64_bits[sizeof(off_t) >= 8 ? 1 : -1];
    (void)off_t_should_be_64_bits;
    const QString src = srcUrl.toLocalFile();
    const QString dest = destUrl.toLocalFile();
    const QByteArray _src(QFile::encodeName(src));
    const QByteArray _dest(QFile::encodeName(dest));
    QT_STATBUF buff_src;
    if (QT_LSTAT(_src.data(), &buff_src) == -1) {
        if (errno == EACCES) {
            return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, src);
        } else {
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, src);
        }
    }

    QT_STATBUF buff_dest;
    // stat symlinks here (lstat, not stat), to avoid ERR_IDENTICAL_FILES when replacing symlink
    // with its target (#169547)
    bool dest_exists = (QT_LSTAT(_dest.data(), &buff_dest) != -1);
    if (dest_exists) {
        // Try QFile::rename(), this can help when renaming 'a' to 'A' on a case-insensitive
        // filesystem, e.g. FAT32/VFAT.
        if (src != dest && QString::compare(src, dest, Qt::CaseInsensitive) == 0) {
            qCDebug(KIO_FILE) << "Dest already exists; detected special case of lower/uppercase renaming"
                              << "in same dir on a case-insensitive filesystem, try with QFile::rename()"
                              << "(which uses 2 rename calls)";
            if (QFile::rename(src, dest)) {
                return WorkerResult::pass();
            }
        }

        if (same_inode(buff_dest, buff_src)) {
            return WorkerResult::fail(KIO::ERR_IDENTICAL_FILES, dest);
        }

        if (S_ISDIR(buff_dest.st_mode)) {
            return WorkerResult::fail(KIO::ERR_DIR_ALREADY_EXIST, dest);
        }

        if (!(_flags & KIO::Overwrite)) {
            return WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest);
        }
    }

    if (::rename(_src.data(), _dest.data()) == -1) {
        const int error = errno;
        if ((error == EACCES) || (error == EPERM)) {
            return WorkerResult::fail(KIO::ERR_WRITE_ACCESS_DENIED, dest);
        } else if (error == EXDEV) {
            return WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, QStringLiteral("rename"));
        } else if (error == EROFS) { // The file is on a read-only filesystem
            return WorkerResult::fail(KIO::ERR_CANNOT_DELETE, src);
        } else if (error == ENOENT) {
            // src was removed, TOCTOU case
            return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, src);
        } else {
            qCWarning(KIO_FILE) << "Could not rename file" << _src << "to" << _dest << ":" << strerror(error) << "(" << error << ")";
            return WorkerResult::fail(KIO::ERR_CANNOT_RENAME, src);
        }
    }

    return WorkerResult::pass();
}

WorkerResult FileProtocol::symlink(const QString &target, const QUrl &destUrl, KIO::JobFlags flags)
{
    // Assume dest is local too (wouldn't be here otherwise)
    const QString dest = destUrl.toLocalFile();
    const QByteArray dest_c = QFile::encodeName(dest);

    if (::symlink(QFile::encodeName(target).constData(), dest_c.constData()) == 0) {
        return WorkerResult::pass();
    }

    // Does the destination already exist ?
    if (errno == EEXIST) {
        if (flags & KIO::Overwrite) {
            // Try to delete the destination
            if (unlink(dest_c.constData()) != 0) {
                return WorkerResult::fail(KIO::ERR_CANNOT_DELETE, dest);
            }

            // Try again - this won't loop forever since unlink succeeded
            return symlink(target, destUrl, flags);
        } else {
            if (QT_STATBUF buff_dest; QT_LSTAT(dest_c.constData(), &buff_dest) == 0) {
                return WorkerResult::fail(S_ISDIR(buff_dest.st_mode) ? KIO::ERR_DIR_ALREADY_EXIST : KIO::ERR_FILE_ALREADY_EXIST, dest);
            } else { // Can't happen, we already know "dest" exists
                return WorkerResult::fail(KIO::ERR_CANNOT_SYMLINK, dest);
            }

            return WorkerResult::pass();
        }
    }

    // Permission error, could be that the filesystem doesn't support symlinks
    if (errno == EPERM) {
        // "dest" doesn't exist, get the filesystem type of the parent dir
        const QString parentDir = destUrl.adjusted(QUrl::StripTrailingSlash | QUrl::RemoveFilename).toLocalFile();
        const KFileSystemType::Type fsType = KFileSystemType::fileSystemType(parentDir);

        if (fsType == KFileSystemType::Fat || fsType == KFileSystemType::Exfat) {
            const QString msg = i18nc(
                "The first arg is the path to the symlink that couldn't be created, the second"
                "arg is the filesystem type (e.g. vfat, exfat)",
                "Could not create symlink \"%1\".\n"
                "The destination filesystem (%2) doesn't support symlinks.",
                dest,
                KFileSystemType::fileSystemName(fsType));

            return WorkerResult::fail(KIO::ERR_WORKER_DEFINED, msg);
        }
    }

    return WorkerResult::fail(KIO::ERR_CANNOT_SYMLINK, dest);
}

WorkerResult FileProtocol::del(const QUrl &url, bool isfile)
{
    const QString path = url.toLocalFile();
    const QByteArray _path(QFile::encodeName(path));
    /*!***
     * Delete files
     *****/

    if (isfile) {
        // qDebug() << "Deleting file "<< url;

        if (::unlink(_path.data()) == -1) {
            if ((errno == EACCES) || (errno == EPERM)) {
                return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, path);
            } else if (errno == EISDIR) {
                return WorkerResult::fail(KIO::ERR_IS_DIRECTORY, path);
            } else {
                return WorkerResult::fail(KIO::ERR_CANNOT_DELETE, path);
            }
        }
    } else {
        /*!***
         * Delete empty directory
         *****/

        // qDebug() << "Deleting directory " << url;
        if (metaData(QStringLiteral("recurse")) == QLatin1String("true")) {
            auto result = deleteRecursive(path);
            if (!result.success()) {
                return result;
            }
        }
        if (wasKilled()) {
            return WorkerResult::pass();
        }
        if (QT_RMDIR(_path.data()) == -1) {
            if ((errno == EACCES) || (errno == EPERM)) {
                return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, path);
            } else {
                // qDebug() << "could not rmdir " << perror;
                return WorkerResult::fail(KIO::ERR_CANNOT_RMDIR, path);
            }
        }
    }
    return WorkerResult::pass();
}

WorkerResult FileProtocol::chown(const QUrl &url, const QString &owner, const QString &group)
{
    const QString path = url.toLocalFile();
    const QByteArray _path(QFile::encodeName(path));
    uid_t uid;
    gid_t gid;

    // get uid from given owner
    {
        struct passwd *p = ::getpwnam(owner.toLocal8Bit().constData());

        if (!p) {
            return WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not get user id for given user name %1", owner));
        }

        uid = p->pw_uid;
    }

    // get gid from given group
    {
        struct group *p = ::getgrnam(group.toLocal8Bit().constData());

        if (!p) {
            return WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Could not get group id for given group name %1", group));
        }

        gid = p->gr_gid;
    }

    if (::chown(_path.constData(), uid, gid) == -1) {
        switch (errno) {
        case EPERM:
        case EACCES:
            return WorkerResult::fail(KIO::ERR_ACCESS_DENIED, path);
            break;
        case ENOSPC:
            return WorkerResult::fail(KIO::ERR_DISK_FULL, path);
            break;
        default:
            return WorkerResult::fail(KIO::ERR_CANNOT_CHOWN, path);
        }
    }

    return WorkerResult::pass();
}

WorkerResult FileProtocol::stat(const QUrl &url)
{
    if (!isLocalFileSameHost(url)) {
        return redirect(url);
    }

    /* directories may not have a slash at the end if
     * we want to stat() them; it requires that we
     * change into it .. which may not be allowed
     * stat("/is/unaccessible")  -> rwx------
     * stat("/is/unaccessible/") -> EPERM            H.Z.
     * This is the reason for the -1
     */
    const QString path(url.adjusted(QUrl::StripTrailingSlash).toLocalFile());
    const QByteArray _path(QFile::encodeName(path));

    const KIO::StatDetails details = getStatDetails();

    UDSEntry entry;
    if (!createUDSEntry(url.fileName(), _path, entry, details, path)) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, path);
    }
    statEntry(entry);

    return WorkerResult::pass();
}

int FileProtocol::setACL(const char *path, mode_t perm, bool directoryDefault)
{
    int ret = 0;
#if HAVE_POSIX_ACL

    const QString ACLString = metaData(QStringLiteral("ACL_STRING"));
    const QString defaultACLString = metaData(QStringLiteral("DEFAULT_ACL_STRING"));
    // Empty strings mean leave as is
    if (!ACLString.isEmpty()) {
        acl_t acl = nullptr;
        if (ACLString == QLatin1String("ACL_DELETE")) {
            // user told us to delete the extended ACL, so let's write only
            // the minimal (UNIX permission bits) part
            acl = ACLPortability::acl_from_mode(perm);
        }
        acl = acl_from_text(ACLString.toLatin1().constData());
        if (acl_valid(acl) == 0) { // let's be safe
            ret = acl_set_file(path, ACL_TYPE_ACCESS, acl);
            // qDebug() << "Set ACL on:" << path << "to:" << aclToText(acl);
        }
        acl_free(acl);
        if (ret != 0) {
            return ret; // better stop trying right away
        }
    }

    if (directoryDefault && !defaultACLString.isEmpty()) {
        if (defaultACLString == QLatin1String("ACL_DELETE")) {
            // user told us to delete the default ACL, do so
            ret += acl_delete_def_file(path);
        } else {
            acl_t acl = acl_from_text(defaultACLString.toLatin1().constData());
            if (acl_valid(acl) == 0) { // let's be safe
                ret += acl_set_file(path, ACL_TYPE_DEFAULT, acl);
                // qDebug() << "Set Default ACL on:" << path << "to:" << aclToText(acl);
            }
            acl_free(acl);
        }
    }
#else
    Q_UNUSED(path);
    Q_UNUSED(perm);
    Q_UNUSED(directoryDefault);
#endif
    return ret;
}
