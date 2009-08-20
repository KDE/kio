/*
   This file is part of the KDE project

   Copyright (C) 2009 Tobias Koenig <tokoe@kde.org>

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
 * @short A class that encapsulates the access to the trash size cache.
 *
 * The trash size cache is used to prevent the scanning of trash size
 * on every move/copy operation to the trash, which might result in performance
 * problems with many files inside the trash.
 *
 * The trash size cache is kept in the ktrashrc configuration file and
 * updated by the moveToTrash/moveFromTrash/copyToTrash and emptyTrash
 * methods in the TrashImpl object.
 */
class TrashSizeCache
{
    public:
        /**
         * Creates a new trash size cache object for the given trash @p path.
         */
        TrashSizeCache( const QString &path );

        /**
         * Initializes the trash metadata config file and does an initial scan
         * if called the first time.
         */
        void initialize();

        /**
         * Increases the size of the trash by @p value.
         */
        void add( qulonglong value );

        /**
         * Decreases the size of the trash by @p value.
         */
        void remove( qulonglong value );

        /**
         * Sets the trash size to 0 bytes.
         */
        void clear();

        /**
         * Returns the current trash size.
         */
        qulonglong size() const;

    private:
        qulonglong currentSize( bool doLocking ) const;

        QString mTrashSizeCachePath;
        QString mTrashPath;
        const QString mTrashSizeGroup;
        const QString mTrashSizeKey;
};

#endif
