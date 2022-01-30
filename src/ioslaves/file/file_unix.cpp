/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Christian Ehrlicher <ch.ehrlicher@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "file.h"

#include "config-kioslave-file.h"

#if HAVE_POSIX_ACL
#include <../../aclhelpers_p.h>
#endif

#include <QDir>
#include <QFile>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QThread>
#include <qplatformdefs.h>

#include <KConfigGroup>
#include <KFileSystemType>
#include <KLocalizedString>
#include <QDebug>
#include <kmountpoint.h>

#include <cerrno>
#include <array>
#include <stdint.h>
#include <utime.h>

#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <KRandom>

#include "fdreceiver.h"
#include "statjob.h"

#ifdef Q_OS_LINUX

#include <linux/fs.h>
#include <sys/ioctl.h>
#include <unistd.h>

#endif // Q_OS_LINUX

#if HAVE_STATX
#include <sys/stat.h>
#include <sys/sysmacros.h> // for makedev()
#endif

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

static const QString socketPath()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return QStringLiteral("%1/filehelper%2%3").arg(runtimeDir, KRandom::randomString(6)).arg(getpid());
}

static QString actionDetails(ActionType actionType, const QVariantList &args)
{
    QString action;
    QString detail;
    switch (actionType) {
    case CHMOD:
        action = i18n("Change File Permissions");
        detail = i18n("New Permissions: %1", args[1].toInt());
        break;
    case CHOWN:
        action = i18n("Change File Owner");
        detail = i18n("New Owner: UID=%1, GID=%2", args[1].toInt(), args[2].toInt());
        break;
    case DEL:
        action = i18n("Remove File");
        break;
    case RMDIR:
        action = i18n("Remove Directory");
        break;
    case MKDIR:
        action = i18n("Create Directory");
        detail = i18n("Directory Permissions: %1", args[1].toInt());
        break;
    case OPEN:
        action = i18n("Open File");
        break;
    case OPENDIR:
        action = i18n("Open Directory");
        break;
    case RENAME:
        action = i18n("Rename");
        detail = i18n("New Filename: %1", args[1].toString());
        break;
    case SYMLINK:
        action = i18n("Create Symlink");
        detail = i18n("Target: %1", args[1].toString());
        break;
    case UTIME:
        action = i18n("Change Timestamp");
        break;
    case COPY:
        action = i18n("Copy");
        detail = i18n("From: %1, To: %2", args[0].toString(), args[1].toString());
        break;
    default:
        action = i18n("Unknown Action");
        break;
    }

    const QString metadata = i18n(
        "Action: %1\n"
        "Source: %2\n"
        "%3",
        action,
        args[0].toString(),
        detail);
    return metadata;
}

bool FileProtocol::privilegeOperationUnitTestMode()
{
    return (metaData(QStringLiteral("UnitTesting")) == QLatin1String("true"))
        && (requestPrivilegeOperation(QStringLiteral("Test Call")) == KIO::OperationAllowed);
}

#if HAVE_POSIX_ACL
bool FileProtocol::isExtendedACL(acl_t acl)
{
    return (ACLPortability::acl_equiv_mode(acl, nullptr) != 0);
}
#endif

static QHash<KUserId, QString> staticUserCache;
static QHash<KGroupId, QString> staticGroupCache;

static QString getUserName(KUserId uid)
{
    if (Q_UNLIKELY(!uid.isValid())) {
        return QString();
    }
    auto it = staticUserCache.find(uid);
    if (it == staticUserCache.end()) {
        KUser user(uid);
        QString name = user.loginName();
        if (name.isEmpty()) {
            name = uid.toString();
        }
        it = staticUserCache.insert(uid, name);
    }
    return *it;
}

static QString getGroupName(KGroupId gid)
{
    if (Q_UNLIKELY(!gid.isValid())) {
        return QString();
    }
    auto it = staticGroupCache.find(gid);
    if (it == staticGroupCache.end()) {
        KUserGroup group(gid);
        QString name = group.name();
        if (name.isEmpty()) {
            name = gid.toString();
        }
        it = staticGroupCache.insert(gid, name);
    }
    return *it;
}

