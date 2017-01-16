/* This file is part of the KDE project
   Copyright (C) 2004 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#define QT_NO_CAST_FROM_ASCII

#include "trashimpl.h"
#include "discspaceutil.h"
#include "trashsizecache.h"
#include "kiotrashdebug.h"

#include <KLocalizedString>
#include <kio/job.h>
#include <kio/chmodjob.h>
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <qdebug.h>
#include <QUrl>
#include <kdirnotify.h>
#include <KSharedConfig>
#include <kfileitem.h>
#include <kconfiggroup.h>
#include <kmountpoint.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QDir>
#include <kjobuidelegate.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <errno.h>

#include <solid/device.h>
#include <solid/block.h>
#include <solid/storageaccess.h>
#include <solid/networkshare.h>
#include <QStandardPaths>
#include <QLockFile>

TrashImpl::TrashImpl() :
    QObject(),
    m_lastErrorCode(0),
    m_initStatus(InitToBeDone),
    m_homeDevice(0),
    m_trashDirectoriesScanned(false),
    // not using kio_trashrc since KIO uses that one already for kio_trash
    // so better have a separate one, for faster parsing by e.g. kmimetype.cpp
    m_config(QStringLiteral("trashrc"), KConfig::SimpleConfig)
{
    QT_STATBUF buff;
    if (QT_LSTAT(QFile::encodeName(QDir::homePath()), &buff) == 0) {
        m_homeDevice = buff.st_dev;
    } else {
        qCWarning(KIO_TRASH) << "Should never happen: couldn't stat $HOME" << strerror(errno);
    }
}

/**
 * Test if a directory exists, create otherwise
 * @param _name full path of the directory
 * @return errorcode, or 0 if the dir was created or existed already
 * Warning, don't use return value like a bool
 */
int TrashImpl::testDir(const QString &_name) const
{
    DIR *dp = opendir(QFile::encodeName(_name));
    if (!dp) {
        QString name = _name;
        if (name.endsWith(QLatin1Char('/'))) {
            name.truncate(name.length() - 1);
        }

        bool ok = QDir().mkdir(name);
        if (!ok && QFile::exists(name)) {
#if 0 // this would require to use SlaveBase's method to ask the question
            //int ret = KMessageBox::warningYesNo( 0, i18n("%1 is a file, but KDE needs it to be a directory. Move it to %2.orig and create directory?").arg(name).arg(name) );
            //if ( ret == KMessageBox::Yes ) {
#endif
            QString new_name = name;
            name.append(QStringLiteral(".orig"));
            if (QFile::rename(name, new_name)) {
                ok = QDir().mkdir(name);
            } else { // foo.orig existed already. How likely is that?
                ok = false;
            }
            if (!ok) {
                return KIO::ERR_DIR_ALREADY_EXIST;
            }
#if 0
            //} else {
            //    return 0;
            //}
#endif
        }
        if (!ok) {
            //KMessageBox::sorry( 0, i18n( "Could not create directory %1. Check for permissions." ).arg( name ) );
            qCWarning(KIO_TRASH) << "could not create" << name;
            return KIO::ERR_COULD_NOT_MKDIR;
        } else {
            //qCDebug(KIO_TRASH) << name << "created.";
        }
    } else { // exists already
        closedir(dp);
    }
    return 0; // success
}

void TrashImpl::deleteEmptyTrashInfrastructure()
{
#ifdef Q_OS_OSX
    // For each known trash directory...
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    TrashDirMap::const_iterator it = m_trashDirectories.constBegin();
    for (; it != m_trashDirectories.constEnd() ; ++it) {
        const QString trashPath = it.value();
        QString infoPath = trashPath + QLatin1String("/info");

        //qCDebug(KIO_TRASH) << "empty Trash" << trashPath << "; removing infrastructure";
        synchronousDel(infoPath, false, true);
        synchronousDel(trashPath + QLatin1String("/files"), false, true);
        if (trashPath.endsWith(QLatin1String("/KDE.trash"))) {
            synchronousDel(trashPath, false, true);
        }
    }
#endif
}

bool TrashImpl::createTrashInfrastructure(int trashId, const QString &path)
{
    int err;
    QString trashDir = path.isEmpty() ? trashDirectoryPath(trashId) : path;
    if ((err = testDir(trashDir))) {
        error(err, trashDir);
        return false;
    }
    if ((err = testDir(trashDir + QLatin1String("/info")))) {
        error(err, trashDir + QLatin1String("/info"));
        return false;
    }
    if ((err = testDir(trashDir + QLatin1String("/files")))) {
        error(err, trashDir + QLatin1String("/files"));
        return false;
    }
    return true;
}

bool TrashImpl::init()
{
    if (m_initStatus == InitOK) {
        return true;
    }
    if (m_initStatus == InitError) {
        return false;
    }

    // Check the trash directory and its info and files subdirs
    // see also kdesktop/init.cc for first time initialization
    m_initStatus = InitError;
#ifndef Q_OS_OSX
    // $XDG_DATA_HOME/Trash, i.e. ~/.local/share/Trash by default.
    const QString xdgDataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/');
    if (!QDir().mkpath(xdgDataDir)) {
        qCWarning(KIO_TRASH) << "failed to create" << xdgDataDir;
        return false;
    }

    const QString trashDir = xdgDataDir + QLatin1String("Trash");
    if (!createTrashInfrastructure(0, trashDir)) {
        return false;
    }
#else
    // we DO NOT create ~/.Trash on OS X, that's the operating system's privilege
    QString trashDir = QDir::homePath() + QLatin1String("/.Trash");
    if (!QFileInfo(trashDir).isDir()) {
        error(KIO::ERR_DOES_NOT_EXIST, trashDir);
        return false;
    }
    trashDir += QLatin1String("/KDE.trash");
    // we don't have to call createTrashInfrastructure() here because it'll be called when needed.
#endif
    m_trashDirectories.insert(0, trashDir);
    m_initStatus = InitOK;
    //qCDebug(KIO_TRASH) << "initialization OK, home trash dir:" << trashDir;
    return true;
}

