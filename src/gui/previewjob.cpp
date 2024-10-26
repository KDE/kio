// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte.starostik@t-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "previewjob.h"
#include "filecopyjob.h"
#include "kiogui_debug.h"
#include "standardthumbnailjob_p.h"
#include "statjob.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
#define WITH_SHM 1
#else
#define WITH_SHM 0
#endif

#if WITH_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <algorithm>
#include <limits>

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QObject>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTimer>

#include <KConfigGroup>
#include <KFileUtils>
#include <KLocalizedString>
#include <KMountPoint>
#include <KPluginMetaData>
#include <KProtocolInfo>
#include <KService>
#include <KSharedConfig>
#include <Solid/Device>
#include <Solid/StorageAccess>

#include "job_p.h"

namespace
{
static qreal s_defaultDevicePixelRatio = 1.0;
}

namespace KIO
{
struct PreviewItem;
}
using namespace KIO;

struct KIO::PreviewItem {
    KFileItem item;
    KPluginMetaData plugin;
    bool standardThumbnailer = false;
};

class KIO::PreviewJobPrivate : public KIO::JobPrivate
{
public:
    PreviewJobPrivate(const KFileItemList &items, const QSize &size)
        : initialItems(items)
        , width(size.width())
        , height(size.height())
        , cacheSize(0)
        , bScale(true)
        , bSave(true)
        , ignoreMaximumSize(false)
        , sequenceIndex(0)
        , succeeded(false)
        , maximumLocalSize(0)
        , maximumRemoteSize(0)
        , enableRemoteFolderThumbnail(false)
        , shmid(-1)
        , shmaddr(nullptr)
    {
        // https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
        thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
    }

    enum {
        STATE_STATORIG, // if the thumbnail exists
        STATE_GETORIG, // if we create it
        STATE_CREATETHUMB, // thumbnail:/ worker
        STATE_DEVICE_INFO, // additional state check to get needed device ids
    } state;

    KFileItemList initialItems;
    QStringList enabledPlugins;
    // Our todo list :)
    // We remove the first item at every step, so use std::list
    std::list<PreviewItem> items;
    // The current item
    PreviewItem currentItem;
    // The modification time of that URL
    QDateTime tOrig;
    // Path to thumbnail cache for the current size
    QString thumbPath;
    // Original URL of current item in RFC2396 format
    // (file:///path/to/a%20file instead of file:/path/to/a file)
    QByteArray origName;
    // Thumbnail file name for current item
    QString thumbName;
    // Size of thumbnail
    int width;
    int height;
    // Unscaled size of thumbnail (128, 256 or 512 if cache is enabled)
    short cacheSize;
    // Whether the thumbnail should be scaled
    bool bScale;
    // Whether we should save the thumbnail
    bool bSave;
    bool ignoreMaximumSize;
    int sequenceIndex;
    bool succeeded;
    // If the file to create a thumb for was a temp file, this is its name
    QString tempName;
    KIO::filesize_t maximumLocalSize;
    KIO::filesize_t maximumRemoteSize;
    // Manage preview for locally mounted remote directories
    bool enableRemoteFolderThumbnail;
    // Shared memory segment Id. The segment is allocated to a size
    // of extent x extent x 4 (32 bit image) on first need.
    int shmid;
    // And the data area
    uchar *shmaddr;
    // Size of the shm segment
    size_t shmsize;
    // Root of thumbnail cache
    QString thumbRoot;
    // Metadata returned from the KIO thumbnail worker
    QMap<QString, QString> thumbnailWorkerMetaData;
    qreal devicePixelRatio = s_defaultDevicePixelRatio;
    static const int idUnknown = -1;
    // Id of a device storing currently processed file
    int currentDeviceId = 0;
    // Device ID for each file. Stored while in STATE_DEVICE_INFO state, used later on.
    QMap<QString, int> deviceIdMap;
    enum CachePolicy {
        Prevent,
        Allow,
        Unknown
    } currentDeviceCachePolicy = Unknown;
    // the path of a unique temporary directory
    QString m_tempDirPath;

    void getOrCreateThumbnail();
    bool statResultThumbnail();
    void createThumbnail(const QString &);
    void cleanupTempFile();
    void determineNextFile();
    void emitPreview(const QImage &thumb);

    void startPreview();
    void slotThumbData(KIO::Job *, const QByteArray &);
    void slotStandardThumbData(KIO::Job *, const QImage &);
    // Checks if thumbnail is on encrypted partition different than thumbRoot
    CachePolicy canBeCached(const QString &path);
    int getDeviceId(const QString &path);
    void saveThumbnailData(QImage &thumb);

