/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kio_trash.h"
#include "../../utils_p.h"
#include "kiotrashdebug.h"
#include "transferjob.h"

#ifdef WITH_QTDBUS
#include <KDirNotify>
#endif

#include <kio/jobuidelegateextension.h>

#include <KLocalizedString>

#include <QCoreApplication>
#include <QDataStream>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QMimeType>

#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.trash" FILE "trash.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    // necessary to use other KIO workers
    QCoreApplication app(argc, argv);

    KIO::setDefaultJobUiDelegateExtension(nullptr);
    // start the worker
    TrashProtocol worker(argv[1], argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}
}

static bool isTopLevelEntry(const QUrl &url)
{
    const QString dir = url.adjusted(QUrl::RemoveFilename).path();
    return dir.length() <= 1;
}

TrashProtocol::TrashProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : WorkerBase(protocol, pool, app)
{
    m_userId = getuid();
    struct passwd *user = getpwuid(m_userId);
    if (user) {
        m_userName = QString::fromLatin1(user->pw_name);
    }
    m_groupId = getgid();
    struct group *grp = getgrgid(m_groupId);
    if (grp) {
        m_groupName = QString::fromLatin1(grp->gr_name);
    }
}

TrashProtocol::~TrashProtocol()
{
}

KIO::WorkerResult TrashProtocol::initImpl()
{
    if (!impl.init()) {
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::enterLoop()
{
    int errorId = 0;
    QString errorText;

    QEventLoop eventLoop;
    connect(this, &TrashProtocol::leaveModality, &eventLoop, [&](int _errorId, const QString &_errorText) {
        errorId = _errorId;
        errorText = _errorText;
        eventLoop.quit();
    });
    eventLoop.exec(QEventLoop::ExcludeUserInputEvents);

    if (errorId != 0) {
        return KIO::WorkerResult::fail(errorId, errorText);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::restore(const QUrl &trashURL)
{
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(trashURL, trashId, fileId, relativePath);
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", trashURL.toString()));
    }
    TrashedFileInfo info;
    ok = impl.infoForFile(trashId, fileId, info);
    if (!ok) {
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
    }
    QUrl dest = QUrl::fromLocalFile(info.origPath);
    if (!relativePath.isEmpty()) {
        dest.setPath(Utils::concatPaths(dest.path(), relativePath));
    }

    // Check that the destination directory exists, to improve the error code in case it doesn't.
    const QString destDir = dest.adjusted(QUrl::RemoveFilename).path();
    QT_STATBUF buff;

    if (QT_LSTAT(QFile::encodeName(destDir).constData(), &buff) == -1) {
        return KIO::WorkerResult::fail(
            KIO::ERR_WORKER_DEFINED,
            i18n("The directory %1 does not exist anymore, so it is not possible to restore this item to its original location. "
                 "You can either recreate that directory and use the restore operation again, or drag the item anywhere else to restore it.",
                 destDir));
    }

    return copyOrMoveFromTrash(trashURL, dest, false /*overwrite*/, Move);
}

KIO::WorkerResult TrashProtocol::rename(const QUrl &oldURL, const QUrl &newURL, KIO::JobFlags flags)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    qCDebug(KIO_TRASH) << "TrashProtocol::rename(): old=" << oldURL << " new=" << newURL << " overwrite=" << (flags & KIO::Overwrite);

    if (oldURL.scheme() == QLatin1String("trash") && newURL.scheme() == QLatin1String("trash")) {
        if (!isTopLevelEntry(oldURL) || !isTopLevelEntry(newURL)) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, oldURL.toString());
        }
        int oldTrashId;
        QString oldFileId;
        QString oldRelativePath;
        bool oldOk = TrashImpl::parseURL(oldURL, oldTrashId, oldFileId, oldRelativePath);
        if (!oldOk) {
            return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", oldURL.toString()));
        }
        if (!oldRelativePath.isEmpty()) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, oldURL.toString());
        }
        // Dolphin/KIO can't specify a trashid in the new URL so here path == filename
        // bool newOk = TrashImpl::parseURL(newURL, newTrashId, newFileId, newRelativePath);
        const QString newFileId = newURL.path().mid(1);
        if (newFileId.contains(QLatin1Char('/'))) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_RENAME, oldURL.toString());
        }
        bool ok = impl.moveInTrash(oldTrashId, oldFileId, newFileId);
        if (!ok) {
            return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
        }
        const QUrl finalUrl = TrashImpl::makeURL(oldTrashId, newFileId, QString());
