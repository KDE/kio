// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte.starostik@t-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "previewjob.h"
#include "kio_widgets_debug.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID)
#define WITH_SHM 1
#else
#define WITH_SHM 0
#endif

#if WITH_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <limits>
#include <set>

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QTimer>

#include <QCryptographicHash>

#include <KConfigGroup>
#include <KMountPoint>
#include <KPluginInfo>
#include <KService>
#include <KServiceTypeTrader>
#include <KSharedConfig>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <Solid/Device>
#include <Solid/StorageAccess>
#include <kprotocolinfo.h>

#include <algorithm>
#include <cmath>

#include "job_p.h"

namespace  {
    static int s_defaultDevicePixelRatio = 1;
}

namespace KIO
{
struct PreviewItem;
}
using namespace KIO;

struct KIO::PreviewItem {
    KFileItem item;
    KPluginMetaData plugin;
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
        , iconSize(0)
        , iconAlpha(70)
        , shmid(-1)
        , shmaddr(nullptr)
    {
        // http://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
        thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
    }

    enum {
        STATE_STATORIG, // if the thumbnail exists
        STATE_GETORIG, // if we create it
        STATE_CREATETHUMB, // thumbnail:/ slave
        STATE_DEVICE_INFO, // additional state check to get needed device ids
    } state;

    KFileItemList initialItems;
    QStringList enabledPlugins;
    // Some plugins support remote URLs, <protocol, mimetypes>
    QHash<QString, QStringList> m_remoteProtocolPlugins;
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
    // the size for the icon overlay
    int iconSize;
    // the transparency of the blended MIME type icon
    int iconAlpha;
    // Shared memory segment Id. The segment is allocated to a size
    // of extent x extent x 4 (32 bit image) on first need.
    int shmid;
    // And the data area
    uchar *shmaddr;
    // Size of the shm segment
    size_t shmsize;
    // Root of thumbnail cache
    QString thumbRoot;
    // Metadata returned from the KIO thumbnail slave
    QMap<QString, QString> thumbnailSlaveMetaData;
    int devicePixelRatio = s_defaultDevicePixelRatio;
    static const int idUnknown = -1;
    // Id of a device storing currently processed file
    int currentDeviceId = 0;
    // Device ID for each file. Stored while in STATE_DEVICE_INFO state, used later on.
    QMap<QString, int> deviceIdMap;
    enum CachePolicy { Prevent, Allow, Unknown } currentDeviceCachePolicy = Unknown;

    void getOrCreateThumbnail();
    bool statResultThumbnail();
    void createThumbnail(const QString &);
    void cleanupTempFile();
    void determineNextFile();
    void emitPreview(const QImage &thumb);

    void startPreview();
    void slotThumbData(KIO::Job *, const QByteArray &);
    // Checks if thumbnail is on encrypted partition different than thumbRoot
    CachePolicy canBeCached(const QString &path);
    int getDeviceId(const QString &path);

    Q_DECLARE_PUBLIC(PreviewJob)

