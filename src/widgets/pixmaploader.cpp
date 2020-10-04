/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "pixmaploader.h"
#include <QUrl>
#include <kio/global.h> // iconNameForUrl

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 75)
QPixmap KIO::pixmapForUrl(const QUrl &url, int mode, KIconLoader::Group group,
                          int force_size, int state, QString *path)
{
    Q_UNUSED(mode);
    const QString iconName = KIO::iconNameForUrl(url);
    return KIconLoader::global()->loadMimeTypeIcon(iconName, group, force_size, state, QStringList(), path);
}
#endif