void TrashImpl::migrateOldTrash()
{
    qCDebug(KIO_TRASH);

    KConfigGroup g(KSharedConfig::openConfig(), "Paths");
    const QString oldTrashDir = g.readPathEntry("Trash", QString());

    if (oldTrashDir.isEmpty()) {
        return;
    }

    const QStringList entries = listDir(oldTrashDir);
    bool allOK = true;
    for (QStringList::const_iterator entryIt = entries.begin(), entryEnd = entries.end();
            entryIt != entryEnd; ++entryIt) {
        QString srcPath = *entryIt;
        if (srcPath == QLatin1String(".") || srcPath == QLatin1String("..") || srcPath == QLatin1String(".directory")) {
            continue;
        }
        srcPath.prepend(oldTrashDir);   // make absolute
        int trashId;
        QString fileId;
        if (!createInfo(srcPath, trashId, fileId)) {
            qCWarning(KIO_TRASH) << "Trash migration: failed to create info for" << srcPath;
            allOK = false;
        } else {
            bool ok = moveToTrash(srcPath, trashId, fileId);
            if (!ok) {
                (void)deleteInfo(trashId, fileId);
                qCWarning(KIO_TRASH) << "Trash migration: failed to create info for" << srcPath;
                allOK = false;
            } else {
                qCDebug(KIO_TRASH) << "Trash migration: moved" << srcPath;
            }
        }
    }
    if (allOK) {
        // We need to remove the old one, otherwise the desktop will have two trashcans...
        qCDebug(KIO_TRASH) << "Trash migration: all OK, removing old trash directory";
        synchronousDel(oldTrashDir, false, true);
    }
}

bool TrashImpl::createInfo(const QString &origPath, int &trashId, QString &fileId)
{
    //qCDebug(KIO_TRASH) << origPath;
    // Check source
    const QByteArray origPath_c(QFile::encodeName(origPath));

    // off_t should be 64bit on Unix systems to have large file support
    // FIXME: on windows this gets disabled until trash gets integrated
    // BUG: 165449
#ifndef Q_OS_WIN
    Q_STATIC_ASSERT(sizeof(off_t) >= 8);
#endif

    QT_STATBUF buff_src;
    if (QT_LSTAT(origPath_c.data(), &buff_src) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, origPath);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, origPath);
        }
        return false;
    }

    // Choose destination trash
    trashId = findTrashDirectory(origPath);
    if (trashId < 0) {
        qCWarning(KIO_TRASH) << "OUCH - internal error, TrashImpl::findTrashDirectory returned" << trashId;
        return false; // ### error() needed?
    }
    //qCDebug(KIO_TRASH) << "trashing to" << trashId;

    // Grab original filename
    QUrl url = QUrl::fromLocalFile(origPath);
    url = url.adjusted(QUrl::StripTrailingSlash);
    const QString origFileName = url.fileName();

    // Make destination file in info/
#ifdef Q_OS_OSX
    createTrashInfrastructure(trashId);
#endif
    url.setPath(infoPath(trashId, origFileName));     // we first try with origFileName
    QUrl baseDirectory = QUrl::fromLocalFile(url.path());
    // Here we need to use O_EXCL to avoid race conditions with other kioslave processes
    int fd = 0;
    QString fileName;
    do {
        //qCDebug(KIO_TRASH) << "trying to create" << url.path();
        fd = ::open(QFile::encodeName(url.path()), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) {
                fileName = url.fileName();
                url = url.adjusted(QUrl::RemoveFilename);
                url.setPath(url.path() + KIO::suggestName(baseDirectory,  fileName));
                // and try again on the next iteration
            } else {
                error(KIO::ERR_COULD_NOT_WRITE, url.path());
                return false;
            }
        }
    } while (fd < 0);
    const QString infoPath = url.path();
    fileId = url.fileName();
    Q_ASSERT(fileId.endsWith(QLatin1String(".trashinfo")));
    fileId.truncate(fileId.length() - 10);   // remove .trashinfo from fileId

    FILE *file = ::fdopen(fd, "w");
    if (!file) {   // can't see how this would happen
        error(KIO::ERR_COULD_NOT_WRITE, infoPath);
        return false;
    }

    // Contents of the info file. We could use KSimpleConfig, but that would
    // mean closing and reopening fd, i.e. opening a race condition...
    QByteArray info = "[Trash Info]\n";
    info += "Path=";
    // Escape filenames according to the way they are encoded on the filesystem
    // All this to basically get back to the raw 8-bit representation of the filename...
    if (trashId == 0) { // home trash: absolute path
        info += QUrl::toPercentEncoding(origPath, "/");
    } else {
        info += QUrl::toPercentEncoding(makeRelativePath(topDirectoryPath(trashId), origPath), "/");
    }
    info += '\n';
    info += "DeletionDate=";
    info += QDateTime::currentDateTime().toString(Qt::ISODate).toLatin1();
    info += '\n';
    size_t sz = info.size();

    size_t written = ::fwrite(info.data(), 1, sz, file);
    if (written != sz) {
        ::fclose(file);
        QFile::remove(infoPath);
        error(KIO::ERR_DISK_FULL, infoPath);
        return false;
    }

    ::fclose(file);

    //qCDebug(KIO_TRASH) << "info file created in trashId=" << trashId << ":" << fileId;
    return true;
}

QString TrashImpl::makeRelativePath(const QString &topdir, const QString &path)
{
    QString realPath = QFileInfo(path).canonicalFilePath();
    if (realPath.isEmpty()) { // shouldn't happen
        realPath = path;
    }
    // topdir ends with '/'
#ifndef Q_OS_WIN
    if (realPath.startsWith(topdir)) {
#else
    if (realPath.startsWith(topdir, Qt::CaseInsensitive)) {
#endif
        const QString rel = realPath.mid(topdir.length());
        Q_ASSERT(rel[0] != QLatin1Char('/'));
        return rel;
    } else { // shouldn't happen...
        qCWarning(KIO_TRASH) << "Couldn't make relative path for" << realPath << "(" << path << "), with topdir=" << topdir;
        return realPath;
    }
}

void TrashImpl::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, SIGNAL(leaveModality()),
            &eventLoop, SLOT(quit()));
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

