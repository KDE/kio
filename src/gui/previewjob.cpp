// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte.starostik@t-online.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "previewjob.h"
#include "filepreviewjob.h"
#include "kiogui_debug.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QMetaMethod>
#include <QMimeDatabase>
#include <QPixmap>
#include <QStandardPaths>

#include "job_p.h"

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusError>

#endif

namespace
{
static qreal s_defaultDevicePixelRatio = 1.0;
}

using namespace KIO;

class KIO::PreviewJobPrivate : public KIO::JobPrivate
{
public:
    PreviewJobPrivate(const KFileItemList &items, const QSize &size)
        : initialItems(items)
        , size(size)
        , scaleType(PreviewJob::ScaleType::ScaledAndCached)
        , ignoreMaximumSize(false)
        , sequenceIndex(0)
    {
        // https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
        thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
    }

    KFileItemList initialItems;
    QStringList enabledPlugins;
    // Our todo list :)
    // We remove the first item at every step, so use std::list
    QSize size;
    std::list<PreviewItem> items;
    // The current item
    PreviewItem currentItem;
    // Whether the thumbnail should be scaled ando/or saved
    PreviewJob::ScaleType scaleType;
    bool ignoreMaximumSize;
    int sequenceIndex;
    // Root of thumbnail cache
    QString thumbRoot;
    // Metadata returned from the KIO thumbnail worker
    QMap<QString, QString> thumbnailWorkerMetaData;
    qreal devicePixelRatio = s_defaultDevicePixelRatio;
    // Cache the deviceIdMap so we dont need to stat the files every time
    QMap<QString, int> deviceIdMap;

    void determineNextFile();
    void startPreview();

    Q_DECLARE_PUBLIC(PreviewJob)

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
}

void PreviewJob::setScaleType(ScaleType type)
{
    Q_D(PreviewJob);
    d->scaleType = type;
}

PreviewJob::ScaleType PreviewJob::scaleType() const
{
    Q_D(const PreviewJob);
    return d->scaleType;
}

void PreviewJobPrivate::startPreview()
{
    Q_Q(PreviewJob);
    // Load the list of plugins to determine which MIME types are supported
    const QList<KPluginMetaData> plugins = KIO::FilePreviewJob::loadAvailablePlugins();
    QMap<QString, KPluginMetaData> mimeMap;

    auto setUpCaching = [this](PreviewItem *previewItem) {
        short cacheSize = 0;
        const int longer = std::max(size.width(), size.height());
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
        QString thumbPath = thumbRoot + thumbDir;
        QDir().mkpath(thumbRoot);
        if (!QDir(thumbPath).exists() && !QDir(thumbRoot).mkdir(thumbDir, QFile::ReadUser | QFile::WriteUser | QFile::ExeUser)) { // 0700
            qCWarning(KIO_GUI) << "couldn't create thumbnail dir " << thumbPath;
        }
        previewItem->thumbPath = thumbPath;
        previewItem->cacheSize = cacheSize;
    };

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
    for (const auto &fileItem : std::as_const(initialItems)) {
        PreviewItem previewItem;
        previewItem.item = fileItem;

        const QString mimeType = previewItem.item.mimetype();

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
            const KPluginMetaData plugin = *pluginIt;

            previewItem.standardThumbnailer = plugin.description() == QStringLiteral("standardthumbnailer");
            previewItem.plugin = plugin;
            previewItem.devicePixelRatio = devicePixelRatio;
            previewItem.sequenceIndex = sequenceIndex;
            previewItem.ignoreMaximumSize = ignoreMaximumSize;
            previewItem.scaleType = scaleType;
            previewItem.size = size;
            previewItem.deviceIdMap = deviceIdMap;

            bool handlesSequencesValue = previewItem.plugin.value(QStringLiteral("HandleSequences"), false);
            thumbnailWorkerMetaData.insert(QStringLiteral("handlesSequences"), QString::number(handlesSequencesValue));
            if (scaleType == PreviewJob::ScaleType::ScaledAndCached && plugin.value(QStringLiteral("CacheThumbnail"), true)) {
                const QUrl url = fileItem.targetUrl();
                if (!url.isLocalFile() || !url.adjusted(QUrl::RemoveFilename).toLocalFile().startsWith(thumbRoot)) {
                    setUpCaching(&previewItem);
                }
            }
            items.push_back(previewItem);
        } else {
            Q_EMIT q->failed(fileItem);
        }
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

void PreviewJobPrivate::determineNextFile()
{
    Q_Q(PreviewJob);
    // No more items ?
    if (items.empty()) {
        q->emitResult();
        return;
    } else {
        // First, stat the orig file
        currentItem = items.front();
        items.pop_front();

        FilePreviewJob *job = KIO::filePreviewJob(currentItem, thumbRoot);
        q->addSubjob(job);
        job->start();
    }
}

void PreviewJob::slotResult(KJob *job)
{
    Q_D(PreviewJob);
    FilePreviewJob *previewJob = static_cast<KIO::FilePreviewJob *>(job);
    if (previewJob) {
        auto fileItem = previewJob->item().item;
        if (!previewJob->previewImage().isNull()) {
            d->thumbnailWorkerMetaData = previewJob->thumbnailWorkerMetaData();
            d->deviceIdMap = previewJob->deviceIdMap();
            auto previewImage = previewJob->previewImage();
            Q_EMIT generated(fileItem, previewImage);
            if (isSignalConnected(QMetaMethod::fromSignal(&PreviewJob::gotPreview))) {
                QPixmap pixmap = QPixmap::fromImage(previewImage);
                pixmap.setDevicePixelRatio(d->devicePixelRatio);
                Q_EMIT gotPreview(fileItem, pixmap);
            }
        } else {
            Q_EMIT failed(fileItem);
        }
    }
    removeSubjob(job);
    if (job->error() > 0) {
        qCWarning(KIO_GUI) << "PreviewJob subjob had an error:" << job->errorString();
    }
    d->determineNextFile();
}

QList<KPluginMetaData> PreviewJob::availableThumbnailerPlugins()
{
    return FilePreviewJob::loadAvailablePlugins();
}

QStringList PreviewJob::availablePlugins()
{
    QStringList result;
    const auto plugins = KIO::FilePreviewJob::loadAvailablePlugins();
    for (const KPluginMetaData &plugin : plugins) {
        result << plugin.pluginId();
    }
    return result;
}

QStringList PreviewJob::defaultPlugins()
{
    const QStringList exclusionList = QStringList() << QStringLiteral("textthumbnail");

    QStringList defaultPlugins = availablePlugins();
    for (const QString &plugin : exclusionList) {
        defaultPlugins.removeAll(plugin);
    }

    return defaultPlugins;
}

QStringList PreviewJob::supportedMimeTypes()
{
    QStringList result;
    const auto plugins = KIO::FilePreviewJob::loadAvailablePlugins();
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
