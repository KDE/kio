/*
   Copyright (C) 2000-2002 Stephan Kulow <coolo@kde.org>
   Copyright (C) 2000-2002 David Faure <faure@kde.org>
   Copyright (C) 2000-2002 Waldo Bastian <bastian@kde.org>
   Copyright (C) 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
   Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
   Copyright (C) 2007 Christian Ehrlicher <ch.ehrlicher@gmx.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later
   version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "file.h"

#include <config-kioslave-file.h>

#include <QFile>
#include <QDir>
#include <qplatformdefs.h>
#include <QStandardPaths>

#include <QDebug>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <kmountpoint.h>

#include <errno.h>
#if HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif
#include <utime.h>

#include <KAuth>
#include <KRandom>

#include "fdreceiver.h"

//sendfile has different semantics in different platforms
#if HAVE_SENDFILE && defined Q_OS_LINUX
#define USE_SENDFILE 1
#endif

#ifdef USE_SENDFILE
#include <sys/sendfile.h>
#endif

using namespace KIO;

#define MAX_IPC_SIZE (1024*32)

static bool
same_inode(const QT_STATBUF &src, const QT_STATBUF &dest)
{
    if (src.st_ino == dest.st_ino &&
            src.st_dev == dest.st_dev) {
        return true;
    }

    return false;
}

static const QString socketPath()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return QStringLiteral("%1/filehelper%2%3").arg(runtimeDir, KRandom::randomString(6)).arg(getpid());
}

bool FileProtocol::privilegeOperationUnitTestMode()
{
    return (metaData(QStringLiteral("UnitTesting")) == QLatin1String("true"))
            && (requestPrivilegeOperation() == KIO::OperationAllowed);
}

PrivilegeOperationReturnValue FileProtocol::tryOpen(QFile &f, const QByteArray &path, int flags, int mode, int errcode)
{
    const QString sockPath = socketPath();
    FdReceiver fdRecv(QFile::encodeName(sockPath).toStdString());
    if (!fdRecv.isListening()) {
        return PrivilegeOperationReturnValue::failure(errcode);
    }

    QIODevice::OpenMode openMode;
    if (flags & O_RDONLY) {
        openMode |= QIODevice::ReadOnly;
    }
    if (flags & O_WRONLY || flags & O_CREAT) {
        openMode |= QIODevice::WriteOnly;
    }
    if (flags & O_RDWR) {
        openMode |= QIODevice::ReadWrite;
    }
    if (flags & O_TRUNC) {
        openMode |= QIODevice::Truncate;
    }
    if (flags & O_APPEND) {
        openMode |= QIODevice::Append;
    }

    if (auto err = execWithElevatedPrivilege(OPEN, {path, flags, mode, sockPath}, errcode)) {
        return err;
    } else {
        int fd = fdRecv.fileDescriptor();
        if (fd < 3 || !f.open(fd, openMode, QFileDevice::AutoCloseHandle)) {
            return PrivilegeOperationReturnValue::failure(errcode);
        }
    }
    return PrivilegeOperationReturnValue::success();
}

PrivilegeOperationReturnValue FileProtocol::tryChangeFileAttr(ActionType action, const QVariantList &args, int errcode)
{
    KAuth::Action execAction(QStringLiteral("org.kde.kio.file.exec"));
    execAction.setHelperId(QStringLiteral("org.kde.kio.file"));
    if (execAction.status() == KAuth::Action::AuthorizedStatus) {
        return execWithElevatedPrivilege(action, args, errcode);
    }
    return PrivilegeOperationReturnValue::failure(errcode);
}

void FileProtocol::copy(const QUrl &srcUrl, const QUrl &destUrl,
                        int _mode, JobFlags _flags)
{
    if (privilegeOperationUnitTestMode()) {
        finished();
        return;
    }

    // qDebug() << "copy(): " << srcUrl << " -> " << destUrl << ", mode=" << _mode;

    const QString src = srcUrl.toLocalFile();
    const QString dest = destUrl.toLocalFile();
    QByteArray _src(QFile::encodeName(src));
    QByteArray _dest(QFile::encodeName(dest));

    QT_STATBUF buff_src;
#if HAVE_POSIX_ACL
    acl_t acl;
#endif
    if (QT_STAT(_src.data(), &buff_src) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, src);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, src);
        }
        return;
    }

    if ((buff_src.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
        error(KIO::ERR_IS_DIRECTORY, src);
        return;
    }
    if (S_ISFIFO(buff_src.st_mode) || S_ISSOCK(buff_src.st_mode)) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
        return;
    }

    QT_STATBUF buff_dest;
    bool dest_exists = (QT_LSTAT(_dest.data(), &buff_dest) != -1);
    if (dest_exists) {
        if (same_inode(buff_dest, buff_src)) {
            error(KIO::ERR_IDENTICAL_FILES, dest);
            return;
        }

        if ((buff_dest.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
            error(KIO::ERR_DIR_ALREADY_EXIST, dest);
            return;
        }

        if (!(_flags & KIO::Overwrite)) {
            error(KIO::ERR_FILE_ALREADY_EXIST, dest);
            return;
        }

        // If the destination is a symlink and overwrite is TRUE,
        // remove the symlink first to prevent the scenario where
        // the symlink actually points to current source!
        if ((_flags & KIO::Overwrite) && ((buff_dest.st_mode & QT_STAT_MASK) == QT_STAT_LNK)) {
            //qDebug() << "copy(): LINK DESTINATION";
            if (!QFile::remove(dest)) {
                if (auto err = execWithElevatedPrivilege(DEL, {_dest}, errno)) {
                    if (!err.wasCanceled()) {
                        error(KIO::ERR_CANNOT_DELETE_ORIGINAL, dest);
                    }
                    return;
                }
            }
        }
    }

    QFile src_file(src);
    if (!src_file.open(QIODevice::ReadOnly)) {
        if (auto err = tryOpen(src_file, _src, O_RDONLY, S_IRUSR, errno)) {
            if (!err.wasCanceled()) {
                error(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
            }
            return;
        }
    }

#if HAVE_FADVISE
    posix_fadvise(src_file.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    QFile dest_file(dest);
    if (!dest_file.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        if (auto err = tryOpen(dest_file, _dest, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR, errno)) {
            if (!err.wasCanceled()) {
                // qDebug() << "###### COULD NOT WRITE " << dest;
                if (err == EACCES) {
                    error(KIO::ERR_WRITE_ACCESS_DENIED, dest);
                } else {
                    error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
                }
            }
            src_file.close();
            return;
        }
    }

    // nobody shall be allowed to peek into the file during creation
    // Note that error handling is omitted for this call, we don't want to error on e.g. VFAT
    dest_file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);

#if HAVE_FADVISE
    posix_fadvise(dest_file.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

#if HAVE_POSIX_ACL
    acl = acl_get_fd(src_file.handle());
    if (acl && !isExtendedACL(acl)) {
        // qDebug() << _dest.data() << " doesn't have extended ACL";
        acl_free(acl);
        acl = nullptr;
    }
#endif
    totalSize(buff_src.st_size);

    KIO::filesize_t processed_size = 0;
    char buffer[ MAX_IPC_SIZE ];
    ssize_t n = 0;
#ifdef USE_SENDFILE
    bool use_sendfile = buff_src.st_size < 0x7FFFFFFF;
#endif
    while (1) {
#ifdef USE_SENDFILE
        if (use_sendfile) {
            off_t sf = processed_size;
            n = ::sendfile(dest_file.handle(), src_file.handle(), &sf, MAX_IPC_SIZE);
            processed_size = sf;
            if (n == -1 && (errno == EINVAL || errno == ENOSYS)) {     //not all filesystems support sendfile()
                // qDebug() << "sendfile() not supported, falling back ";
                use_sendfile = false;
            }
        }
        if (!use_sendfile)
#endif
            n = ::read(src_file.handle(), buffer, MAX_IPC_SIZE);

        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
#ifdef USE_SENDFILE
            if (use_sendfile) {
                // qDebug() << "sendfile() error:" << strerror(errno);
                if (errno == ENOSPC) { // disk full
                    error(KIO::ERR_DISK_FULL, dest);
                } else {
                    error(KIO::ERR_SLAVE_DEFINED,
                          i18n("Cannot copy file from %1 to %2. (Errno: %3)",
                               src, dest, errno));
                }
            } else
#endif
                error(KIO::ERR_CANNOT_READ, src);
            src_file.close();
            dest_file.close();
#if HAVE_POSIX_ACL
            if (acl) {
                acl_free(acl);
            }
#endif
            if (!QFile::remove(dest)) {  // don't keep partly copied file
                execWithElevatedPrivilege(DEL, {_dest}, errno);
            }
            return;
        }
        if (n == 0) {
            break;    // Finished
        }
#ifdef USE_SENDFILE
        if (!use_sendfile) {
#endif
            if (dest_file.write(buffer, n) != n) {
                if (dest_file.error() == QFileDevice::ResourceError) {  // disk full
                    error(KIO::ERR_DISK_FULL, dest);
                } else {
                    qCWarning(KIO_FILE) << "Couldn't write[2]. Error:" << dest_file.errorString();
                    error(KIO::ERR_CANNOT_WRITE, dest);
                }
#if HAVE_POSIX_ACL
                if (acl) {
                    acl_free(acl);
                }
#endif
                if (!QFile::remove(dest)) {  // don't keep partly copied file
                    execWithElevatedPrivilege(DEL, {_dest}, errno);
                }
                return;
            }
            processed_size += n;
#ifdef USE_SENDFILE
        }
#endif
        processedSize(processed_size);
    }

    src_file.close();
    dest_file.close();

    if (dest_file.error() != QFile::NoError) {
        qCWarning(KIO_FILE) << "Error when closing file descriptor[2]:" << dest_file.errorString();
        error(KIO::ERR_CANNOT_WRITE, dest);
#if HAVE_POSIX_ACL
        if (acl) {
            acl_free(acl);
        }
#endif
        if (!QFile::remove(dest)) {  // don't keep partly copied file
            execWithElevatedPrivilege(DEL, {_dest}, errno);
        }
        return;
    }

    // set final permissions
    // if no special mode given, preserve the mode from the sourcefile
    if (_mode == -1) {
        _mode = buff_src.st_mode;
    }

    if ((::chmod(_dest.data(), _mode) != 0)
#if HAVE_POSIX_ACL
        || (acl && acl_set_file(_dest.data(), ACL_TYPE_ACCESS, acl) != 0)
#endif
       ) {
        const int errCode = errno;
        KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(dest);
        // Eat the error if the filesystem apparently doesn't support chmod.
        if (mp && mp->testFileSystemFlag(KMountPoint::SupportsChmod)) {
            if (tryChangeFileAttr(CHMOD, {_dest, _mode}, errCode)) {
                warning(i18n("Could not change permissions for '%1'", dest));
            }
        }
    }
#if HAVE_POSIX_ACL
    if (acl) {
        acl_free(acl);
    }
#endif

    // preserve ownership
    if (::chown(_dest.data(), -1 /*keep user*/, buff_src.st_gid) == 0) {
        // as we are the owner of the new file, we can always change the group, but
        // we might not be allowed to change the owner
        (void)::chown(_dest.data(), buff_src.st_uid, -1 /*keep group*/);
    } else {
        if (tryChangeFileAttr(CHOWN, {_dest, buff_src.st_uid, buff_src.st_gid}, errno)) {
            qCWarning(KIO_FILE) << QStringLiteral("Couldn't preserve group for '%1'").arg(dest);
        }
    }

    // copy access and modification time
    struct utimbuf ut;
    ut.actime = buff_src.st_atime;
    ut.modtime = buff_src.st_mtime;
    if (::utime(_dest.data(), &ut) != 0) {
        if (tryChangeFileAttr(UTIME, {_dest, qint64(ut.actime), qint64(ut.modtime)}, errno)) {
            qCWarning(KIO_FILE) << QStringLiteral("Couldn't preserve access and modification time for '%1'").arg(dest);
        }
    }

    processedSize(buff_src.st_size);
    finished();
}