QString TrashImpl::infoPath(int trashId, const QString &fileId) const
{
    QString trashPath = trashDirectoryPath(trashId);
    trashPath += QLatin1String("/info/");
    trashPath += fileId;
    trashPath += QLatin1String(".trashinfo");
    return trashPath;
}

QString TrashImpl::filesPath(int trashId, const QString &fileId) const
{
    QString trashPath = trashDirectoryPath(trashId);
    trashPath += QLatin1String("/files/");
    trashPath += fileId;
    return trashPath;
}

bool TrashImpl::deleteInfo(int trashId, const QString &fileId)
{
#ifdef Q_OS_OSX
    createTrashInfrastructure(trashId);
#endif
    bool ok = QFile::remove(infoPath(trashId, fileId));
    if (ok) {
        fileRemoved();
    }
    return ok;
}

bool TrashImpl::moveToTrash(const QString &origPath, int trashId, const QString &fileId)
{
    //qCDebug(KIO_TRASH) << "Trashing" << origPath << trashId << fileId;
    if (!adaptTrashSize(origPath, trashId)) {
        return false;
    }

    const qulonglong pathSize = DiscSpaceUtil::sizeOfPath(origPath);

#ifdef Q_OS_OSX
    createTrashInfrastructure(trashId);
#endif
    const QString dest = filesPath(trashId, fileId);
    if (!move(origPath, dest)) {
        // Maybe the move failed due to no permissions to delete source.
        // In that case, delete dest to keep things consistent, since KIO doesn't do it.
        if (QFileInfo(dest).isFile()) {
            QFile::remove(dest);
        } else {
            synchronousDel(dest, false, true);
        }
        return false;
    }

    if (QFileInfo(dest).isDir()) {
        TrashSizeCache trashSize(trashDirectoryPath(trashId));
        trashSize.add(fileId, pathSize);
    }

    fileAdded();
    return true;
}

bool TrashImpl::moveFromTrash(const QString &dest, int trashId, const QString &fileId, const QString &relativePath)
{
    QString src = filesPath(trashId, fileId);
    if (!relativePath.isEmpty()) {
        src += QLatin1Char('/');
        src += relativePath;
    }
    if (!move(src, dest)) {
        return false;
    }

    TrashSizeCache trashSize(trashDirectoryPath(trashId));
    trashSize.remove(fileId);

    return true;
}

bool TrashImpl::move(const QString &src, const QString &dest)
{
    if (directRename(src, dest)) {
        // This notification is done by KIO::moveAs when using the code below
        // But if we do a direct rename we need to do the notification ourselves
        org::kde::KDirNotify::emitFilesAdded(QUrl::fromLocalFile(dest));
        return true;
    }
    if (m_lastErrorCode != KIO::ERR_UNSUPPORTED_ACTION) {
        return false;
    }

    QUrl urlSrc = QUrl::fromLocalFile(src);
    QUrl urlDest = QUrl::fromLocalFile(dest);

    //qCDebug(KIO_TRASH) << urlSrc << "->" << urlDest;
    KIO::CopyJob *job = KIO::moveAs(urlSrc, urlDest, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(jobFinished(KJob*)));
    enterLoop();

    return m_lastErrorCode == 0;
}

void TrashImpl::jobFinished(KJob *job)
{
    //qCDebug(KIO_TRASH) << "error=" << job->error() << job->errorText();
    error(job->error(), job->errorText());

    emit leaveModality();
}

bool TrashImpl::copyToTrash(const QString &origPath, int trashId, const QString &fileId)
{
    //qCDebug(KIO_TRASH);
    if (!adaptTrashSize(origPath, trashId)) {
        return false;
    }

    const qulonglong pathSize = DiscSpaceUtil::sizeOfPath(origPath);

#ifdef Q_OS_OSX
    createTrashInfrastructure(trashId);
#endif
    const QString dest = filesPath(trashId, fileId);
    if (!copy(origPath, dest)) {
        return false;
    }

    if (QFileInfo(dest).isDir()) {
        TrashSizeCache trashSize(trashDirectoryPath(trashId));
        trashSize.add(fileId, pathSize);
    }

    fileAdded();
    return true;
}

bool TrashImpl::copyFromTrash(const QString &dest, int trashId, const QString &fileId, const QString &relativePath)
{
    QString src = filesPath(trashId, fileId);
    if (!relativePath.isEmpty()) {
        src += QLatin1Char('/');
        src += relativePath;
    }
    return copy(src, dest);
}

bool TrashImpl::copy(const QString &src, const QString &dest)
{
    // kio_file's copy() method is quite complex (in order to be fast), let's just call it...
    m_lastErrorCode = 0;
    QUrl urlSrc = QUrl::fromLocalFile(src);
    QUrl urlDest = QUrl::fromLocalFile(dest);
    //qCDebug(KIO_TRASH) << "copying" << src << "to" << dest;
    KIO::CopyJob *job = KIO::copyAs(urlSrc, urlDest, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(jobFinished(KJob*)));
    enterLoop();

    return m_lastErrorCode == 0;
}

bool TrashImpl::directRename(const QString &src, const QString &dest)
{
    //qCDebug(KIO_TRASH) << src << "->" << dest;
    // Do not use QFile::rename here, we need to be able to move broken symlinks too
    // (and we need to make sure errno is set)
    if (::rename(QFile::encodeName(src), QFile::encodeName(dest)) != 0) {
        if (errno == EXDEV) {
            error(KIO::ERR_UNSUPPORTED_ACTION, QStringLiteral("rename"));
        } else {
            if ((errno == EACCES) || (errno == EPERM)) {
                error(KIO::ERR_ACCESS_DENIED, dest);
            } else if (errno == EROFS) { // The file is on a read-only filesystem
                error(KIO::ERR_CANNOT_DELETE, src);
            } else {
                error(KIO::ERR_CANNOT_RENAME, src);
            }
        }
        return false;
    }
    return true;
}

