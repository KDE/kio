/*
    SPDX-FileCopyrightText: 2000-2002 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2002 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <sandfeld@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "file.h"

#include <QDirIterator>

#include <QStorageInfo>

#include "kioglobal_p.h"

#ifdef Q_OS_UNIX
#include "legacycodec.h"
#endif

#include <assert.h>
#include <cerrno>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <sys/utime.h>
#include <winsock2.h> //struct timeval
#else
#include <utime.h>
#endif

#include <QCoreApplication>
#include <QDate>
#include <QTemporaryFile>
#include <QVarLengthArray>
#ifdef Q_OS_WIN
#include <QDir>
#include <QFileInfo>
#endif

#include <KConfigGroup>
#include <KLocalizedString>
#include <KShell>
#include <QDataStream>
#include <QDebug>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <kmountpoint.h>

#include <ioslave_defaults.h>
#include <kdirnotify.h>
#include <workerfactory.h>

Q_LOGGING_CATEGORY(KIO_FILE, "kf.kio.slaves.file")

class KIOPluginFactory : public KIO::WorkerFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.file" FILE "file.json")
public:
    std::unique_ptr<KIO::SlaveBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        return std::make_unique<FileProtocol>(pool, app);
    }
};

using namespace KIO;

static constexpr int s_maxIPCSize = 1024 * 32;

static QString readLogFile(const QByteArray &_filename);

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv); // needed for QSocketNotifier
    app.setApplicationName(QStringLiteral("kio_file"));

    if (argc != 4) {
        fprintf(stderr, "Usage: kio_file protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

#ifdef Q_OS_UNIX
    // From Qt doc : "Note that you should not delete codecs yourself: once created they become Qt's responsibility"
    (void)new LegacyCodec;
#endif

    FileProtocol slave(argv[2], argv[3]);

    // Make sure the first kDebug is after the slave ctor (which sets a SIGPIPE handler)
    // This is useful in case kdeinit was autostarted by another app, which then exited and closed fd2
    // (e.g. ctest does that, or closing the terminal window would do that)
    // qDebug() << "Starting" << getpid();

    slave.dispatchLoop();

    // qDebug() << "Done";
    return 0;
}

static QFile::Permissions modeToQFilePermissions(int mode)
{
    QFile::Permissions perms;
    if (mode & S_IRUSR) {
        perms |= QFile::ReadOwner;
    }
    if (mode & S_IWUSR) {
        perms |= QFile::WriteOwner;
    }
    if (mode & S_IXUSR) {
        perms |= QFile::ExeOwner;
    }
    if (mode & S_IRGRP) {
        perms |= QFile::ReadGroup;
    }
    if (mode & S_IWGRP) {
        perms |= QFile::WriteGroup;
    }
    if (mode & S_IXGRP) {
        perms |= QFile::ExeGroup;
    }
    if (mode & S_IROTH) {
        perms |= QFile::ReadOther;
    }
    if (mode & S_IWOTH) {
        perms |= QFile::WriteOther;
    }
    if (mode & S_IXOTH) {
        perms |= QFile::ExeOther;
    }

    return perms;
}

FileProtocol::FileProtocol(const QByteArray &pool, const QByteArray &app)
    : SlaveBase(QByteArrayLiteral("file"), pool, app)
    , mFile(nullptr)
{
    testMode = !qEnvironmentVariableIsEmpty("KIOSLAVE_FILE_ENABLE_TESTMODE");
}

FileProtocol::~FileProtocol()
{
}

void FileProtocol::chmod(const QUrl &url, int permissions)
{
    const QString path(url.toLocalFile());
    const QByteArray _path(QFile::encodeName(path));
    /* FIXME: Should be atomic */
