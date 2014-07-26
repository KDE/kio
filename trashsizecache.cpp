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

#include "trashsizecache.h"

#include "discspaceutil.h"

#include <qplatformdefs.h> // QT_LSTAT, QT_STAT, QT_STATBUF
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QDateTime>
#include <QSaveFile>
#include <QDebug>

TrashSizeCache::TrashSizeCache( const QString &path )
    : mTrashSizeCachePath( path + QString::fromLatin1( "/directorysizes" ) ),
      mTrashPath( path )
{
    //qDebug() << "CACHE:" << mTrashSizeCachePath;
}

void TrashSizeCache::add( const QString &directoryName, qulonglong directorySize )
{
    //qDebug() << directoryName << directorySize;
    const QByteArray encodedDir = QFile::encodeName(directoryName).toPercentEncoding();
    const QByteArray spaceAndDirAndNewline = ' ' + encodedDir + '\n';
    QFile file( mTrashSizeCachePath );
    QSaveFile out( mTrashSizeCachePath );
    if (out.open(QIODevice::WriteOnly)) {
        if (file.open(QIODevice::ReadOnly)) {
            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                if (line.endsWith(spaceAndDirAndNewline)) {
                    // Already there!
                    out.cancelWriting();
                    //qDebug() << "already there!";
                    return;
                }
                out.write(line);
            }
        }

        const QString fileInfoPath = mTrashPath + "/info/" + directoryName + ".trashinfo";
        QDateTime mtime = QFileInfo(fileInfoPath).lastModified();
        QByteArray newLine = QByteArray::number(directorySize) + ' ' + QByteArray::number(mtime.toMSecsSinceEpoch()) + spaceAndDirAndNewline;
        out.write(newLine);
        out.commit();
    }
    //qDebug() << mTrashSizeCachePath << "exists:" << QFile::exists(mTrashSizeCachePath);
}

void TrashSizeCache::remove( const QString &directoryName )
{
    //qDebug() << directoryName;
    const QByteArray encodedDir = QFile::encodeName(directoryName).toPercentEncoding();
    const QByteArray spaceAndDirAndNewline = ' ' + encodedDir + '\n';
    QFile file( mTrashSizeCachePath );
    QSaveFile out( mTrashSizeCachePath );
    if (file.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly)) {
        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            if (line.endsWith(spaceAndDirAndNewline)) {
                // Found it -> skip it
                continue;
            }
            out.write(line);
        }
    }
    out.commit();
}

void TrashSizeCache::clear()
{
    QFile::remove(mTrashSizeCachePath);
}

struct CacheData {
    qint64 mtime;
    qulonglong size;
};

qulonglong TrashSizeCache::calculateSize()
{
    // First read the directorysizes cache into memory
    QFile file( mTrashSizeCachePath );
    typedef QHash<QByteArray, CacheData> DirCacheHash;
    DirCacheHash dirCache;
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            const int firstSpace = line.indexOf(' ');
            const int secondSpace = line.indexOf(' ', firstSpace + 1);
            CacheData data;
            data.mtime = line.left(firstSpace).toLongLong();
            // "012 4567 name" -> firstSpace=3, secondSpace=8, we want mid(4,4)
            data.size = line.mid(firstSpace + 1, secondSpace - firstSpace - 1).toULongLong();
            dirCache.insert(line.mid(secondSpace + 1), data);
        }
    }
    // Iterate over the actual trashed files.
    // Orphan items (no .fileinfo) still take space.
    QDirIterator it( mTrashPath + QString::fromLatin1( "/files/" ), QDirIterator::NoIteratorFlags );

    qulonglong sum = 0;
    while ( it.hasNext() ) {
        const QFileInfo file = it.next();
        if (file.fileName() == QLatin1String(".") || file.fileName() == QLatin1String("..")) {
            continue;
        }
        if ( file.isSymLink() ) {
            // QFileInfo::size does not return the actual size of a symlink. #253776
            QT_STATBUF buff;
            return static_cast<qulonglong>(QT_LSTAT(QFile::encodeName(file.absoluteFilePath()), &buff) == 0 ? buff.st_size : 0);
        } else if (file.isFile()) {
            sum += file.size();
        } else {
            bool usableCache = false;
            const QString fileId = file.fileName();
            DirCacheHash::const_iterator it = dirCache.constFind(QFile::encodeName(fileId));
            if (it != dirCache.constEnd()) {
                const CacheData &data = *it;
                const QString fileInfoPath = mTrashPath + "/info/" + fileId + ".trashinfo";
                if (QFileInfo(fileInfoPath).lastModified().toMSecsSinceEpoch() == data.mtime) {
                    sum += data.size;
                    usableCache = true;
                }
            }
            if (!usableCache) {
                const qulonglong size = DiscSpaceUtil::sizeOfPath(file.absoluteFilePath());
                sum += size;
                add(fileId, size);
            }
        }

    }

    return sum;
}