#if HAVE_STATX
// statx syscall is available
inline int LSTAT(const char *path, struct statx *buff, KIO::StatDetails details)
{
    uint32_t mask = 0;
    if (details & KIO::StatBasic) {
        // filename, access, type, size, linkdest
        mask |= STATX_SIZE | STATX_TYPE;
    }
    if (details & KIO::StatUser) {
        // uid, gid
        mask |= STATX_UID | STATX_GID;
    }
    if (details & KIO::StatTime) {
        // atime, mtime, btime
        mask |= STATX_ATIME | STATX_MTIME | STATX_BTIME;
    }
    if (details & KIO::StatInode) {
        // dev, inode
        mask |= STATX_INO;
    }
    return statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, mask, buff);
}
inline int STAT(const char *path, struct statx *buff, const KIO::StatDetails &details)
{
    uint32_t mask = 0;
    // KIO::StatAcl needs type
    if (details & (KIO::StatBasic | KIO::StatAcl | KIO::StatResolveSymlink)) {
        // filename, access, type
        mask |= STATX_TYPE;
    }
    if (details & (KIO::StatBasic | KIO::StatResolveSymlink)) {
        // size, linkdest
        mask |= STATX_SIZE;
    }
    if (details & KIO::StatUser) {
        // uid, gid
        mask |= STATX_UID | STATX_GID;
    }
    if (details & KIO::StatTime) {
        // atime, mtime, btime
        mask |= STATX_ATIME | STATX_MTIME | STATX_BTIME;
    }
    // KIO::Inode is ignored as when STAT is called, the entry inode field has already been filled
    return statx(AT_FDCWD, path, AT_STATX_SYNC_AS_STAT, mask, buff);
}
inline static uint16_t stat_mode(const struct statx &buf)
{
    return buf.stx_mode;
}
inline static dev_t stat_dev(const struct statx &buf)
{
    return makedev(buf.stx_dev_major, buf.stx_dev_minor);
}
inline static uint64_t stat_ino(const struct statx &buf)
{
    return buf.stx_ino;
}
inline static uint64_t stat_size(const struct statx &buf)
{
    return buf.stx_size;
}
inline static uint32_t stat_uid(const struct statx &buf)
{
    return buf.stx_uid;
}
inline static uint32_t stat_gid(const struct statx &buf)
{
    return buf.stx_gid;
}
inline static int64_t stat_atime(const struct statx &buf)
{
    return buf.stx_atime.tv_sec;
}
inline static int64_t stat_mtime(const struct statx &buf)
{
    return buf.stx_mtime.tv_sec;
}
#else
// regular stat struct
inline int LSTAT(const char *path, QT_STATBUF *buff, KIO::StatDetails details)
{
    Q_UNUSED(details)
    return QT_LSTAT(path, buff);
}
inline int STAT(const char *path, QT_STATBUF *buff, KIO::StatDetails details)
{
    Q_UNUSED(details)
    return QT_STAT(path, buff);
}
inline static mode_t stat_mode(const QT_STATBUF &buf)
{
    return buf.st_mode;
}
inline static dev_t stat_dev(const QT_STATBUF &buf)
{
    return buf.st_dev;
}
inline static ino_t stat_ino(const QT_STATBUF &buf)
{
    return buf.st_ino;
}
inline static off_t stat_size(const QT_STATBUF &buf)
{
    return buf.st_size;
}
inline static uid_t stat_uid(const QT_STATBUF &buf)
{
    return buf.st_uid;
}
inline static gid_t stat_gid(const QT_STATBUF &buf)
{
    return buf.st_gid;
}
inline static time_t stat_atime(const QT_STATBUF &buf)
{
    return buf.st_atime;
}
inline static time_t stat_mtime(const QT_STATBUF &buf)
{
    return buf.st_mtime;
}
#endif

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
    entry.reserve(entries);

    if (details & KIO::StatBasic) {
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
    }

    bool isBrokenSymLink = false;
#if HAVE_POSIX_ACL
    QByteArray targetPath = path;
