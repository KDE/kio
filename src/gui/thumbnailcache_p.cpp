/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "thumbnailcache_p.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QStandardPaths>

#include <algorithm>

namespace KIO
{
namespace ThumbnailCache
{

QString rootPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1String("/thumbnails/");
}

short cacheSize(const QSize &size)
{
    const int longer = std::max(size.width(), size.height());
    if (longer <= 128) {
        return 128;
    } else if (longer <= 256) {
        return 256;
    } else if (longer <= 512) {
        return 512;
    }
    return 1024;
}

QString tierDir(short cacheSize, qreal devicePixelRatio)
{
    struct CachePool {
        QLatin1String path;
        int minSize;
    };
    static const CachePool pools[] = {
        {QLatin1String("normal/"), 128},
        {QLatin1String("large/"), 256},
        {QLatin1String("x-large/"), 512},
        {QLatin1String("xx-large/"), 1024},
    };
    const int wants = devicePixelRatio * cacheSize;
    for (const auto &p : pools) {
        if (p.minSize >= wants) {
            return QString(p.path);
        }
    }
    return QString();
}

QString filePath(const QByteArray &uri, const QString &thumbRoot, const QSize &size, qreal devicePixelRatio)
{
    const QString tier = tierDir(cacheSize(size), devicePixelRatio);
    if (tier.isEmpty()) {
        return QString();
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(uri);
    const QString name = QString::fromLatin1(md5.result().toHex()) + QLatin1String(".png");
    return thumbRoot + tier + name;
}

QImage load(const QString &path, qreal devicePixelRatio)
{
    QImage thumb;
    QFile thumbFile(path);
    if (!thumbFile.open(QIODevice::ReadOnly) || !thumb.load(&thumbFile, "png")) {
        return QImage();
    }
    // The DPR of the loaded thumbnail is unspecified (and typically irrelevant).
    // When a thumbnail is DPR-invariant, use the DPR passed in the request.
    thumb.setDevicePixelRatio(devicePixelRatio);
    return thumb;
}

bool isCurrent(const QImage &thumb, qint64 sourceMTimeSecs, KIO::filesize_t sourceSize)
{
    if (thumb.isNull() || thumb.text(QStringLiteral("Thumb::MTime")).toLongLong() != sourceMTimeSecs) {
        return false;
    }

    // Thumb::Size is not required, but if it is set it should match
    const QString cachedSize = thumb.text(QStringLiteral("Thumb::Size"));
    return cachedSize.isEmpty() || cachedSize.toULongLong() == sourceSize;
}

bool matches(const QImage &thumb,
             const QByteArray &uri,
             qint64 sourceMTimeSecs,
             KIO::filesize_t sourceSize,
             const QSize &requestSize,
             qreal devicePixelRatio,
             bool requireSufficientResolution)
{
    if (thumb.isNull()) {
        return false;
    }
    if (thumb.text(QStringLiteral("Thumb::URI")) != QString::fromUtf8(uri)) {
        return false;
    }
    if (!isCurrent(thumb, sourceMTimeSecs, sourceSize)) {
        return false;
    }

    if (requireSufficientResolution) {
        // Reject a cached thumbnail smaller than needed now (blurry if scaled up), but
        // only when the original is large enough to yield a bigger one; else keep it.
        const int neededPixels = qMax(requestSize.width(), requestSize.height()) * devicePixelRatio;
        const int cachedPixels = qMax(thumb.width(), thumb.height());
        if (cachedPixels < neededPixels) {
            const int origWidth = thumb.text(QStringLiteral("Thumb::Image::Width")).toInt();
            const int origHeight = thumb.text(QStringLiteral("Thumb::Image::Height")).toInt();
            if (qMax(origWidth, origHeight) > cachedPixels) {
                return false;
            }
        }
    }
    return true;
}

QImage scaledToFit(const QImage &thumb, const QSize &size)
{
    // Deliver at the same device-pixel size PreviewJob::generated() would: downscale
    // only when the cached image is larger than the request.
    const qreal ratio = thumb.devicePixelRatio();
    if (thumb.width() > size.width() * ratio || thumb.height() > size.height() * ratio) {
        return thumb.scaled(QSize(size.width() * ratio, size.height() * ratio), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return thumb;
}

QImage
thumbnailFor(const QByteArray &uri, const QString &thumbRoot, const QSize &size, qreal devicePixelRatio, qint64 sourceMTimeSecs, KIO::filesize_t sourceSize)
{
    const QString path = filePath(uri, thumbRoot, size, devicePixelRatio);
    if (path.isEmpty()) {
        return QImage();
    }

    const QImage thumb = load(path, devicePixelRatio);
    // A cached thumbnail smaller than the request is still worth showing at once on
    // the first paint: the async job replaces it with a sharper one moments later, so
    // do not reject it on resolution here.
    if (!matches(thumb, uri, sourceMTimeSecs, sourceSize, size, devicePixelRatio, false)) {
        return QImage();
    }

    return scaledToFit(thumb, size);
}
}
}