#ifdef Q_OS_UNIX
    // QFile::Permissions does not support special attributes like sticky
    if (::chmod(_path.constData(), permissions) == -1 ||
#else
    if (!QFile::setPermissions(path, modeToQFilePermissions(permissions)) ||
#endif
        (setACL(_path.data(), permissions, false) == -1) ||
        /* if not a directory, cannot set default ACLs */
        (setACL(_path.data(), permissions, true) == -1 && errno != ENOTDIR)) {
        if (auto err = execWithElevatedPrivilege(CHMOD, {_path, permissions}, errno)) {
            if (!err.wasCanceled()) {
                switch (err) {
                case EPERM:
                case EACCES:
                    error(KIO::ERR_ACCESS_DENIED, path);
                    break;
#if defined(ENOTSUP)
                case ENOTSUP: // from setACL since chmod can't return ENOTSUP
                    error(KIO::ERR_UNSUPPORTED_ACTION, i18n("Setting ACL for %1", path));
                    break;
#endif
                case ENOSPC:
                    error(KIO::ERR_DISK_FULL, path);
                    break;
                default:
                    error(KIO::ERR_CANNOT_CHMOD, path);
                }
                return;
            }
        }
    }

    finished();
}

void FileProtocol::setModificationTime(const QUrl &url, const QDateTime &mtime)
{
    const QString path(url.toLocalFile());
    QT_STATBUF statbuf;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &statbuf) == 0) {
        struct utimbuf utbuf;
        utbuf.actime = statbuf.st_atime; // access time, unchanged
        utbuf.modtime = mtime.toSecsSinceEpoch(); // modification time
        if (::utime(QFile::encodeName(path).constData(), &utbuf) != 0) {
            if (auto err = execWithElevatedPrivilege(UTIME, {path, qint64(utbuf.actime), qint64(utbuf.modtime)}, errno)) {
                if (!err.wasCanceled()) {
                    // TODO: errno could be EACCES, EPERM, EROFS
                    error(KIO::ERR_CANNOT_SETTIME, path);
                }
            }
        } else {
            finished();
        }
    } else {
        error(KIO::ERR_DOES_NOT_EXIST, path);
    }
}

void FileProtocol::mkdir(const QUrl &url, int permissions)
{
    const QString path(url.toLocalFile());

    // qDebug() << path << "permission=" << permissions;

    // Remove existing file or symlink, if requested (#151851)
    if (metaData(QStringLiteral("overwrite")) == QLatin1String("true")) {
        if (!QFile::remove(path)) {
            execWithElevatedPrivilege(DEL, {path}, errno);
        }
    }

    QT_STATBUF buff;
    if (QT_LSTAT(QFile::encodeName(path).constData(), &buff) == -1) {
        bool dirCreated = QDir().mkdir(path);
        if (!dirCreated) {
            if (auto err = execWithElevatedPrivilege(MKDIR, {path}, errno)) {
                if (!err.wasCanceled()) {
                    // TODO: add access denied & disk full (or another reasons) handling (into Qt, possibly)
                    error(KIO::ERR_CANNOT_MKDIR, path);
                }
                return;
            }
            dirCreated = true;
        }

        if (dirCreated) {
            if (permissions != -1) {
                chmod(url, permissions);
            } else {
                finished();
            }
            return;
        }
    }

    if ((buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
        // qDebug() << "ERR_DIR_ALREADY_EXIST";
        error(KIO::ERR_DIR_ALREADY_EXIST, path);
        return;
    }
    error(KIO::ERR_FILE_ALREADY_EXIST, path);
    return;
}

void FileProtocol::redirect(const QUrl &url)
{
    QUrl redir(url);
    redir.setScheme(configValue(QStringLiteral("DefaultRemoteProtocol"), QStringLiteral("smb")));

    // if we would redirect into the Windows world, let's also check for the
    // DavWWWRoot "token" which in the Windows world tells win explorer to access
    // a webdav url
    // https://www.webdavsystem.com/server/access/windows
    if ((redir.scheme() == QLatin1String("smb")) && redir.path().startsWith(QLatin1String("/DavWWWRoot/"))) {
        redir.setPath(redir.path().mid(11)); // remove /DavWWWRoot
        redir.setScheme(QStringLiteral("webdav"));
    }

    redirection(redir);
    finished();
}

void FileProtocol::get(const QUrl &url)
{
    if (!url.isLocalFile()) {
        redirect(url);
        return;
    }

    const QString path(url.toLocalFile());
    QT_STATBUF buff;
    if (QT_STAT(QFile::encodeName(path).constData(), &buff) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, path);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, path);
        }
        return;
    }

    if ((buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
        error(KIO::ERR_IS_DIRECTORY, path);
        return;
    }
    if ((buff.st_mode & QT_STAT_MASK) != QT_STAT_REG) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
        return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (auto err = tryOpen(f, QFile::encodeName(path), O_RDONLY, S_IRUSR, errno)) {
            if (!err.wasCanceled()) {
                error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
            }
            return;
        }
    }

