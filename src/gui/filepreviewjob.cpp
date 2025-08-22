/*
 *  This file is part of the KDE libraries
 *  SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *  SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
 *  SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
 *  SPDX-FileCopyrightText: 2001 Malte Starostik <malte.starostik@t-online.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "filepreviewjob.h"
#include "filecopyjob.h"
#include "kiogui_debug.h"
#include "previewjob.h"
#include "standardthumbnailjob_p.h"
#include "statjob.h"
#include "transferjob.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID) && !defined(Q_OS_HAIKU)
#define WITH_SHM 1
#else
#define WITH_SHM 0
#endif

#if WITH_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#include <KConfigGroup>
#include <KFileUtils>
#include <KProtocolInfo>
#include <KSharedConfig>
#include <Solid/Device>
#include <Solid/StorageAccess>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrent>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusError>

#include "kiofuse_interface.h"
#endif

using namespace KIO;
class FileDeviceJob : public KIO::Job
{
public:
    FileDeviceJob(const QStringList paths);

    void getDeviceId(const QString &path);
    void slotResult(KJob *job) override;

    QMap<QString, int> m_deviceIdMap;
};

FilePreviewJob::FilePreviewJob(const PreviewItem &item, const QString &thumbRoot, const QMap<QString, KPluginMetaData> &mimeMap)
    : m_item(item)
    , m_size(m_item.size)
    , m_cacheSize(m_item.cacheSize)
    , m_scaleType(m_item.scaleType)
    , m_ignoreMaximumSize(m_item.ignoreMaximumSize)
    , m_sequenceIndex(m_item.sequenceIndex)
    , m_thumbRoot(thumbRoot)
    , m_devicePixelRatio(m_item.devicePixelRatio)
    , m_deviceIdMap(m_item.deviceIdMap)
    , m_preview(QImage())
    , m_mimeMap(mimeMap)
{
}

FilePreviewJob::~FilePreviewJob()
{
    if (!m_tempName.isEmpty()) {
        Q_ASSERT((!QFileInfo(m_tempName).isDir() && QFileInfo(m_tempName).isFile()) || QFileInfo(m_tempName).isSymLink());
        QFile::remove(m_tempName);
        m_tempName.clear();
    }
    if (!m_tempDirPath.isEmpty()) {
        Q_ASSERT(m_tempDirPath.startsWith(QStandardPaths::writableLocation(QStandardPaths::TempLocation)));
        QDir tempDir(m_tempDirPath);
        tempDir.removeRecursively();
    }
}

void FilePreviewJob::start()
{
    // If our deviceIdMap does not have these items, run FilePreviewStatJob to get them
    auto parentDir = parentDirPath(m_item.item.localPath());
    QStringList paths;
    if (!m_deviceIdMap.contains(m_thumbRoot)) {
        paths.append(m_thumbRoot);
    }
    if (!parentDir.isEmpty() && !m_deviceIdMap.contains(parentDir)) {
        paths.append(parentDir);
    }

    if (!paths.isEmpty()) {
        auto *firstJob = new FileDeviceJob(paths);
        connect(firstJob, &KIO::Job::result, this, [this](KJob *job) {
            FileDeviceJob *previewStatJob = static_cast<FileDeviceJob *>(job);
            for (auto item : previewStatJob->m_deviceIdMap.asKeyValueRange()) {
                m_deviceIdMap.insert(item.first, item.second);
            }
            statFile();
        });
        firstJob->start();
    } else {
        statFile();
    }
}

QString FilePreviewJob::parentDirPath(const QString &path) const
{
    if (!path.isEmpty()) {
        // If checked file is directory on a different filesystem than its parent, we need to check it separately
        int separatorIndex = path.lastIndexOf(QLatin1Char('/'));
        // special case for root folders
        const QString parentDirPath = separatorIndex == 0 ? path : path.left(separatorIndex);
        return parentDirPath;
    }
    return path;
}

void FilePreviewJob::statFile()
{
    if (!m_item.item.targetUrl().isValid()) {
        emitResult();
        return;
    }
    // We need to first check the device id's so we can find out if the images can be cached
    QFlags<KIO::StatDetail> details = KIO::StatDefaultDetails | KIO::StatInode;

    if (!m_item.item.isMimeTypeKnown()) {
        details.setFlag(KIO::StatMimeType);
    }

    KIO::Job *statJob = KIO::stat(m_item.item.targetUrl(), StatJob::SourceSide, details, KIO::HideProgressInfo);
    statJob->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
    statJob->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    connect(statJob, &KIO::Job::result, this, &FilePreviewJob::slotStatFile);
    statJob->start();
}

void FilePreviewJob::preparePluginForMimetype(const QString &mimeType)
{
    auto setUpCaching = [this]() {
        short cacheSize = 0;
        const int longer = std::max(m_item.size.width(), m_item.size.height());
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
        int wants = m_item.devicePixelRatio * cacheSize;
        for (const auto &p : pools) {
            if (p.minSize < wants) {
                continue;
            } else {
                thumbDir = p.path;
                break;
            }
        }
        QString thumbPath = m_thumbRoot + thumbDir;
        QDir().mkpath(m_thumbRoot);
        if (!QDir(thumbPath).exists() && !QDir(m_thumbRoot).mkdir(thumbDir, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser)) { // 0700
            qCWarning(KIO_GUI) << "couldn't create thumbnail dir " << thumbPath;
        }
        m_thumbPath = thumbPath;
        m_cacheSize = cacheSize;
    };

    auto pluginIt = m_mimeMap.constFind(mimeType);
    if (pluginIt == m_mimeMap.constEnd()) {
        // check MIME type inheritance, resolve aliases
        QMimeDatabase db;
        const QMimeType mimeInfo = db.mimeTypeForName(mimeType);
        if (mimeInfo.isValid()) {
            const QStringList parentMimeTypes = mimeInfo.allAncestors();
            for (const QString &parentMimeType : parentMimeTypes) {
                pluginIt = m_mimeMap.constFind(parentMimeType);
                if (pluginIt != m_mimeMap.constEnd()) {
                    break;
                }
            }
        }

        if (pluginIt == m_mimeMap.constEnd()) {
            // Check the wildcards last, see BUG 453480
            QString groupMimeType = mimeType;
            const int slashIdx = groupMimeType.indexOf(QLatin1Char('/'));
            if (slashIdx != -1) {
                // Replace everything after '/' with '*'
                groupMimeType.truncate(slashIdx + 1);
                groupMimeType += QLatin1Char('*');
            }
            pluginIt = m_mimeMap.constFind(groupMimeType);
        }
    }

    if (pluginIt != m_mimeMap.constEnd()) {
        const KPluginMetaData plugin = *pluginIt;

        if (!plugin.isValid()) {
            qCDebug(KIO_GUI) << "Plugin for item " << m_item.item << " is not valid. Emitting result.";
            emitResult();
            return;
        }

        m_standardThumbnailer = plugin.description() == QStringLiteral("standardthumbnailer");
        m_plugin = plugin;
        m_thumbnailWorkerMetaData.insert(QStringLiteral("handlesSequences"), QString::number(m_plugin.value(QStringLiteral("HandleSequences"), false)));

        if (m_item.scaleType == PreviewJob::ScaleType::ScaledAndCached && plugin.value(QStringLiteral("CacheThumbnail"), true)) {
            const QUrl url = m_item.item.targetUrl();
            if (!url.isLocalFile() || !url.adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(m_thumbRoot)) {
                setUpCaching();
            }
        }
    } else {
        qCDebug(KIO_GUI) << "Could not get plugin for, " << m_item.item << " - emitting result.";
        emitResult();
        return;
    }
}

void FilePreviewJob::slotStatFile(KJob *job)
{
    if (job->error()) {
        qCDebug(KIO_GUI) << "Job stat failed" << job->errorString();
        emitResult();
        return;
    }
    bool isLocal;

    const QUrl itemUrl = m_item.item.mostLocalUrl(&isLocal);
    const KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
    const KIO::UDSEntry statResult = statJob->statResult();
    m_currentDeviceId = statResult.numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
    m_tOrig = QDateTime::fromSecsSinceEpoch(statResult.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0));

    if (m_item.item.isMimeTypeKnown()) {
        preparePluginForMimetype(m_item.item.mimetype());
    } else {
        preparePluginForMimetype(statResult.stringValue(KIO::UDSEntry::UDS_MIME_TYPE));
    }

    if (isLocal) {
        const QFileInfo localFile(itemUrl.toLocalFile());
        const QString canonicalPath = localFile.canonicalFilePath();
        m_origName = QUrl::fromLocalFile(canonicalPath).toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
        if (m_origName.isEmpty()) {
            qCDebug(KIO_GUI) << "Failed to convert" << itemUrl << "to canonical path, possibly a broken symlink";
            emitResult();
        }
    } else {
        // Don't include the password if any
        m_origName = m_item.item.targetUrl().toEncoded(QUrl::RemovePassword);
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(m_origName);
    m_thumbName = QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");

    const KIO::filesize_t size = static_cast<KIO::filesize_t>(statResult.numberValue(KIO::UDSEntry::UDS_SIZE, 0));
    if (size == 0) {
        qCDebug(KIO_GUI) << "FilePreviewJob: skipping an empty file, migth be a broken symlink" << m_item.item.url();
        emitResult();
        return;
    }

    bool skipCurrentItem = false;
    const KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("PreviewSettings"));
    if ((itemUrl.isLocalFile() || KProtocolInfo::protocolClass(itemUrl.scheme()) == QLatin1String(":local")) && !m_item.item.isSlow()) {
        const KIO::filesize_t maximumLocalSize = cg.readEntry("MaximumSize", std::numeric_limits<KIO::filesize_t>::max());
        skipCurrentItem = !m_ignoreMaximumSize && size > maximumLocalSize && !m_plugin.value(QStringLiteral("IgnoreMaximumSize"), false);
    } else {
        // For remote items the "IgnoreMaximumSize" plugin property is not respected
        // Also we need to check if remote (but locally mounted) folder preview is enabled
        const KIO::filesize_t maximumRemoteSize = cg.readEntry<KIO::filesize_t>("MaximumRemoteSize", 0);
        const bool enableRemoteFolderThumbnail = cg.readEntry("EnableRemoteFolderThumbnail", false);
        skipCurrentItem = (!m_ignoreMaximumSize && size > maximumRemoteSize) || (m_item.item.isDir() && !enableRemoteFolderThumbnail);
    }
    if (skipCurrentItem) {
        emitResult();
        return;
    }

    bool pluginHandlesSequences = m_plugin.value(QStringLiteral("HandleSequences"), false);
    if (!m_plugin.value(QStringLiteral("CacheThumbnail"), true) || (m_sequenceIndex && pluginHandlesSequences) || m_thumbPath.isEmpty()) {
        // This preview will not be cached, no need to look for a saved thumbnail
        // Just create it, and be done
        getOrCreateThumbnail();
        return;
    }

    auto watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher]() {
        watcher->deleteLater();
        QImage thumb = watcher->result();
        if (isCacheValid(thumb)) {
            emitPreview(thumb);
            emitResult();
        } else {
            getOrCreateThumbnail();
        }

    });
    QFuture<QImage> future = QtConcurrent::run(loadThumbnailFromCache, QString(m_thumbPath + m_thumbName), m_devicePixelRatio);

    watcher->setFuture(future);
}

QImage FilePreviewJob::loadThumbnailFromCache(const QString &path, qreal dpr)
{
    QImage thumb;
    QFile thumbFile(path);
    if (!thumbFile.open(QIODevice::ReadOnly) || !thumb.load(&thumbFile, "png")) {
        return QImage();
    }
    // The DPR of the loaded thumbnail is unspecified (and typically irrelevant).
    // When a thumbnail is DPR-invariant, use the DPR passed in the request.
    thumb.setDevicePixelRatio(dpr);
    return thumb;
}

bool FilePreviewJob::isCacheValid(const QImage &thumb)
{
    if (thumb.isNull()) {
        return false;
    }
    if (thumb.text(QStringLiteral("Thumb::URI")) != QString::fromUtf8(m_origName)
        || thumb.text(QStringLiteral("Thumb::MTime")).toLongLong() != m_tOrig.toSecsSinceEpoch()) {
        return false;
    }

    const QString origSize = thumb.text(QStringLiteral("Thumb::Size"));
    if (!origSize.isEmpty() && origSize.toULongLong() != m_item.item.size()) {
        // Thumb::Size is not required, but if it is set it should match
        return false;
    }

    QString thumbnailerVersion = m_plugin.value(QStringLiteral("ThumbnailerVersion"));

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
    return true;
}

void FilePreviewJob::getOrCreateThumbnail()
{
    // We still need to load the orig file ! (This is getting tedious) :)
    const KFileItem &item = m_item.item;
    const QString localPath = item.localPath();
    if (!localPath.isEmpty()) {
        createThumbnail(localPath);
        return;
    }

    if (item.isDir() || !KProtocolInfo::isKnownProtocol(item.targetUrl().scheme())) {
        // Skip remote dirs (bug 208625)
        emitResult();
        return;
    }
    // The plugin does not support this remote content, either copy the
    // file, or try to get a local path using KIOFuse
    if (m_tryKioFuse) {
        createThumbnailViaFuse(item.targetUrl(), item.mostLocalUrl());
        return;
    }

    createThumbnailViaLocalCopy(item.mostLocalUrl());
}

void FilePreviewJob::createThumbnailViaFuse(const QUrl &fileUrl, const QUrl &localUrl)
{
#if defined(WITH_QTDBUS) && !defined(Q_OS_ANDROID)
    org::kde::KIOFuse::VFS kiofuse_iface(QStringLiteral("org.kde.KIOFuse"), QStringLiteral("/org/kde/KIOFuse"), QDBusConnection::sessionBus());
    kiofuse_iface.setTimeout(s_kioFuseMountTimeout);
    QDBusPendingReply<QString> reply = kiofuse_iface.mountUrl(fileUrl.toString());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);

    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, localUrl](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QString> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            // Don't try kio-fuse again if it is not available
            if (reply.error().type() == QDBusError::ServiceUnknown || reply.error().type() == QDBusError::NoReply) {
                m_tryKioFuse = false;
            }

            // Fall back to copying the file to the local machine
            createThumbnailViaLocalCopy(localUrl);
        } else {
            // Use file exposed via the local fuse mount point
            createThumbnail(reply.value());
        }
    });
#else
    createThumbnailViaLocalCopy(localUrl);
#endif
}

void FilePreviewJob::slotGetOrCreateThumbnail(KJob *job)
{
    auto fileCopyJob = static_cast<KIO::FileCopyJob *>(job);
    if (fileCopyJob) {
        auto pixPath = fileCopyJob->destUrl().toLocalFile();
        if (!pixPath.isEmpty()) {
            createThumbnail(pixPath);
            return;
        }
    }
    emitResult();
}

void FilePreviewJob::createThumbnailViaLocalCopy(const QUrl &url)
{
    // Only download for the first sequence
    if (m_sequenceIndex) {
        emitResult();
        return;
    }
    // No plugin support access to this remote content, copy the file
    // to the local machine, then create the thumbnail
    const KFileItem &item = m_item.item;

    // Build the destination filename: ~/.cache/app/kpreviewjob/pid/UUID.extension
    QString krun_writable =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/kpreviewjob/%1/").arg(QCoreApplication::applicationPid());
    if (!QDir().mkpath(krun_writable)) {
        qCWarning(KIO_GUI) << "Could not create a cache folder for preview creation:" << krun_writable;
        emitResult();
        return;
    }
    m_tempName = QStringLiteral("%1%2.%3").arg(krun_writable, QUuid(item.mostLocalUrl().toString()).createUuid().toString(QUuid::WithoutBraces), item.suffix());

    KIO::Job *job = KIO::file_copy(url, QUrl::fromLocalFile(m_tempName), -1, KIO::Overwrite | KIO::HideProgressInfo /* No GUI */);
    job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
    connect(job, &KIO::FileCopyJob::result, this, &FilePreviewJob::slotGetOrCreateThumbnail);
    job->start();
}

