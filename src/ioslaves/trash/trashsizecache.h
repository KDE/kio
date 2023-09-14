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
 * Since version 1.0, https://specifications.freedesktop.org/trash-spec/trashspec-latest.html specifies this cache
 * as a standard way to cache this information.
 *
 */
class TrashSizeCache
{
public:
    struct SizeAndModTime {
        qint64 size;
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
    void add(const QString &directoryName, qint64 directorySize);

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
    qint64 calculateSize();

    /**
     * Calculates and returns the current trash size and its last modification date
     */
    SizeAndModTime calculateSizeAndLatestModDate();

    /**
     * Returns the space occupied by directories in trash and their latest modification dates
     */
    QHash<QByteArray, TrashSizeCache::SizeAndModTime> readDirCache();

private:
    enum ScanFilesInTrashOption { CheckModificationTime, DonTcheckModificationTime };
    TrashSizeCache::SizeAndModTime scanFilesInTrash(ScanFilesInTrashOption checkDateTime = CheckModificationTime);

    QString mTrashSizeCachePath;
    QString mTrashPath;
    QFileInfo getTrashFileInfo(const QString &fileName);
};

#endif