#ifdef WITH_QTDBUS
        org::kde::KDirNotify::emitFileRenamed(oldURL, finalUrl);
#endif
        return KIO::WorkerResult::pass();
    }

    if (oldURL.scheme() == QLatin1String("trash") && newURL.isLocalFile()) {
        return copyOrMoveFromTrash(oldURL, newURL, (flags & KIO::Overwrite), Move);
    }
    if (oldURL.isLocalFile() && newURL.scheme() == QLatin1String("trash")) {
        return copyOrMoveToTrash(oldURL, newURL, Move);
    }
    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Invalid combination of protocols."));
}

KIO::WorkerResult TrashProtocol::copy(const QUrl &src, const QUrl &dest, int /*permissions*/, KIO::JobFlags flags)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    qCDebug(KIO_TRASH) << "TrashProtocol::copy(): " << src << " " << dest;

    if (src.scheme() == QLatin1String("trash") && dest.scheme() == QLatin1String("trash")) {
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("This file is already in the trash bin."));
    }

    if (src.scheme() == QLatin1String("trash") && dest.isLocalFile()) {
        return copyOrMoveFromTrash(src, dest, (flags & KIO::Overwrite), Copy);
    }
    if (src.isLocalFile() && dest.scheme() == QLatin1String("trash")) {
        return copyOrMoveToTrash(src, dest, Copy);
    }
    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, i18n("Invalid combination of protocols."));
}

KIO::WorkerResult TrashProtocol::copyOrMoveFromTrash(const QUrl &src, const QUrl &dest, bool overwrite, CopyOrMove action)
{
    // Extracting (e.g. via dnd). Ignore original location stored in info file.
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(src, trashId, fileId, relativePath);
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", src.toString()));
    }
    const QString destPath = dest.path();
    if (QFile::exists(destPath)) {
        if (overwrite) {
            ok = QFile::remove(destPath);
            Q_ASSERT(ok); // ### TODO
        } else {
            return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, destPath);
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
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
    }

    if (action == Move && relativePath.isEmpty()) {
        (void)impl.deleteInfo(trashId, fileId);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::copyOrMoveToTrash(const QUrl &src, const QUrl &dest, CopyOrMove action)
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
            return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
        }
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
            return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
        }
        // Inform caller of the final URL. Used by konq_undo.
        const QUrl url = impl.makeURL(trashId, fileId, QString());
        setMetaData(QLatin1String("trashURL-") + srcPath, url.url());
        return KIO::WorkerResult::pass();
    }

    qCDebug(KIO_TRASH) << "returning KIO::ERR_ACCESS_DENIED, it's not allowed to add a file to an existing trash directory";
    // It's not allowed to add a file to an existing trash directory.
    return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, dest.toString());
}

void TrashProtocol::createTopLevelDirEntry(KIO::UDSEntry &entry)
{
    entry.reserve(entry.count() + 8);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, i18n("Trash"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, impl.isEmpty() ? QStringLiteral("user-trash") : QStringLiteral("user-trash-full"));
    entry.fastInsert(KIO::UDSEntry::UDS_USER, m_userName);
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, m_groupName);
    entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_USER_ID, m_userId);
    entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_GROUP_ID, m_groupId);
}

KIO::StatDetails TrashProtocol::getStatDetails()
{
    const QString statDetails = metaData(QStringLiteral("details"));
    return statDetails.isEmpty() ? KIO::StatDefaultDetails : static_cast<KIO::StatDetails>(statDetails.toInt());
}