FilePreviewJob::CachePolicy FilePreviewJob::canBeCached(const QString &path)
{
    const QString parentDir = parentDirPath(path);

    int parentId = getDeviceId(parentDir);
    if (parentId == m_idUnknown) {
        return CachePolicy::Unknown;
    }

    bool isDifferentSystem = !parentId || parentId != m_currentDeviceId;
    if (!isDifferentSystem && m_currentDeviceCachePolicy != CachePolicy::Unknown) {
        return m_currentDeviceCachePolicy;
    }
    int checkedId;
    QString checkedPath;
    if (isDifferentSystem) {
        checkedId = m_currentDeviceId;
        checkedPath = path;
    } else {
        checkedId = getDeviceId(parentDir);
        checkedPath = parentDir;
        if (checkedId == m_idUnknown) {
            return CachePolicy::Unknown;
        }
    }
    // If we're checking different filesystem or haven't checked yet see if filesystem matches thumbRoot
    int thumbRootId = getDeviceId(m_thumbRoot);
    if (thumbRootId == m_idUnknown) {
        return CachePolicy::Unknown;
    }
    bool shouldAllow = checkedId && checkedId == thumbRootId;
    if (!shouldAllow) {
        Solid::Device device = Solid::Device::storageAccessFromPath(checkedPath);
        if (device.isValid()) {
            // If the checked device is encrypted, allow thumbnailing if the thumbnails are stored in an encrypted location.
            // Or, if the checked device is unencrypted, allow thumbnailing.
            if (device.as<Solid::StorageAccess>()->isEncrypted()) {
                const Solid::Device thumbRootDevice = Solid::Device::storageAccessFromPath(m_thumbRoot);
                shouldAllow = thumbRootDevice.isValid() && thumbRootDevice.as<Solid::StorageAccess>()->isEncrypted();
            } else {
                shouldAllow = true;
            }
        }
    }
    if (!isDifferentSystem) {
        m_currentDeviceCachePolicy = shouldAllow ? CachePolicy::Allow : CachePolicy::Prevent;
    }
    return shouldAllow ? CachePolicy::Allow : CachePolicy::Prevent;
}