#endif

#if HAVE_STATX
    // statx syscall is available
    struct statx buff;
#else
    QT_STATBUF buff;
#endif

    if (LSTAT(path.data(), &buff, details) == 0) {
        if ((stat_mode(buff) & QT_STAT_MASK) == QT_STAT_LNK) {
            QByteArray linkTargetBuffer;
            if (details & (KIO::StatBasic | KIO::StatResolveSymlink)) {
// Use readlink on Unix because symLinkTarget turns relative targets into absolute (#352927)
#if HAVE_STATX
                size_t lowerBound = 256;
                size_t higherBound = 1024;
                uint64_t s = stat_size(buff);
                if (s > SIZE_MAX) {
                    qCWarning(KIO_FILE) << "file size bigger than SIZE_MAX, too big for readlink use!" << path;
                    return false;
                }
                size_t size = static_cast<size_t>(s);
                using SizeType = size_t;
#else
                off_t lowerBound = 256;
                off_t higherBound = 1024;
                off_t size = stat_size(buff);
                using SizeType = off_t;
#endif
                SizeType bufferSize = qBound(lowerBound, size + 1, higherBound);
                linkTargetBuffer.resize(bufferSize);
                while (true) {
                    ssize_t n = readlink(path.constData(), linkTargetBuffer.data(), bufferSize);
                    if (n < 0 && errno != ERANGE) {
                        qCWarning(KIO_FILE) << "readlink failed!" << path;
                        return false;
                    } else if (n > 0 && static_cast<SizeType>(n) != bufferSize) {
                        // the buffer was not filled in the last iteration
                        // we are finished reading, break the loop
                        linkTargetBuffer.truncate(n);
                        break;
                    }
                    bufferSize *= 2;
                    linkTargetBuffer.resize(bufferSize);
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
        entry.fastInsert(KIO::UDSEntry::UDS_USER, getUserName(KUserId(stat_uid(buff))));
        entry.fastInsert(KIO::UDSEntry::UDS_GROUP, getGroupName(KGroupId(stat_gid(buff))));
    }

    if (details & KIO::StatTime) {
        entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, stat_mtime(buff));
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, stat_atime(buff));

#ifdef st_birthtime
        /* For example FreeBSD's and NetBSD's stat contains a field for
         * the inode birth time: st_birthtime
         * This however only works on UFS and ZFS, and not, on say, NFS.
         * Instead of setting a bogus fallback like st_mtime, only use
         * it if it is greater than 0. */
        if (buff.st_birthtime > 0) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.st_birthtime);
        }
#elif defined __st_birthtime
        /* As above, but OpenBSD calls it slightly differently. */
        if (buff.__st_birthtime > 0) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.__st_birthtime);
        }
#elif HAVE_STATX
        /* And linux version using statx syscall */
        if (buff.stx_mask & STATX_BTIME) {
            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, buff.stx_btime.tv_sec);
        }
#endif
    }

    if (details & KIO::StatInode) {
        entry.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, stat_dev(buff));
        entry.fastInsert(KIO::UDSEntry::UDS_INODE, stat_ino(buff));
    }

    if (details & KIO::StatMimeType) {
        QMimeDatabase db;
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, db.mimeTypeForFile(fullPath).name());
    }

    return true;
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
            qCDebug(KIO_FILE) << "the file doesn't have any xattr";
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
        auto next_key = [&]() { keyPtr += keyLen + 1; };
#elif HAVE_SYS_EXTATTR_H
        keyLen = static_cast<unsigned char>(*keyPtr);
        keyPtr++;
        auto next_key = [&]() { keyPtr += keyLen; };
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

