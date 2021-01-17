/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio_trash.h"
#include "kiotrashdebug.h"
#include "../../pathhelpers_p.h"

#include <kio/job.h>
#include <kio/jobuidelegateextension.h>
#include <KLocalizedString>
#include <KDirNotify>

#include <QMimeDatabase>
#include <QMimeType>
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QEventLoop>

#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.trash" FILE "trash.json")
};

extern "C" {
    int Q_DECL_EXPORT kdemain(int argc, char **argv)
    {
        // necessary to use other kio slaves
        QCoreApplication app(argc, argv);

        KIO::setDefaultJobUiDelegateExtension(nullptr);
        // start the slave
        TrashProtocol slave(argv[1], argv[2], argv[3]);
        slave.dispatchLoop();
        return 0;
    }
}

static bool isTopLevelEntry(const QUrl &url)
{
    const QString dir = url.adjusted(QUrl::RemoveFilename).path();
    return dir.length() <= 1;
}

#define INIT_IMPL \
    if ( !impl.init() ) { \
        error( impl.lastErrorCode(), impl.lastErrorMessage() ); \
        return; \
    }

TrashProtocol::TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : SlaveBase(protocol, pool, app)
{
    struct passwd *user = getpwuid(getuid());
    if (user) {
        m_userName = QString::fromLatin1(user->pw_name);
    }
    struct group *grp = getgrgid(getgid());
    if (grp) {
        m_groupName = QString::fromLatin1(grp->gr_name);
    }
}

TrashProtocol::~TrashProtocol()
{
}

void TrashProtocol::enterLoop()
{
    QEventLoop eventLoop;
    connect(this, &TrashProtocol::leaveModality,
            &eventLoop, &QEventLoop::quit);
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void TrashProtocol::restore(const QUrl &trashURL)
{
    int trashId;
    QString fileId, relativePath;
    bool ok = TrashImpl::parseURL(trashURL, trashId, fileId, relativePath);
    if (!ok) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", trashURL.toString()));
        return;
    }
    TrashedFileInfo info;
    ok = impl.infoForFile(trashId, fileId, info);
    if (!ok) {
        error(impl.lastErrorCode(), impl.lastErrorMessage());
        return;
    }
    QUrl dest = QUrl::fromLocalFile(info.origPath);
    if (!relativePath.isEmpty()) {
        dest.setPath(concatPaths(dest.path(), relativePath));
    }

    // Check that the destination directory exists, to improve the error code in case it doesn't.
    const QString destDir = dest.adjusted(QUrl::RemoveFilename).path();
    QT_STATBUF buff;

    if (QT_LSTAT(QFile::encodeName(destDir).constData(), &buff) == -1) {
        error(KIO::ERR_SLAVE_DEFINED,
              i18n("The directory %1 does not exist anymore, so it is not possible to restore this item to its original location. "
                   "You can either recreate that directory and use the restore operation again, or drag the item anywhere else to restore it.", destDir));
        return;
    }

    copyOrMoveFromTrash(trashURL, dest, false /*overwrite*/, Move);
}

void TrashProtocol::rename(const QUrl &oldURL, const QUrl &newURL, KIO::JobFlags flags)
{
    INIT_IMPL;

    qCDebug(KIO_TRASH) << "TrashProtocol::rename(): old=" << oldURL << " new=" << newURL << " overwrite=" << (flags & KIO::Overwrite);

    if (oldURL.scheme() == QLatin1String("trash") && newURL.scheme() == QLatin1String("trash")) {
        if (!isTopLevelEntry(oldURL) || !isTopLevelEntry(newURL)) {
            error(KIO::ERR_CANNOT_RENAME, oldURL.toString());
            return;
        }
        int oldTrashId;
        QString oldFileId, oldRelativePath;
        bool oldOk = TrashImpl::parseURL(oldURL, oldTrashId, oldFileId, oldRelativePath);
        if (!oldOk) {
            error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", oldURL.toString()));
            return;
        }
        if (!oldRelativePath.isEmpty()) {
            error(KIO::ERR_CANNOT_RENAME, oldURL.toString());
            return;
        }
        // Dolphin/KIO can't specify a trashid in the new URL so here path == filename
        //bool newOk = TrashImpl::parseURL(newURL, newTrashId, newFileId, newRelativePath);
        const QString newFileId = newURL.path().mid(1);
        if (newFileId.contains(QLatin1Char('/'))) {
            error(KIO::ERR_CANNOT_RENAME, oldURL.toString());
            return;
        }
        bool ok = impl.moveInTrash(oldTrashId, oldFileId, newFileId);
        if (!ok) {
            error(impl.lastErrorCode(), impl.lastErrorMessage());
            return;
        }
        const QUrl finalUrl = TrashImpl::makeURL(oldTrashId, newFileId, QString());
        org::kde::KDirNotify::emitFileRenamed(oldURL, finalUrl);
        finished();
        return;
    }

    if (oldURL.scheme() == QLatin1String("trash") && newURL.isLocalFile()) {
        copyOrMoveFromTrash(oldURL, newURL, (flags & KIO::Overwrite), Move);
    } else if (oldURL.isLocalFile() && newURL.scheme() == QLatin1String("trash")) {
        copyOrMoveToTrash(oldURL, newURL, Move);
    } else {
        error(KIO::ERR_UNSUPPORTED_ACTION, i18n("Invalid combination of protocols."));
    }
}