#if 0
bool TrashImplKDE_mkdir(int trashId, const QString &fileId, int permissions)
{
    const QString path = filesPath(trashId, fileId);
    if (KDE_mkdir(QFile::encodeName(path), permissions) != 0) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, path);
            return false;
        } else if (errno == ENOSPC) {
            error(KIO::ERR_DISK_FULL, path);
            return false;
        } else {
            error(KIO::ERR_COULD_NOT_MKDIR, path);
            return false;
        }
    } else {
        if (permissions != -1) {
            ::chmod(QFile::encodeName(path), permissions);
        }
    }
    return true;
}
#endif

bool TrashImpl::del(int trashId, const QString &fileId)
{
#ifdef Q_OS_OSX
    createTrashInfrastructure(trashId);
#endif

    QString info = infoPath(trashId, fileId);
    QString file = filesPath(trashId, fileId);

    QByteArray info_c = QFile::encodeName(info);

    QT_STATBUF buff;
    if (QT_LSTAT(info_c.data(), &buff) == -1) {
        if (errno == EACCES) {
            error(KIO::ERR_ACCESS_DENIED, file);
        } else {
            error(KIO::ERR_DOES_NOT_EXIST, file);
        }
        return false;
    }

    const bool isDir = QFileInfo(file).isDir();
    if (!synchronousDel(file, true, isDir)) {
        return false;
    }

    if (isDir) {
        TrashSizeCache trashSize(trashDirectoryPath(trashId));
        trashSize.remove(fileId);
    }

    QFile::remove(info);
    fileRemoved();
    return true;
}

bool TrashImpl::synchronousDel(const QString &path, bool setLastErrorCode, bool isDir)
{
    const int oldErrorCode = m_lastErrorCode;
    const QString oldErrorMsg = m_lastErrorMessage;
    QUrl url = QUrl::fromLocalFile(path);
    // First ensure that all dirs have u+w permissions,
    // otherwise we won't be able to delete files in them (#130780).
    if (isDir) {
//         qCDebug(KIO_TRASH) << "chmod'ing" << url;
        KFileItem fileItem(url, QStringLiteral("inode/directory"), KFileItem::Unknown);
        KFileItemList fileItemList;
        fileItemList.append(fileItem);
        KIO::ChmodJob *chmodJob = KIO::chmod(fileItemList, 0200, 0200, QString(), QString(), true /*recursive*/, KIO::HideProgressInfo);
        connect(chmodJob, SIGNAL(result(KJob*)),
                this, SLOT(jobFinished(KJob*)));
        enterLoop();
    }

    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(jobFinished(KJob*)));
    enterLoop();
    bool ok = m_lastErrorCode == 0;
    if (!setLastErrorCode) {
        m_lastErrorCode = oldErrorCode;
        m_lastErrorMessage = oldErrorMsg;
    }
    return ok;
}

bool TrashImpl::emptyTrash()
{
    //qCDebug(KIO_TRASH);
    // The naive implementation "delete info and files in every trash directory"
    // breaks when deleted directories contain files owned by other users.
    // We need to ensure that the .trashinfo file is only removed when the
    // corresponding files could indeed be removed (#116371)

    // On the other hand, we certainly want to remove any file that has no associated
    // .trashinfo file for some reason (#167051)

    QSet<QString> unremoveableFiles;

    int myErrorCode = 0;
    QString myErrorMsg;
    const TrashedFileInfoList fileInfoList = list();

    TrashedFileInfoList::const_iterator it = fileInfoList.begin();
    const TrashedFileInfoList::const_iterator end = fileInfoList.end();
    for (; it != end; ++it) {
        const TrashedFileInfo &info = *it;
        const QString filesPath = info.physicalPath;
        if (synchronousDel(filesPath, true, true) || m_lastErrorCode == KIO::ERR_DOES_NOT_EXIST) {
            QFile::remove(infoPath(info.trashId, info.fileId));
        } else {
            // error code is set by synchronousDel, let's remember it
            // (so that successfully removing another file doesn't erase the error)
            myErrorCode = m_lastErrorCode;
            myErrorMsg = m_lastErrorMessage;
            // and remember not to remove this file
            unremoveableFiles.insert(filesPath);
            qCDebug(KIO_TRASH) << "Unremoveable:" << filesPath;
        }

        TrashSizeCache trashSize(trashDirectoryPath(info.trashId));
        trashSize.clear();
    }

    // Now do the orphaned-files cleanup
    TrashDirMap::const_iterator trit = m_trashDirectories.constBegin();
    for (; trit != m_trashDirectories.constEnd(); ++trit) {
        //const int trashId = trit.key();
        QString filesDir = trit.value();
        filesDir += QLatin1String("/files");
        Q_FOREACH (const QString &fileName, listDir(filesDir)) {
            if (fileName == QLatin1String(".") || fileName == QLatin1String("..")) {
                continue;
            }
            const QString filePath = filesDir + QLatin1Char('/') + fileName;
            if (!unremoveableFiles.contains(filePath)) {
                qCWarning(KIO_TRASH) << "Removing orphaned file" << filePath;
                QFile::remove(filePath);
            }
        }
    }

    m_lastErrorCode = myErrorCode;
    m_lastErrorMessage = myErrorMsg;

    fileRemoved();

    return m_lastErrorCode == 0;
}

