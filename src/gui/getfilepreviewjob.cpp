
#include "filecopyjob.h"
#include "getfilepreviewjob_p.h"
#include "kiogui_debug.h"
#include "previewjob.h"
#include "standardthumbnailjob_p.h"
#include "statjob.h"

#if defined(Q_OS_UNIX) && !defined(Q_OS_ANDROID) && !defined(Q_OS_HAIKU)
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
#include <QUuid>

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

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusReply>

#include "kiofuse_interface.h"
#endif

namespace
{
static qreal s_defaultDevicePixelRatio = 1.0;
// Time (in milliseconds) to wait for kio-fuse in a PreviewJob before giving up.
static constexpr int s_kioFuseMountTimeout = 10000;
}

using namespace KIO;

GetFilePreviewJob::GetFilePreviewJob(const PreviewItem &item)
    : m_currentItem(item)
    , m_thumbPath(QString())
    , m_size(QSize())
    , m_cacheSize(0)
    , m_scaleType(PreviewJob::ScaleType::ScaledAndCached)
    , m_ignoreMaximumSize(false)
    , m_sequenceIndex(0)
    , m_succeeded(false)
    , m_maximumLocalSize(0)
    , m_maximumRemoteSize(0)
    , m_enableRemoteFolderThumbnail(false)
    , m_shmid(-1)
    , m_shmaddr(nullptr)
{
    // https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
    m_thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");

    m_thumbPath = item.thumbPath;
    m_size = item.size;
    m_scaleType = item.scaleType;
    m_ignoreMaximumSize = item.ignoreMaximumSize;
    m_sequenceIndex = item.sequenceIndex;
    m_devicePixelRatio = item.devicePixelRatio;

#if WITH_SHM
    size_t requiredSize = m_size.width() * m_devicePixelRatio * m_size.height() * m_devicePixelRatio * 4;
    if (m_shmid == -1 || m_shmsize < requiredSize) {
        if (m_shmaddr) {
            // clean previous shared memory segment
            shmdt((char *)m_shmaddr);
            m_shmaddr = nullptr;
            shmctl(m_shmid, IPC_RMID, nullptr);
            m_shmid = -1;
        }
        if (requiredSize > 0) {
            m_shmid = shmget(IPC_PRIVATE, requiredSize, IPC_CREAT | 0600);
            if (m_shmid != -1) {
                m_shmsize = requiredSize;
                m_shmaddr = (uchar *)(shmat(m_shmid, nullptr, SHM_RDONLY));
                if (m_shmaddr == (uchar *)-1) {
                    shmctl(m_shmid, IPC_RMID, nullptr);
                    m_shmaddr = nullptr;
                    m_shmid = -1;
                }
            }
        }
    }
#endif
}

GetFilePreviewJob::~GetFilePreviewJob()
{
#if WITH_SHM
    if (m_shmaddr) {
        shmdt((char *)m_shmaddr);
        shmctl(m_shmid, IPC_RMID, nullptr);
    }
#endif
}

/*
- After stat, we either skip file or getOrCreateThumbnail
- OR
- If we get original, take it
- If we need to create new one, check if caching is done
    - If caching is done, run the jobs for it
    - Then create thumbnail
- Then return it
*/