KIO::WorkerResult TrashProtocol::stat(const QUrl &url)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        // The root is "virtual" - it's not a single physical directory
        KIO::UDSEntry entry = impl.trashUDSEntry(getStatDetails());
        createTopLevelDirEntry(entry);
        statEntry(entry);
    } else {
        int trashId;
        QString fileId;
        QString relativePath;

        bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);

        if (!ok) {
            // ######## do we still need this?
            qCDebug(KIO_TRASH) << url << " looks fishy, returning does-not-exist";
            // A URL like trash:/file simply means that CopyJob is trying to see if
            // the destination exists already (it made up the URL by itself).
            // error( KIO::ERR_WORKER_DEFINED, i18n( "Malformed URL %1" ).arg( url.toString() ) );
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toString());
        }

        qCDebug(KIO_TRASH) << "parsed" << url << "got" << trashId << fileId << relativePath;

        const QString filePath = impl.physicalPath(trashId, fileId, relativePath);
        if (filePath.isEmpty()) {
            return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
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
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_STAT, url.toString());
        }

        statEntry(entry);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::del(const QUrl &url, bool /*isfile*/)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    int trashId;
    QString fileId;
    QString relativePath;

    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", url.toString()));
    }

    ok = relativePath.isEmpty();
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, url.toString());
    }

    ok = impl.del(trashId, fileId);
    if (!ok) {
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
    }

    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::listDir(const QUrl &url)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    qCDebug(KIO_TRASH) << "listdir: " << url;
    const QString path = url.path();
    if (path.isEmpty() || path == QLatin1String("/")) {
        return listRoot();
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", url.toString()));
    }
    // was: const QString physicalPath = impl.physicalPath( trashId, fileId, relativePath );

    // Get info for deleted directory - the date of deletion and orig path will be used
    // for all the items in it, and we need the physicalPath.
    TrashedFileInfo info;
    ok = impl.infoForFile(trashId, fileId, info);
    if (!ok || info.physicalPath.isEmpty()) {
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
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
        // const QString url = TrashImpl::makeURL( trashId, fileId, relativePath + '/' + fileName );
        entry.clear();
        TrashedFileInfo infoForItem(info);
        infoForItem.origPath += QLatin1Char('/') + fileName;
        if (createUDSEntry(filePath, QFileInfo(infoForItem.origPath).fileName(), fileName, entry, infoForItem)) {
            listEntry(entry);
        }
    }
    entry.clear();
    return KIO::WorkerResult::pass();
}

bool TrashProtocol::createUDSEntry(const QString &physicalPath,
                                   const QString &displayFileName,
                                   const QString &internalFileName,
                                   KIO::UDSEntry &entry,
                                   const TrashedFileInfo &info)
{
    entry.reserve(14);
    QByteArray physicalPath_c = QFile::encodeName(physicalPath);
    QT_STATBUF buff;
    if (QT_LSTAT(physicalPath_c.constData(), &buff) == -1) {
        qCWarning(KIO_TRASH) << "couldn't stat " << physicalPath << ", relevant trashinfo file will be removed";
        impl.deleteInfo(info.trashId, info.fileId);
        return false;
    }
    if (S_ISLNK(buff.st_mode)) {
        char buffer2[1000];
        int n = ::readlink(physicalPath_c.constData(), buffer2, 999);
        if (n != -1) {
            buffer2[n] = 0;
        }

        // this does not follow symlink on purpose
        entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QFile::decodeName(buffer2));
    }

    mode_t type = buff.st_mode & S_IFMT; // extract file type
    mode_t access = buff.st_mode & 07777; // extract permissions
    access &= 07555; // make it readonly, since it's in the trashcan
    Q_ASSERT(!internalFileName.isEmpty());
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, internalFileName); // internal filename, like "0-foo"
    entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, displayFileName); // user-visible filename, like "foo"
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
    entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, physicalPath);
    // if ( !url.isEmpty() )
    //    entry.insert( KIO::UDSEntry::UDS_URL, url );

    QMimeDatabase db;
    QMimeType mt = db.mimeTypeForFile(physicalPath);
    if (mt.isValid()) {
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, mt.name());
    }
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, access);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, buff.st_size);
    entry.fastInsert(KIO::UDSEntry::UDS_USER, m_userName); // assumption
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, m_groupName); // assumption
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, buff.st_mtime);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, buff.st_atime); // ## or use it for deletion time?
    entry.fastInsert(KIO::UDSEntry::UDS_EXTRA, info.origPath);
    entry.fastInsert(KIO::UDSEntry::UDS_EXTRA + 1, info.deletionDate.toString(Qt::ISODate));
    return true;
}

