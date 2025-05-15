#ifndef KIO_GETFILEPREVIEWJOB_H
#define KIO_GETFILEPREVIEWJOB_H

#include "kiogui_export.h"
#include <KConfigGroup>
#include <KFileUtils>
#include <KPluginMetaData>
#include <KSharedConfig>
#include <QDir>
#include <QMimeDatabase>
#include <QSize>
#include <QStandardPaths>
#include <kfileitem.h>
#include <kio/job.h>

namespace KIO
{

struct PreviewItem {
    KFileItem item;
    KPluginMetaData plugin;
    bool standardThumbnailer = false;
};

static qreal s_defaultDevicePixelRatio = 1.0;
// Time (in milliseconds) to wait for kio-fuse in a PreviewJob before giving up.
static constexpr int s_kioFuseMountTimeout = 10000;
class GetFilePreviewJob : public KIO::Job
{
    Q_OBJECT
public:
    GetFilePreviewJob(const PreviewItem &item, const QSize &size, const bool scaleItem, const bool saveItem);

    struct StandardThumbnailerData {
        QString exec;
        QStringList mimetypes;
    };

    enum {
        STATE_STATORIG, // if the thumbnail exists
        STATE_GETORIG, // if we create it
        STATE_CREATETHUMB, // thumbnail:/ worker
        STATE_DEVICE_INFO, // additional state check to get needed device ids
    } m_state;

    enum CachePolicy {
        Prevent,
        Allow,
        Unknown
    } m_currentDeviceCachePolicy = Unknown;

    QStringList m_enabledPlugins;
    // The current item
    const PreviewItem m_currentItem;
    // The modification time of that URL
    QDateTime m_tOrig;
    // Path to thumbnail cache for the current size
    QString m_thumbPath;
    // Original URL of current item in RFC2396 format
    // (file:///path/to/a%20file instead of file:/path/to/a file)
    QByteArray m_origName;
    // Thumbnail file name for current item
    QString m_thumbName;
    // Size of thumbnail
    int m_width;
    int m_height;
    // Unscaled size of thumbnail (128, 256 or 512 if cache is enabled)
    short m_cacheSize;
    // Whether the thumbnail should be scaled
    bool m_scaleItem;
    // Whether we should save the thumbnail
    bool m_saveItem;
    bool m_ignoreMaximumSize;
    int m_sequenceIndex;
    bool m_succeeded;
    // If the file to create a thumb for was a temp file, this is its name
    QString m_tempName;
    KIO::filesize_t m_maximumLocalSize;
    KIO::filesize_t m_maximumRemoteSize;
    // Manage preview for locally mounted remote directories
    bool m_enableRemoteFolderThumbnail;
    // Shared memory segment Id. The segment is allocated to a size
    // of extent x extent x 4 (32 bit image) on first need.
    int m_shmid;
    // And the data area
    uchar *m_shmaddr;
    // Size of the shm segment
    size_t m_shmsize;
    // Root of thumbnail cache
    QString m_thumbRoot;
    // Metadata returned from the KIO thumbnail worker
    QMap<QString, QString> m_thumbnailWorkerMetaData;
    qreal m_devicePixelRatio = s_defaultDevicePixelRatio;
    static const int m_idUnknown = -1;
    // Id of a device storing currently processed file
    int m_currentDeviceId = 0;
    // Device ID for each file. Stored while in STATE_DEVICE_INFO state, used later on.
    QMap<QString, int> m_deviceIdMap;
    // the path of a unique temporary directory
    QString m_tempDirPath;
    // Whether to try using KIOFuse to resolve files. Set to false if KIOFuse is not available.
    bool m_tryKioFuse = true;

    void getOrCreateThumbnail();
    void createThumbnailViaFuse(const QUrl &, const QUrl &);
    void createThumbnailViaLocalCopy(const QUrl &);
    bool statResultThumbnail();
    void createThumbnail(const QString &);
    void cleanupTempFile();
    void emitPreview(const QImage &thumb);

    void startPreview();
    void slotThumbData(KIO::Job *, const QByteArray &);
    void slotStandardThumbData(KIO::Job *, const QImage &);
    // Checks if thumbnail is on encrypted partition different than thumbRoot
    CachePolicy canBeCached(const QString &path);
    int getDeviceId(const QString &path);
    void saveThumbnailData(QImage &thumb);

    static QList<KPluginMetaData> loadAvailablePlugins();
    static QMap<QString, StandardThumbnailerData> standardThumbnailers();

    void start() override;
    void slotResult(KJob *job) override;

Q_SIGNALS:
    void gotPreview(const KFileItem &item, const QPixmap &preview);
    void failed(const KFileItem &item);

private:
    QDir createTemporaryDir();
};
}

#endif