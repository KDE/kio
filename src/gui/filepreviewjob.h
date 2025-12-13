/*
 *  This file is part of the KDE libraries
 *  SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KIO_FILEPREVIEWJOB_H
#define KIO_FILEPREVIEWJOB_H

#include "previewjob.h"
#include <KPluginMetaData>
#include <QDir>
#include <QImage>
#include <QSize>
#include <kfileitem.h>
#include <kio/job.h>

namespace KIO
{

struct PreviewOptions {
    // Size of thumbnail
    QSize size;
    qreal devicePixelRatio = 1.0;
    bool ignoreMaximumSize = false;
    int sequenceIndex = 0;
    // Whether the thumbnail should be scaled and/or saved
    PreviewJob::ScaleType scaleType = PreviewJob::ScaleType::ScaledAndCached;
};

struct PreviewSetupData {
    // Root of thumbnail cache
    QString thumbRoot;

    QMap<QString, KPluginMetaData> pluginByMimeTable;
    QStringList enabledPluginIds;
};

struct PreviewItem {
    KFileItem item;
    QMap<QString, int> deviceIdMap;
};

class SHM
{
public:
    SHM(int id, uchar *address);
    ~SHM();

    static std::unique_ptr<SHM> create(int size);

    Q_DISABLE_COPY(SHM);

    int id() const;
    uchar *address() const;

private:
    // Shared memory segment Id. The segment is allocated to a size
    // of extent x extent x 4 (32 bit image) on first need.
    int m_id;
    // And the data area
    uchar *m_address;
};

// Time (in milliseconds) to wait for kio-fuse in a PreviewJob before giving up.
static constexpr int s_kioFuseMountTimeout = 10000;

/*
 * This job does multiple small chained jobs to get the thumbnail for an item,
 * and returns the result.
 *
 * First, it attempts to get the device id's for the given paths: thumbRoot and item.localUrl
 * However if past jobs have already done this, it can reuse the old deviceId table and skip
 * getting the device id's again.
 *
 * Device id's are required for checking if the thumbnail can be cached.
 *
 * After getting all this information, if the item has sequenceId higher than 0,
 * we just get the next item in the sequence and return that result.
 *
 * If we're not sequencing, first we try to pull the thumbnail from the cache.
 * If that is successful, we just return the file and end the job.
 *
 * If not successful, it's likely we do not have thumbnail for this item, so
 * we generate one, either by using thumbnailerPlugin or standardThumbnailer.
 *
 * We then return the result, whatever it may be.
 */
class FilePreviewJob : public KIO::Job
{
    Q_OBJECT
public:
    FilePreviewJob(const PreviewItem &item, const PreviewOptions &options, const PreviewSetupData &setupData);
    ~FilePreviewJob();

    void start() override;

    QMap<QString, QString> thumbnailWorkerMetaData() const;
    QMap<QString, int> deviceIdMap() const;
    QImage previewImage() const;
    PreviewItem item() const;
    static QList<KPluginMetaData> loadAvailablePlugins();
    static QList<KPluginMetaData> standardThumbnailers();

private Q_SLOTS:
    void slotStatFile(KJob *job);
    void slotGetOrCreateThumbnail(KJob *job);

private:
    enum CachePolicy {
        Prevent,
        Allow,
        Unknown
    } m_currentDeviceCachePolicy = Unknown;

    // The current item
    KIO::PreviewItem m_item;
    // The modification time of that URL
    QDateTime m_tOrig;
    // Path to thumbnail cache for the current size
    QString m_thumbPath;
    // Original URL of current item in RFC2396 format
    // (file:///path/to/a%20file instead of file:/path/to/a file)
    QByteArray m_origName;
    // Thumbnail file name for current item
    QString m_thumbName;

    const PreviewOptions m_options;
    const PreviewSetupData m_setupData;

    // Unscaled size of thumbnail (128, 256 or 512 if cache is enabled)
    short m_cacheSize;
    // If the file to create a thumb for was a temp file, this is its name
    QString m_tempName;
    // The shared memory
    std::unique_ptr<SHM> m_shm;
    // Metadata returned from the KIO thumbnail worker
    QMap<QString, QString> m_thumbnailWorkerMetaData;
    static const int m_idUnknown = -1;
    // Id of a device storing currently processed file
    int m_currentDeviceId = 0;
    // Device ID for each file. Stored while in STATE_DEVICE_INFO state, used later on.
    QMap<QString, int> m_deviceIdMap;
    // the path of a unique temporary directory
    QString m_tempDirPath;
    // Whether to try using KIOFuse to resolve files. Set to false if KIOFuse is not available.
    bool m_tryKioFuse = true;
    // The preview image. If when emitting return this is empty, job can be considered as failed.
    QImage m_preview;

    bool m_standardThumbnailer = false;
    KPluginMetaData m_plugin;

    void statFile();
    void getOrCreateThumbnail();
    static QImage loadThumbnailFromCache(const QString &url, qreal dpr);
    bool isCacheValid(const QImage &thumb);
    void createThumbnail(const QString &);
    void createThumbnailViaFuse(const QUrl &, const QUrl &);
    void createThumbnailViaLocalCopy(const QUrl &);
    QString parentDirPath(const QString &path) const;

    void emitPreview(const QImage &thumb);
    void slotThumbData(KIO::Job *, const QByteArray &);
    void slotStandardThumbData(KIO::Job *, const QImage &);
    // Checks if thumbnail is on encrypted partition different than thumbRoot
    CachePolicy canBeCached(const QString &path);
    int getDeviceId(const QString &path);
    void saveThumbnailData(QImage &thumb);
    bool preparePluginForMimetype(const QString &mimeType);
    static void saveThumbnailToCache(const QImage &thumb, const QString &path);
};

inline FilePreviewJob *filePreviewJob(const PreviewItem &item, const PreviewOptions &options, const PreviewSetupData &setupData)
{
    auto job = new FilePreviewJob(item, options, setupData);
    return job;
}
}

#endif
