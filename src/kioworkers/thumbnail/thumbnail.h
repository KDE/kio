/*  This file is part of the KDE libraries
 *    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>
 *
 *    SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _THUMBNAIL_H_
#define _THUMBNAIL_H_

#include <QHash>
#include <QPainter>
#include <QRandomGenerator>
#include <QSet>

#include <KIO/ThumbnailCreator>
#include <KIO/WorkerBase>
#include <KPluginMetaData>

class ThumbCreator;
class QImage;

struct ThumbCreatorWithMetadata {
    std::unique_ptr<KIO::ThumbnailCreator> creator;
    bool cacheThumbnail = true;
    bool devicePixelRatioDependent = false;
    bool handleSequences = false;
};

class ThumbnailProtocol : public KIO::WorkerBase
{
public:
    ThumbnailProtocol(const QByteArray &pool, const QByteArray &app);
    ~ThumbnailProtocol() override;

    KIO::WorkerResult get(const QUrl &url) override;

protected:
    ThumbCreatorWithMetadata *getThumbCreator(const QString &plugin);
    bool isOpaque(const QImage &image) const;
    void drawPictureFrame(QPainter *painter, const QPoint &pos, const QImage &image, int frameWidth, QSize imageTargetSize, int rotationAngle) const;
    QImage thumbForDirectory(const QString &directory);
    KPluginMetaData pluginForMimeType(const QString &mimeType);

    float sequenceIndex() const;

private:
    /**
     * Creates a sub thumbnail for the directory thumbnail. If a cached
     * version of the sub thumbnail is available, the cached version will be used.
     * If no cached version is available, the created sub thumbnail will be
     * added to the cache for later use.
     */
    bool createSubThumbnail(QImage &thumbnail, const QString &filePath, int segmentWidth, int segmentHeight);

    /**
     * Draw the SubThumbnail
     **/
    void drawSubThumbnail(QPainter &p, QImage subThumbnail, int width, int height, int xPos, int yPos, int borderStrokeWidth);

private:
    void ensureDirsCreated();
    bool createThumbnail(ThumbCreatorWithMetadata *subCreator, const QString &filePath, int width, int height, QImage &thumbnail);

    QString m_mimeType;
    int m_width;
    int m_height;
    qreal m_devicePixelRatio;
    // Thumbnail creators
    QHash<QString, ThumbCreatorWithMetadata *> m_creators;
    QStringList m_enabledPlugins;
    QSet<QString> m_propagationDirectories;
    QString m_thumbBasePath;
    KIO::filesize_t m_maxFileSize;
    QRandomGenerator m_randomGenerator;
    float m_sequenceIndexWrapAroundPoint = -1;
};

#endif