#if HAVE_FADVISE
    // TODO check return code
    posix_fadvise(f.handle(), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    // Determine the MIME type of the file to be retrieved, and emit it.
    // This is mandatory in all slaves (for KRun/BrowserRun to work)
    // In real "remote" slaves, this is usually done using mimeTypeForFileNameAndData
    // after receiving some data. But we don't know how much data the mimemagic rules
    // need, so for local files, better use mimeTypeForFile.
    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForFile(url.toLocalFile());
    mimeType(mt.name());
    // Emit total size AFTER the MIME type
    totalSize(buff.st_size);

    KIO::filesize_t processed_size = 0;

    QString resumeOffset = metaData(QStringLiteral("range-start"));
    if (resumeOffset.isEmpty()) {
        resumeOffset = metaData(QStringLiteral("resume")); // old name
    }
    if (!resumeOffset.isEmpty()) {
        bool ok;
        KIO::fileoffset_t offset = resumeOffset.toLongLong(&ok);
        if (ok && (offset > 0) && (offset < buff.st_size)) {
            if (f.seek(offset)) {
                canResume();
                processed_size = offset;
                // qDebug() << "Resume offset:" << KIO::number(offset);
            }
        }
    }

    char buffer[s_maxIPCSize];
    QByteArray array;

    while (1) {
        int n = f.read(buffer, s_maxIPCSize);
        if (n == -1) {
            if (errno == EINTR) {
                continue;
            }
            error(KIO::ERR_CANNOT_READ, path);
            f.close();
            return;
        }
        if (n == 0) {
            break; // Finished
        }

        array = QByteArray::fromRawData(buffer, n);
        data(array);
        array.clear();

        processed_size += n;
        processedSize(processed_size);

        // qDebug() << "Processed: " << KIO::number (processed_size);
    }

    data(QByteArray());

    f.close();

    processedSize(buff.st_size);
    finished();
}

void FileProtocol::open(const QUrl &url, QIODevice::OpenMode mode)
{
    // qDebug() << url;

    QString openPath = url.toLocalFile();
    QT_STATBUF buff;
    if (QT_STAT(QFile::encodeName(openPath).constData(), &buff) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, openPath);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, openPath);
        }
        return;
    }

    if ((buff.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
        error(KIO::ERR_IS_DIRECTORY, openPath);
        return;
    }
    if ((buff.st_mode & QT_STAT_MASK) != QT_STAT_REG) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, openPath);
        return;
    }

    mFile = new QFile(openPath);
    if (!mFile->open(mode)) {
        if (mode & QIODevice::ReadOnly) {
            error(KIO::ERR_CANNOT_OPEN_FOR_READING, openPath);
        } else {
            error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, openPath);
        }

        return;
    }
    // Determine the MIME type of the file to be retrieved, and emit it.
    // This is mandatory in all slaves (for KRun/BrowserRun to work).
    // If we're not opening the file ReadOnly or ReadWrite, don't attempt to
    // read the file and send the MIME type.
    if (mode & QIODevice::ReadOnly) {
        QMimeDatabase db;
        QMimeType mt = db.mimeTypeForFile(url.toLocalFile());
        mimeType(mt.name());
    }

    totalSize(buff.st_size);
    position(0);

    opened();
}