int FilePreviewJob::getDeviceId(const QString &path)
{
    auto iter = m_deviceIdMap.find(path);
    if (iter != m_deviceIdMap.end()) {
        return iter.value();
    }
    QUrl url = QUrl::fromLocalFile(path);
    if (!url.isValid()) {
        qCWarning(KIO_GUI) << "Could not get device id for file preview, Invalid url" << path;
        return 0;
    }
    return m_idUnknown;
}

void FilePreviewJob::createThumbnail(const QString &pixPath)
{
    QFileInfo info(pixPath);
    Q_ASSERT_X(info.isAbsolute(), "PreviewJobPrivate::createThumbnail", qPrintable(QLatin1String("path is not absolute: ") + info.path()));

    bool save = m_scaleType == PreviewJob::ScaledAndCached && m_plugin.value(QStringLiteral("CacheThumbnail"), true) && !m_sequenceIndex;

    bool isRemoteProtocol = m_item.item.localPath().isEmpty();
    m_currentDeviceCachePolicy = isRemoteProtocol ? CachePolicy::Allow : canBeCached(pixPath);

    if (m_currentDeviceCachePolicy == CachePolicy::Unknown) {
        emitResult();
        return;
    }

    if (m_standardThumbnailer) {
        if (m_tempDirPath.isEmpty()) {
            auto tempDir = QTemporaryDir();
            Q_ASSERT(tempDir.isValid());

            tempDir.setAutoRemove(false);
            // restrict read access to current User
            QFile::setPermissions(tempDir.path(), QFile::Permission::ReadOwner | QFile::Permission::WriteOwner | QFile::Permission::ExeOwner);

            m_tempDirPath = tempDir.path();
        }

        if (pixPath.startsWith(m_tempDirPath)) {
            // don't generate thumbnails for images already in temporary directory
            emitResult();
            return;
        }

        KIO::StandardThumbnailJob *job = new KIO::StandardThumbnailJob(m_plugin.value(u"Exec"), m_size.width() * m_devicePixelRatio, pixPath, m_tempDirPath);
        connect(job, &KIO::StandardThumbnailJob::data, this, [=, this](KIO::Job *job, const QImage &thumb) {
            slotStandardThumbData(job, thumb);
        });
        connect(job, &KIO::StandardThumbnailJob::result, this, &FilePreviewJob::emitResult);
        job->start();
        return;
    }

    // Using thumbnailer plugin
    QUrl thumbURL;
    thumbURL.setScheme(QStringLiteral("thumbnail"));
    thumbURL.setPath(pixPath);
    KIO::TransferJob *job = KIO::get(thumbURL, NoReload, HideProgressInfo);
    connect(job, &KIO::TransferJob::data, this, [this](KIO::Job *job, const QByteArray &data) {
        slotThumbData(job, data);
    });
    connect(job, &KIO::TransferJob::result, this, &FilePreviewJob::emitResult);
    int thumb_width = m_size.width();
    int thumb_height = m_size.height();
    if (save) {
        thumb_width = thumb_height = m_cacheSize;
    }

    job->addMetaData(QStringLiteral("mimeType"), m_item.item.mimetype());
    job->addMetaData(QStringLiteral("width"), QString::number(thumb_width));
    job->addMetaData(QStringLiteral("height"), QString::number(thumb_height));
    job->addMetaData(QStringLiteral("plugin"), m_plugin.fileName());
    job->addMetaData(QStringLiteral("enabledPlugins"), m_enabledPlugins.join(QLatin1Char(',')));
    job->addMetaData(QStringLiteral("devicePixelRatio"), QString::number(m_devicePixelRatio));
    job->addMetaData(QStringLiteral("cache"), QString::number(m_currentDeviceCachePolicy == CachePolicy::Allow));
    if (m_sequenceIndex) {
        job->addMetaData(QStringLiteral("sequence-index"), QString::number(m_sequenceIndex));
    }

    size_t requiredSize = thumb_width * m_devicePixelRatio * thumb_height * m_devicePixelRatio * 4;
    m_shm = SHM::create(requiredSize);

    if (m_shm) {
        job->addMetaData(QStringLiteral("shmid"), QString::number(m_shm->id()));
    }

    job->start();
}