void TrashProtocol::copy(const QUrl &src, const QUrl &dest, int /*permissions*/, KIO::JobFlags flags)
{
    INIT_IMPL;

    qCDebug(KIO_TRASH) << "TrashProtocol::copy(): " << src << " " << dest;

    if (src.scheme() == QLatin1String("trash") && dest.scheme() == QLatin1String("trash")) {
        error(KIO::ERR_UNSUPPORTED_ACTION, i18n("This file is already in the trash bin."));
        return;
    }

    if (src.scheme() == QLatin1String("trash") && dest.isLocalFile()) {
        copyOrMoveFromTrash(src, dest, (flags & KIO::Overwrite), Copy);
    } else if (src.isLocalFile() && dest.scheme() == QLatin1String("trash")) {
        copyOrMoveToTrash(src, dest, Copy);
    } else {
        error(KIO::ERR_UNSUPPORTED_ACTION, i18n("Invalid combination of protocols."));
    }
}

void TrashProtocol::copyOrMoveFromTrash(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action)
{
    // Extracting (e.g. via dnd). Ignore original location stored in info file.
    int trashId;
    QString fileId, relativePath;
    bool ok = TrashImpl::parseURL(src, trashId, fileId, relativePath);
    if (!ok) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", src.toString()));
        return;
    }
    const QString destPath = dest.path();
    if (QFile::exists(destPath)) {
        if (overwrite) {
            ok = QFile::remove(destPath);
            Q_ASSERT(ok);   // ### TODO
        } else {
            error(KIO::ERR_FILE_ALREADY_EXIST, destPath);
            return;
        }
    }

    if (action == Move) {
        qCDebug(KIO_TRASH) << "calling moveFromTrash(" << destPath << " " << trashId << " " << fileId << ")";
        ok = impl.moveFromTrash(destPath, trashId, fileId, relativePath);
    } else { // Copy
        qCDebug(KIO_TRASH) << "calling copyFromTrash(" << destPath << " " << trashId << " " << fileId << ")";
        ok = impl.copyFromTrash(destPath, trashId, fileId, relativePath);
    }
    if (!ok) {
        error(impl.lastErrorCode(), impl.lastErrorMessage());
    } else {
        if (action == Move && relativePath.isEmpty()) {
            (void)impl.deleteInfo(trashId, fileId);
        }
        finished();
    }
}

void TrashProtocol::copyOrMoveToTrash(const QUrl &src, const QUrl &dest, CopyOrMove action)
{
    qCDebug(KIO_TRASH) << "trashing a file" << src << dest;

    // Trashing a file
    // We detect the case where this isn't normal trashing, but
    // e.g. if kwrite tries to save (moving tempfile over destination)
    if (isTopLevelEntry(dest) && src.fileName() == dest.fileName()) { // new toplevel entry
        const QString srcPath = src.path();
        // In theory we should use TrashImpl::parseURL to give the right filename to createInfo,
        // in case the trash URL didn't contain the same filename as srcPath.
        // But this can only happen with copyAs/moveAs, not available in the GUI
        // for the trash (New/... or Rename from iconview/listview).
        int trashId;
        QString fileId;
        if (!impl.createInfo(srcPath, trashId, fileId)) {
            error(impl.lastErrorCode(), impl.lastErrorMessage());
        } else {
            bool ok;
            if (action == Move) {
                qCDebug(KIO_TRASH) << "calling moveToTrash(" << srcPath << " " << trashId << " " << fileId << ")";
                ok = impl.moveToTrash(srcPath, trashId, fileId);
            } else { // Copy
                qCDebug(KIO_TRASH) << "calling copyToTrash(" << srcPath << " " << trashId << " " << fileId << ")";
                ok = impl.copyToTrash(srcPath, trashId, fileId);
            }
            if (!ok) {
                (void)impl.deleteInfo(trashId, fileId);
                error(impl.lastErrorCode(), impl.lastErrorMessage());
            } else {
                // Inform caller of the final URL. Used by konq_undo.
                const QUrl url = impl.makeURL(trashId, fileId, QString());
                setMetaData(QLatin1String("trashURL-") + srcPath, url.url());
                finished();
            }
        }
    } else {
        qCDebug(KIO_TRASH) << "returning KIO::ERR_ACCESS_DENIED, it's not allowed to add a file to an existing trash directory";
        // It's not allowed to add a file to an existing trash directory.
        error(KIO::ERR_ACCESS_DENIED, dest.toString());
    }
}