void GetFilePreviewJob::statFile()
{
    qWarning() << "Stat file" << m_currentItem.item.targetUrl();
    KIO::Job *job = KIO::stat(m_currentItem.item.targetUrl(), StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
    job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
    job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    connect(job, &KIO::Job::result, this, &GetFilePreviewJob::slotStatFile);
    addSubjob(job);
}

void GetFilePreviewJob::slotStatFile(KJob *job)
{
    qWarning() << "slotStatFile";
    if (job->error()) {
        qCWarning(KIO_GUI) << "Job failed" << job->errorString();
        return;
    }
    const QUrl itemUrl = m_currentItem.item.mostLocalUrl();
    const KIO::StatJob *statJob = static_cast<KIO::StatJob *>(job);
    const KIO::UDSEntry statResult = statJob->statResult();
    m_currentDeviceId = statResult.numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
    m_tOrig = QDateTime::fromSecsSinceEpoch(statResult.numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME, 0));
    int id = statJob->statResult().numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
    m_deviceIdMap[statJob->url().toLocalFile()] = id;

    const KIO::filesize_t size = static_cast<KIO::filesize_t>(statResult.numberValue(KIO::UDSEntry::UDS_SIZE, 0));
    if (size == 0) {
        qCWarning(KIO_GUI) << "GetFilePreviewJob: skipping an empty file, migth be a broken symlink" << m_currentItem.item.url();
        return;
    }

    bool skipCurrentItem = false;
    if ((itemUrl.isLocalFile() || KProtocolInfo::protocolClass(itemUrl.scheme()) == QLatin1String(":local")) && !m_currentItem.item.isSlow()) {
        skipCurrentItem = !m_ignoreMaximumSize && size > m_maximumLocalSize && !m_currentItem.plugin.value(QStringLiteral("IgnoreMaximumSize"), false);
    } else {
        // For remote items the "IgnoreMaximumSize" plugin property is not respected
        // Also we need to check if remote (but locally mounted) folder preview is enabled
        skipCurrentItem = (!m_ignoreMaximumSize && size > m_maximumRemoteSize) || (m_currentItem.item.isDir() && !m_enableRemoteFolderThumbnail);
    }
    if (skipCurrentItem) {
        return;
    }

    bool pluginHandlesSequences = m_currentItem.plugin.value(QStringLiteral("HandleSequences"), false);
    if (!m_currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) || (m_sequenceIndex && pluginHandlesSequences)) {
        // This preview will not be cached, no need to look for a saved thumbnail
        // Just create it, and be done
        getOrCreateThumbnail();
        return;
    }

    if (statResultThumbnail()) {
        m_succeeded = true;
        return;
    }

    getOrCreateThumbnail();
    return;
}

void GetFilePreviewJob::slotResult(KJob *job)
{
    qWarning() << "Result of " << job;
    removeSubjob(job);
    if (job->error()) {
        qCWarning(KIO_GUI) << "Job failed" << job->errorString();
    }
    qWarning() << m_currentItem.item.url() << " = " << m_thumbPath + m_thumbName;
    emitResult();
}

void GetFilePreviewJob::cleanupTempFile()
{
    if (!m_tempName.isEmpty()) {
        Q_ASSERT((!QFileInfo(m_tempName).isDir() && QFileInfo(m_tempName).isFile()) || QFileInfo(m_tempName).isSymLink());
        QFile::remove(m_tempName);
        m_tempName.clear();
    }
}

