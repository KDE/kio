/*
 *    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 */

#ifndef MOCKTHUMBNAILPLUGIN_H
#define MOCKTHUMBNAILPLUGIN_H
#include "thumbnailcreator.h"
class MockThumbnail : public KIO::ThumbnailCreator
{
    Q_OBJECT
public:
    MockThumbnail(QObject *parent, const QVariantList &args);
    KIO::ThumbnailResult create(const KIO::ThumbnailRequest &request) override;
};

#endif
