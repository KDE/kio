/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef THUMBNAILCACHE_P_H
#define THUMBNAILCACHE_P_H

#include <kfileitem.h>
#include <kio/global.h>

#include <QByteArray>
#include <QImage>
#include <QSize>
#include <QString>

// The shared thumbnail cache the thumbnail specification describes, as plain functions of their
// arguments, so that a job writing a thumbnail and a lookup reading it agree on where it lives.
// https://specifications.freedesktop.org/thumbnail-spec/thumbnail-spec-latest.html#DIRECTORY
namespace KIO
{
namespace ThumbnailCache
{

/*! Root of the cache, holding one directory per thumbnail size. */
QString rootPath();

/*! The size a thumbnail of \a size is generated and cached at: 128, 256, 512 or 1024. */
short cacheSize(const QSize &size);

/*!
 * The directory ("normal/", "large/", ...) that holds thumbnails of \a cacheSize at
 * \a devicePixelRatio, empty when none of them is large enough. A thumbnailer multiplies what it
 * is asked for by the ratio, so that product is what decides the directory.
 */
QString tierDir(short cacheSize, qreal devicePixelRatio);

/*! Cache file path for the encoded \a uri, empty when no directory holds thumbnails of that size. */
QString filePath(const QByteArray &uri, const QString &thumbRoot, const QSize &size, qreal devicePixelRatio);

/*! Reads the thumbnail at \a path, tagging it with \a devicePixelRatio, or a null image. */
QImage load(const QString &path, qreal devicePixelRatio);

/*!
 * Whether \a thumb was made from the file \a uri names, of this modification time and size. With
 * \a requireSufficientResolution a thumbnail smaller than the request is rejected as well, which
 * a job wants and a lookup that shows what there is right now does not.
 */
bool matches(const QImage &thumb,
             const QByteArray &uri,
             qint64 sourceMTimeSecs,
             KIO::filesize_t sourceSize,
             const QSize &requestSize,
             qreal devicePixelRatio,
             bool requireSufficientResolution = true);

/*! Scales \a thumb down to fit \a size, honouring its ratio, or returns it as it is. */
QImage scaledToFit(const QImage &thumb, const QSize &size);

/*! Whether \a thumb was made from the file as it stands now. */
bool isCurrent(const QImage &thumb, qint64 sourceMTimeSecs, KIO::filesize_t sourceSize);

/*!
 * The cached thumbnail for the file \a uri names, ready to be shown at \a size, or a null image
 * when nothing usable is cached. Takes what it needs by value, so it can run on any thread.
 */
QImage
thumbnailFor(const QByteArray &uri, const QString &thumbRoot, const QSize &size, qreal devicePixelRatio, qint64 sourceMTimeSecs, KIO::filesize_t sourceSize);

/*!
 * The cached thumbnail for \a item, ready to be shown at \a size, whether or not it was made from
 * the file as it stands now, which isCurrent() tells. Only local files are served.
 */
QImage thumbnailForItem(const KFileItem &item, const QSize &size, qreal devicePixelRatio);

}
}

#endif