TrashImpl::TrashedFileInfoList TrashImpl::list()
{
    // Here we scan for trash directories unconditionally. This allows
    // noticing plugged-in [e.g. removeable] devices, or new mounts etc.
    scanTrashDirectories();

    TrashedFileInfoList lst;
    // For each known trash directory...
    TrashDirMap::const_iterator it = m_trashDirectories.constBegin();
    for (; it != m_trashDirectories.constEnd(); ++it) {
        const int trashId = it.key();
        QString infoPath = it.value();
        infoPath += QLatin1String("/info");
        // Code taken from kio_file
        const QStringList entryNames = listDir(infoPath);
        //char path_buffer[PATH_MAX];
        //getcwd(path_buffer, PATH_MAX - 1);
        //if ( chdir( infoPathEnc ) )
        //    continue;
        for (QStringList::const_iterator entryIt = entryNames.constBegin(), entryEnd = entryNames.constEnd();
                entryIt != entryEnd; ++entryIt) {
            QString fileName = *entryIt;
            if (fileName == QLatin1String(".") || fileName == QLatin1String("..")) {
                continue;
            }
            if (!fileName.endsWith(QLatin1String(".trashinfo"))) {
                qCWarning(KIO_TRASH) << "Invalid info file found in" << infoPath << ":" << fileName;
                continue;
            }
            fileName.truncate(fileName.length() - 10);

            TrashedFileInfo info;
            if (infoForFile(trashId, fileName, info)) {
                lst << info;
            }
        }
    }
    return lst;
}

// Returns the entries in a given directory - including "." and ".."
QStringList TrashImpl::listDir(const QString &physicalPath)
{
    QDir dir(physicalPath);
    return dir.entryList(QDir::Dirs | QDir::Files | QDir::Hidden);
}

bool TrashImpl::infoForFile(int trashId, const QString &fileId, TrashedFileInfo &info)
{
    //qCDebug(KIO_TRASH) << trashId << fileId;
    info.trashId = trashId; // easy :)
    info.fileId = fileId; // equally easy
    info.physicalPath = filesPath(trashId, fileId);
    return readInfoFile(infoPath(trashId, fileId), info, trashId);
}

bool TrashImpl::trashSpaceInfo(const QString &path, TrashSpaceInfo &info)
{
    const int trashId = findTrashDirectory(path);
    if (trashId < 0) {
        qCWarning(KIO_TRASH) << "No trash directory found! TrashImpl::findTrashDirectory returned" << trashId;
        return false;
    }

    const KConfig config(QStringLiteral("ktrashrc"));

    const QString trashPath = trashDirectoryPath(trashId);
    const auto group = config.group(trashPath);

    const bool useSizeLimit = group.readEntry("UseSizeLimit", true);
    const double percent = group.readEntry("Percent", 10.0);

    DiscSpaceUtil util(trashPath + QLatin1String("/files/"));
    qulonglong total = util.size();
    if (useSizeLimit) {
        total *= percent / 100.0;
    }

    TrashSizeCache trashSize(trashPath);
    const qulonglong used = trashSize.calculateSize();

    info.totalSize = total;
    info.availableSize = total - used;

    return true;
}

bool TrashImpl::readInfoFile(const QString &infoPath, TrashedFileInfo &info, int trashId)
{
    KConfig cfg(infoPath, KConfig::SimpleConfig);
    if (!cfg.hasGroup("Trash Info")) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, infoPath);
        return false;
    }
    const KConfigGroup group = cfg.group("Trash Info");
    info.origPath = QUrl::fromPercentEncoding(group.readEntry("Path").toLatin1());
    if (info.origPath.isEmpty()) {
        return false;    // path is mandatory...
    }
    if (trashId == 0) {
        Q_ASSERT(info.origPath[0] == QLatin1Char('/'));
    } else {
        const QString topdir = topDirectoryPath(trashId);   // includes trailing slash
        info.origPath.prepend(topdir);
    }
    const QString line = group.readEntry("DeletionDate");
    if (!line.isEmpty()) {
        info.deletionDate = QDateTime::fromString(line, Qt::ISODate);
    }
    return true;
}

QString TrashImpl::physicalPath(int trashId, const QString &fileId, const QString &relativePath)
{
    QString filePath = filesPath(trashId, fileId);
    if (!relativePath.isEmpty()) {
        filePath += QLatin1Char('/');
        filePath += relativePath;
    }
    return filePath;
}

void TrashImpl::error(int e, const QString &s)
{
    if (e) {
        qCDebug(KIO_TRASH) << e << s;
    }
    m_lastErrorCode = e;
    m_lastErrorMessage = s;
}

bool TrashImpl::isEmpty() const
{
    // For each known trash directory...
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    TrashDirMap::const_iterator it = m_trashDirectories.constBegin();
    for (; it != m_trashDirectories.constEnd(); ++it) {
        QString infoPath = it.value();
        infoPath += QLatin1String("/info");

        DIR *dp = opendir(QFile::encodeName(infoPath));
        if (dp) {
            struct dirent *ep;
            ep = readdir(dp);
            ep = readdir(dp);   // ignore '.' and '..' dirent
            ep = readdir(dp);   // look for third file
            closedir(dp);
            if (ep != nullptr) {
                //qCDebug(KIO_TRASH) << ep->d_name << "in" << infoPath << "-> not empty";
                return false; // not empty
            }
        }
    }
    return true;
}

void TrashImpl::fileAdded()
{
    m_config.reparseConfiguration();
    KConfigGroup group = m_config.group("Status");
    if (group.readEntry("Empty", true) == true) {
        group.writeEntry("Empty", false);
        m_config.sync();
    }
    // The apps showing the trash (e.g. kdesktop) will be notified
    // of this change when KDirNotify::FilesAdded("trash:/") is emitted,
    // which will be done by the job soon after this.
}

void TrashImpl::fileRemoved()
{
    if (isEmpty()) {
        deleteEmptyTrashInfrastructure();
        KConfigGroup group = m_config.group("Status");
        group.writeEntry("Empty", true);
        m_config.sync();
    }
    // The apps showing the trash (e.g. kdesktop) will be notified
    // of this change when KDirNotify::FilesRemoved(...) is emitted,
    // which will be done by the job soon after this.
}

#ifdef Q_OS_OSX
#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>
#include <sys/param.h>
#include <sys/mount.h>