void TrashProtocol::createTopLevelDirEntry(KIO::UDSEntry &entry)
{
    entry.reserve(entry.count() + 8);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Trash"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, impl.isEmpty() ? QStringLiteral("user-trash") : QStringLiteral("user-trash-full") );
    entry.fastInsert(KIO::UDSEntry::UDS_USER, m_userName);
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, m_groupName);
}

KIO::StatDetails TrashProtocol::getStatDetails()
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

void TrashProtocol::stat(const QUrl &url)
{
    INIT_IMPL;
    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        // The root is "virtual" - it's not a single physical directory
        KIO::UDSEntry entry = impl.trashUDSEntry(getStatDetails());
        createTopLevelDirEntry(entry);
        statEntry(entry);
        finished();
    } else {
        int trashId;
        QString fileId, relativePath;

        bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);

        if (!ok) {
            // ######## do we still need this?
            qCDebug(KIO_TRASH) << url << " looks fishy, returning does-not-exist";
            // A URL like trash:/file simply means that CopyJob is trying to see if
            // the destination exists already (it made up the URL by itself).
            error(KIO::ERR_DOES_NOT_EXIST, url.toString());
            //error( KIO::ERR_SLAVE_DEFINED, i18n( "Malformed URL %1" ).arg( url.toString() ) );
            return;
        }

        qCDebug(KIO_TRASH) << "parsed" << url << "got" << trashId << fileId << relativePath;

        const QString filePath = impl.physicalPath(trashId, fileId, relativePath);
        if (filePath.isEmpty()) {
            error(impl.lastErrorCode(), impl.lastErrorMessage());
            return;
        }

        // For a toplevel file, use the fileId as display name (to hide the trashId)
        // For a file in a subdir, use the fileName as is.
        QString fileDisplayName = relativePath.isEmpty() ? fileId : url.fileName();

        QUrl fileURL;
        if (url.path().length() > 1) {
            fileURL = url;
        }

        KIO::UDSEntry entry;
        TrashedFileInfo info;
        ok = impl.infoForFile(trashId, fileId, info);
        if (ok) {
            ok = createUDSEntry(filePath, fileDisplayName, fileURL.fileName(), entry, info);
        }

        if (!ok) {
            error(KIO::ERR_CANNOT_STAT, url.toString());
            return;
        }

        statEntry(entry);
        finished();
    }
}

void TrashProtocol::del(const QUrl &url, bool /*isfile*/)
{
    INIT_IMPL;
    int trashId;
    QString fileId, relativePath;

    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", url.toString()));
        return;
    }

    ok = relativePath.isEmpty();
    if (!ok) {
        error(KIO::ERR_ACCESS_DENIED, url.toString());
        return;
    }

    ok = impl.del(trashId, fileId);
    if (!ok) {
        error(impl.lastErrorCode(), impl.lastErrorMessage());
        return;
    }

    finished();
}