void FilePreviewJob::slotStandardThumbData(KIO::Job *job, const QImage &thumbData)
{
    m_thumbnailWorkerMetaData = job->metaData();

    if (thumbData.isNull()) {
        // let succeeded in false state
        // failed will get called in determineNextFile()
        emitResult();
        return;
    }

    QImage thumb = thumbData;
    saveThumbnailData(thumb);

    emitPreview(thumb);
}

void FilePreviewJob::slotThumbData(KIO::Job *job, const QByteArray &data)
{
    QImage thumb;
    // Keep this in sync with kio-extras|thumbnail/thumbnail.cpp
    QDataStream str(data);

    int width;
    int height;
    QImage::Format format;
    qreal imgDevicePixelRatio;
    // TODO KF7: add a version number as first parameter
    // always read those, even when !WITH_SHM, because the other side always writes them
    str >> width >> height >> format >> imgDevicePixelRatio;

    if (m_shm) {
        thumb = QImage(m_shm->address(), width, height, format).copy();
        thumb.setDevicePixelRatio(imgDevicePixelRatio);
    }

    if (thumb.isNull()) {
        // fallback a raw QImage
        str >> thumb;
        thumb.setDevicePixelRatio(imgDevicePixelRatio);
    }

    slotStandardThumbData(job, thumb);
}

