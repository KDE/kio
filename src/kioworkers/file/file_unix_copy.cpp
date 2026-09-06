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

#include <cerrno>
#include <fcntl.h>
#include <stdint.h>
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
    bool publishViaRename = false; // write to destName + ".part", then rename over destName
    bool truncateInPlace = false; // overwrite destName in place (no ".part", used on CIFS)
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
            if (isOnCifs(buffDest, dest)) {
                truncateInPlace = true;
            } else {
                publishViaRename = true;
            }
        }
    }

    const QByteArray outName = publishViaRename ? (destName + ".part") : destName;

    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
    }

#if HAVE_FADVISE
    posix_fadvise(srcFile.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    // KIO passes -1 to keep the system default permissions. Comparing in mode_t width
    // also matches FreeBSD, where a 16-bit mode_t delivers the sentinel as (mode_t)-1.
    const bool defaultPermissions = mode_t(_mode) == mode_t(-1);

    // O_NOFOLLOW: never write through a symlink at the final name. O_EXCL makes the
    // ".part"/fresh create fail rather than clobber an unexpected file; the CIFS
    // path truncates the existing regular file in place.
    const mode_t createMode = defaultPermissions ? mode_t(0666) : mode_t(_mode);
    const int oflags = O_WRONLY | O_CREAT | O_NOFOLLOW | O_CLOEXEC | (truncateInPlace ? O_TRUNC : O_EXCL);
    int destFd = ::openat(dfd, outName.constData(), oflags, createMode);
    if (destFd < 0 && errno == EEXIST && publishViaRename) {
        // Stale ".part" from a previous interrupted copy: drop it and retry once.
        ::unlinkat(dfd, outName.constData(), 0);
        destFd = ::openat(dfd, outName.constData(), oflags, createMode);
    }
    if (destFd < 0) {
        return WorkerResult::fail(errno == EACCES ? KIO::ERR_WRITE_ACCESS_DENIED : KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
    }

    QFile destFile;
    if (!destFile.open(destFd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) {
        ::close(destFd);
        // Only remove a file we created; never the original we overwrite in place.
        if (!truncateInPlace) {
            ::unlinkat(dfd, outName.constData(), 0);
        }
        return WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
    }

    // Until the copy is known-good, any early return unlinks the partial output (the
    // ".part", or the freshly created file). Dismissed once the result is committed.
    auto cleanupOutput = qScopeGuard([dfd, &outName] {
        if (::unlinkat(dfd, outName.constData(), 0) != 0 && errno != ENOENT) {
            qCWarning(KIO_FILE) << "Could not delete partially copied file" << QFile::decodeName(outName);
        }
    });

    // Default permissions: leave the create mode plus umask in place, otherwise set the
    // requested mode below.
    if (!defaultPermissions) {
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

    const bool slowTestMode = testMode && dest.contains(QLatin1String("slow"));

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
                if (publishViaRename && !existingDestDeleteAttempted) {
                    ::unlinkat(dfd, destName.constData(), 0);
                    existingDestDeleteAttempted = true;
                    continue;
                }

                return WorkerResult::fail(KIO::ERR_DISK_FULL, dest);
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
            if (testMode && dest.contains(QLatin1String("slow"))) {
                QThread::msleep(50);
            }

            const ssize_t readBytes = ::read(srcFile.handle(), buffer.data(), s_maxIPCSize);

            if (readBytes == -1) {
                if (errno == EINTR) { // Interrupted
                    continue;
                } else {
                    qCWarning(KIO_FILE) << "Couldn't read[2]. Error:" << srcFile.errorString();
                }

                return WorkerResult::fail(KIO::ERR_CANNOT_READ, src);
            }

            if (destFile.write(buffer.data(), readBytes) != readBytes) {
                int error = KIO::ERR_CANNOT_WRITE;
                if (destFile.error() == QFileDevice::ResourceError) { // disk full
                    // attempt to free disk space occupied by file being overwritten
                    if (publishViaRename && !existingDestDeleteAttempted) {
                        ::unlinkat(dfd, destName.constData(), 0);
                        existingDestDeleteAttempted = true;
                        if (destFile.write(buffer.data(), readBytes) == readBytes) { // retry
                            continue;
                        }
                    }
                    error = KIO::ERR_DISK_FULL;
                } else {
                    qCWarning(KIO_FILE) << "Couldn't write[2]. Error:" << destFile.errorString();
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

#if HAVE_POSIX_ACL
    // If no explicit mode was requested, carry over an extended ACL from the source
    // (a plain mode-only ACL is left to the create mode and umask, as before). Read
    // it while the source is still open and set it on the destination descriptor
    // below, so it lands on the file we just wrote rather than on the path.
    // Gate on the literal -1 sentinel: on FreeBSD acl_equiv_mode_np() does not report a
    // plain access ACL as mode-equivalent, so the source mode would leak onto the copy.
    acl_t acl = nullptr;
    if (_mode == -1) {
        acl = acl_get_fd(srcFile.handle());
        if (acl && !isExtendedACL(acl)) {
            acl_free(acl);
            acl = nullptr;
        }
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
    if (!defaultPermissions) {
        // A folder with the setgid bit gives its group to whatever is made in it, and that group
        // is the one the new file is meant to carry, so it is left alone. The folder is asked
        // through the descriptor the copy holds on it, which is the folder the file was written
        // to whatever happened to the path meanwhile.
        QT_STATBUF buffDestDir;
        const bool destDirIsSetgid = QT_FSTAT(dfd, &buffDestDir) == 0 && (buffDestDir.st_mode & S_ISGID);

        if (destDirIsSetgid || ::fchown(destFile.handle(), -1 /*keep user*/, buffSrc.st_gid) == 0) {
            // as we are the owner of the new file, we can always change the group, but
            // we might not be allowed to change the owner
            if (::fchown(destFile.handle(), buffSrc.st_uid, -1 /*keep group*/) < 0) {
                qCWarning(KIO_FILE) << "Couldn't chown destFile" << dest << "(" << strerror(errno) << ")";
            }
        } else {
            qCWarning(KIO_FILE) << "Couldn't preserve group for" << dest;
        }
    }

#if HAVE_POSIX_ACL
    if (acl) {
        if (acl_set_fd(destFile.handle(), acl) != 0) {
            qCWarning(KIO_FILE) << "Could not set ACL permissions for" << dest;
        }
        acl_free(acl);
    }
#endif

    destFile.close();

    if (wasKilled()) {
        qCDebug(KIO_FILE) << "Clean dest file after KIO worker was killed:" << dest;
        return WorkerResult::fail(KIO::ERR_USER_CANCELED, dest);
    }

    if (destFile.error() != QFile::NoError) {
        qCWarning(KIO_FILE) << "Error when closing file descriptor[2]:" << destFile.errorString();
        return WorkerResult::fail(KIO::ERR_CANNOT_WRITE, dest);
    }

    if (publishViaRename) { // atomically replace the destination with the written ".part"
        if (::renameat(dfd, outName.constData(), dfd, destName.constData()) == -1) {
            qCWarning(KIO_FILE) << "Couldn't rename" << outName << "to" << destName << "(" << strerror(errno) << ")";
            return WorkerResult::fail(KIO::ERR_CANNOT_WRITE, dest);
        }
    }

    // The result is committed: keep the output we just wrote (or renamed into place).
    cleanupOutput.dismiss();
    processedSize(srcSize);
    return WorkerResult::pass();
}