void TrashProtocol::listDir(const QUrl &url)
{
    INIT_IMPL;
    qCDebug(KIO_TRASH) << "listdir: " << url;
    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        listRoot();
        return;
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", url.toString()));
        return;
    }
    //was: const QString physicalPath = impl.physicalPath( trashId, fileId, relativePath );

    // Get info for deleted directory - the date of deletion and orig path will be used
    // for all the items in it, and we need the physicalPath.
    TrashedFileInfo info;
    ok = impl.infoForFile(trashId, fileId, info);
    if (!ok || info.physicalPath.isEmpty()) {
        error(impl.lastErrorCode(), impl.lastErrorMessage());
        return;
    }
    if (!relativePath.isEmpty()) {
        info.physicalPath += QLatin1Char('/') + relativePath;
    }

    // List subdir. Can't use kio_file here since we provide our own info...
    qCDebug(KIO_TRASH) << "listing " << info.physicalPath;
    const QStringList entryNames = impl.listDir(info.physicalPath);
    totalSize(entryNames.count());
    KIO::UDSEntry entry;
    for (const QString &fileName : entryNames) {
        if (fileName == QLatin1String("..")) {
            continue;
        }
        const QString filePath = info.physicalPath + QLatin1Char('/') + fileName;
        // shouldn't be necessary
        //const QString url = TrashImpl::makeURL( trashId, fileId, relativePath + '/' + fileName );
        entry.clear();
        TrashedFileInfo infoForItem(info);
        infoForItem.origPath += QLatin1Char('/') + fileName;
        if (createUDSEntry(filePath, fileName, fileName, entry, infoForItem)) {
            listEntry(entry);
        }
    }
    entry.clear();
    finished();
}

bool TrashProtocol::createUDSEntry(const QString &physicalPath, const QString &displayFileName, const QString &internalFileName, KIO::UDSEntry &entry, const TrashedFileInfo &info)
{
    entry.reserve(13);
    QByteArray physicalPath_c = QFile::encodeName(physicalPath);
    QT_STATBUF buff;
    if (QT_LSTAT(physicalPath_c.constData(), &buff) == -1) {
        qCWarning(KIO_TRASH) << "couldn't stat " << physicalPath << ", relevant trashinfo file will be removed";
        impl.deleteInfo(info.trashId, info.fileId);
        return false;
    }
    if (S_ISLNK(buff.st_mode)) {
        char buffer2[ 1000 ];
        int n = ::readlink(physicalPath_c.constData(), buffer2, 999);
        if (n != -1) {
            buffer2[ n ] = 0;
        }

        entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QFile::decodeName(buffer2));
        // Follow symlink
        // That makes sense in kio_file, but not in the trash, especially for the size
        // #136876
#if 0
        if (KDE_stat(physicalPath_c, &buff) == -1) {
            // It is a link pointing to nowhere
            buff.st_mode = S_IFLNK | S_IRWXU | S_IRWXG | S_IRWXO;
            buff.st_mtime = 0;
            buff.st_atime = 0;
            buff.st_size = 0;
        }
#endif
    }

    mode_t type = buff.st_mode & S_IFMT; // extract file type
    mode_t access = buff.st_mode & 07777; // extract permissions
    access &= 07555; // make it readonly, since it's in the trashcan
    Q_ASSERT(!internalFileName.isEmpty());
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, internalFileName);   // internal filename, like "0-foo"
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, displayFileName);   // user-visible filename, like "foo"
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
    //if ( !url.isEmpty() )
    //    entry.insert( KIO::UDSEntry::UDS_URL, url );

    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForFile(physicalPath);
    if (mt.isValid()) {
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, mt.name());
    }
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, access);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, buff.st_size);
    entry.fastInsert(KIO::UDSEntry::UDS_USER, m_userName);   // assumption
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, m_groupName);   // assumption
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, buff.st_mtime);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, buff.st_atime);   // ## or use it for deletion time?
    entry.fastInsert(KIO::UDSEntry::UDS_EXTRA, info.origPath);
    entry.fastInsert(KIO::UDSEntry::UDS_EXTRA + 1, info.deletionDate.toString(Qt::ISODate));
    return true;
}

void TrashProtocol::listRoot()
{
    INIT_IMPL;
    const TrashedFileInfoList lst = impl.list();
    totalSize(lst.count());
    KIO::UDSEntry entry;
    createTopLevelDirEntry(entry);
    listEntry(entry);
    for (const TrashedFileInfo &fileInfo : lst) {
        const QUrl url = TrashImpl::makeURL(fileInfo.trashId, fileInfo.fileId, QString());
        entry.clear();
        const QString fileDisplayName = fileInfo.fileId;

        if (createUDSEntry(fileInfo.physicalPath, fileDisplayName, url.fileName(), entry, fileInfo)) {
            listEntry(entry);
        }
    }
    entry.clear();
    finished();
}

void TrashProtocol::special(const QByteArray &data)
{
    INIT_IMPL;
    QDataStream stream(data);
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1:
        if (impl.emptyTrash()) {
            finished();
        } else {
            error(impl.lastErrorCode(), impl.lastErrorMessage());
        }
        break;
    case 2:
        impl.migrateOldTrash();
        finished();
        break;
    case 3: {
        QUrl url;
        stream >> url;
        restore(url);
        break;
    }
    default:
        qCWarning(KIO_TRASH) << "Unknown command in special(): " << cmd;
        error(KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd));
        break;
    }
}

