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
#include "statjob.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QMetaMethod>
#include <QMimeDatabase>
#include <QPixmap>
#include <QStandardPaths>
#include <QTimer>

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

class PathsFileDeviceIdsJob : public KIO::Job
{
public:
    explicit PathsFileDeviceIdsJob(const QStringList &paths);

    QMap<QString, int> takeDeviceIdByPathTable() const;

protected:
    void slotResult(KJob *job) override;

private:
    QMap<QString, int> m_deviceIdByPathTable;
};

// Stat multiple files at same time
PathsFileDeviceIdsJob::PathsFileDeviceIdsJob(const QStringList &paths)
{
    for (const QString &path : paths) {
        const QUrl url = QUrl::fromLocalFile(path);
        KIO::Job *job = KIO::stat(url, StatJob::SourceSide, KIO::StatDefaultDetails | KIO::StatInode, KIO::HideProgressInfo);
        job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
        addSubjob(job);
    }
}

void PathsFileDeviceIdsJob::slotResult(KJob *job)
{
    auto *const statJob = static_cast<KIO::StatJob *>(job);

    const QString path = statJob->url().toLocalFile();
    if (!path.isEmpty()) {
        int id;
        if (job->error()) {
            // We set id to 0 to know we tried getting it
            qCDebug(KIO_GUI) << "Cannot read information about filesystem under path" << path;
            id = 0;
        } else {
            id = statJob->statResult().numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
        }
        m_deviceIdByPathTable.insert(path, id);
    }

    removeSubjob(job);
    if (!hasSubjobs()) {
        emitResult();
    }
}

QMap<QString, int> PathsFileDeviceIdsJob::takeDeviceIdByPathTable() const
{
    return std::move(m_deviceIdByPathTable);
}

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
    // Cache the deviceIdByPathTable so we dont need to stat the files every time
    QMap<QString, int> deviceIdByPathTable;

    QTimer nextBatchScheduler;

    void startNextFilePreviewJobBatch();
    void startPreview();
    void scheduleNextFilePreviewJobBatch();

    Q_DECLARE_PUBLIC(PreviewJob)

private:
    int deviceIdForLocalPath(const QString &localPath) const;
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
    Q_Q(PreviewJob);

    nextBatchScheduler.setSingleShot(true);
    nextBatchScheduler.callOnTimeout(q, [this]() {
        startNextFilePreviewJobBatch();
    });

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

    // estimate the device ids for relevant paths
    QStringList paths;
    for (const auto &fileItem : std::as_const(fileItems)) {
        auto parentDir = FilePreviewJob::parentDirPath(fileItem.localPath());
        if (!parentDir.isEmpty() && !paths.contains(parentDir)) {
            paths.append(parentDir);
        }
    }
    // last add thumbRoot, to not add cost to above paths.contains() check
    paths.append(setupData.thumbRoot);

    auto *const pathsFileDeviceIdsJob = new PathsFileDeviceIdsJob(paths);
    QObject::connect(pathsFileDeviceIdsJob, &KIO::Job::result, q, [this](KJob *job) {
        auto *const pathsFileDeviceIdsJob = static_cast<PathsFileDeviceIdsJob *>(job);
        deviceIdByPathTable = pathsFileDeviceIdsJob->takeDeviceIdByPathTable();
        // caching info about thumbroot device id separately, to avoid repeated lookup
        setupData.thumbRootDeviceId = deviceIdForLocalPath(setupData.thumbRoot);

        startNextFilePreviewJobBatch();
    });
    pathsFileDeviceIdsJob->start();
}

void PreviewJobPrivate::scheduleNextFilePreviewJobBatch()
{
    if (!nextBatchScheduler.isActive()) {
        nextBatchScheduler.start();
    }
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
            d->scheduleNextFilePreviewJobBatch();
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

int PreviewJobPrivate::deviceIdForLocalPath(const QString &localPath) const
{
    if (localPath.isEmpty()) {
        return 0;
    }
    auto it = deviceIdByPathTable.find(localPath);
    if (it != deviceIdByPathTable.end()) {
        return it.value();
    }
    return FilePreviewJob::UnknownDeviceId;
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
        auto fileItem = fileItems.takeFirst();

        const auto parentDir = FilePreviewJob::parentDirPath(fileItem.localPath());
        const int parentDirDeviceId = deviceIdForLocalPath(parentDir);

        FilePreviewJob *job = KIO::filePreviewJob(fileItem, parentDirDeviceId, options, setupData);
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
    if (job->error() && job->error() != KIO::ERR_INTERNAL) {
        qCWarning(KIO_GUI) << "PreviewJob subjob had an error:" << job->errorString();
    }
    // slot might have been called synchronously from startNextFilePreviewJobBatch(), as KIO::stat currently can do
    // so always delay the next call to the next event-loop, to ensure startNextFilePreviewJobBatch() has exited
    d->scheduleNextFilePreviewJobBatch();
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