void FilePreviewJob::saveThumbnailData(QImage &thumb)
{
    const bool save = m_scaleType == PreviewJob::ScaledAndCached && !m_sequenceIndex && m_currentDeviceCachePolicy == CachePolicy::Allow
        && m_plugin.value(QStringLiteral("CacheThumbnail"), true)
        && (!m_item.item.targetUrl().isLocalFile() || !m_item.item.targetUrl().adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(m_thumbRoot));

    if (save) {
        thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(m_origName));
        thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(m_tOrig.toSecsSinceEpoch()));
        thumb.setText(QStringLiteral("Thumb::Size"), number(m_item.item.size()));
        thumb.setText(QStringLiteral("Thumb::Mimetype"), m_item.item.mimetype());
        QString thumbnailerVersion = m_plugin.value(QStringLiteral("ThumbnailerVersion"));
        QString signature = QLatin1String("KDE Thumbnail Generator ") + m_plugin.name();
        if (!thumbnailerVersion.isEmpty()) {
            signature.append(QLatin1String(" (v") + thumbnailerVersion + QLatin1Char(')'));
        }
        thumb.setText(QStringLiteral("Software"), signature);
        // we don't need to block for the saving to complete, it can run in it's own time
        QFuture<void> future = QtConcurrent::run(saveThumbnailToCache, thumb, QString(m_thumbPath + m_thumbName));
    }
}

