/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef TRASHSIZECACHE_H
#define TRASHSIZECACHE_H

#include <QString>

#include <KConfig>

class QFileInfo;

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

    struct SizeAndModTime {
        qulonglong size;
        qint64 mtime;
    };

    /**
     * Creates a new trash size cache object for the given trash @p path.
     */
    explicit TrashSizeCache(const QString &path);

    /**
     * Adds a directory to the cache.
     * @param directoryName fileId of the directory
     * @param directorySize size in bytes
     */
    void add(const QString &directoryName, qulonglong directorySize);

    /**
     * Removes a directory from the cache.
     */
    void remove(const QString &directoryName);

    /**
     * Renames a directory in the cache.
     */
    void rename(const QString &oldDirectoryName, const QString &newDirectoryName);

    /**
     * Sets the trash size to 0 bytes.
     */
    void clear();

    /**
     * Calculates and returns the current trash size.
     */
    qulonglong calculateSize();

    /**
     * Calculates and returns the current trash size and its last modification date
     */
    SizeAndModTime calculateSizeAndLatestModDate();

private:
    QString mTrashSizeCachePath;
    QString mTrashPath;
    QFileInfo getTrashFileInfo(const QString &fileName);
};

#endif