void FileProtocol::read(KIO::filesize_t bytes)
{
    // qDebug() << "File::open -- read";
    Q_ASSERT(mFile && mFile->isOpen());

    QVarLengthArray<char> buffer(bytes);

    qint64 bytesRead = mFile->read(buffer.data(), bytes);

    if (bytesRead == -1) {
        qCWarning(KIO_FILE) << "Couldn't read. Error:" << mFile->errorString();
        error(KIO::ERR_CANNOT_READ, mFile->fileName());
        closeWithoutFinish();
    } else {
        const QByteArray fileData = QByteArray::fromRawData(buffer.data(), bytesRead);
        data(fileData);
    }
}

void FileProtocol::write(const QByteArray &data)
{
    // qDebug() << "File::open -- write";
    Q_ASSERT(mFile && mFile->isWritable());

    qint64 bytesWritten = mFile->write(data);

    if (bytesWritten == -1) {
        if (mFile->error() == QFileDevice::ResourceError) { // disk full
            error(KIO::ERR_DISK_FULL, mFile->fileName());
            closeWithoutFinish();
        } else {
            qCWarning(KIO_FILE) << "Couldn't write. Error:" << mFile->errorString();
            error(KIO::ERR_CANNOT_WRITE, mFile->fileName());
            closeWithoutFinish();
        }
    } else {
        mFile->flush();
        written(bytesWritten);
    }
}

void FileProtocol::seek(KIO::filesize_t offset)
{
    // qDebug() << "File::open -- seek";
    Q_ASSERT(mFile && mFile->isOpen());

    if (mFile->seek(offset)) {
        position(offset);
    } else {
        error(KIO::ERR_CANNOT_SEEK, mFile->fileName());
        closeWithoutFinish();
    }
}

void FileProtocol::truncate(KIO::filesize_t length)
{
    Q_ASSERT(mFile && mFile->isOpen());

    if (mFile->resize(length)) {
        truncated(length);
    } else {
        error(KIO::ERR_CANNOT_TRUNCATE, mFile->fileName());
        closeWithoutFinish();
    }
}

void FileProtocol::closeWithoutFinish()
{
    Q_ASSERT(mFile);

    delete mFile;
    mFile = nullptr;
}

void FileProtocol::close()
{
    // qDebug() << "File::open -- close ";
    closeWithoutFinish();
    finished();
}