    static QVector<KPluginMetaData> loadAvailablePlugins()
    {
        static QVector<KPluginMetaData> jsonMetaDataPlugins;
        if (!jsonMetaDataPlugins.isEmpty()) {
            return jsonMetaDataPlugins;
        }
        jsonMetaDataPlugins = KPluginMetaData::findPlugins(QStringLiteral("kf" QT_STRINGIFY(QT_VERSION_MAJOR) "/thumbcreator"));
        std::set<QString> pluginIds;
        for (const KPluginMetaData &data : std::as_const(jsonMetaDataPlugins)) {
            pluginIds.insert(data.pluginId());
        }
#if KSERVICE_ENABLE_DEPRECATED_SINCE(5, 88)
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_CLANG("-Wdeprecated-declarations")
        QT_WARNING_DISABLE_GCC("-Wdeprecated-declarations")
        const KService::List plugins = KServiceTypeTrader::self()->query(QStringLiteral("ThumbCreator"));
        for (const auto &plugin : plugins) {
            if (KPluginInfo info(plugin); info.isValid()) {
                if (auto [it, inserted] = pluginIds.insert(info.pluginName()); inserted) {
                    jsonMetaDataPlugins << info.toMetaData();
                }
            } else {
                // Hack for directory thumbnailer: It has a hardcoded plugin id in the kio-slave and not any C++ plugin
                // Consequently we just use the base name as the plugin file for our KPluginMetaData object
                const QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("kservices5/") + plugin->entryPath());
                KPluginMetaData tmpData = KPluginMetaData::fromDesktopFile(path);
                jsonMetaDataPlugins << KPluginMetaData(tmpData.rawData(), QFileInfo(path).baseName(), path);
            }
        }
        QT_WARNING_POP
#else
#pragma message("TODO: directory thumbnailer needs a non-desktop file solution ")
#endif
        return jsonMetaDataPlugins;
    }
};

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 86)
void PreviewJob::setDefaultDevicePixelRatio(int defaultDevicePixelRatio)
{
    s_defaultDevicePixelRatio = defaultDevicePixelRatio;
}
#endif

void PreviewJob::setDefaultDevicePixelRatio(qreal defaultDevicePixelRatio)
{
    s_defaultDevicePixelRatio = std::ceil(defaultDevicePixelRatio);
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 7)
PreviewJob::PreviewJob(const KFileItemList &items, int width, int height, int iconSize, int iconAlpha, bool scale, bool save, const QStringList *enabledPlugins)
    : KIO::Job(*new PreviewJobPrivate(items, QSize(width, height ? height : width)))
{
    Q_D(PreviewJob);
    d->enabledPlugins = enabledPlugins ? *enabledPlugins : availablePlugins();
    d->iconSize = iconSize;
    d->iconAlpha = iconAlpha;
    d->bScale = scale;
    d->bSave = save && scale;

    // Return to event loop first, determineNextFile() might delete this;
    QTimer::singleShot(0, this, SLOT(startPreview()));
}
#endif

PreviewJob::PreviewJob(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins)
    : KIO::Job(*new PreviewJobPrivate(items, size))
{
    Q_D(PreviewJob);

    if (enabledPlugins) {
        d->enabledPlugins = *enabledPlugins;
    } else {
        const KConfigGroup globalConfig(KSharedConfig::openConfig(), "PreviewSettings");
        d->enabledPlugins =
            globalConfig.readEntry("Plugins",
                                   QStringList{QStringLiteral("directorythumbnail"), QStringLiteral("imagethumbnail"), QStringLiteral("jpegthumbnail")});
    }

    // Return to event loop first, determineNextFile() might delete this;
    QTimer::singleShot(0, this, SLOT(startPreview()));
}

PreviewJob::~PreviewJob()
{
#if WITH_SHM
    Q_D(PreviewJob);
    if (d->shmaddr) {
        shmdt((char *)d->shmaddr);
        shmctl(d->shmid, IPC_RMID, nullptr);
    }
#endif
}

void PreviewJob::setOverlayIconSize(int size)
{
    Q_D(PreviewJob);
    d->iconSize = size;
}

int PreviewJob::overlayIconSize() const
{
    Q_D(const PreviewJob);
    return d->iconSize;
}

void PreviewJob::setOverlayIconAlpha(int alpha)
{
    Q_D(PreviewJob);
    d->iconAlpha = qBound(0, alpha, 255);
}

