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

    QMap<QString, KPluginMetaData> mimeMap;

    void determineNextFile();
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
        d->enabledPlugins = *enabledPlugins;
    } else {
        d->enabledPlugins =
            globalConfig.readEntry("Plugins",
                                   QStringList{QStringLiteral("directorythumbnail"), QStringLiteral("imagethumbnail"), QStringLiteral("jpegthumbnail")});
    }

    d->maximumWorkers = KProtocolInfo::maxWorkers(QStringLiteral("thumbnail"));
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

    for (const KPluginMetaData &plugin : plugins) {
        bool pluginIsEnabled = enabledPlugins.contains(plugin.pluginId());
        const auto mimeTypes = plugin.mimeTypes();
        for (const QString &mimeType : mimeTypes) {
            if (pluginIsEnabled && !mimeMap.contains(mimeType)) {
                mimeMap.insert(mimeType, plugin);
            }
        }
    }

    for (const auto &fileItem : std::as_const(initialItems)) {
        PreviewItem previewItem;
        previewItem.item = fileItem;
        previewItem.devicePixelRatio = devicePixelRatio;
        previewItem.sequenceIndex = sequenceIndex;
        previewItem.ignoreMaximumSize = ignoreMaximumSize;
        previewItem.scaleType = scaleType;
        previewItem.size = size;
        previewItem.deviceIdMap = deviceIdMap;
        items.push_back(previewItem);
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

    for (auto subjob : subjobs()) {
        FilePreviewJob *previewJob = static_cast<KIO::FilePreviewJob *>(subjob);
        if (previewJob && previewJob->item().item.url() == url) {
            subjob->kill();
            removeSubjob(subjob);
            d->determineNextFile();
            break;
        }
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

    if (q->subjobs().count() == 0 && items.empty()) {
        q->emitResult();
        return;
    }

    const int jobsToRun = qMin((int)items.size(), maximumWorkers - q->subjobs().count());
    for (int i = 0; i < jobsToRun; i++) {
        auto item = items.front();
        items.pop_front();
        FilePreviewJob *job = KIO::filePreviewJob(item, thumbRoot, mimeMap);
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
