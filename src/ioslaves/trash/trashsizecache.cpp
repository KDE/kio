/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "trashsizecache.h"

#include "discspaceutil.h"
#include "kiotrashdebug.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <qplatformdefs.h> // QT_LSTAT, QT_STAT, QT_STATBUF

TrashSizeCache::TrashSizeCache(const QString &path)
    : mTrashSizeCachePath(path + QLatin1String("/directorysizes"))
    , mTrashPath(path)
{
    // qCDebug(KIO_TRASH) << "CACHE:" << mTrashSizeCachePath;
}

// Only the last part of the line: space, directory name, '\n'
static QByteArray spaceAndDirectoryAndNewline(const QString &directoryName)
{
    const QByteArray encodedDir = QFile::encodeName(directoryName).toPercentEncoding();
    return ' ' + encodedDir + '\n';
}

void TrashSizeCache::add(const QString &directoryName, qint64 directorySize)
{
    // qCDebug(KIO_TRASH) << directoryName << directorySize;
    const QByteArray spaceAndDirAndNewline = spaceAndDirectoryAndNewline(directoryName);
    QFile file(mTrashSizeCachePath);
    QSaveFile out(mTrashSizeCachePath);
    if (out.open(QIODevice::WriteOnly)) {
        if (file.open(QIODevice::ReadOnly)) {
            while (!file.atEnd()) {
                const QByteArray line = file.readLine();
                if (line.endsWith(spaceAndDirAndNewline)) {
                    // Already there!
                    out.cancelWriting();
                    // qCDebug(KIO_TRASH) << "already there!";
                    return;
                }
                out.write(line);
            }
        }

        const qint64 mtime = getTrashFileInfo(directoryName).lastModified().toMSecsSinceEpoch();
        QByteArray newLine = QByteArray::number(directorySize) + ' ' + QByteArray::number(mtime) + spaceAndDirAndNewline;
        out.write(newLine);
        out.commit();
    }
    // qCDebug(KIO_TRASH) << mTrashSizeCachePath << "exists:" << QFile::exists(mTrashSizeCachePath);
}