    Q_DECLARE_PUBLIC(PreviewJob)

    struct StandardThumbnailerData {
        QString exec;
        QStringList mimetypes;
    };

    static QList<KPluginMetaData> loadAvailablePlugins()
    {
        static QList<KPluginMetaData> jsonMetaDataPlugins;
        if (jsonMetaDataPlugins.isEmpty()) {
            jsonMetaDataPlugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/thumbcreator"));
            for (const auto &thumbnailer : standardThumbnailers().asKeyValueRange()) {
                // Check if our own plugins support the mimetype. If so, we use the plugin instead
                // and ignore the standard thumbnailer
                auto handledMimes = thumbnailer.second.mimetypes;
                for (const auto &plugin : std::as_const(jsonMetaDataPlugins)) {
                    for (const auto &mime : handledMimes) {
                        if (plugin.mimeTypes().contains(mime)) {
                            handledMimes.removeOne(mime);
                        }
                    }
                }
                if (handledMimes.isEmpty()) {
                    continue;
                }

                QMimeDatabase db;
                // We only need the first mimetype since the names/comments are often shared between multiple types
                auto mime = db.mimeTypeForName(handledMimes.first());
                auto name = mime.name().isEmpty() ? handledMimes.first() : mime.name();
                if (!mime.comment().isEmpty()) {
                    name = mime.comment();
                }
                if (name.isEmpty()) {
                    continue;
                }
                // the plugin metadata
                QJsonObject kplugin;
                kplugin[QStringLiteral("MimeTypes")] = QJsonValue::fromVariant(handledMimes);
                kplugin[QStringLiteral("Name")] = name;
                kplugin[QStringLiteral("Description")] = QStringLiteral("standardthumbnailer");

                QJsonObject root;
                root[QStringLiteral("CacheThumbnail")] = true;
                root[QStringLiteral("KPlugin")] = kplugin;

                KPluginMetaData standardThumbnailerPlugin(root, thumbnailer.first);
                jsonMetaDataPlugins.append(standardThumbnailerPlugin);
            }
        }
        return jsonMetaDataPlugins;
    }

    static QMap<QString, StandardThumbnailerData> standardThumbnailers()
    {
        //         mimetype, exec
        static QMap<QString, StandardThumbnailerData> standardThumbs;
        if (standardThumbs.empty()) {
            QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("thumbnailers/"), QStandardPaths::LocateDirectory);
            const auto thumbnailerPaths = KFileUtils::findAllUniqueFiles(dirs, QStringList{QStringLiteral("*.thumbnailer")});
            for (const QString &thumbnailerPath : thumbnailerPaths) {
                const KConfigGroup thumbnailerConfig(KSharedConfig::openConfig(thumbnailerPath), QStringLiteral("Thumbnailer Entry"));
                StandardThumbnailerData data;
                QString thumbnailerName = QFileInfo(thumbnailerPath).baseName();
                QStringList mimetypes = thumbnailerConfig.readEntry("MimeType", QString{}).split(QStringLiteral(";"));
                mimetypes.removeAll(QLatin1String(""));
                QString exec = thumbnailerConfig.readEntry("Exec", QString{});
                if (!exec.isEmpty() && !mimetypes.isEmpty()) {
                    data.exec = exec;
                    data.mimetypes = mimetypes;
                    standardThumbs.insert(thumbnailerName, data);
                }
            }
        }
        return standardThumbs;
    }

private:
    QDir createTemporaryDir();
};

void PreviewJob::setDefaultDevicePixelRatio(qreal defaultDevicePixelRatio)
{
    s_defaultDevicePixelRatio = defaultDevicePixelRatio;
}

PreviewJob::PreviewJob(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins)
    : KIO::Job(*new PreviewJobPrivate(items, size))
{
    Q_D(PreviewJob);

    const KConfigGroup globalConfig(KSharedConfig::openConfig(), QStringLiteral("PreviewSettings"));
    if (enabledPlugins) {
        d->enabledPlugins = *enabledPlugins;
    } else {
        d->enabledPlugins =
            globalConfig.readEntry("Plugins",
                                   QStringList{QStringLiteral("directorythumbnail"), QStringLiteral("imagethumbnail"), QStringLiteral("jpegthumbnail")});
    }

    // Return to event loop first, determineNextFile() might delete this;
    QTimer::singleShot(0, this, [d]() {
        d->startPreview();
    });
}