int TrashImpl::idForMountPoint(const QString &mountPoint) const
{
    DADiskRef disk;
    CFDictionaryRef descDict;
    DASessionRef session = DASessionCreate(NULL);
    int devId = -1;
    if (session) {
        QByteArray mp = QFile::encodeName(mountPoint);
        struct statfs statFS;
        statfs(mp.constData(), &statFS);
        disk = DADiskCreateFromBSDName(kCFAllocatorDefault, session, statFS.f_mntfromname);
        if (disk) {
            descDict = DADiskCopyDescription(disk);
            if (descDict) {
                CFNumberRef cfMajor = (CFNumberRef)CFDictionaryGetValue(descDict, kDADiskDescriptionMediaBSDMajorKey);
                CFNumberRef cfMinor = (CFNumberRef)CFDictionaryGetValue(descDict, kDADiskDescriptionMediaBSDMinorKey);
                int major, minor;
                if (CFNumberGetValue(cfMajor, kCFNumberIntType, &major) && CFNumberGetValue(cfMinor, kCFNumberIntType, &minor)) {
                    qCWarning(KIO_TRASH) << "major=" << major << " minor=" << minor;
                    devId = 1000 * major + minor;
                }
                CFRelease(cfMajor);
                CFRelease(cfMinor);
            }
            else {
                qCWarning(KIO_TRASH) << "couldn't get DADiskCopyDescription from" << disk;
            }
            CFRelease(disk);
        }
        else {
            qCWarning(KIO_TRASH) << "DADiskCreateFromBSDName failed on statfs from" << mp;
        }
        CFRelease(session);
    }
    else {
        qCWarning(KIO_TRASH) << "couldn't create DASession";
    }
    return devId;
}

#else

int TrashImpl::idForDevice(const Solid::Device &device) const
{
    const Solid::Block *block = device.as<Solid::Block>();
    if (block) {
        //qCDebug(KIO_TRASH) << "major=" << block->deviceMajor() << "minor=" << block->deviceMinor();
        return block->deviceMajor() * 1000 + block->deviceMinor();
    } else {
        const Solid::NetworkShare *netshare = device.as<Solid::NetworkShare>();

        if (netshare) {
            QString url = netshare->url().url();

            QLockFile configLock(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                                 QStringLiteral("/trashrc.nextid.lock"));

            if (!configLock.lock()) {
                return -1;
            }

            m_config.reparseConfiguration();
            KConfigGroup group = m_config.group("NetworkShares");
            int id = group.readEntry(url, -1);

            if (id == -1) {
                id = group.readEntry("NextID", 0);
                //qCDebug(KIO_TRASH) << "new share=" << url << " id=" << id;

                group.writeEntry(url, id);
                group.writeEntry("NextID", id + 1);
                group.sync();
            }

            return 6000000 + id;
        }

        // Not a block device nor a network share
        return -1;
    }
}

void TrashImpl::refreshDevices() const
{
    // this is needed because Solid's fstab backend uses QSocketNotifier
    // to get notifications about changes to mtab
    // otherwise we risk getting old device list
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
}
#endif

int TrashImpl::findTrashDirectory(const QString &origPath)
{
    //qCDebug(KIO_TRASH) << origPath;
    // First check if same device as $HOME, then we use the home trash right away.
    QT_STATBUF buff;
    if (QT_LSTAT(QFile::encodeName(origPath), &buff) == 0
            && buff.st_dev == m_homeDevice) {
        return 0;
    }

    KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(origPath);
    if (!mp) {
        //qCDebug(KIO_TRASH) << "KMountPoint found no mount point for" << origPath;
        return 0;
    }
    QString mountPoint = mp->mountPoint();
    const QString trashDir = trashForMountPoint(mountPoint, true);
    //qCDebug(KIO_TRASH) << "mountPoint=" << mountPoint << "trashDir=" << trashDir;
#ifndef Q_OS_OSX
    if (trashDir.isEmpty()) {
        return 0;    // no trash available on partition
    }
#endif
    int id = idForTrashDirectory(trashDir);
    if (id > -1) {
        //qCDebug(KIO_TRASH) << "known with id" << id;
        return id;
    }
    // new trash dir found, register it
    // but we need stability in the trash IDs, so that restoring or asking
    // for properties works even kio_trash gets killed because idle.
#if 0
    qCDebug(KIO_TRASH) << "found" << trashDir;
    m_trashDirectories.insert(++m_lastId, trashDir);
    if (!mountPoint.endsWith('/')) {
        mountPoint += '/';
    }
    m_topDirectories.insert(m_lastId, mountPoint);
    return m_lastId;
#endif

#ifdef Q_OS_OSX
    id = idForMountPoint(mountPoint);
#else
    refreshDevices();
    const QString query = QLatin1String("[StorageAccess.accessible == true AND StorageAccess.filePath == '") + mountPoint + QLatin1String("']");
    //qCDebug(KIO_TRASH) << "doing solid query:" << query;
    const QList<Solid::Device> lst = Solid::Device::listFromQuery(query);
    //qCDebug(KIO_TRASH) << "got" << lst.count() << "devices";
    if (lst.isEmpty()) { // not a device. Maybe some tmpfs mount for instance.
        return 0;    // use the home trash instead
    }
    // Pretend we got exactly one...
    const Solid::Device device = lst[0];

    // new trash dir found, register it
    id = idForDevice(device);
#endif
    if (id == -1) {
        return 0;
    }
    m_trashDirectories.insert(id, trashDir);
    //qCDebug(KIO_TRASH) << "found" << trashDir << "gave it id" << id;
    if (!mountPoint.endsWith(QLatin1Char('/'))) {
        mountPoint += QLatin1Char('/');
    }
    m_topDirectories.insert(id, mountPoint);

    return idForTrashDirectory(trashDir);
}