KIO::WorkerResult TrashProtocol::listRoot()
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    const TrashedFileInfoList lst = impl.list();
    totalSize(lst.count());
    KIO::UDSEntry entry;
    createTopLevelDirEntry(entry);
    listEntry(entry);
    for (const TrashedFileInfo &fileInfo : lst) {
        const QUrl url = TrashImpl::makeURL(fileInfo.trashId, fileInfo.fileId, QString());
        entry.clear();
        const QString fileDisplayName = fileInfo.fileId;

        if (createUDSEntry(fileInfo.physicalPath,  QFileInfo(fileInfo.origPath).fileName(), url.fileName(), entry, fileInfo)) {
            listEntry(entry);
        }
    }
    entry.clear();
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::special(const QByteArray &data)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    QDataStream stream(data);
    int cmd;
    stream >> cmd;

    switch (cmd) {
    case 1:
        if (!impl.emptyTrash()) {
            return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
        }
        break;
    case 2:
        impl.migrateOldTrash();
        break;
    case 3: {
        QUrl url;
        stream >> url;
        return restore(url);
    }
    case 4: {
        QJsonObject json;
        const auto map = impl.trashDirectories();
        for (auto it = map.begin(); it != map.end(); ++it) {
            json[QString::number(it.key())] = it.value();
        }
        setMetaData(QStringLiteral("TRASH_DIRECTORIES"), QString::fromLocal8Bit(QJsonDocument(json).toJson()));
        sendMetaData();
        break;
    }
    default:
        qCWarning(KIO_TRASH) << "Unknown command in special(): " << cmd;
        return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, QString::number(cmd));
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult TrashProtocol::put(const QUrl &url, int /*permissions*/, KIO::JobFlags)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    qCDebug(KIO_TRASH) << "put: " << url;
    // create deleted file. We need to get the mtime and original location from metadata...
    // Maybe we can find the info file for url.fileName(), in case ::rename() was called first, and failed...
    return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, url.toString());
}

KIO::WorkerResult TrashProtocol::get(const QUrl &url)
{
    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    qCDebug(KIO_TRASH) << "get() : " << url;
    if (!url.isValid()) {
        // qCDebug(KIO_TRASH) << kBacktrace();
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", url.url()));
    }
    if (url.path().length() <= 1) {
        return KIO::WorkerResult::fail(KIO::ERR_IS_DIRECTORY, url.toString());
    }
    int trashId;
    QString fileId;
    QString relativePath;
    bool ok = TrashImpl::parseURL(url, trashId, fileId, relativePath);
    if (!ok) {
        return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, i18n("Malformed URL %1", url.toString()));
    }
    const QString physicalPath = impl.physicalPath(trashId, fileId, relativePath);
    if (physicalPath.isEmpty()) {
        return KIO::WorkerResult::fail(impl.lastErrorCode(), impl.lastErrorMessage());
    }

    // Usually we run jobs in TrashImpl (for e.g. future kdedmodule)
    // But for this one we wouldn't use DCOP for every bit of data...
    QUrl fileURL = QUrl::fromLocalFile(physicalPath);
    KIO::TransferJob *job = KIO::get(fileURL, KIO::NoReload, KIO::HideProgressInfo);
    connect(job, &KIO::TransferJob::data, this, &TrashProtocol::slotData);
    connect(job, &KIO::TransferJob::mimeTypeFound, this, &TrashProtocol::slotMimetype);
    connect(job, &KJob::result, this, &TrashProtocol::jobFinished);
    return enterLoop();
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
    Q_EMIT leaveModality(job->error(), job->errorText());
}

KIO::WorkerResult TrashProtocol::fileSystemFreeSpace(const QUrl &url)
{
    qCDebug(KIO_TRASH) << "fileSystemFreeSpace:" << url;

    if (const auto initResult = initImpl(); !initResult.success()) {
        return initResult;
    }

    TrashImpl::TrashSpaceInfo spaceInfo;
    if (!impl.trashSpaceInfo(url.path(), spaceInfo)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_STAT, url.toDisplayString());
    }

    setMetaData(QStringLiteral("total"), QString::number(spaceInfo.totalSize));
    setMetaData(QStringLiteral("available"), QString::number(spaceInfo.availableSize));

    return KIO::WorkerResult::pass();
}

#include "kio_trash.moc"

#include "moc_kio_trash.cpp"
