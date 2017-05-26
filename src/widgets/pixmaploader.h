/* This file is part of the KDE libraries
   Copyright (C) 2000-2005 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KIO_PIXMAPLOADER_H
#define KIO_PIXMAPLOADER_H

#include <QPixmap>
#include <kiconloader.h>
#include "kiowidgets_export.h"

class QUrl;

namespace KIO
{

/**
 * Convenience method to find the pixmap for a URL.
 *
 * Call this one when you don't know the mimetype.
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
 */
KIOWIDGETS_EXPORT QPixmap pixmapForUrl(const QUrl &url, int dummy = 0, KIconLoader::Group group = KIconLoader::Desktop,
                                       int force_size = 0, int state = 0, QString *path = nullptr);

}

#endif /* KIO_PIXMAPLOADER_H */