void TrashImpl::scanTrashDirectories() const
{
#ifndef Q_OS_OSX
    refreshDevices();
#endif

    const QList<Solid::Device> lst = Solid::Device::listFromQuery(QStringLiteral("StorageAccess.accessible == true"));
    for (QList<Solid::Device>::ConstIterator it = lst.begin(); it != lst.end(); ++it) {
        QString topdir = (*it).as<Solid::StorageAccess>()->filePath();
        QString trashDir = trashForMountPoint(topdir, false);
        if (!trashDir.isEmpty()) {
            // OK, trashDir is a valid trash directory. Ensure it's registered.
            int trashId = idForTrashDirectory(trashDir);
            if (trashId == -1) {
                // new trash dir found, register it
#ifdef Q_OS_OSX
                trashId = idForMountPoint(topdir);
#else
                trashId = idForDevice(*it);
#endif
                if (trashId == -1) {
                    continue;
                }
                m_trashDirectories.insert(trashId, trashDir);
                //qCDebug(KIO_TRASH) << "found" << trashDir << "gave it id" << trashId;
                if (!topdir.endsWith(QLatin1Char('/'))) {
                    topdir += QLatin1Char('/');
                }
                m_topDirectories.insert(trashId, topdir);
            }
        }
    }
    m_trashDirectoriesScanned = true;
}

TrashImpl::TrashDirMap TrashImpl::trashDirectories() const
{
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    return m_trashDirectories;
}

TrashImpl::TrashDirMap TrashImpl::topDirectories() const
{
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    return m_topDirectories;
}

QString TrashImpl::trashForMountPoint(const QString &topdir, bool createIfNeeded) const
{
    // (1) Administrator-created $topdir/.Trash directory

#ifndef Q_OS_OSX
    const QString rootTrashDir = topdir + QLatin1String("/.Trash");
#else
    const QString rootTrashDir = topdir + QLatin1String("/.Trashes");
#endif
    const QByteArray rootTrashDir_c = QFile::encodeName(rootTrashDir);
    // Can't use QFileInfo here since we need to test for the sticky bit
    uid_t uid = getuid();
    QT_STATBUF buff;
    const unsigned int requiredBits = S_ISVTX; // Sticky bit required
    if (QT_LSTAT(rootTrashDir_c, &buff) == 0) {
        if ((S_ISDIR(buff.st_mode))  // must be a dir
                && (!S_ISLNK(buff.st_mode)) // not a symlink
                && ((buff.st_mode & requiredBits) == requiredBits)
                && (::access(rootTrashDir_c, W_OK) == 0) // must be user-writable
           ) {
#ifndef Q_OS_OSX
            const QString trashDir = rootTrashDir + QLatin1Char('/') + QString::number(uid);
#else
            QString trashDir = rootTrashDir + QLatin1Char('/') + QString::number(uid);
#endif
            const QByteArray trashDir_c = QFile::encodeName(trashDir);
            if (QT_LSTAT(trashDir_c, &buff) == 0) {
                if ((buff.st_uid == uid)  // must be owned by user
                        && (S_ISDIR(buff.st_mode)) // must be a dir
                        && (!S_ISLNK(buff.st_mode)) // not a symlink
                        && (buff.st_mode & 0777) == 0700) {  // rwx for user
#ifdef Q_OS_OSX
                    trashDir += QStringLiteral("/KDE.trash");
#endif
                    return trashDir;
                }
                qCWarning(KIO_TRASH) << "Directory" << trashDir << "exists but didn't pass the security checks, can't use it";
            } else if (createIfNeeded && initTrashDirectory(trashDir_c)) {
                return trashDir;
            }
        } else {
            qCWarning(KIO_TRASH) << "Root trash dir" << rootTrashDir << "exists but didn't pass the security checks, can't use it";
        }
    }

#ifndef Q_OS_OSX
    // (2) $topdir/.Trash-$uid
    const QString trashDir = topdir + QLatin1String("/.Trash-") + QString::number(uid);
    const QByteArray trashDir_c = QFile::encodeName(trashDir);
    if (QT_LSTAT(trashDir_c, &buff) == 0) {
        if ((buff.st_uid == uid)  // must be owned by user
                && (S_ISDIR(buff.st_mode)) // must be a dir
                && (!S_ISLNK(buff.st_mode)) // not a symlink
                && ((buff.st_mode & 0777) == 0700)) {  // rwx for user, ------ for group and others

            if (checkTrashSubdirs(trashDir_c)) {
                return trashDir;
            }
        }
        qCWarning(KIO_TRASH) << "Directory" << trashDir << "exists but didn't pass the security checks, can't use it";
        // Exists, but not useable
        return QString();
    }
    if (createIfNeeded && initTrashDirectory(trashDir_c)) {
        return trashDir;
    }
#endif
    return QString();
}

int TrashImpl::idForTrashDirectory(const QString &trashDir) const
{
    // If this is too slow we can always use a reverse map...
    TrashDirMap::ConstIterator it = m_trashDirectories.constBegin();
    for (; it != m_trashDirectories.constEnd(); ++it) {
        if (it.value() == trashDir) {
            return it.key();
        }
    }
    return -1;
}

bool TrashImpl::initTrashDirectory(const QByteArray &trashDir_c) const
{
    //qCDebug(KIO_TRASH) << trashDir_c;
    if (mkdir(trashDir_c, 0700) != 0) {
        return false;
    }
    //qCDebug(KIO_TRASH);
    // This trash dir will be useable only if the directory is owned by user.
    // In theory this is the case, but not on e.g. USB keys...
    uid_t uid = getuid();
    QT_STATBUF buff;
    if (QT_LSTAT(trashDir_c, &buff) != 0) {
        return false;    // huh?
    }
    if ((buff.st_uid == uid)  // must be owned by user
            && ((buff.st_mode & 0777) == 0700)) {  // rwx for user, --- for group and others

        return checkTrashSubdirs(trashDir_c);

    } else {
        qCWarning(KIO_TRASH) << trashDir_c << "just created, by it doesn't have the right permissions, probably some strange unsupported filesystem";
        ::rmdir(trashDir_c);
        return false;
    }
    return true;
}