bool GetFilePreviewJob::statResultThumbnail()
{
    qWarning() << "statResultThumbnail" << m_thumbPath;
    if (m_thumbPath.isEmpty()) {
        qWarning() << "1";
        return false;
    }

    bool isLocal;
    const QUrl url = m_currentItem.item.mostLocalUrl(&isLocal);
    if (isLocal) {
        const QFileInfo localFile(url.toLocalFile());
        const QString canonicalPath = localFile.canonicalFilePath();
        m_origName = QUrl::fromLocalFile(canonicalPath).toEncoded(QUrl::RemovePassword | QUrl::FullyEncoded);
        if (m_origName.isEmpty()) {
            qCDebug(KIO_GUI) << "Failed to convert" << url << "to canonical path, possibly a broken symlink";
            qWarning() << "2";
            return false;
        }
    } else {
        // Don't include the password if any
        m_origName = m_currentItem.item.targetUrl().toEncoded(QUrl::RemovePassword);
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(m_origName);
    m_thumbName = QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");

    QImage thumb;
    QFile thumbFile(m_thumbPath + m_thumbName);
    qWarning() << m_thumbPath << m_thumbName;
    if (!thumbFile.open(QIODevice::ReadOnly) || !thumb.load(&thumbFile, "png")) {
        qWarning() << "3";
        return false;
    }

    if (thumb.text(QStringLiteral("Thumb::URI")) != QString::fromUtf8(m_origName)
        || thumb.text(QStringLiteral("Thumb::MTime")).toLongLong() != m_tOrig.toSecsSinceEpoch()) {
        qWarning() << "4";
        return false;
    }

    const QString origSize = thumb.text(QStringLiteral("Thumb::Size"));
    if (!origSize.isEmpty() && origSize.toULongLong() != m_currentItem.item.size()) {
        // Thumb::Size is not required, but if it is set it should match
        qWarning() << "5";
        return false;
    }

    // The DPR of the loaded thumbnail is unspecified (and typically irrelevant).
    // When a thumbnail is DPR-invariant, use the DPR passed in the request.
    thumb.setDevicePixelRatio(m_devicePixelRatio);

    QString thumbnailerVersion = m_currentItem.plugin.value(QStringLiteral("ThumbnailerVersion"));

    if (!thumbnailerVersion.isEmpty() && thumb.text(QStringLiteral("Software")).startsWith(QLatin1String("KDE Thumbnail Generator"))) {
        // Check if the version matches
        // The software string should read "KDE Thumbnail Generator pluginName (vX)"
        QString softwareString = thumb.text(QStringLiteral("Software")).remove(QStringLiteral("KDE Thumbnail Generator")).trimmed();
        if (softwareString.isEmpty()) {
            // The thumbnail has been created with an older version, recreating
            qWarning() << "6";
            return false;
        }
        int versionIndex = softwareString.lastIndexOf(QLatin1String("(v"));
        if (versionIndex < 0) {
            qWarning() << "7";
            return false;
        }

        QString cachedVersion = softwareString.remove(0, versionIndex + 2);
        cachedVersion.chop(1);
        uint thumbnailerMajor = thumbnailerVersion.toInt();
        uint cachedMajor = cachedVersion.toInt();
        if (thumbnailerMajor > cachedMajor) {
            qWarning() << "8";
            return false;
        }
    }

    // Found it, use it
    emitPreview(thumb);
    return true;
}

void GetFilePreviewJob::getOrCreateThumbnail()
{
    qWarning() << "getOrCreateThumbnail";
    // We still need to load the orig file ! (This is getting tedious) :)
    const KFileItem &item = m_currentItem.item;
    const QString localPath = item.localPath();
    if (!localPath.isEmpty()) {
        createThumbnail(localPath);
        return;
    }

    if (item.isDir() || !KProtocolInfo::isKnownProtocol(item.targetUrl().scheme())) {
        // Skip remote dirs (bug 208625)
        cleanupTempFile();
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

void GetFilePreviewJob::createThumbnailViaFuse(const QUrl &fileUrl, const QUrl &localUrl)
{
#if defined(WITH_QTDBUS) && !defined(Q_OS_ANDROID)
    qWarning() << "createThumbnailViaFuse";
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

void GetFilePreviewJob::slotGetOrCreateThumbnail(KJob *job)
{
    auto fileCopyJob = static_cast<KIO::FileCopyJob *>(job);
    if (fileCopyJob) {
        auto pixPath = fileCopyJob->destUrl().toLocalFile();
        if (!pixPath.isEmpty()) {
            createThumbnail(pixPath);
        }
    }
}

void GetFilePreviewJob::createThumbnailViaLocalCopy(const QUrl &url)
{
    // Only download for the first sequence
    if (m_sequenceIndex) {
        cleanupTempFile();
        // determineNextFile();
        return;
    }
    qWarning() << "createThumbnailViaLocalCopy";
    // No plugin support access to this remote content, copy the file
    // to the local machine, then create the thumbnail
    // state = PreviewJobPrivate::STATE_GETORIG;
    const KFileItem &item = m_currentItem.item;

    // Build the destination filename: ~/.cache/app/kpreviewjob/pid/UUID.extension
    QString krun_writable =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/kpreviewjob/%1/").arg(QCoreApplication::applicationPid());
    if (!QDir().mkpath(krun_writable)) {
        qCWarning(KIO_GUI) << "Could not create a cache folder for preview creation:" << krun_writable;
        cleanupTempFile();
        // determineNextFile();
        return;
    }
    m_tempName =
        QStringLiteral("%1%2.%3").arg(krun_writable).arg(QUuid(item.mostLocalUrl().toString()).createUuid().toString(QUuid::WithoutBraces)).arg(item.suffix());

    KIO::Job *job = KIO::file_copy(url, QUrl::fromLocalFile(m_tempName), -1, KIO::Overwrite | KIO::HideProgressInfo /* No GUI */);
    job->addMetaData(QStringLiteral("thumbnail"), QStringLiteral("1"));
    connect(job, &KIO::FileCopyJob::result, this, &GetFilePreviewJob::slotGetOrCreateThumbnail);
    job->start();
}

GetFilePreviewJob::CachePolicy GetFilePreviewJob::canBeCached(const QString &path)
{
    // If checked file is directory on a different filesystem than its parent, we need to check it separately
    int separatorIndex = path.lastIndexOf(QLatin1Char('/'));
    // special case for root folders
    const QString parentDirPath = separatorIndex == 0 ? path : path.left(separatorIndex);

    int parentId = getDeviceId(parentDirPath);
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
        checkedId = getDeviceId(parentDirPath);
        checkedPath = parentDirPath;
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

int GetFilePreviewJob::getDeviceId(const QString &path)
{
    auto iter = m_deviceIdMap.find(path);
    if (iter != m_deviceIdMap.end()) {
        return iter.value();
    }
    QUrl url = QUrl::fromLocalFile(path);
    if (!url.isValid()) {
        qCWarning(KIO_GUI) << "Could not get device id for file preview, Invalid url" << path;
    }
    return 0;
}

QDir GetFilePreviewJob::createTemporaryDir()
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

void GetFilePreviewJob::createThumbnail(const QString &pixPath)
{
    qWarning() << "Create thumbnail";
    QFileInfo info(pixPath);
    Q_ASSERT_X(info.isAbsolute(), "PreviewJobPrivate::createThumbnail", qPrintable(QLatin1String("path is not absolute: ") + info.path()));

    // state = PreviewJobPrivate::STATE_CREATETHUMB;

    bool save = m_scaleType == PreviewJob::ScaledAndCached && m_currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) && !m_sequenceIndex;

    qWarning() << "save" << save << " - " << m_scaleType << m_currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true) << m_sequenceIndex;

    bool isRemoteProtocol = m_currentItem.item.localPath().isEmpty();
    m_currentDeviceCachePolicy = isRemoteProtocol ? CachePolicy::Allow : canBeCached(pixPath);

    if (m_currentDeviceCachePolicy == CachePolicy::Unknown) {
        // If Unknown is returned, creating thumbnail should be called again by slotResult
        return;
    }

    if (m_currentItem.standardThumbnailer) {
        // Using /usr/share/thumbnailers
        QString exec;
        for (const auto &thumbnailer : standardThumbnailers().asKeyValueRange()) {
            for (const auto &mimetype : std::as_const(thumbnailer.second.mimetypes)) {
                if (m_currentItem.plugin.supportsMimeType(mimetype)) {
                    exec = thumbnailer.second.exec;
                }
            }
        }
        if (exec.isEmpty()) {
            qCWarning(KIO_GUI) << "The exec entry for standard thumbnailer " << m_currentItem.plugin.name() << " was empty!";
            return;
        }
        auto tempDir = createTemporaryDir();
        if (pixPath.startsWith(tempDir.path())) {
            // don't generate thumbnails for images already in temporary directory
            return;
        }

        KIO::StandardThumbnailJob *job = new KIO::StandardThumbnailJob(exec, m_size.width() * m_devicePixelRatio, pixPath, tempDir.path());
        addSubjob(job);
        connect(job, &KIO::StandardThumbnailJob::data, this, [=, this](KIO::Job *job, const QImage &thumb) {
            slotStandardThumbData(job, thumb);
        });
        job->start();
        return;
    }

    // Using thumbnailer plugin
    qWarning() << "using plugin";
    QUrl thumbURL;
    thumbURL.setScheme(QStringLiteral("thumbnail"));
    thumbURL.setPath(pixPath);
    KIO::TransferJob *job = KIO::get(thumbURL, NoReload, HideProgressInfo);
    connect(job, &KIO::TransferJob::data, this, [this](KIO::Job *job, const QByteArray &data) {
        slotThumbData(job, data);
    });
    addSubjob(job);
    int thumb_width = m_size.width();
    int thumb_height = m_size.height();
    if (save) {
        thumb_width = thumb_height = m_cacheSize;
    }

    job->addMetaData(QStringLiteral("mimeType"), m_currentItem.item.mimetype());
    job->addMetaData(QStringLiteral("width"), QString::number(thumb_width));
    job->addMetaData(QStringLiteral("height"), QString::number(thumb_height));
    job->addMetaData(QStringLiteral("plugin"), m_currentItem.plugin.fileName());
    job->addMetaData(QStringLiteral("enabledPlugins"), m_enabledPlugins.join(QLatin1Char(',')));
    job->addMetaData(QStringLiteral("devicePixelRatio"), QString::number(m_devicePixelRatio));
    job->addMetaData(QStringLiteral("cache"), QString::number(m_currentDeviceCachePolicy == CachePolicy::Allow));
    if (m_sequenceIndex) {
        job->addMetaData(QStringLiteral("sequence-index"), QString::number(m_sequenceIndex));
    }
#if WITH_SHM
    if (m_shmid != -1) {
        job->addMetaData(QStringLiteral("shmid"), QString::number(m_shmid));
    }
#endif
}

void GetFilePreviewJob::slotStandardThumbData(KIO::Job *job, const QImage &thumbData)
{
    qWarning() << "slotStandardThumbData";
    m_thumbnailWorkerMetaData = job->metaData();

    if (thumbData.isNull()) {
        // let succeeded in false state
        // failed will get called in determineNextFile()
        qWarning() << "thumbdata is null!!";
        return;
    }

    QImage thumb = thumbData;
    saveThumbnailData(thumb);

    emitPreview(thumb);
    m_succeeded = true;
}

void GetFilePreviewJob::slotThumbData(KIO::Job *job, const QByteArray &data)
{
    QImage thumb;
    // Keep this in sync with kio-extras|thumbnail/thumbnail.cpp
    QDataStream str(data);

#if WITH_SHM
    if (m_shmaddr != nullptr) {
        int width;
        int height;
        QImage::Format format;
        qreal imgDevicePixelRatio;
        // TODO KF6: add a version number as first parameter
        str >> width >> height >> format >> imgDevicePixelRatio;
        thumb = QImage(m_shmaddr, width, height, format).copy();
        thumb.setDevicePixelRatio(imgDevicePixelRatio);
    }
#endif

    if (thumb.isNull()) {
        // fallback a raw QImage
        str >> thumb;
    }
    qWarning() << thumb;

    slotStandardThumbData(job, thumb);
}

void GetFilePreviewJob::saveThumbnailData(QImage &thumb)
{
    const bool save = m_scaleType == PreviewJob::ScaledAndCached && !m_sequenceIndex && m_currentDeviceCachePolicy == CachePolicy::Allow
        && m_currentItem.plugin.value(QStringLiteral("CacheThumbnail"), true)
        && (!m_currentItem.item.targetUrl().isLocalFile()
            || !m_currentItem.item.targetUrl().adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(m_thumbRoot));

    qWarning() << "saveThumbnailData" << save;
    if (save) {
        thumb.setText(QStringLiteral("Thumb::URI"), QString::fromUtf8(m_origName));
        thumb.setText(QStringLiteral("Thumb::MTime"), QString::number(m_tOrig.toSecsSinceEpoch()));
        thumb.setText(QStringLiteral("Thumb::Size"), number(m_currentItem.item.size()));
        thumb.setText(QStringLiteral("Thumb::Mimetype"), m_currentItem.item.mimetype());
        QString thumbnailerVersion = m_currentItem.plugin.value(QStringLiteral("ThumbnailerVersion"));
        QString signature = QLatin1String("KDE Thumbnail Generator ") + m_currentItem.plugin.name();
        if (!thumbnailerVersion.isEmpty()) {
            signature.append(QLatin1String(" (v") + thumbnailerVersion + QLatin1Char(')'));
        }
        thumb.setText(QStringLiteral("Software"), signature);
        QSaveFile saveFile(m_thumbPath + m_thumbName);
        if (saveFile.open(QIODevice::WriteOnly)) {
            if (thumb.save(&saveFile, "PNG")) {
                saveFile.commit();
            }
        }
    }
}

void GetFilePreviewJob::emitPreview(const QImage &thumb)
{
    qWarning() << "emitPreview";
    QPixmap pix;
    const qreal ratio = thumb.devicePixelRatio();
    if (thumb.width() > m_size.width() * ratio || thumb.height() > m_size.height() * ratio) {
        pix = QPixmap::fromImage(thumb.scaled(QSize(m_size.width() * ratio, m_size.height() * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        pix = QPixmap::fromImage(thumb);
    }
    pix.setDevicePixelRatio(ratio);
    // Emit result of currentGetFilePreviewJob
    Q_EMIT gotPreview(m_currentItem.item, pix);
}

QList<KPluginMetaData> GetFilePreviewJob::loadAvailablePlugins()
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

QMap<QString, KIO::GetFilePreviewJob::StandardThumbnailerData> GetFilePreviewJob::standardThumbnailers()
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

#include "moc_getfilepreviewjob_p.cpp"