int PreviewJob::overlayIconAlpha() const
{
    Q_D(const PreviewJob);
    return d->iconAlpha;
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
    const QVector<KPluginMetaData> plugins = KIO::PreviewJobPrivate::loadAvailablePlugins();
    QMap<QString, KPluginMetaData> mimeMap;
    QHash<QString, QHash<QString, KPluginMetaData>> protocolMap;

    for (const KPluginMetaData &plugin : plugins) {
        QStringList protocols = plugin.value(QStringLiteral("X-KDE-Protocols"), QStringList());
        const QString p = plugin.value(QStringLiteral("X-KDE-Protocol"));
        if (!p.isEmpty()) {
            protocols.append(p);
        }
        for (const QString &protocol : std::as_const(protocols)) {
            // Add supported MIME type for this protocol
            QStringList &_ms = m_remoteProtocolPlugins[protocol];
            const auto mimeTypes = plugin.mimeTypes();
            for (const QString &_m : mimeTypes) {
                protocolMap[protocol].insert(_m, plugin);
                if (!_ms.contains(_m)) {
                    _ms.append(_m);
                }
            }
        }
        if (enabledPlugins.contains(plugin.pluginId())) {
            const auto mimeTypes = plugin.mimeTypes();
            for (const QString &mimeType : mimeTypes) {
                mimeMap.insert(mimeType, plugin);
            }
        }
    }

    // Look for images and store the items in our todo list :)
    bool bNeedCache = false;
    for (const auto &fileItem : std::as_const(initialItems)) {
        PreviewItem item;
        item.item = fileItem;

        const QString mimeType = item.item.mimetype();
        KPluginMetaData plugin;

        // look for protocol-specific thumbnail plugins first
        auto it = protocolMap.constFind(item.item.url().scheme());
        if (it != protocolMap.constEnd()) {
            plugin = it.value().value(mimeType);
        }

        if (!plugin.isValid()) {
            auto pluginIt = mimeMap.constFind(mimeType);
            if (pluginIt == mimeMap.constEnd()) {
                QString groupMimeType = mimeType;
                static const QRegularExpression expr(QStringLiteral("/.*"));
                groupMimeType.replace(expr, QStringLiteral("/*"));
                pluginIt = mimeMap.constFind(groupMimeType);

                if (pluginIt == mimeMap.constEnd()) {
                    QMimeDatabase db;
                    // check MIME type inheritance, resolve aliases
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
                }
            }

            if (pluginIt != mimeMap.constEnd()) {
                plugin = *pluginIt;
            }
        }

        if (plugin.isValid()) {
            item.plugin = plugin;
            items.push_back(item);
            if (!bNeedCache && bSave && plugin.value(QStringLiteral("CacheThumbnail"), true)) {
                const QUrl url = fileItem.url();
                if (!url.isLocalFile() || !url.adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(thumbRoot)) {
                    bNeedCache = true;
                }
            }
        } else {
            Q_EMIT q->failed(fileItem);
        }
    }

    KConfigGroup cg(KSharedConfig::openConfig(), "PreviewSettings");
    maximumLocalSize = cg.readEntry("MaximumSize", std::numeric_limits<KIO::filesize_t>::max());
    maximumRemoteSize = cg.readEntry("MaximumRemoteSize", 0);

    if (bNeedCache) {

        if (width <= 128 && height <= 128) {
            cacheSize = 128;
        } else if (width <= 256 && height <= 256) {
            cacheSize = 256;
        } else {
            cacheSize = 512;
        }

        struct CachePool {
            QString path;
            int minSize;
        };

        const static auto pools = {
            CachePool{QStringLiteral("/normal/"), 128},
            CachePool{QStringLiteral("/large/"), 256},
            CachePool{QStringLiteral("/x-large/"), 512},
            CachePool{QStringLiteral("/xx-large/"), 1024},
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

        if (!QDir(thumbPath).exists()) {
            if (QDir().mkpath(thumbPath)) { // Qt5 TODO: mkpath(dirPath, permissions)
                QFile f(thumbPath);
                f.setPermissions(QFile::ReadUser | QFile::WriteUser | QFile::ExeUser); // 0700
            }
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
    return d_func()->thumbnailSlaveMetaData.value(QStringLiteral("sequenceIndexWraparoundPoint"), QStringLiteral("-1.0")).toFloat();
}

bool KIO::PreviewJob::handlesSequences() const
{
    return d_func()->thumbnailSlaveMetaData.value(QStringLiteral("handlesSequences")) == QStringLiteral("1");
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 86)
void KIO::PreviewJob::setDevicePixelRatio(int dpr)
{
    d_func()->devicePixelRatio = dpr;
}
#endif

void KIO::PreviewJob::setDevicePixelRatio(qreal dpr)
{
    d_func()->devicePixelRatio = std::ceil(dpr);
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
        KIO::Job *job = KIO::statDetails(currentItem.item.url(), StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
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
            skipCurrentItem = !d->ignoreMaximumSize && size > d->maximumRemoteSize;

            // Remote directories are not supported, don't try to do a file_copy on them
            if (!skipCurrentItem) {
                // TODO update item.mimeType from the UDS entry, in case it wasn't set initially
                // But we don't use the MIME type anymore, we just use isDir().
                if (d->currentItem.item.isDir()) {
                    skipCurrentItem = true;
                }
            }
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
            qCWarning(KIO_WIDGETS) << "Cannot read information about filesystem under path" << path;
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
            qCWarning(KIO_WIDGETS) << "Failed to convert" << url << "to canonical path";
            return false;
        }
    } else {
        // Don't include the password if any
        origName = url.toEncoded(QUrl::RemovePassword);
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
    succeeded = true;
    determineNextFile();
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
    } else {
        const QUrl fileUrl = item.url();
        // heuristics for remote URL support
        bool supportsProtocol = false;
        if (m_remoteProtocolPlugins.value(fileUrl.scheme()).contains(item.mimetype())) {
            // There's a plugin supporting this protocol and MIME type
            supportsProtocol = true;
        } else if (m_remoteProtocolPlugins.value(QStringLiteral("KIO")).contains(item.mimetype())) {
            // Assume KIO understands any URL, ThumbCreator slaves who have
            // X-KDE-Protocols=KIO will get fed the remote URL directly.
            supportsProtocol = true;
        }

        if (supportsProtocol) {
            createThumbnail(fileUrl.toString());
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
        localFile.setAutoRemove(false);
        localFile.open();
        tempName = localFile.fileName();
        const QUrl currentURL = item.mostLocalUrl();
        KIO::Job *job = KIO::file_copy(currentURL, QUrl::fromLocalFile(tempName), -1, KIO::Overwrite | KIO::HideProgressInfo /* No GUI */);
        job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
        q->addSubjob(job);
    }
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
            shouldAllow = !device.as<Solid::StorageAccess>()->isEncrypted();
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
        qCWarning(KIO_WIDGETS) << "Could not get device id for file preview, Invalid url" << path;
        return 0;
    }
    state = PreviewJobPrivate::STATE_DEVICE_INFO;
    KIO::Job *job = KIO::statDetails(url, StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
    job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    q->addSubjob(job);

    return idUnknown;
}

void PreviewJobPrivate::createThumbnail(const QString &pixPath)
{
    Q_Q(PreviewJob);
    state = PreviewJobPrivate::STATE_CREATETHUMB;
    QUrl thumbURL;
    thumbURL.setScheme(QStringLiteral("thumbnail"));
    thumbURL.setPath(pixPath);

    bool save = bSave && currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) && !sequenceIndex;

    bool isRemoteProtocol = currentItem.item.localPath().isEmpty();
    CachePolicy cachePolicy = isRemoteProtocol ? CachePolicy::Prevent : canBeCached(pixPath);

    if (cachePolicy == CachePolicy::Unknown) {
        // If Unknown is returned, creating thumbnail should be called again by slotResult
        return;
    }

    KIO::TransferJob *job = KIO::get(thumbURL, NoReload, HideProgressInfo);
    q->addSubjob(job);
    q->connect(job, &KIO::TransferJob::data, q, [this](KIO::Job *job, const QByteArray &data) {
        slotThumbData(job, data);
    });

    int thumb_width = width;
    int thumb_height = height;
    int thumb_iconSize = iconSize;
    if (save) {
        thumb_width = thumb_height = cacheSize;
        thumb_iconSize = 64;
    }

    job->addMetaData(QStringLiteral("mimeType"), currentItem.item.mimetype());
    job->addMetaData(QStringLiteral("width"), QString::number(thumb_width));
    job->addMetaData(QStringLiteral("height"), QString::number(thumb_height));
    job->addMetaData(QStringLiteral("iconSize"), QString::number(thumb_iconSize));
    job->addMetaData(QStringLiteral("iconAlpha"), QString::number(iconAlpha));
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

void PreviewJobPrivate::slotThumbData(KIO::Job *job, const QByteArray &data)
{
    thumbnailSlaveMetaData = job->metaData();
    /* clang-format off */
    const bool save = bSave
                      && !sequenceIndex
                      && currentDeviceCachePolicy == CachePolicy::Allow
                      && currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true)
                      && (!currentItem.item.url().isLocalFile()
                          || !currentItem.item.url().adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(thumbRoot));
    /* clang-format on */

    QImage thumb;
#if WITH_SHM
    if (shmaddr) {
        // Keep this in sync with kdebase/kioslave/thumbnail.cpp
        QDataStream str(data);
        int width;
        int height;
        quint8 iFormat;
        int imgDevicePixelRatio = 1;
        // TODO KF6: add a version number as first parameter
        str >> width >> height >> iFormat;
        if (iFormat & 0x80) {
            // HACK to deduce if imgDevicePixelRatio is present
            iFormat &= 0x7f;
            str >> imgDevicePixelRatio;
        }
        QImage::Format format = static_cast<QImage::Format>(iFormat);
        thumb = QImage(shmaddr, width, height, format).copy();
        thumb.setDevicePixelRatio(imgDevicePixelRatio);
    } else {
        thumb.loadFromData(data);
    }
#else
    thumb.loadFromData(data);
#endif

    if (thumb.isNull()) {
        QDataStream s(data);
        s >> thumb;
    }

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
    emitPreview(thumb);
    succeeded = true;
}

void PreviewJobPrivate::emitPreview(const QImage &thumb)
{
    Q_Q(PreviewJob);
    QPixmap pix;
    if (thumb.width() > width * thumb.devicePixelRatio() || thumb.height() > height * thumb.devicePixelRatio()) {
        pix = QPixmap::fromImage(thumb.scaled(QSize(width * thumb.devicePixelRatio(), height * thumb.devicePixelRatio()), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        pix = QPixmap::fromImage(thumb);
    }
    pix.setDevicePixelRatio(thumb.devicePixelRatio());
    Q_EMIT q->gotPreview(currentItem.item, pix);
}

QVector<KPluginMetaData> PreviewJob::availableThumbnailerPlugins()
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

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 7)
PreviewJob *
KIO::filePreview(const KFileItemList &items, int width, int height, int iconSize, int iconAlpha, bool scale, bool save, const QStringList *enabledPlugins)
{
    return new PreviewJob(items, width, height, iconSize, iconAlpha, scale, save, enabledPlugins);
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 7)
PreviewJob *
KIO::filePreview(const QList<QUrl> &items, int width, int height, int iconSize, int iconAlpha, bool scale, bool save, const QStringList *enabledPlugins)
{
    KFileItemList fileItems;
    fileItems.reserve(items.size());
    for (const QUrl &url : items) {
        Q_ASSERT(url.isValid()); // please call us with valid urls only
        fileItems.append(KFileItem(url));
    }
    return new PreviewJob(fileItems, width, height, iconSize, iconAlpha, scale, save, enabledPlugins);
}
#endif

PreviewJob *KIO::filePreview(const KFileItemList &items, const QSize &size, const QStringList *enabledPlugins)
{
    return new PreviewJob(items, size, enabledPlugins);
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 5)
KIO::filesize_t PreviewJob::maximumFileSize()
{
    KConfigGroup cg(KSharedConfig::openConfig(), "PreviewSettings");
    return cg.readEntry("MaximumSize", 5 * 1024 * 1024LL /* 5MB */);
}
#endif

#include "moc_previewjob.cpp"