static bool isLocalFileSameHost(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return false;
    }

    if (url.host().isEmpty() || (url.host() == QLatin1String("localhost"))) {
        return true;
    }

    char hostname[ 256 ];
    hostname[ 0 ] = '\0';
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
#ifdef Q_OS_MACOS
    auto length = getxattr(filenameEncoded.data(), attrName, nullptr, 0, 0, XATTR_NOFOLLOW);
#else
    auto length = getxattr(filenameEncoded.data(), attrName, nullptr, 0);
#endif
    if (length <= 0) {
        return false;
    }
    constexpr size_t xattr_size = 1024;
    char strAttr[xattr_size];
#ifdef Q_OS_MACOS
    length = getxattr(filenameEncoded.data(), attrName, strAttr, xattr_size, 0, XATTR_NOFOLLOW);
#else
    length = getxattr(filenameEncoded.data(), attrName, strAttr, xattr_size);
#endif
    if (length <= 0) {
        return false;
    }

    // Decode result to hex string
    static constexpr auto digits = "0123456789abcdef";
    QVarLengthArray<char> hexAttr(static_cast<int>(length) * 2 + 4);
    char *c = strAttr;
    char *e = hexAttr.data();
    *e++ ='0';
    *e++ = 'x';
    for (auto n = 0; n < length; n++, c++) {
        *e++ = digits[(static_cast<uchar>(*c) >> 4)];
        *e++ = digits[(static_cast<uchar>(*c) & 0x0F)];
    }
    *e = '\0';

    // Decode hex string to int
    auto intAttr = static_cast<uint>(strtol(hexAttr.data(), nullptr, 16));

    constexpr auto FILE_ATTRIBUTE_HIDDEN = 0x2u;
    return static_cast<bool>(intAttr & FILE_ATTRIBUTE_HIDDEN);
}
#endif


