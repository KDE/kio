/*
   This file is part of the KDE project

   Copyright (C) 2009 Tobias Koenig <tokoe@kde.org>
   Copyright (C) 2014 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef TRASHSIZECACHE_H
#define TRASHSIZECACHE_H

#include <QtCore/QString>

#include <kconfig.h>

/**
 * @short A class that encapsulates the directory size cache.
 *
 * The directory size cache is used to speed up the determination of the trash size.
 *
 * Since version 1.0, http://standards.freedesktop.org/trash-spec/trashspec-latest.html specifies this cache
 * as a standard way to cache this information.
 *
 */
class TrashSizeCache
{
    public:
        /**
         * Creates a new trash size cache object for the given trash @p path.
         */
        TrashSizeCache( const QString &path );

        /**
         * Adds a directory to the cache.
         * @param directoryName fileId of the directory
         * @param directorySize size in bytes
         */
        void add( const QString &directoryName, qulonglong directorySize );

        /**
         * Removes a directory from the cache.
         */
        void remove( const QString &directoryName );

        /**
         * Sets the trash size to 0 bytes.
         */
        void clear();

        /**
         * Calculates and returns the current trash size.
         */
        qulonglong calculateSize();

    private:
        QString mTrashSizeCachePath;
        QString mTrashPath;
};

#endif