PreviewJob::~PreviewJob()
{
    Q_D(PreviewJob);
    if (!d->m_tempDirPath.isEmpty()) {
        QDir tempDir(d->m_tempDirPath);
        tempDir.removeRecursively();
    }
#if WITH_SHM
    if (d->shmaddr) {
        shmdt((char *)d->shmaddr);
        shmctl(d->shmid, IPC_RMID, nullptr);
    }
#endif
}

void PreviewJob::setScaleType(ScaleType type)
{
    Q_D(PreviewJob);
    switch (type) {
    case Unscaled:
        d->bScale = false;
        d->bSave = false;
        break;
    case Scaled:
        d->bScale = true;
        d->bSave = false;
        break;
    case ScaledAndCached:
        d->bScale = true;
        d->bSave = true;
        break;
    default:
        break;
    }
}

PreviewJob::ScaleType PreviewJob::scaleType() const
{
    Q_D(const PreviewJob);
    if (d->bScale) {
        return d->bSave ? ScaledAndCached : Scaled;
    }
    return Unscaled;
}

void PreviewJobPrivate::startPreview()
{
    Q_Q(PreviewJob);
    // Load the list of plugins to determine which MIME types are supported
    const QList<KPluginMetaData> plugins = KIO::PreviewJobPrivate::loadAvailablePlugins();
    QMap<QString, KPluginMetaData> mimeMap;

    // Using thumbnailer plugin
    for (const KPluginMetaData &plugin : plugins) {
        bool pluginIsEnabled = enabledPlugins.contains(plugin.pluginId());
        const auto mimeTypes = plugin.mimeTypes();
        for (const QString &mimeType : mimeTypes) {
            if (pluginIsEnabled) {
                mimeMap.insert(mimeType, plugin);
            }
        }
    }

    // Look for images and store the items in our todo list :)
    bool bNeedCache = false;
    for (const auto &fileItem : std::as_const(initialItems)) {
        PreviewItem item;
        item.item = fileItem;
        item.standardThumbnailer = false;

        const QString mimeType = item.item.mimetype();
        KPluginMetaData plugin;

        auto pluginIt = mimeMap.constFind(mimeType);
        if (pluginIt == mimeMap.constEnd()) {
            // check MIME type inheritance, resolve aliases
            QMimeDatabase db;
            const QMimeType mimeInfo = db.mimeTypeForName(mimeType);
            if (mimeInfo.isValid()) {
                const QStringList parentMimeTypes = mimeInfo.allAncestors();
                for (const QString &parentMimeType : parentMimeTypes) {
                    pluginIt = mimeMap.constFind(parentMimeType);
                    if (pluginIt != mimeMap.constEnd()) {
                        break;
                    }
                }
            }

            if (pluginIt == mimeMap.constEnd()) {
                // Check the wildcards last, see BUG 453480
                QString groupMimeType = mimeType;
                const int slashIdx = groupMimeType.indexOf(QLatin1Char('/'));
                if (slashIdx != -1) {
                    // Replace everything after '/' with '*'
                    groupMimeType.truncate(slashIdx + 1);
                    groupMimeType += QLatin1Char('*');
                }
                pluginIt = mimeMap.constFind(groupMimeType);
            }
        }

        if (pluginIt != mimeMap.constEnd()) {
            plugin = *pluginIt;
        }

        if (plugin.isValid()) {
            item.standardThumbnailer = plugin.description() == QStringLiteral("standardthumbnailer");
            item.plugin = plugin;
            items.push_back(item);

            if (!bNeedCache && bSave && plugin.value(QStringLiteral("CacheThumbnail"), true)) {
                const QUrl url = fileItem.targetUrl();
                if (!url.isLocalFile() || !url.adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(thumbRoot)) {
                    bNeedCache = true;
                }
            }
        } else {
            Q_EMIT q->failed(fileItem);
        }
    }

    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("PreviewSettings"));
    maximumLocalSize = cg.readEntry("MaximumSize", std::numeric_limits<KIO::filesize_t>::max());
    maximumRemoteSize = cg.readEntry<KIO::filesize_t>("MaximumRemoteSize", 0);
    enableRemoteFolderThumbnail = cg.readEntry("EnableRemoteFolderThumbnail", false);

    if (bNeedCache) {
        const int longer = std::max(width, height);
        if (longer <= 128) {
            cacheSize = 128;
        } else if (longer <= 256) {
            cacheSize = 256;
        } else if (longer <= 512) {
            cacheSize = 512;
        } else {
            cacheSize = 1024;
        }

        struct CachePool {
            QString path;
            int minSize;
        };

        const static auto pools = {
            CachePool{QStringLiteral("normal/"), 128},
            CachePool{QStringLiteral("large/"), 256},
            CachePool{QStringLiteral("x-large/"), 512},
            CachePool{QStringLiteral("xx-large/"), 1024},
        };

        QString thumbDir;
        int wants = devicePixelRatio * cacheSize;
        for (const auto &p : pools) {
            if (p.minSize < wants) {
                continue;
            } else {
                thumbDir = p.path;
                break;
            }
        }
        thumbPath = thumbRoot + thumbDir;

        if (!QDir(thumbPath).exists() && !QDir(thumbRoot).mkdir(thumbDir, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser)) { // 0700
            qCWarning(KIO_GUI) << "couldn't create thumbnail dir " << thumbPath;
        }
    } else {
        bSave = false;
    }

    initialItems.clear();
    determineNextFile();
}