void FileProtocol::listDir(const QUrl &url)
{
    if (!isLocalFileSameHost(url)) {
        QUrl redir(url);
        redir.setScheme(config()->readEntry("DefaultRemoteProtocol", "smb"));
        redirection(redir);
        // qDebug() << "redirecting to " << redir;
        finished();
        return;
    }
    const QString path(url.toLocalFile());
    const QByteArray _path(QFile::encodeName(path));
    DIR *dp = opendir(_path.data());
    if (dp == nullptr) {
        switch (errno) {
        case ENOENT:
            error(KIO::ERR_DOES_NOT_EXIST, path);
            return;
        case ENOTDIR:
            error(KIO::ERR_IS_FILE, path);
            break;
#ifdef ENOMEDIUM
        case ENOMEDIUM:
            error(ERR_SLAVE_DEFINED,
                  i18n("No media in device for %1", path));
            break;
#endif
        default:
            error(KIO::ERR_CANNOT_ENTER_DIRECTORY, path);
            break;
        }
        return;
    }

    /* set the current dir to the path to speed up
       in not having to pass an absolute path.
       We restore the path later to get out of the
       path - the kernel wouldn't unmount or delete
       directories we keep as active directory. And
       as the slave runs in the background, it's hard
       to see for the user what the problem would be */
    const QString pathBuffer(QDir::currentPath());
    if (!QDir::setCurrent(path)) {
        closedir(dp);
        error(ERR_CANNOT_ENTER_DIRECTORY, path);
        return;
    }

    const QString sDetails = metaData(QStringLiteral("details"));
    const int details = sDetails.isEmpty() ? 2 : sDetails.toInt();
    //qDebug() << "========= LIST " << url << "details=" << details << " =========";
    UDSEntry entry;

#ifndef HAVE_DIRENT_D_TYPE
    QT_STATBUF st;
#endif
    QT_DIRENT *ep;
    while ((ep = QT_READDIR(dp)) != nullptr) {
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
        if (details == 0) {
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
#ifdef HAVE_DIRENT_D_TYPE
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE,
                         (ep->d_type == DT_DIR) ? S_IFDIR : S_IFREG);
            const bool isSymLink = (ep->d_type == DT_LNK);
#else
            // oops, no fast way, we need to stat (e.g. on Solaris)
            if (QT_LSTAT(ep->d_name, &st) == -1) {
                continue; // how can stat fail?
            }
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE,
                         ((st.st_mode & QT_STAT_MASK) == QT_STAT_DIR) ? S_IFDIR : S_IFREG);
            const bool isSymLink = ((st.st_mode & QT_STAT_MASK) == QT_STAT_LNK);
#endif
            if (isSymLink) {
                // for symlinks obey the UDSEntry contract and provide UDS_LINK_DEST
                // even if we don't know the link dest (and DeleteJob doesn't care...)
                entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QStringLiteral("Dummy Link Target"));
            }
            listEntry(entry);

        } else {
            if (createUDSEntry(filename, QByteArray(ep->d_name), entry, details)) {
#if HAVE_SYS_XATTR_H
                if (isNtfsHidden(filename)) {
                    bool ntfsHidden = true;

                    // Bug 392913: NTFS root volume is always "hidden", ignore this
                    if (ep->d_type == DT_DIR || ep->d_type == DT_UNKNOWN) {
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

    // Restore the path
    QDir::setCurrent(pathBuffer);

    finished();
}

void FileProtocol::rename(const QUrl &srcUrl, const QUrl &destUrl,
                          KIO::JobFlags _flags)
{
    char off_t_should_be_64_bits[sizeof(off_t) >= 8 ? 1 : -1]; (void) off_t_should_be_64_bits;
    const QString src = srcUrl.toLocalFile();
    const QString dest = destUrl.toLocalFile();
    const QByteArray _src(QFile::encodeName(src));
    const QByteArray _dest(QFile::encodeName(dest));
    QT_STATBUF buff_src;
    if (QT_LSTAT(_src.data(), &buff_src) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, src);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, src);
        }
        return;
    }

    QT_STATBUF buff_dest;
    // stat symlinks here (lstat, not stat), to avoid ERR_IDENTICAL_FILES when replacing symlink
    // with its target (#169547)
    bool dest_exists = (QT_LSTAT(_dest.data(), &buff_dest) != -1);
    if (dest_exists) {
        if (same_inode(buff_dest, buff_src)) {
            error(KIO::ERR_IDENTICAL_FILES, dest);
            return;
        }

        if ((buff_dest.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
            error(KIO::ERR_DIR_ALREADY_EXIST, dest);
            return;
        }

        if (!(_flags & KIO::Overwrite)) {
            error(KIO::ERR_FILE_ALREADY_EXIST, dest);
            return;
        }
    }

    if (::rename(_src.data(), _dest.data())) {
        if (auto err = execWithElevatedPrivilege(RENAME, {_src, _dest}, errno)) {
            if (!err.wasCanceled()) {
                if ((err == EACCES) || (err == EPERM)) {
                    error(KIO::ERR_ACCESS_DENIED, dest);
                } else if (err == EXDEV) {
                    error(KIO::ERR_UNSUPPORTED_ACTION, QStringLiteral("rename"));
                } else if (err == EROFS) { // The file is on a read-only filesystem
                    error(KIO::ERR_CANNOT_DELETE, src);
                } else {
                    error(KIO::ERR_CANNOT_RENAME, src);
                }
            }
            return;
        }
    }

    finished();
}

void FileProtocol::symlink(const QString &target, const QUrl &destUrl, KIO::JobFlags flags)
{
    const QString dest = destUrl.toLocalFile();
    // Assume dest is local too (wouldn't be here otherwise)
    if (::symlink(QFile::encodeName(target).constData(), QFile::encodeName(dest).constData()) == -1) {
        // Does the destination already exist ?
        if (errno == EEXIST) {
            if ((flags & KIO::Overwrite)) {
                // Try to delete the destination
                if (unlink(QFile::encodeName(dest).constData()) != 0) {
                    if (auto err = execWithElevatedPrivilege(DEL, {dest}, errno)) {
                        if (!err.wasCanceled()) {
                            error(KIO::ERR_CANNOT_DELETE, dest);
                        }
                        return;
                    }
                }
                // Try again - this won't loop forever since unlink succeeded
                symlink(target, destUrl, flags);
                return;
            } else {
                QT_STATBUF buff_dest;
                if (QT_LSTAT(QFile::encodeName(dest).constData(), &buff_dest) == 0 && ((buff_dest.st_mode & QT_STAT_MASK) == QT_STAT_DIR)) {
                    error(KIO::ERR_DIR_ALREADY_EXIST, dest);
                } else {
                    error(KIO::ERR_FILE_ALREADY_EXIST, dest);
                }
                return;
            }
        } else {
            if (auto err = execWithElevatedPrivilege(SYMLINK, {dest, target}, errno)) {
                if (!err.wasCanceled()) {
                    // Some error occurred while we tried to symlink
                    error(KIO::ERR_CANNOT_SYMLINK, dest);
                }
                return;
            }
        }
    }
    finished();
}

void FileProtocol::del(const QUrl &url, bool isfile)
{
    const QString path = url.toLocalFile();
    const QByteArray _path(QFile::encodeName(path));
    /*****
     * Delete files
     *****/

    if (isfile) {
        // qDebug() << "Deleting file "<< url;

        if (unlink(_path.data()) == -1) {
            if (auto err = execWithElevatedPrivilege(DEL, {_path}, errno)) {
                if (!err.wasCanceled()) {
                    if ((err == EACCES) || (err ==  EPERM)) {
                        error(KIO::ERR_ACCESS_DENIED, path);
                    } else if (err == EISDIR) {
                        error(KIO::ERR_IS_DIRECTORY, path);
                    } else {
                        error(KIO::ERR_CANNOT_DELETE, path);
                    }
                }
                return;
            }
        }
    } else {

        /*****
         * Delete empty directory
         *****/

        // qDebug() << "Deleting directory " << url;
        if (metaData(QStringLiteral("recurse")) == QLatin1String("true")) {
            if (!deleteRecursive(path)) {
                return;
            }
        }
        if (QT_RMDIR(_path.data()) == -1) {
            if (auto err = execWithElevatedPrivilege(RMDIR, {_path}, errno)) {
                if (!err.wasCanceled()) {
                    if ((err == EACCES) || (err == EPERM)) {
                        error(KIO::ERR_ACCESS_DENIED, path);
                    } else {
                        // qDebug() << "could not rmdir " << perror;
                        error(KIO::ERR_CANNOT_RMDIR, path);
                    }
                }
                return;
            }
        }
    }

    finished();
}

void FileProtocol::chown(const QUrl &url, const QString &owner, const QString &group)
{
    const QString path = url.toLocalFile();
    const QByteArray _path(QFile::encodeName(path));
    uid_t uid;
    gid_t gid;

    // get uid from given owner
    {
        struct passwd *p = ::getpwnam(owner.toLocal8Bit().constData());

        if (! p) {
            error(KIO::ERR_SLAVE_DEFINED,
                  i18n("Could not get user id for given user name %1", owner));
            return;
        }

        uid = p->pw_uid;
    }

    // get gid from given group
    {
        struct group *p = ::getgrnam(group.toLocal8Bit().constData());

        if (! p) {
            error(KIO::ERR_SLAVE_DEFINED,
                  i18n("Could not get group id for given group name %1", group));
            return;
        }

        gid = p->gr_gid;
    }

    if (::chown(_path.constData(), uid, gid) == -1) {
        if (auto err = execWithElevatedPrivilege(CHOWN, {_path, uid, gid}, errno)) {
            if (!err.wasCanceled()) {
                switch (err) {
                case EPERM:
                case EACCES:
                    error(KIO::ERR_ACCESS_DENIED, path);
                    break;
                case ENOSPC:
                    error(KIO::ERR_DISK_FULL, path);
                    break;
                default:
                    error(KIO::ERR_CANNOT_CHOWN, path);
                }
            }
        }
    } else {
        finished();
    }
}

void FileProtocol::stat(const QUrl &url)
{
    if (!isLocalFileSameHost(url)) {
        redirect(url);
        return;
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
    const QString sDetails = metaData(QStringLiteral("details"));
    const int details = sDetails.isEmpty() ? 2 : sDetails.toInt();

    UDSEntry entry;
    if (!createUDSEntry(url.fileName(), _path, entry, details)) {
        error(KIO::ERR_DOES_NOT_EXIST, path);
        return;
    }
#if 0
///////// debug code
    MetaData::iterator it1 = mOutgoingMetaData.begin();
    for (; it1 != mOutgoingMetaData.end(); it1++) {
        // qDebug() << it1.key() << " = " << it1.data();
    }
/////////
#endif
    statEntry(entry);

    finished();
}

PrivilegeOperationReturnValue FileProtocol::execWithElevatedPrivilege(ActionType action, const QVariantList &args, int errcode)
{
    if (privilegeOperationUnitTestMode()) {
        return PrivilegeOperationReturnValue::success();
    }

    // temporarily disable privilege execution
    if (true) {
        return PrivilegeOperationReturnValue::failure(errcode);
    }

    if (!(errcode == EACCES || errcode == EPERM)) {
        return PrivilegeOperationReturnValue::failure(errcode);
    }

    KIO::PrivilegeOperationStatus opStatus = requestPrivilegeOperation();
    if (opStatus != KIO::OperationAllowed) {
        if (opStatus == KIO::OperationCanceled) {
            error(KIO::ERR_USER_CANCELED, QString());
            return PrivilegeOperationReturnValue::canceled();
        }
        return PrivilegeOperationReturnValue::failure(errcode);
    }

    QByteArray helperArgs;
    QDataStream out(&helperArgs, QIODevice::WriteOnly);
    out << action;
    foreach (const QVariant &arg, args) {
        out << arg;
    }

    const QString actionId = QStringLiteral("org.kde.kio.file.exec");
    KAuth::Action execAction(actionId);
    execAction.setHelperId(QStringLiteral("org.kde.kio.file"));

    QVariantMap argv;
    argv.insert(QStringLiteral("arguments"), helperArgs);
    execAction.setArguments(argv);

    auto reply = execAction.execute();
    if (reply->exec()) {
        addTemporaryAuthorization(actionId);
        return PrivilegeOperationReturnValue::success();
    }

    return PrivilegeOperationReturnValue::failure(KIO::ERR_ACCESS_DENIED);
}