void TrashProtocol::put(const QUrl &url, int /*permissions*/, KIO::JobFlags)
{
    INIT_IMPL;
    qCDebug(KIO_TRASH) << "put: " << url;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    error(KIO::ERR_ACCESS_DENIED, url.toString());
}

void TrashProtocol::get(const QUrl &url)
{
    INIT_IMPL;
    qCDebug(KIO_TRASH) << "get() : " << url;
    if (!url.isValid()) {
        //qCDebug(KIO_TRASH) << kBacktrace();
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", url.url()));
        return;
    }
    if (url.path().length() <= 1) {
        error(KIO::ERR_IS_DIRECTORY, url.toString());
        return;
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        error(KIO::ERR_SLAVE_DEFINED, i18n("Malformed URL %1", url.toString()));
        return;
    }
    const QString physicalPath = impl.physicalPath(trashId, fileId, relativePath);
    if (physicalPath.isEmpty()) {
        error(impl.lastErrorCode(), impl.lastErrorMessage());
        return;
    }

    // Usually we run jobs in TrashImpl (for e.g. future kdedmodule)
    // But for this one we wouldn't use DCOP for every bit of data...
    QUrl fileURL = QUrl::fromLocalFile(physicalPath);
    KIO::TransferJob *job = KIO::get(fileURL, KIO::NoReload, KIO::HideProgressInfo);
    connect(job, &KIO::TransferJob::data,
            this, &TrashProtocol::slotData);
    connect(job,  &KIO::TransferJob::mimeTypeFound,
            this, &TrashProtocol::slotMimetype);
    connect(job, &KJob::result,
            this, &TrashProtocol::jobFinished);
    enterLoop();
}

void TrashProtocol::slotData(KIO::Job *, const QByteArray &arr)
{
    data(arr);
}

void TrashProtocol::slotMimetype(KIO::Job *, const QString &mt)
{
    mimeType(mt);
}

void TrashProtocol::jobFinished(KJob *job)
{
    if (job->error()) {
        error(job->error(), job->errorText());
    } else {
        finished();
    }
    Q_EMIT leaveModality();
}

#if 0
void TrashProtocol::mkdir(const QUrl &url, int /*permissions*/)
{
    INIT_IMPL;
    // create info about deleted dir
    // ############ Problem: we don't know the original path.
    // Let's try to avoid this case (we should get to copy() instead, for local files)
    qCDebug(KIO_TRASH) << "mkdir: " << url;
    QString dir = url.adjusted(QUrl::RemoveFilename).path();

    if (dir.length() <= 1) { // new toplevel entry
        // ## we should use TrashImpl::parseURL to give the right filename to createInfo
        int trashId;
        QString fileId;
        if (!impl.createInfo(url.path(), trashId, fileId)) {
            error(impl.lastErrorCode(), impl.lastErrorMessage());
        } else {
            if (!impl.mkdir(trashId, fileId, permissions)) {
                (void)impl.deleteInfo(trashId, fileId);
                error(impl.lastErrorCode(), impl.lastErrorMessage());
            } else {
                finished();
            }
        }
    } else {
        // Well it's not allowed to add a directory to an existing deleted directory.
        error(KIO::ERR_ACCESS_DENIED, url.toString());
    }
}
#endif

void TrashProtocol::virtual_hook(int id, void *data)
{
    switch(id) {
        case SlaveBase::GetFileSystemFreeSpace: {
            QUrl *url = static_cast<QUrl *>(data);
            fileSystemFreeSpace(*url);
        }   break;
        default:
            SlaveBase::virtual_hook(id, data);
    }
}

void TrashProtocol::fileSystemFreeSpace(const QUrl &url)
{
    qCDebug(KIO_TRASH) << "fileSystemFreeSpace:" << url;

    INIT_IMPL;

    TrashImpl::TrashSpaceInfo spaceInfo;
    if (!impl.trashSpaceInfo(url.path(), spaceInfo)) {
        error(KIO::ERR_CANNOT_STAT, url.toDisplayString());
        return;
    }

    setMetaData(QStringLiteral("total"), QString::number(spaceInfo.totalSize));
    setMetaData(QStringLiteral("available"), QString::number(spaceInfo.availableSize));

    finished();
}

#include "kio_trash.moc"