void PreviewJob::removeItem(const QUrl &url)
{
    Q_D(PreviewJob);

    auto it = std::find_if(d->items.cbegin(), d->items.cend(), [&url](const PreviewItem &pItem) {
        return url == pItem.item.url();
    });
    if (it != d->items.cend()) {
        d->items.erase(it);
    }

    if (d->currentItem.item.url() == url) {
        KJob *job = subjobs().first();
        job->kill();
        removeSubjob(job);
        d->determineNextFile();
    }
}

void KIO::PreviewJob::setSequenceIndex(int index)
{
    d_func()->sequenceIndex = index;
}

int KIO::PreviewJob::sequenceIndex() const
{
    return d_func()->sequenceIndex;
}

float KIO::PreviewJob::sequenceIndexWraparoundPoint() const
{
    return d_func()->thumbnailWorkerMetaData.value(QStringLiteral("sequenceIndexWraparoundPoint"), QStringLiteral("-1.0")).toFloat();
}

bool KIO::PreviewJob::handlesSequences() const
{
    return d_func()->thumbnailWorkerMetaData.value(QStringLiteral("handlesSequences")) == QStringLiteral("1");
}

void KIO::PreviewJob::setDevicePixelRatio(qreal dpr)
{
    d_func()->devicePixelRatio = dpr;
}

void PreviewJob::setIgnoreMaximumSize(bool ignoreSize)
{
    d_func()->ignoreMaximumSize = ignoreSize;
}

void PreviewJobPrivate::cleanupTempFile()
{
    if (!tempName.isEmpty()) {
        Q_ASSERT((!QFileInfo(tempName).isDir() && QFileInfo(tempName).isFile()) || QFileInfo(tempName).isSymLink());
        QFile::remove(tempName);
        tempName.clear();
    }
}

void PreviewJobPrivate::determineNextFile()
{
    Q_Q(PreviewJob);
    if (!currentItem.item.isNull()) {
        if (!succeeded) {
            Q_EMIT q->failed(currentItem.item);
        }
    }
    // No more items ?
    if (items.empty()) {
        q->emitResult();
        return;
    } else {
        // First, stat the orig file
        state = PreviewJobPrivate::STATE_STATORIG;
        currentItem = items.front();
        items.pop_front();
        succeeded = false;
        KIO::Job *job = KIO::stat(currentItem.item.targetUrl(), StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
        job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
        job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
        q->addSubjob(job);
    }
}

void PreviewJob::slotResult(KJob *job)
{
    Q_D(PreviewJob);

    removeSubjob(job);
    Q_ASSERT(!hasSubjobs()); // We should have only one job at a time ...
    switch (d->state) {
    case PreviewJobPrivate::STATE_STATORIG: {
        if (job->error()) { // that's no good news...
            // Drop this one and move on to the next one
            d->determineNextFile();
            return;
        }
        const KIO::UDSEntry statResult = static_cast<KIO::StatJob *>(job)->statResult();
        d->currentDeviceId = statResult.numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
        d->tOrig = QDateTime::fromSecsSinceEpoch(statResult.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0));

        bool skipCurrentItem = false;
        const KIO::filesize_t size = (KIO::filesize_t)statResult.numberValue(KIO::UDSEntry::UDS_SIZE, 0);
        const QUrl itemUrl = d->currentItem.item.mostLocalUrl();

        if ((itemUrl.isLocalFile() || KProtocolInfo::protocolClass(itemUrl.scheme()) == QLatin1String(":local")) && !d->currentItem.item.isSlow()) {
            skipCurrentItem = !d->ignoreMaximumSize && size > d->maximumLocalSize && !d->currentItem.plugin.value(QStringLiteral("IgnoreMaximumSize"), false);
        } else {
            // For remote items the "IgnoreMaximumSize" plugin property is not respected
            // Also we need to check if remote (but locally mounted) folder preview is enabled
            skipCurrentItem = (!d->ignoreMaximumSize && size > d->maximumRemoteSize) || (d->currentItem.item.isDir() && !d->enableRemoteFolderThumbnail);
        }
        if (skipCurrentItem) {
            d->determineNextFile();
            return;
        }

        bool pluginHandlesSequences = d->currentItem.plugin.value(QStringLiteral("HandleSequences"), false);
        if (!d->currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) || (d->sequenceIndex && pluginHandlesSequences)) {
            // This preview will not be cached, no need to look for a saved thumbnail
            // Just create it, and be done
            d->getOrCreateThumbnail();
            return;
        }

        if (d->statResultThumbnail()) {
            d->succeeded = true;
            d->determineNextFile();
            return;
        }

        d->getOrCreateThumbnail();
        return;
    }
    case PreviewJobPrivate::STATE_DEVICE_INFO: {
        KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
        int id;
        QString path = statJob->url().toLocalFile();
        if (job->error()) {
            // We set id to 0 to know we tried getting it
            qCWarning(KIO_GUI) << "Cannot read information about filesystem under path" << path;
            id = 0;
        } else {
            id = statJob->statResult().numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
        }
        d->deviceIdMap[path] = id;
        d->createThumbnail(d->currentItem.item.localPath());
        return;
    }
    case PreviewJobPrivate::STATE_GETORIG: {
        if (job->error()) {
            d->cleanupTempFile();
            d->determineNextFile();
            return;
        }

        d->createThumbnail(static_cast<KIO::FileCopyJob *>(job)->destUrl().toLocalFile());
        return;
    }
    case PreviewJobPrivate::STATE_CREATETHUMB: {
        d->cleanupTempFile();
        d->determineNextFile();
        return;
    }
    }
}

