/*
 *    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 */

#include "mockthumbnailplugin.h"
#include "thumbnailcreator.h"

#include <KPluginFactory>
#include <QImage>

K_PLUGIN_CLASS_WITH_JSON(MockThumbnail, "mockthumbnailplugin.json")

MockThumbnail::MockThumbnail(QObject *parent, const QVariantList &args)
    : KIO::ThumbnailCreator(parent, args)
{
}

KIO::ThumbnailResult MockThumbnail::create(const KIO::ThumbnailRequest &request)
{
    // Creates a red square of requested size
    QImage image(request.targetSize().width(), request.targetSize().height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::red);
    return KIO::ThumbnailResult::pass(image);
}

#include "moc_mockthumbnailplugin.cpp"
#include "mockthumbnailplugin.moc"
