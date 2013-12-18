/* This file is part of the KDE libraries

   Copyright (c) 2000 Carsten Pfeiffer <pfeiffer@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kurlpixmapprovider.h"
#include <QUrl>
#include <kio/global.h>
#include <pixmaploader.h>

KUrlPixmapProvider::KUrlPixmapProvider()
    : d(0)
{
}

KUrlPixmapProvider::~KUrlPixmapProvider()
{
}

QPixmap KUrlPixmapProvider::pixmapFor( const QString& url, int size )
{
    const QUrl u = QUrl::fromUserInput(url); // absolute path or URL
    return KIO::pixmapForUrl( u, 0, KIconLoader::Desktop, size );
}

void KUrlPixmapProvider::virtual_hook( int id, void* data )
{ KPixmapProvider::virtual_hook( id, data ); }