bool PreviewJobPrivate::statResultThumbnail()
{
    if (thumbPath.isEmpty()) {
        return false;
    }

    bool isLocal;
    const QUrl url = currentItem.item.mostLocalUrl(&isLocal);
    if (isLocal) {
        const QFileInfo localFile(url.toLocalFile());
        const QString canonicalPath = localFile.canonicalFilePath();
        origName = QUrl::fromLocalFile(canonicalPath).toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
        if (origName.isEmpty()) {
            qCWarning(KIO_GUI) << "Failed to convert" << url << "to canonical path";
            return false;
        }
    } else {
        // Don't include the password if any
        origName = currentItem.item.targetUrl().toEncoded(QUrl::RemovePassword);
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(origName);
    thumbName = QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");

    QImage thumb;
    QFile thumbFile(thumbPath + thumbName);
    if (!thumbFile.open(QIODevice::ReadOnly) || !thumb.load(&thumbFile, "png")) {
        return false;
    }

    if (thumb.text(QStringLiteral("Thumb::URI")) != QString::fromUtf8(origName)
        || thumb.text(QStringLiteral("Thumb::MTime")).toLongLong() != tOrig.toSecsSinceEpoch()) {
        return false;
    }

    const QString origSize = thumb.text(QStringLiteral("Thumb::Size"));
    if (!origSize.isEmpty() && origSize.toULongLong() != currentItem.item.size()) {
        // Thumb::Size is not required, but if it is set it should match
        return false;
    }

    // The DPR of the loaded thumbnail is unspecified (and typically irrelevant).
    // When a thumbnail is DPR-invariant, use the DPR passed in the request.
    thumb.setDevicePixelRatio(devicePixelRatio);

    QString thumbnailerVersion = currentItem.plugin.value(QStringLiteral("ThumbnailerVersion"));

    if (!thumbnailerVersion.isEmpty() && thumb.text(QStringLiteral("Software")).startsWith(QLatin1String("KDE Thumbnail Generator"))) {
        // Check if the version matches
        // The software string should read "KDE Thumbnail Generator pluginName (vX)"
        QString softwareString = thumb.text(QStringLiteral("Software")).remove(QStringLiteral("KDE Thumbnail Generator")).trimmed();
        if (softwareString.isEmpty()) {
            // The thumbnail has been created with an older version, recreating
            return false;
        }
        int versionIndex = softwareString.lastIndexOf(QLatin1String("(v"));
        if (versionIndex < 0) {
            return false;
        }

        QString cachedVersion = softwareString.remove(0, versionIndex + 2);
        cachedVersion.chop(1);
        uint thumbnailerMajor = thumbnailerVersion.toInt();
        uint cachedMajor = cachedVersion.toInt();
        if (thumbnailerMajor > cachedMajor) {
            return false;
        }
    }

    // Found it, use it
    emitPreview(thumb);
    return true;
}

void PreviewJobPrivate::getOrCreateThumbnail()
{
    Q_Q(PreviewJob);
    // We still need to load the orig file ! (This is getting tedious) :)
    const KFileItem &item = currentItem.item;
    const QString localPath = item.localPath();
    if (!localPath.isEmpty()) {
        createThumbnail(localPath);
        return;
    }

    if (item.isDir()) {
        // Skip remote dirs (bug 208625)
        cleanupTempFile();
        determineNextFile();
        return;
    }
    // No plugin support access to this remote content, copy the file
    // to the local machine, then create the thumbnail
    state = PreviewJobPrivate::STATE_GETORIG;
    QTemporaryFile localFile;

    // Some thumbnailers, like libkdcraw, depend on the file extension being
    // correct
    const QString extension = item.suffix();
    if (!extension.isEmpty()) {
        localFile.setFileTemplate(QStringLiteral("%1.%2").arg(localFile.fileTemplate(), extension));
    }

    localFile.setAutoRemove(false);
    localFile.open();
    tempName = localFile.fileName();
    const QUrl currentURL = item.mostLocalUrl();
    KIO::Job *job = KIO::file_copy(currentURL, QUrl::fromLocalFile(tempName), -1, KIO::Overwrite | KIO::HideProgressInfo /* No GUI */);
    job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
    q->addSubjob(job);
}

PreviewJobPrivate::CachePolicy PreviewJobPrivate::canBeCached(const QString &path)
{
    // If checked file is directory on a different filesystem than its parent, we need to check it separately
    int separatorIndex = path.lastIndexOf(QLatin1Char('/'));
    // special case for root folders
    const QString parentDirPath = separatorIndex == 0 ? path : path.left(separatorIndex);

    int parentId = getDeviceId(parentDirPath);
    if (parentId == idUnknown) {
        return CachePolicy::Unknown;
    }

    bool isDifferentSystem = !parentId || parentId != currentDeviceId;
    if (!isDifferentSystem && currentDeviceCachePolicy != CachePolicy::Unknown) {
        return currentDeviceCachePolicy;
    }
    int checkedId;
    QString checkedPath;
    if (isDifferentSystem) {
        checkedId = currentDeviceId;
        checkedPath = path;
    } else {
        checkedId = getDeviceId(parentDirPath);
        checkedPath = parentDirPath;
        if (checkedId == idUnknown) {
            return CachePolicy::Unknown;
        }
    }
    // If we're checking different filesystem or haven't checked yet see if filesystem matches thumbRoot
    int thumbRootId = getDeviceId(thumbRoot);
    if (thumbRootId == idUnknown) {
        return CachePolicy::Unknown;
    }
    bool shouldAllow = checkedId && checkedId == thumbRootId;
    if (!shouldAllow) {
        Solid::Device device = Solid::Device::storageAccessFromPath(checkedPath);
        if (device.isValid()) {
            // If the checked device is encrypted, allow thumbnailing if the thumbnails are stored in an encrypted location.
            // Or, if the checked device is unencrypted, allow thumbnailing.
            if (device.as<Solid::StorageAccess>()->isEncrypted()) {
                const Solid::Device thumbRootDevice = Solid::Device::storageAccessFromPath(thumbRoot);
                shouldAllow = thumbRootDevice.isValid() && thumbRootDevice.as<Solid::StorageAccess>()->isEncrypted();
            } else {
                shouldAllow = true;
            }
        }
    }
    if (!isDifferentSystem) {
        currentDeviceCachePolicy = shouldAllow ? CachePolicy::Allow : CachePolicy::Prevent;
    }
    return shouldAllow ? CachePolicy::Allow : CachePolicy::Prevent;
}

int PreviewJobPrivate::getDeviceId(const QString &path)
{
    Q_Q(PreviewJob);
    auto iter = deviceIdMap.find(path);
    if (iter != deviceIdMap.end()) {
        return iter.value();
    }
    QUrl url = QUrl::fromLocalFile(path);
    if (!url.isValid()) {
        qCWarning(KIO_GUI) << "Could not get device id for file preview, Invalid url" << path;
        return 0;
    }
    state = PreviewJobPrivate::STATE_DEVICE_INFO;
    KIO::Job *job = KIO::stat(url, StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
    job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    q->addSubjob(job);

    return idUnknown;
}

QDir KIO::PreviewJobPrivate::createTemporaryDir()
{
    if (m_tempDirPath.isEmpty()) {
        auto tempDir = QTemporaryDir();
        Q_ASSERT(tempDir.isValid());

        tempDir.setAutoRemove(false);
        // restrict read access to current User
        QFile::setPermissions(tempDir.path(), QFile::Permission::ReadOwner | QFile::Permission::WriteOwner | QFile::Permission::ExeOwner);

        m_tempDirPath = tempDir.path();
    }

    return QDir(m_tempDirPath);
}

void PreviewJobPrivate::createThumbnail(const QString &pixPath)
{
    Q_Q(PreviewJob);
    state = PreviewJobPrivate::STATE_CREATETHUMB;

    bool save = bSave && currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) && !sequenceIndex;

    bool isRemoteProtocol = currentItem.item.localPath().isEmpty();
    CachePolicy cachePolicy = isRemoteProtocol ? CachePolicy::Prevent : canBeCached(pixPath);

    if (cachePolicy == CachePolicy::Unknown) {
        // If Unknown is returned, creating thumbnail should be called again by slotResult
        return;
    }

    if (currentItem.standardThumbnailer) {
        // Using /usr/share/thumbnailers
        QString exec;
        for (const auto &thumbnailer : standardThumbnailers().asKeyValueRange()) {
            for (const auto &mimetype : std::as_const(thumbnailer.second.mimetypes)) {
                if (currentItem.plugin.supportsMimeType(mimetype)) {
                    exec = thumbnailer.second.exec;
                }
            }
        }
        if (exec.isEmpty()) {
            qCWarning(KIO_GUI) << "The exec entry for standard thumbnailer " << currentItem.plugin.name() << " was empty!";
            return;
        }
        auto tempDir = createTemporaryDir();
        if (pixPath.startsWith(tempDir.path())) {
            // don't generate thumbnails for images already in temporary directory
            return;
        }

        KIO::StandardThumbnailJob *job = new KIO::StandardThumbnailJob(exec, width * devicePixelRatio, pixPath, tempDir.path());
        q->addSubjob(job);
        q->connect(job, &KIO::StandardThumbnailJob::data, q, [=, this](KIO::Job *job, const QImage &thumb) {
            slotStandardThumbData(job, thumb);
        });
        job->start();
        return;
    }

    // Using thumbnailer plugin
    QUrl thumbURL;
    thumbURL.setScheme(QStringLiteral("thumbnail"));
    thumbURL.setPath(pixPath);
    KIO::TransferJob *job = KIO::get(thumbURL, NoReload, HideProgressInfo);
    q->addSubjob(job);
    q->connect(job, &KIO::TransferJob::data, q, [this](KIO::Job *job, const QByteArray &data) {
        slotThumbData(job, data);
    });
    int thumb_width = width;
    int thumb_height = height;
    if (save) {
        thumb_width = thumb_height = cacheSize;
    }

    job->addMetaData(QStringLiteral("mimeType"), currentItem.item.mimetype());
    job->addMetaData(QStringLiteral("width"), QString::number(thumb_width));
    job->addMetaData(QStringLiteral("height"), QString::number(thumb_height));
    job->addMetaData(QStringLiteral("plugin"), currentItem.plugin.fileName());
    job->addMetaData(QStringLiteral("enabledPlugins"), enabledPlugins.join(QLatin1Char(',')));
    job->addMetaData(QStringLiteral("devicePixelRatio"), QString::number(devicePixelRatio));
    job->addMetaData(QStringLiteral("cache"), QString::number(cachePolicy == CachePolicy::Allow));
    if (sequenceIndex) {
        job->addMetaData(QStringLiteral("sequence-index"), QString::number(sequenceIndex));
    }

#if WITH_SHM
    size_t requiredSize = thumb_width * devicePixelRatio * thumb_height * devicePixelRatio * 4;
    if (shmid == -1 || shmsize < requiredSize) {
        if (shmaddr) {
            // clean previous shared memory segment
            shmdt((char *)shmaddr);
            shmaddr = nullptr;
            shmctl(shmid, IPC_RMID, nullptr);
            shmid = -1;
        }
        if (requiredSize > 0) {
            shmid = shmget(IPC_PRIVATE, requiredSize, IPC_CREAT | 0600);
            if (shmid != -1) {
                shmsize = requiredSize;
                shmaddr = (uchar *)(shmat(shmid, nullptr, SHM_RDONLY));
                if (shmaddr == (uchar *)-1) {
                    shmctl(shmid, IPC_RMID, nullptr);
                    shmaddr = nullptr;
                    shmid = -1;
                }
            }
        }
    }
    if (shmid != -1) {
        job->addMetaData(QStringLiteral("shmid"), QString::number(shmid));
    }
#endif
}

void PreviewJobPrivate::slotStandardThumbData(KIO::Job *job, const QImage &thumbData)
{
    thumbnailWorkerMetaData = job->metaData();

    if (thumbData.isNull()) {
        // let succeeded in false state
        // failed will get called in determineNextFile()
        return;
    }

    QImage thumb = thumbData;
    saveThumbnailData(thumb);

    emitPreview(thumb);
    succeeded = true;
}

void PreviewJobPrivate::slotThumbData(KIO::Job *job, const QByteArray &data)
{
    QImage thumb;
    // Keep this in sync with kio-extras|thumbnail/thumbnail.cpp
    QDataStream str(data);

#if WITH_SHM
    if (shmaddr != nullptr) {
        int width;
        int height;
        QImage::Format format;
        qreal imgDevicePixelRatio;
        // TODO KF6: add a version number as first parameter
        str >> width >> height >> format >> imgDevicePixelRatio;
        thumb = QImage(shmaddr, width, height, format).copy();
        thumb.setDevicePixelRatio(imgDevicePixelRatio);
    }
#endif

    if (thumb.isNull()) {
        // fallback a raw QImage
        str >> thumb;
    }

    slotStandardThumbData(job, thumb);
}

void PreviewJobPrivate::saveThumbnailData(QImage &thumb)
{
    const bool save = bSave && !sequenceIndex && currentDeviceCachePolicy == CachePolicy::Allow
        && currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true)
        && (!currentItem.item.targetUrl().isLocalFile() || !currentItem.item.targetUrl().adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(thumbRoot));

    if (save) {
        thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(origName));
        thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(tOrig.toSecsSinceEpoch()));
        thumb.setText(QStringLiteral("Thumb::Size"), number(currentItem.item.size()));
        thumb.setText(QStringLiteral("Thumb::Mimetype"), currentItem.item.mimetype());
        QString thumbnailerVersion = currentItem.plugin.value(QStringLiteral("ThumbnailerVersion"));
        QString signature = QLatin1String("KDE Thumbnail Generator ") + currentItem.plugin.name();
        if (!thumbnailerVersion.isEmpty()) {
            signature.append(QLatin1String(" (v") + thumbnailerVersion + QLatin1Char(')'));
        }
        thumb.setText(QStringLiteral("Software"), signature);
        QSaveFile saveFile(thumbPath + thumbName);
        if (saveFile.open(QIODevice::WriteOnly)) {
            if (thumb.save(&saveFile, "PNG")) {
                saveFile.commit();
            }
        }
    }
}