void FileProtocol::copy(const QUrl &srcUrl, const QUrl &destUrl, int _mode, JobFlags _flags)
{
    if (privilegeOperationUnitTestMode()) {
        finished();
        return;
    }

    qCDebug(KIO_FILE) << "copy()" << srcUrl << "to" << destUrl << "mode=" << _mode;

    const QString src = srcUrl.toLocalFile();
    QString dest = destUrl.toLocalFile();
    QByteArray _src(QFile::encodeName(src));
    QByteArray _dest(QFile::encodeName(dest));
    QByteArray _destBackup;

    QT_STATBUF buffSrc;
    if (QT_STAT(_src.data(), &buffSrc) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, src);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, src);
        }
        return;
    }

    if (S_ISDIR(buffSrc.st_mode)) {
        error(KIO::ERR_IS_DIRECTORY, src);
        return;
    }
    if (S_ISFIFO(buffSrc.st_mode) || S_ISSOCK(buffSrc.st_mode)) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
        return;
    }

    QT_STATBUF buffDest;
    bool dest_exists = (QT_LSTAT(_dest.data(), &buffDest) != -1);
    if (dest_exists) {
        if (same_inode(buffDest, buffSrc)) {
            error(KIO::ERR_IDENTICAL_FILES, dest);
            return;
        }

        if (S_ISDIR(buffDest.st_mode)) {
            error(KIO::ERR_DIR_ALREADY_EXIST, dest);
            return;
        }

        if (_flags & KIO::Overwrite) {
            // If the destination is a symlink and overwrite is TRUE,
            // remove the symlink first to prevent the scenario where
            // the symlink actually points to current source!
            if (S_ISLNK(buffDest.st_mode)) {
                // qDebug() << "copy(): LINK DESTINATION";
                if (!QFile::remove(dest)) {
                    if (auto err = execWithElevatedPrivilege(DEL, {_dest}, errno)) {
                        if (!err.wasCanceled()) {
                            error(KIO::ERR_CANNOT_DELETE_ORIGINAL, dest);
                        }
                        return;
                    }
                }
            } else if (S_ISREG(buffDest.st_mode)) {
                _destBackup = _dest;
                dest.append(QStringLiteral(".part"));
                _dest = QFile::encodeName(dest);
            }
        } else {
            error(KIO::ERR_FILE_ALREADY_EXIST, dest);
            return;
        }
    }

    QFile srcFile(src);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        if (auto err = tryOpen(srcFile, _src, O_RDONLY, S_IRUSR, errno)) {
            if (!err.wasCanceled()) {
                error(KIO::ERR_CANNOT_OPEN_FOR_READING, src);
            }
            return;
        }
    }

