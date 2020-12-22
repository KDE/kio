/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_PIXMAPLOADER_H
#define KIO_PIXMAPLOADER_H

#include <QPixmap>
#include <KIconLoader>
#include "kiowidgets_export.h"

class QUrl;

namespace KIO
{

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(5, 75)
/**
 * Convenience method to find the pixmap for a URL.
 *
 * Call this one when you don't know the MIME type.
 *
 * @param url URL for the file.
 * @param dummy ignored (was a mode_t parameter in kdelibs 4)
 * @param group The icon group where the icon is going to be used.
 * @param force_size Override globally configured icon size.
 *        Use 0 for the default size
 * @param state The icon state, one of: KIconLoader::DefaultState,
 * KIconLoader::ActiveState or KIconLoader::DisabledState.
 * @param path Output parameter to get the full path. Seldom needed.
 *              Ignored if null pointer.
 * @return the pixmap of the URL, can be a default icon if not found
 * @deprecated since 5.75, use KIO::iconNameForUrl() to get the
 * icon name and
 * QIcon::fromTheme(name, QIcon::fromTheme(QStringLiteral("application-octet-stream")))
 */
KIOWIDGETS_DEPRECATED_VERSION(5, 75, "Use KIO::iconNameForUrl")
KIOWIDGETS_EXPORT QPixmap pixmapForUrl(const QUrl &url, int dummy = 0, KIconLoader::Group group = KIconLoader::Desktop,
                                       int force_size = 0, int state = 0, QString *path = nullptr);

#endif
}

#endif /* KIO_PIXMAPLOADER_H */