void FileProtocol::put(const QUrl &url, int _mode, KIO::JobFlags _flags)
{
    if (privilegeOperationUnitTestMode()) {
        finished();
        return;
    }

    const QString dest_orig = url.toLocalFile();

    // qDebug() << dest_orig << "mode=" << _mode;

    QString dest_part(dest_orig + QLatin1String(".part"));

    QT_STATBUF buff_orig;
    const bool bOrigExists = (QT_LSTAT(QFile::encodeName(dest_orig).constData(), &buff_orig) != -1);
    bool bPartExists = false;
    const bool bMarkPartial = configValue(QStringLiteral("MarkPartial"), true);

    if (bMarkPartial) {
        QT_STATBUF buff_part;
        bPartExists = (QT_LSTAT(QFile::encodeName(dest_part).constData(), &buff_part) != -1);

        if (bPartExists && !(_flags & KIO::Resume) && !(_flags & KIO::Overwrite) && buff_part.st_size > 0
            && ((buff_part.st_mode & QT_STAT_MASK) == QT_STAT_REG)) {
            // qDebug() << "calling canResume with" << KIO::number(buff_part.st_size);

            // Maybe we can use this partial file for resuming
            // Tell about the size we have, and the app will tell us
            // if it's ok to resume or not.
            _flags |= canResume(buff_part.st_size) ? KIO::Resume : KIO::DefaultFlags;

            // qDebug() << "got answer" << (_flags & KIO::Resume);
        }
    }

    if (bOrigExists && !(_flags & KIO::Overwrite) && !(_flags & KIO::Resume)) {
        if ((buff_orig.st_mode & QT_STAT_MASK) == QT_STAT_DIR) {
            error(KIO::ERR_DIR_ALREADY_EXIST, dest_orig);
        } else {
            error(KIO::ERR_FILE_ALREADY_EXIST, dest_orig);
        }
        return;
    }

    int result;
    QString dest;
    QFile f;

    // Loop until we got 0 (end of data)
    do {
        QByteArray buffer;
        dataReq(); // Request for data
        result = readData(buffer);

        if (result >= 0) {
            if (dest.isEmpty()) {
                if (bMarkPartial) {
                    // qDebug() << "Appending .part extension to" << dest_orig;
                    dest = dest_part;
                    if (bPartExists && !(_flags & KIO::Resume)) {
                        // qDebug() << "Deleting partial file" << dest_part;
                        QFile::remove(dest_part);
                        // Catch errors when we try to open the file.
                    }
                } else {
                    dest = dest_orig;
                    if (bOrigExists && !(_flags & KIO::Resume)) {
                        // qDebug() << "Deleting destination file" << dest_orig;
                        QFile::remove(dest_orig);
                        // Catch errors when we try to open the file.
                    }
                }

                f.setFileName(dest);

                if ((_flags & KIO::Resume)) {
                    f.open(QIODevice::ReadWrite | QIODevice::Append);
                } else {
                    f.open(QIODevice::Truncate | QIODevice::WriteOnly);
                    if (_mode != -1) {
                        // WABA: Make sure that we keep writing permissions ourselves,
                        // otherwise we can be in for a surprise on NFS.
                        mode_t initialMode = _mode | S_IWUSR | S_IRUSR;
                        f.setPermissions(modeToQFilePermissions(initialMode));
                    }
                }

                if (!f.isOpen()) {
                    int oflags = 0;
                    int filemode = _mode;

                    if ((_flags & KIO::Resume)) {
                        oflags = O_RDWR | O_APPEND;
                    } else {
                        oflags = O_WRONLY | O_TRUNC | O_CREAT;
                        if (_mode != -1) {
                            filemode = _mode | S_IWUSR | S_IRUSR;
                        }
                    }

                    if (auto err = tryOpen(f, QFile::encodeName(dest), oflags, filemode, errno)) {
                        if (!err.wasCanceled()) {
                            // qDebug() << "####################### COULD NOT WRITE" << dest << "_mode=" << _mode;
                            // qDebug() << "QFile error==" << f.error() << "(" << f.errorString() << ")";

                            if (f.error() == QFileDevice::PermissionsError) {
                                error(KIO::ERR_WRITE_ACCESS_DENIED, dest);
                            } else {
                                error(KIO::ERR_CANNOT_OPEN_FOR_WRITING, dest);
                            }
                        }
                        return;
                    } else {
#ifndef Q_OS_WIN
                        if ((_flags & KIO::Resume)) {
                            execWithElevatedPrivilege(CHOWN, {dest, getuid(), getgid()}, errno);
                            QFile::setPermissions(dest, modeToQFilePermissions(filemode));
                        }
#endif
                    }
                }
            }

            if (f.write(buffer) == -1) {
                if (f.error() == QFile::ResourceError) { // disk full
                    error(KIO::ERR_DISK_FULL, dest_orig);
                    result = -2; // means: remove dest file
                } else {
                    qCWarning(KIO_FILE) << "Couldn't write. Error:" << f.errorString();
                    error(KIO::ERR_CANNOT_WRITE, dest_orig);
                    result = -1;
                }
            }
        } else {
            qCWarning(KIO_FILE) << "readData() returned" << result;
            error(KIO::ERR_CANNOT_WRITE, dest_orig);
        }
    } while (result > 0);

    // An error occurred deal with it.
    if (result < 0) {
        // qDebug() << "Error during 'put'. Aborting.";

        if (f.isOpen()) {
            f.close();

            QT_STATBUF buff;
            if (QT_STAT(QFile::encodeName(dest).constData(), &buff) == 0) {
                int size = configValue(QStringLiteral("MinimumKeepSize"), DEFAULT_MINIMUM_KEEP_SIZE);
                if (buff.st_size < size) {
                    QFile::remove(dest);
                }
            }
        }
        return;
    }

    if (!f.isOpen()) { // we got nothing to write out, so we never opened the file
        finished();
        return;
    }

    f.close();

    if (f.error() != QFile::NoError) {
        qCWarning(KIO_FILE) << "Error when closing file descriptor:" << f.errorString();
        error(KIO::ERR_CANNOT_WRITE, dest_orig);
        return;
    }

    // after full download rename the file back to original name
    if (bMarkPartial) {
        // QFile::rename() never overwrites the destination file unlike ::remove,
        // so we must remove it manually first
        if (_flags & KIO::Overwrite) {
            if (!QFile::remove(dest_orig)) {
                execWithElevatedPrivilege(DEL, {dest_orig}, errno);
            }
        }

        if (!QFile::rename(dest, dest_orig)) {
            if (auto err = execWithElevatedPrivilege(RENAME, {dest, dest_orig}, errno)) {
                if (!err.wasCanceled()) {
                    qCWarning(KIO_FILE) << " Couldn't rename " << dest << " to " << dest_orig;
                    error(KIO::ERR_CANNOT_RENAME_PARTIAL, dest_orig);
                }
                return;
            }
        }
        org::kde::KDirNotify::emitFileRenamed(QUrl::fromLocalFile(dest), QUrl::fromLocalFile(dest_orig));
    }

    // set final permissions
    if (_mode != -1 && !(_flags & KIO::Resume)) {
        if (!QFile::setPermissions(dest_orig, modeToQFilePermissions(_mode))) {
            // couldn't chmod. Eat the error if the filesystem apparently doesn't support it.
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(dest_orig);
            if (mp && mp->testFileSystemFlag(KMountPoint::SupportsChmod)) {
                if (tryChangeFileAttr(CHMOD, {dest_orig, _mode}, errno)) {
                    warning(i18n("Could not change permissions for\n%1", dest_orig));
                }
            }
        }
    }

    // set modification time
    const QString mtimeStr = metaData(QStringLiteral("modified"));
    if (!mtimeStr.isEmpty()) {
        QDateTime dt = QDateTime::fromString(mtimeStr, Qt::ISODate);
        if (dt.isValid()) {
            QT_STATBUF dest_statbuf;
            if (QT_STAT(QFile::encodeName(dest_orig).constData(), &dest_statbuf) == 0) {
#ifndef Q_OS_WIN
                struct timeval utbuf[2];
                // access time
                utbuf[0].tv_sec = dest_statbuf.st_atime; // access time, unchanged  ## TODO preserve msec
                utbuf[0].tv_usec = 0;
                // modification time
                utbuf[1].tv_sec = dt.toSecsSinceEpoch();
                utbuf[1].tv_usec = dt.time().msec() * 1000;
                utimes(QFile::encodeName(dest_orig).constData(), utbuf);
#else
                struct utimbuf utbuf;
                utbuf.actime = dest_statbuf.st_atime;
                utbuf.modtime = dt.toSecsSinceEpoch();
                if (utime(QFile::encodeName(dest_orig).constData(), &utbuf) != 0) {
                    tryChangeFileAttr(UTIME, {dest_orig, qint64(utbuf.actime), qint64(utbuf.modtime)}, errno);
                }
#endif
            }
        }
    }

    // We have done our job => finish
    finished();
}