#if HAVE_FADVISE
    posix_fadvise(srcFile.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    QFile destFile(dest);
    if (!destFile.open(QIODevice::Truncate | QIODevice::WriteOnly)) {
        if (auto err = tryOpen(destFile, _dest, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR, errno)) {
            if (!err.wasCanceled()) {
                // qDebug() << "###### COULD NOT WRITE " << dest;
                if (err == EACCES) {
                    error(KIO::ERR_WRITE_ACCESS_DENIED, dest);
                } else {
                    error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
                }
            }
            return;
        }
    }

    // _mode == -1 means don't touch dest permissions, leave it with the system default ones
    if (_mode != -1) {
        if (::chmod(_dest.constData(), _mode) == -1) {
            const int errCode = errno;
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(dest);
            // Eat the error if the filesystem apparently doesn't support chmod.
            // This test isn't fullproof though, vboxsf (VirtualBox shared folder) supports
            // chmod if the host is Linux, and doesn't if the host is Windows. Hard to detect.
            if (mp && mp->testFileSystemFlag(KMountPoint::SupportsChmod)) {
                if (tryChangeFileAttr(CHMOD, {_dest, _mode}, errCode)) {
                    qCWarning(KIO_FILE) << "Could not change permissions for" << dest;
                }
            }
        }
    }

#if HAVE_FADVISE
    posix_fadvise(destFile.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    totalSize(buffSrc.st_size);

    off_t sizeProcessed = 0;

#ifdef FICLONE
    // Share data blocks ("reflink") on supporting filesystems, like brfs and XFS
    int ret = ::ioctl(destFile.handle(), FICLONE, srcFile.handle());
    if (ret != -1) {
        sizeProcessed = srcFile.size();
        processedSize(srcFile.size());
    }
    // if fs does not support reflinking, files are on different devices...
#endif

    bool existingDestDeleteAttempted = false;

    processedSize(sizeProcessed);

#if HAVE_COPY_FILE_RANGE
    while (!wasKilled() && sizeProcessed < srcFile.size()) {
        if (testMode && destFile.fileName().contains(QLatin1String("slow"))) {
            QThread::msleep(50);
        }

        const ssize_t copiedBytes = ::copy_file_range(srcFile.handle(), nullptr, destFile.handle(), nullptr, s_maxIPCSize, 0);

        if (copiedBytes == -1) {
            if (errno == EINVAL || errno == EXDEV) {
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
                error(KIO::ERR_DISK_FULL, dest);
            } else {
                error(KIO::ERR_SLAVE_DEFINED, i18n("Cannot copy file from %1 to %2. (Errno: %3)", src, dest, errno));
            }

            if (!QFile::remove(dest)) { // don't keep partly copied file
                execWithElevatedPrivilege(DEL, {_dest}, errno);
            }
            return;
        }

        sizeProcessed += copiedBytes;
        processedSize(sizeProcessed);
    }
#endif

    /* standard read/write fallback */
    if (sizeProcessed < srcFile.size()) {
        std::array<char, s_maxIPCSize> buffer;
        while (!wasKilled() && sizeProcessed < srcFile.size()) {
            if (testMode && destFile.fileName().contains(QLatin1String("slow"))) {
                QThread::msleep(50);
            }

            const ssize_t readBytes = ::read(srcFile.handle(), &buffer, s_maxIPCSize);

            if (readBytes == -1) {
                if (errno == EINTR) { // Interrupted
                    continue;
                } else {
                    qCWarning(KIO_FILE) << "Couldn't read[2]. Error:" << srcFile.errorString();
                    error(KIO::ERR_CANNOT_READ, src);
                }

                if (!QFile::remove(dest)) { // don't keep partly copied file
                    execWithElevatedPrivilege(DEL, {_dest}, errno);
                }
                return;
            }

            if (destFile.write(buffer.data(), readBytes) != readBytes) {
                if (destFile.error() == QFileDevice::ResourceError) { // disk full
                    // attempt to free disk space occupied by file being overwritten
                    if (!_destBackup.isEmpty() && !existingDestDeleteAttempted) {
                        ::unlink(_destBackup.constData());
                        existingDestDeleteAttempted = true;
                        if (destFile.write(buffer.data(), readBytes) == readBytes) { // retry
                            continue;
                        }
                    }
                    error(KIO::ERR_DISK_FULL, dest);
                } else {
                    qCWarning(KIO_FILE) << "Couldn't write[2]. Error:" << destFile.errorString();
                    error(KIO::ERR_CANNOT_WRITE, dest);
                }

                if (!QFile::remove(dest)) { // don't keep partly copied file
                    execWithElevatedPrivilege(DEL, {_dest}, errno);
                }
                return;
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

    destFile.flush(); // so the write() happens before futimes()

    // copy access and modification time
    if (!wasKilled()) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
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
            if (tryChangeFileAttr(UTIME, {_dest, qint64(buffSrc.st_atime), qint64(buffSrc.st_mtime)}, errno)) {
                qCWarning(KIO_FILE) << "Couldn't preserve access and modification time for" << dest;
            }
        }
    }

    destFile.close();

    if (wasKilled()) {
        qCDebug(KIO_FILE) << "Clean dest file after ioslave was killed:" << dest;
        if (!QFile::remove(dest)) { // don't keep partly copied file
            execWithElevatedPrivilege(DEL, {_dest}, errno);
        }
        error(KIO::ERR_USER_CANCELED, dest);
        return;
    }

    if (destFile.error() != QFile::NoError) {
        qCWarning(KIO_FILE) << "Error when closing file descriptor[2]:" << destFile.errorString();
        error(KIO::ERR_CANNOT_WRITE, dest);

        if (!QFile::remove(dest)) { // don't keep partly copied file
            execWithElevatedPrivilege(DEL, {_dest}, errno);
        }
        return;
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

    // preserve ownership
    if (_mode != -1) {
        if (::chown(_dest.data(), -1 /*keep user*/, buffSrc.st_gid) == 0) {
            // as we are the owner of the new file, we can always change the group, but
            // we might not be allowed to change the owner
            (void)::chown(_dest.data(), buffSrc.st_uid, -1 /*keep group*/);
        } else {
            if (tryChangeFileAttr(CHOWN, {_dest, buffSrc.st_uid, buffSrc.st_gid}, errno)) {
                qCWarning(KIO_FILE) << "Couldn't preserve group for" << dest;
            }
        }
    }

    if (!_destBackup.isEmpty()) { // Overwrite final dest file with new file
        if (::unlink(_destBackup.constData()) == -1) {
            qCWarning(KIO_FILE) << "Couldn't remove original dest" << _destBackup << "(" << strerror(errno) << ")";
        }

        if (::rename(_dest.constData(), _destBackup.constData()) == -1) {
            qCWarning(KIO_FILE) << "Couldn't rename" << _dest << "to" << _destBackup << "(" << strerror(errno) << ")";
        }
    }

    processedSize(buffSrc.st_size);
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

void FileProtocol::listDir(const QUrl &url)
{
    if (!isLocalFileSameHost(url)) {
        QUrl redir(url);
        redir.setScheme(configValue(QStringLiteral("DefaultRemoteProtocol"), QStringLiteral("smb")));
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
            error(ERR_SLAVE_DEFINED, i18n("No media in device for %1", path));
            break;
#endif
        default:
            error(KIO::ERR_CANNOT_ENTER_DIRECTORY, path);
            break;
        }
        return;
    }

    const QByteArray encodedBasePath = _path + '/';

    const KIO::StatDetails details = getStatDetails();
    // qDebug() << "========= LIST " << url << "details=" << details << " =========";
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
        if (details == KIO::StatBasic) {
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
#ifdef HAVE_DIRENT_D_TYPE
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
            QString fullPath(path);
            if (!fullPath.endsWith(QLatin1Char('/'))) {
                fullPath += QLatin1Char('/');
            }
            fullPath += filename;

            if (createUDSEntry(filename, encodedBasePath + QByteArray(ep->d_name), entry, details, fullPath)) {
#if HAVE_SYS_XATTR_H
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

    finished();
}

void FileProtocol::rename(const QUrl &srcUrl, const QUrl &destUrl, KIO::JobFlags _flags)
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
        // Try QFile::rename(), this can help when renaming 'a' to 'A' on a case-insensitive
        // filesystem, e.g. FAT32/VFAT.
        if (src != dest && QString::compare(src, dest, Qt::CaseInsensitive) == 0) {
            qCDebug(KIO_FILE) << "Dest already exists; detected special case of lower/uppercase renaming"
                              << "in same dir on a case-insensitive filesystem, try with QFile::rename()"
                              << "(which uses 2 rename calls)";
            if (QFile::rename(src, dest)) {
                finished();
                return;
            }
        }

        if (same_inode(buff_dest, buff_src)) {
            error(KIO::ERR_IDENTICAL_FILES, dest);
            return;
        }

        if (S_ISDIR(buff_dest.st_mode)) {
            error(KIO::ERR_DIR_ALREADY_EXIST, dest);
            return;
        }

        if (!(_flags & KIO::Overwrite)) {
            error(KIO::ERR_FILE_ALREADY_EXIST, dest);
            return;
        }
    }

    if (::rename(_src.data(), _dest.data()) == -1) {
        if (auto err = execWithElevatedPrivilege(RENAME, {_src, _dest}, errno)) {
            if (!err.wasCanceled()) {
                if ((err == EACCES) || (err == EPERM)) {
                    error(KIO::ERR_WRITE_ACCESS_DENIED, dest);
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
    // Assume dest is local too (wouldn't be here otherwise)
    const QString dest = destUrl.toLocalFile();
    const QByteArray dest_c = QFile::encodeName(dest);

    if (::symlink(QFile::encodeName(target).constData(), dest_c.constData()) == 0) {
        finished();
        return;
    }

    // Does the destination already exist ?
    if (errno == EEXIST) {
        if (flags & KIO::Overwrite) {
            // Try to delete the destination
            if (unlink(dest_c.constData()) != 0) {
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
            if (QT_STATBUF buff_dest; QT_LSTAT(dest_c.constData(), &buff_dest) == 0) {
                error(S_ISDIR(buff_dest.st_mode) ? KIO::ERR_DIR_ALREADY_EXIST : KIO::ERR_FILE_ALREADY_EXIST, dest);
            } else { // Can't happen, we already know "dest" exists
                error(KIO::ERR_CANNOT_SYMLINK, dest);
            }

            return;
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

            error(KIO::ERR_SLAVE_DEFINED, msg);
            return;
        }
    }

    auto res = execWithElevatedPrivilege(SYMLINK, {dest, target}, errno);
    if (res == PrivilegeOperationReturnValue::success()) {
        finished();
    } else if (!res.wasCanceled()) {
        // Some error occurred while we tried to symlink
        error(KIO::ERR_CANNOT_SYMLINK, dest);
    }
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
                    if ((err == EACCES) || (err == EPERM)) {
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

        if (!p) {
            error(KIO::ERR_SLAVE_DEFINED, i18n("Could not get user id for given user name %1", owner));
            return;
        }

        uid = p->pw_uid;
    }

    // get gid from given group
    {
        struct group *p = ::getgrnam(group.toLocal8Bit().constData());

        if (!p) {
            error(KIO::ERR_SLAVE_DEFINED, i18n("Could not get group id for given group name %1", group));
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

KIO::StatDetails FileProtocol::getStatDetails()
{
    // takes care of converting old metadata details to new StatDetails
    // TODO KF6 : remove legacy "details" code path
    KIO::StatDetails details;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
    if (hasMetaData(QStringLiteral("statDetails"))) {
#endif
        const QString statDetails = metaData(QStringLiteral("statDetails"));
        details = statDetails.isEmpty() ? KIO::StatDefaultDetails : static_cast<KIO::StatDetails>(statDetails.toInt());
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
    } else {
        const QString sDetails = metaData(QStringLiteral("details"));
        details = sDetails.isEmpty() ? KIO::StatDefaultDetails : KIO::detailsToStatDetails(sDetails.toInt());
    }
#endif
    return details;
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

    const KIO::StatDetails details = getStatDetails();

    UDSEntry entry;
    if (!createUDSEntry(url.fileName(), _path, entry, details, path)) {
        error(KIO::ERR_DOES_NOT_EXIST, path);
        return;
    }
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

    const QString operationDetails = actionDetails(action, args);
    KIO::PrivilegeOperationStatus opStatus = requestPrivilegeOperation(operationDetails);
    if (opStatus != KIO::OperationAllowed) {
        if (opStatus == KIO::OperationCanceled) {
            error(KIO::ERR_USER_CANCELED, QString());
            return PrivilegeOperationReturnValue::canceled();
        }
        return PrivilegeOperationReturnValue::failure(errcode);
    }

    const QUrl targetUrl = QUrl::fromLocalFile(args.first().toString()); // target is always the first item.
    const bool useParent = action != CHOWN && action != CHMOD && action != UTIME;
    const QString targetPath = useParent ? targetUrl.adjusted(QUrl::RemoveFilename).toLocalFile() : targetUrl.toLocalFile();
    bool userIsOwner = QFileInfo(targetPath).ownerId() == getuid();
    if (action == RENAME) { // for rename check src and dest owner
        QString dest = QUrl(args[1].toString()).toLocalFile();
        userIsOwner = userIsOwner && QFileInfo(dest).ownerId() == getuid();
    }
    if (userIsOwner) {
        error(KIO::ERR_PRIVILEGE_NOT_REQUIRED, targetPath);
        return PrivilegeOperationReturnValue::canceled();
    }

    QByteArray helperArgs;
    QDataStream out(&helperArgs, QIODevice::WriteOnly);
    out << action;
    for (const QVariant &arg : args) {
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
