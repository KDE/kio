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
#include "kprotocolinfo.h"

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
        : fileItems(items)
        , options{size, s_defaultDevicePixelRatio, false, 0, PreviewJob::ScaleType::ScaledAndCached}
    {
        // https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
        setupData.thumbRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
    }

    KFileItemList fileItems;

    PreviewOptions options;
    PreviewSetupData setupData;

    // Metadata returned from the KIO thumbnail worker
    QMap<QString, QString> thumbnailWorkerMetaData;
    // Cache the deviceIdMap so we dont need to stat the files every time
    QMap<QString, int> deviceIdMap;

    void startNextFilePreviewJobBatch();
    void startPreview();

    Q_DECLARE_PUBLIC(PreviewJob)

private:
    QDir createTemporaryDir();
    int maximumWorkers = 1;
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
        d->setupData.enabledPluginIds = *enabledPlugins;
    } else {
        d->setupData.enabledPluginIds =
            globalConfig.readEntry("Plugins",
                                   QStringList{QStringLiteral("directorythumbnail"), QStringLiteral("imagethumbnail"), QStringLiteral("jpegthumbnail")});
    }

    d->maximumWorkers = KProtocolInfo::maxWorkers(QStringLiteral("thumbnail"));
    // Return to event loop first, startNextFilePreviewJobBatch() might delete this;
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
    d->options.scaleType = type;
}

PreviewJob::ScaleType PreviewJob::scaleType() const
{
    Q_D(const PreviewJob);
    return d->options.scaleType;
}

void PreviewJobPrivate::startPreview()
{
    // Load the list of plugins to determine which MIME types are supported
    const QList<KPluginMetaData> plugins = KIO::FilePreviewJob::loadAvailablePlugins();

    for (const KPluginMetaData &plugin : plugins) {
        bool pluginIsEnabled = setupData.enabledPluginIds.contains(plugin.pluginId());
        const auto mimeTypes = plugin.mimeTypes();
        for (const QString &mimeType : mimeTypes) {
            if (pluginIsEnabled && !setupData.pluginByMimeTable.contains(mimeType)) {
                setupData.pluginByMimeTable.insert(mimeType, plugin);
            }
        }
    }

    startNextFilePreviewJobBatch();
}

#if KIOGUI_BUILD_DEPRECATED_SINCE(6, 22)
void PreviewJob::removeItem(const QUrl &url)
{
    Q_D(PreviewJob);

    auto it = std::find_if(d->fileItems.cbegin(), d->fileItems.cend(), [&url](const KFileItem &pItem) {
        return url == pItem.url();
    });
    if (it != d->fileItems.cend()) {
        d->fileItems.erase(it);
    }

    for (auto subjob : subjobs()) {
        FilePreviewJob *previewJob = static_cast<KIO::FilePreviewJob *>(subjob);
        if (previewJob && previewJob->item().url() == url) {
            subjob->kill();
            removeSubjob(subjob);
            d->startNextFilePreviewJobBatch();
            break;
        }
    }
}
#endif

void KIO::PreviewJob::setSequenceIndex(int index)
{
    d_func()->options.sequenceIndex = index;
}

int KIO::PreviewJob::sequenceIndex() const
{
    return d_func()->options.sequenceIndex;
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
    d_func()->options.devicePixelRatio = dpr;
}

void PreviewJob::setIgnoreMaximumSize(bool ignoreSize)
{
    d_func()->options.ignoreMaximumSize = ignoreSize;
}

void PreviewJobPrivate::startNextFilePreviewJobBatch()
{
    Q_Q(PreviewJob);

    if (q->subjobs().empty() && fileItems.empty()) {
        q->emitResult();
        return;
    }

    const int jobsToRun = qMin((int)fileItems.size(), maximumWorkers - q->subjobs().count());
    for (int i = 0; i < jobsToRun; i++) {
        auto fileItem = fileItems.front();
        fileItems.pop_front();
        FilePreviewJob *job = KIO::filePreviewJob(fileItem, deviceIdMap, options, setupData);
        q->addSubjob(job);
        job->start();
    }
}

void PreviewJob::slotResult(KJob *job)
{
    Q_D(PreviewJob);
    FilePreviewJob *previewJob = static_cast<KIO::FilePreviewJob *>(job);
    if (previewJob) {
        const auto &fileItem = previewJob->item();
        if (!previewJob->previewImage().isNull()) {
            d->thumbnailWorkerMetaData = previewJob->thumbnailWorkerMetaData();
            d->deviceIdMap = previewJob->deviceIdMap();
            auto previewImage = previewJob->previewImage();
            Q_EMIT generated(fileItem, previewImage);
            if (isSignalConnected(QMetaMethod::fromSignal(&PreviewJob::gotPreview))) {
                QPixmap pixmap = QPixmap::fromImage(previewImage);
                pixmap.setDevicePixelRatio(d->options.devicePixelRatio);
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
    d->startNextFilePreviewJobBatch();
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