void FilePreviewJob::saveThumbnailToCache(const QImage &thumb, const QString &path)
{
    QEventLoopLocker lock; // stop the application from quitting until we finish
    QSaveFile saveFile(path);
    if (saveFile.open(QIODevice::WriteOnly)) {
        if (thumb.save(&saveFile, "PNG")) {
            saveFile.commit();
        }
    }
}

void FilePreviewJob::emitPreview(const QImage &thumb)
{
    const qreal ratio = thumb.devicePixelRatio();

    QImage preview = thumb;
    if (preview.width() > m_size.width() * ratio || preview.height() > m_size.height() * ratio) {
        preview = preview.scaled(QSize(m_size.width() * ratio, m_size.height() * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    m_preview = preview;
    emitResult();
}

QList<KPluginMetaData> FilePreviewJob::loadAvailablePlugins()
{
    static QList<KPluginMetaData> jsonMetaDataPlugins;
    if (jsonMetaDataPlugins.isEmpty()) {
        // Insert plugins first so they take precedence over standard thumbnailers
        jsonMetaDataPlugins = KPluginMetaData::findPlugins(QStringLiteral("kf6/thumbcreator"));
        jsonMetaDataPlugins << standardThumbnailers();
    }
    return jsonMetaDataPlugins;
}

QList<KPluginMetaData> FilePreviewJob::standardThumbnailers()
{
    static QList<KPluginMetaData> standardThumbs;
    if (standardThumbs.empty()) {
        const QStringList dirs =
            QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("thumbnailers/"), QStandardPaths::LocateDirectory);
        const auto thumbnailerPaths = KFileUtils::findAllUniqueFiles(dirs, QStringList{QStringLiteral("*.thumbnailer")});
        for (const QString &thumbnailerPath : thumbnailerPaths) {
            const KConfig thumbnailerFile(thumbnailerPath);
            const KConfigGroup thumbnailerConfig = thumbnailerFile.group(QStringLiteral("Thumbnailer Entry"));
            const QStringList mimetypes = thumbnailerConfig.readXdgListEntry("MimeType");
            const QString exec = thumbnailerConfig.readEntry("Exec", QString{});

            if (exec.isEmpty() || mimetypes.isEmpty()) {
                continue;
            }

            QMimeDatabase db;
            // We only need the first mimetype since the names/comments are often shared between multiple types
            auto mime = db.mimeTypeForName(mimetypes.first());
            auto name = mime.name().isEmpty() ? mimetypes.first() : mime.name();
            if (!mime.comment().isEmpty()) {
                name = mime.comment();
            }

            // the plugin metadata
            QJsonObject kplugin;
            kplugin[QStringLiteral("Id")] = QFileInfo(thumbnailerPath).completeBaseName();
            kplugin[QStringLiteral("MimeTypes")] = QJsonArray::fromStringList(mimetypes);
            kplugin[QStringLiteral("Name")] = name;
            kplugin[QStringLiteral("Description")] = QStringLiteral("standardthumbnailer");

            QJsonObject root;
            root[QStringLiteral("CacheThumbnail")] = true;
            root[QStringLiteral("Exec")] = exec;
            root[QStringLiteral("KPlugin")] = kplugin;

            KPluginMetaData standardThumbnailerPlugin(root, QString());
            standardThumbs.append(standardThumbnailerPlugin);
        }
    }
    return standardThumbs;
}

QMap<QString, QString> FilePreviewJob::thumbnailWorkerMetaData() const
{
    return m_thumbnailWorkerMetaData;
}

QMap<QString, int> FilePreviewJob::deviceIdMap() const
{
    return m_deviceIdMap;
}

QImage FilePreviewJob::previewImage() const
{
    return m_preview;
}

PreviewItem FilePreviewJob::item() const
{
    return m_item;
}

// Stat multiple files at same time
FileDeviceJob::FileDeviceJob(const QStringList paths)
{
    for (const QString &path : paths) {
        getDeviceId(path);
    }
}

void FileDeviceJob::slotResult(KJob *job)
{
    KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
    int id;
    QString path = statJob->url().toLocalFile();
    if (job->error()) {
        // We set id to 0 to know we tried getting it
        qCDebug(KIO_GUI) << "Cannot read information about filesystem under path" << path;
        id = 0;
    } else {
        id = statJob->statResult().numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
    }
    if (!path.isEmpty()) {
        m_deviceIdMap[path] = id;
    }
    removeSubjob(job);
    if (!hasSubjobs()) {
        emitResult();
    }
}

void FileDeviceJob::getDeviceId(const QString &path)
{
    QUrl url = QUrl::fromLocalFile(path);
    KIO::Job *job = KIO::stat(url, StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
    job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    addSubjob(job);
}

std::unique_ptr<SHM> SHM::create(int size)
{
#if WITH_SHM
    int id = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);

    if (id == -1) {
        return nullptr;
    }

    uchar *address = (uchar *)(shmat(id, nullptr, SHM_RDONLY));

    if (address == (uchar *)-1) {
        shmctl(id, IPC_RMID, nullptr);
        return nullptr;
    }

    return std::make_unique<SHM>(id, address);
#else
    return nullptr;
#endif
}

int SHM::id() const
{
    return m_id;
}

uchar *SHM::address() const
{
    return m_address;
}

SHM::~SHM()
{
#if WITH_SHM
    shmdt((char *)m_address);
    shmctl(m_id, IPC_RMID, nullptr);
#endif
}

SHM::SHM(int id, uchar *address)
    : m_id(id)
    , m_address(address)
{
}

#include "moc_filepreviewjob.cpp"