void TrashSizeCache::remove(const QString &directoryName)
{
    // qCDebug(KIO_TRASH) << directoryName;
    const QByteArray spaceAndDirAndNewline = spaceAndDirectoryAndNewline(directoryName);
    QFile file(mTrashSizeCachePath);
    QSaveFile out(mTrashSizeCachePath);
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

void TrashSizeCache::rename(const QString &oldDirectoryName, const QString &newDirectoryName)
{
    const QByteArray spaceAndDirAndNewline = spaceAndDirectoryAndNewline(oldDirectoryName);
    QFile file(mTrashSizeCachePath);
    QSaveFile out(mTrashSizeCachePath);
    if (file.open(QIODevice::ReadOnly) && out.open(QIODevice::WriteOnly)) {
        while (!file.atEnd()) {
            QByteArray line = file.readLine();
            if (line.endsWith(spaceAndDirAndNewline)) {
                // Found it -> rename it, keeping the size
                line = line.left(line.length() - spaceAndDirAndNewline.length()) + spaceAndDirectoryAndNewline(newDirectoryName);
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

QFileInfo TrashSizeCache::getTrashFileInfo(const QString &fileName)
{
    const QString fileInfoPath = mTrashPath + QLatin1String("/info/") + fileName + QLatin1String(".trashinfo");
    Q_ASSERT(QFile::exists(fileInfoPath));
    return QFileInfo(fileInfoPath);
}

QHash<QByteArray, TrashSizeCache::SizeAndModTime> TrashSizeCache::readDirCache()
{
    // First read the directorysizes cache into memory
    QFile file(mTrashSizeCachePath);
    QHash<QByteArray, SizeAndModTime> dirCache;
    if (file.open(QIODevice::ReadOnly)) {
        while (!file.atEnd()) {
            const QByteArray line = file.readLine();
            const int firstSpace = line.indexOf(' ');
            const int secondSpace = line.indexOf(' ', firstSpace + 1);
            SizeAndModTime data;
            data.size = line.left(firstSpace).toLongLong();
            // "012 4567 name\n" -> firstSpace=3, secondSpace=8, we want mid(4,4)
            data.mtime = line.mid(firstSpace + 1, secondSpace - firstSpace - 1).toLongLong();
            const auto name = line.mid(secondSpace + 1, line.length() - secondSpace - 2);
            dirCache.insert(name, data);
        }
    }
    return dirCache;
}

qint64 TrashSizeCache::calculateSize()
{
    return scanFilesInTrash(ScanFilesInTrashOption::DonTcheckModificationTime).size;
}

TrashSizeCache::SizeAndModTime TrashSizeCache::calculateSizeAndLatestModDate()
{
    return scanFilesInTrash(ScanFilesInTrashOption::CheckModificationTime);
}

TrashSizeCache::SizeAndModTime TrashSizeCache::scanFilesInTrash(ScanFilesInTrashOption checkDateTime)
{
    const QHash<QByteArray, SizeAndModTime> dirCache = readDirCache();

    // Iterate over the actual trashed files.
    // Orphan items (no .fileinfo) still take space.
    QDirIterator it(mTrashPath + QLatin1String("/files/"), QDir::NoDotAndDotDot);
    qint64 sum = 0;
    qint64 max_mtime = 0;
    const auto checkMaxTime = [&max_mtime](const qint64 lastModTime) {
        if (lastModTime > max_mtime) {
            max_mtime = lastModTime;
        }
    };
    const auto checkLastModTime = [this, checkMaxTime](const QString &fileName) {
        const auto trashFileInfo = getTrashFileInfo(fileName);
        if (!trashFileInfo.exists()) {
            return;
        }
        checkMaxTime(trashFileInfo.lastModified().toMSecsSinceEpoch());
    };
    while (it.hasNext()) {
        it.next();
        const QString fileName = it.fileName();
        const QFileInfo fileInfo = it.fileInfo();
        if (fileInfo.isSymLink()) {
            // QFileInfo::size does not return the actual size of a symlink. #253776
            QT_STATBUF buff;
            if (QT_LSTAT(QFile::encodeName(fileInfo.absoluteFilePath()).constData(), &buff) == 0) {
                sum += static_cast<unsigned long long>(buff.st_size);
                if (checkDateTime == ScanFilesInTrashOption::CheckModificationTime) {
                    checkLastModTime(fileName);
                }
            }
        } else if (fileInfo.isFile()) {
            sum += static_cast<unsigned long long>(fileInfo.size());
            if (checkDateTime == ScanFilesInTrashOption::CheckModificationTime) {
                checkLastModTime(fileName);
            }
        } else {
            // directories
            bool usableCache = false;
            auto dirIt = dirCache.constFind(QFile::encodeName(fileName));
            if (dirIt != dirCache.constEnd()) {
                const SizeAndModTime &data = *dirIt;
                const auto trashFileInfo = getTrashFileInfo(fileName);
                if (trashFileInfo.exists() && trashFileInfo.lastModified().toMSecsSinceEpoch() == data.mtime) {
                    sum += data.size;
                    usableCache = true;
                    if (checkDateTime == ScanFilesInTrashOption::CheckModificationTime) {
                        checkMaxTime(data.mtime);
                    }
                }
            }
            if (!usableCache) {
                // directories with no cache data (or outdated)
                const qint64 size = DiscSpaceUtil::sizeOfPath(fileInfo.absoluteFilePath());
                sum += size;
                if (checkDateTime == ScanFilesInTrashOption::CheckModificationTime) {
                    // NOTE: this does not take into account the directory content modification date
                    checkMaxTime(QFileInfo(fileInfo.absolutePath()).lastModified().toMSecsSinceEpoch());
                }
                add(fileName, size);
            }
        }
    }
    return {sum, max_mtime};
}
