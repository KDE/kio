/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kurlpixmapprovider.h"
#include <QUrl>
#include <kio/global.h>
#include <pixmaploader.h>

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 66) && KCOMPLETION_ENABLE_DEPRECATED_SINCE(5, 66)

KUrlPixmapProvider::KUrlPixmapProvider()
    : d(nullptr)
{
}

KUrlPixmapProvider::~KUrlPixmapProvider()
{
}

QPixmap KUrlPixmapProvider::pixmapFor(const QString &url, int size)
{
    const QUrl u = QUrl::fromUserInput(url); // absolute path or URL
    return KIO::pixmapForUrl(u, 0, KIconLoader::Desktop, size);
}

void KUrlPixmapProvider::virtual_hook(int id, void *data)
{
    KPixmapProvider::virtual_hook(id, data);
}

#endif