void FileProtocol::special(const QByteArray &data)
{
    int tmp;
    QDataStream stream(data);

    stream >> tmp;
    switch (tmp) {
    case 1: {
        QString fstype;
        QString dev;
        QString point;
        qint8 iRo;

        stream >> iRo >> fstype >> dev >> point;

        bool ro = (iRo != 0);

        // qDebug() << "MOUNTING fstype=" << fstype << " dev=" << dev << " point=" << point << " ro=" << ro;
        mount(ro, fstype.toLatin1().constData(), dev, point);
        break;
    }
    case 2: {
        QString point;
        stream >> point;
        unmount(point);
        break;
    }
    default:
        break;
    }
}

static QStringList fallbackSystemPath()
{
    return QStringList{
        QStringLiteral("/sbin"),
        QStringLiteral("/bin"),
    };
}

void FileProtocol::mount(bool _ro, const char *_fstype, const QString &_dev, const QString &_point)
{
    // qDebug() << "fstype=" << _fstype;

#ifdef _WIN32_WCE
    error(KIO::ERR_CANNOT_MOUNT, i18n("mounting is not supported by Windows CE."));
    return;
#endif

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);
    tmpFile.open();
    QByteArray tmpFileName = QFile::encodeName(tmpFile.fileName());
    QByteArray dev;
    if (_dev.startsWith(QLatin1String("LABEL="))) { // turn LABEL=foo into -L foo (#71430)
        QString labelName = _dev.mid(6);
        dev = "-L " + QFile::encodeName(KShell::quoteArg(labelName)); // is it correct to assume same encoding as filesystem?
    } else if (_dev.startsWith(QLatin1String("UUID="))) { // and UUID=bar into -U bar
        QString uuidName = _dev.mid(5);
        dev = "-U " + QFile::encodeName(KShell::quoteArg(uuidName));
    } else {
        dev = QFile::encodeName(KShell::quoteArg(_dev)); // get those ready to be given to a shell
    }

    QByteArray point = QFile::encodeName(KShell::quoteArg(_point));
    bool fstype_empty = !_fstype || !*_fstype;
    QByteArray fstype = KShell::quoteArg(QString::fromLatin1(_fstype)).toLatin1(); // good guess
    QByteArray readonly = _ro ? "-r" : "";
    QByteArray mountProg = QStandardPaths::findExecutable(QStringLiteral("mount")).toLocal8Bit();
    if (mountProg.isEmpty()) {
        mountProg = QStandardPaths::findExecutable(QStringLiteral("mount"), fallbackSystemPath()).toLocal8Bit();
    }
    if (mountProg.isEmpty()) {
        error(KIO::ERR_CANNOT_MOUNT, i18n("Could not find program \"mount\""));
        return;
    }

    // Two steps, in case mount doesn't like it when we pass all options
    for (int step = 0; step <= 1; step++) {
        QByteArray buffer = mountProg + ' ';
        // Mount using device only if no fstype nor mountpoint (KDE-1.x like)
        if (!dev.isEmpty() && _point.isEmpty() && fstype_empty) {
            buffer += dev;
        } else if (!_point.isEmpty() && dev.isEmpty() && fstype_empty) {
            // Mount using the mountpoint, if no fstype nor device (impossible in first step)
            buffer += point;
        } else if (!_point.isEmpty() && !dev.isEmpty() && fstype_empty) { // mount giving device + mountpoint but no fstype
            buffer += readonly + ' ' + dev + ' ' + point;
        } else { // mount giving device + mountpoint + fstype
            buffer += readonly + " -t " + fstype + ' ' + dev + ' ' + point;
        }
        if (fstype == "ext2" || fstype == "ext3" || fstype == "ext4") {
            buffer += " -o errors=remount-ro";
        }

        buffer += " 2>" + tmpFileName;
        // qDebug() << buffer;

        int mount_ret = system(buffer.constData());

        QString err = readLogFile(tmpFileName);
        if (err.isEmpty() && mount_ret == 0) {
            finished();
            return;
        } else {
            // Didn't work - or maybe we just got a warning
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice(_dev);
            // Is the device mounted ?
            if (mp && mount_ret == 0) {
                // qDebug() << "mount got a warning:" << err;
                warning(err);
                finished();
                return;
            } else {
                if ((step == 0) && !_point.isEmpty()) {
                    // qDebug() << err;
                    // qDebug() << "Mounting with those options didn't work, trying with only mountpoint";
                    fstype = "";
                    fstype_empty = true;
                    dev = "";
                    // The reason for trying with only mountpoint (instead of
                    // only device) is that some people (hi Malte!) have the
                    // same device associated with two mountpoints
                    // for different fstypes, like /dev/fd0 /mnt/e2floppy and
                    // /dev/fd0 /mnt/dosfloppy.
                    // If the user has the same mountpoint associated with two
                    // different devices, well they shouldn't specify the
                    // mountpoint but just the device.
                } else {
                    error(KIO::ERR_CANNOT_MOUNT, err);
                    return;
                }
            }
        }
    }
}