void PreviewJobPrivate::emitPreview(const QImage &thumb)
{
    Q_Q(PreviewJob);
    QPixmap pix;
    const qreal ratio = thumb.devicePixelRatio();
    if (thumb.width() > width * ratio || thumb.height() > height * ratio) {
        pix = QPixmap::fromImage(thumb.scaled(QSize(width * ratio, height * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        pix = QPixmap::fromImage(thumb);
    }
    pix.setDevicePixelRatio(ratio);
    Q_EMIT q->gotPreview(currentItem.item, pix);
}

QList<KPluginMetaData> PreviewJob::availableThumbnailerPlugins()
{
    return PreviewJobPrivate::loadAvailablePlugins();
}

QStringList PreviewJob::availablePlugins()
{
    QStringList result;
    const auto plugins = KIO::PreviewJobPrivate::loadAvailablePlugins();
    for (const KPluginMetaData &plugin : plugins) {
        result << plugin.pluginId();
    }
    return result;
}

QStringList PreviewJob::defaultPlugins()
{
    const QStringList blacklist = QStringList() << QStringLiteral("textthumbnail");

    QStringList defaultPlugins = availablePlugins();
    for (const QString &plugin : blacklist) {
        defaultPlugins.removeAll(plugin);
    }

    return defaultPlugins;
}

QStringList PreviewJob::supportedMimeTypes()
{
    QStringList result;
    const auto plugins = KIO::PreviewJobPrivate::loadAvailablePlugins();
    for (const KPluginMetaData &plugin : plugins) {
        result += plugin.mimeTypes();
    }
    return result;
}

PreviewJob *KIO::filePreview(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins)
{
    return new PreviewJob(items, size, enabledPlugins);
}

#include "moc_previewjob.cpp"