bool TrashImpl::checkTrashSubdirs(const QByteArray &trashDir_c) const
{
    // testDir currently works with a QString - ## optimize
    QString trashDir = QFile::decodeName(trashDir_c);
    const QString info = trashDir + QLatin1String("/info");
    if (testDir(info) != 0) {
        return false;
    }
    const QString files = trashDir + QLatin1String("/files");
    if (testDir(files) != 0) {
        return false;
    }
    return true;
}

QString TrashImpl::trashDirectoryPath(int trashId) const
{
    // Never scanned for trash dirs? (This can happen after killing kio_trash
    // and reusing a directory listing from the earlier instance.)
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    Q_ASSERT(m_trashDirectories.contains(trashId));
    return m_trashDirectories[trashId];
}

QString TrashImpl::topDirectoryPath(int trashId) const
{
    if (!m_trashDirectoriesScanned) {
        scanTrashDirectories();
    }
    assert(trashId != 0);
    Q_ASSERT(m_topDirectories.contains(trashId));
    return m_topDirectories[trashId];
}

// Helper method. Creates a URL with the format trash:/trashid-fileid or
// trash:/trashid-fileid/relativePath/To/File for a file inside a trashed directory.
QUrl TrashImpl::makeURL(int trashId, const QString &fileId, const QString &relativePath)
{
    QUrl url;
    url.setScheme(QStringLiteral("trash"));
    QString path = QStringLiteral("/");
    path += QString::number(trashId);
    path += QLatin1Char('-');
    path += fileId;
    if (!relativePath.isEmpty()) {
        path += QLatin1Char('/');
        path += relativePath;
    }
    url.setPath(path);
    return url;
}

// Helper method. Parses a trash URL with the URL scheme defined in makeURL.
// The trash:/ URL itself isn't parsed here, must be caught by the caller before hand.
bool TrashImpl::parseURL(const QUrl &url, int &trashId, QString &fileId, QString &relativePath)
{
    if (url.scheme() != QLatin1String("trash")) {
        return false;
    }
    const QString path = url.path();
    if (path.isEmpty()) {
        return false;
    }
    int start = 0;
    if (path[0] == QLatin1Char('/')) { // always true I hope
        start = 1;
    }
    int slashPos = path.indexOf(QLatin1Char('-'), 0); // don't match leading slash
    if (slashPos <= 0) {
        return false;
    }
    bool ok = false;
    trashId = path.midRef(start, slashPos - start).toInt(&ok);
    Q_ASSERT(ok);
    if (!ok) {
        return false;
    }
    start = slashPos + 1;
    slashPos = path.indexOf(QLatin1Char('/'), start);
    if (slashPos <= 0) {
        fileId = path.mid(start);
        relativePath.clear();
        return true;
    }
    fileId = path.mid(start, slashPos - start);
    relativePath = path.mid(slashPos + 1);
    return true;
}

bool TrashImpl::adaptTrashSize(const QString &origPath, int trashId)
{
    KConfig config(QStringLiteral("ktrashrc"));

    const QString trashPath = trashDirectoryPath(trashId);
    KConfigGroup group = config.group(trashPath);

    bool useTimeLimit = group.readEntry("UseTimeLimit", false);
    bool useSizeLimit = group.readEntry("UseSizeLimit", true);
    double percent = group.readEntry("Percent", 10.0);
    int actionType = group.readEntry("LimitReachedAction", 0);

    if (useTimeLimit) {   // delete all files in trash older than X days
        const int maxDays = group.readEntry("Days", 7);
        const QDateTime currentDate = QDateTime::currentDateTime();

        const TrashedFileInfoList trashedFiles = list();
        for (int i = 0; i < trashedFiles.count(); ++i) {
            struct TrashedFileInfo info = trashedFiles.at(i);
            if (info.trashId != trashId) {
                continue;
            }

            if (info.deletionDate.daysTo(currentDate) > maxDays) {
                del(info.trashId, info.fileId);
            }
        }
    }

    if (useSizeLimit) {   // check if size limit exceeded

        // calculate size of the files to be put into the trash
        qulonglong additionalSize = DiscSpaceUtil::sizeOfPath(origPath);

#ifdef Q_OS_OSX
        createTrashInfrastructure(trashId);
#endif
        TrashSizeCache trashSize(trashPath);
        DiscSpaceUtil util(trashPath + QLatin1String("/files/"));
        if (util.usage(trashSize.calculateSize() + additionalSize) >= percent) {
            // before we start to remove any files from the trash,
            // check whether the new file will fit into the trash
            // at all...
            qulonglong partitionSize = util.size();

            if ((((double)additionalSize / (double)partitionSize) * 100) >= percent) {
                m_lastErrorCode = KIO::ERR_SLAVE_DEFINED;
                m_lastErrorMessage = i18n("The file is too large to be trashed.");
                return false;
            }

            if (actionType == 0) {   // warn the user only
                m_lastErrorCode = KIO::ERR_SLAVE_DEFINED;
                m_lastErrorMessage = i18n("The trash has reached its maximum size!\nCleanup the trash manually.");
                return false;
            } else {
                // lets start removing some other files from the trash

                QDir dir(trashPath + QLatin1String("/files"));
                QFileInfoList infoList;
                if (actionType == 1) {  // delete oldest files first
                    infoList = dir.entryInfoList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Time | QDir::Reversed);
                } else if (actionType == 2) { // delete biggest files first
                    infoList = dir.entryInfoList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDir::Size);
                } else {
                    qWarning("Should never happen!");
                }

                bool deleteFurther = true;
                for (int i = 0; (i < infoList.count()) && deleteFurther; ++i) {
                    const QFileInfo info = infoList.at(i);

                    del(trashId, info.fileName());   // delete trashed file

                    TrashSizeCache trashSize(trashPath);
                    if (util.usage(trashSize.calculateSize() + additionalSize) < percent) {   // check whether we have enough space now
                        deleteFurther = false;
                    }
                }
            }
        }
    }

    return true;
}

#include "moc_trashimpl.cpp"