void FileProtocol::unmount(const QString &_point)
{
#ifdef _WIN32_WCE
    error(KIO::ERR_CANNOT_MOUNT, i18n("unmounting is not supported by Windows CE."));
    return;
#endif

    QByteArray buffer;

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(false);
    tmpFile.open();

    QByteArray umountProg = QStandardPaths::findExecutable(QStringLiteral("umount")).toLocal8Bit();
    if (umountProg.isEmpty()) {
        umountProg = QStandardPaths::findExecutable(QStringLiteral("umount"), fallbackSystemPath()).toLocal8Bit();
    }
    if (umountProg.isEmpty()) {
        error(KIO::ERR_CANNOT_UNMOUNT, i18n("Could not find program \"umount\""));
        return;
    }

    QByteArray tmpFileName = QFile::encodeName(tmpFile.fileName());

    buffer = umountProg + ' ' + QFile::encodeName(KShell::quoteArg(_point)) + " 2>" + tmpFileName;
    system(buffer.constData());

    QString err = readLogFile(tmpFileName);
    if (err.isEmpty()) {
        finished();
    } else {
        error(KIO::ERR_CANNOT_UNMOUNT, err);
    }
}

/*************************************
 *
 * Utilities
 *
 *************************************/

static QString readLogFile(const QByteArray &_filename)
{
    QString result;
    QFile file(QFile::decodeName(_filename));
    if (file.open(QIODevice::ReadOnly)) {
        result = QString::fromLocal8Bit(file.readAll());
    }
    (void)file.remove();
    return result;
}

// We could port this to KTempDir::removeDir but then we wouldn't be able to tell the user
// where exactly the deletion failed, in case of errors.
bool FileProtocol::deleteRecursive(const QString &path)
{
    // qDebug() << path;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDirIterator::Subdirectories);
    QStringList dirsToDelete;
    while (it.hasNext()) {
        const QString itemPath = it.next();
        // qDebug() << "itemPath=" << itemPath;
        const QFileInfo info = it.fileInfo();
        if (info.isDir() && !info.isSymLink()) {
            dirsToDelete.prepend(itemPath);
        } else {
            // qDebug() << "QFile::remove" << itemPath;
            if (!QFile::remove(itemPath)) {
                if (auto err = execWithElevatedPrivilege(DEL, {itemPath}, errno)) {
                    if (!err.wasCanceled()) {
                        error(KIO::ERR_CANNOT_DELETE, itemPath);
                    }
                    return false;
                }
            }
        }
    }
    QDir dir;
    for (const QString &itemPath : std::as_const(dirsToDelete)) {
        // qDebug() << "QDir::rmdir" << itemPath;
        if (!dir.rmdir(itemPath)) {
            if (auto err = execWithElevatedPrivilege(RMDIR, {itemPath}, errno)) {
                if (!err.wasCanceled()) {
                    error(KIO::ERR_CANNOT_DELETE, itemPath);
                }
                return false;
            }
        }
    }
    return true;
}

void FileProtocol::fileSystemFreeSpace(const QUrl &url)
{
    if (url.isLocalFile()) {
        QStorageInfo storageInfo(url.toLocalFile());
        if (storageInfo.isValid() && storageInfo.isReady()) {
            setMetaData(QStringLiteral("total"), QString::number(storageInfo.bytesTotal()));
            setMetaData(QStringLiteral("available"), QString::number(storageInfo.bytesAvailable()));

            finished();
        } else {
            error(KIO::ERR_CANNOT_STAT, url.url());
        }
    } else {
        error(KIO::ERR_UNSUPPORTED_PROTOCOL, url.url());
    }
}

void FileProtocol::virtual_hook(int id, void *data)
{
    switch (id) {
    case SlaveBase::GetFileSystemFreeSpace: {
        QUrl *url = static_cast<QUrl *>(data);
        fileSystemFreeSpace(*url);
        break;
    }
    case SlaveBase::Truncate: {
        auto length = static_cast<KIO::filesize_t *>(data);
        truncate(*length);
        break;
    }
    default: {
        SlaveBase::virtual_hook(id, data);
        break;
    }
    }
}

// needed for JSON file embedding
#include "file.moc"
