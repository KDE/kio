/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "discspaceutil.h"
#include "kiotrashdebug.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QStorageInfo>

#include <qplatformdefs.h> // QT_LSTAT, QT_STAT, QT_STATBUF

DiscSpaceUtil::DiscSpaceUtil(const QString &directory)
    : mFullSize(0)
{
    QStorageInfo storageInfo(directory);
    if (storageInfo.isValid() && storageInfo.isReady()) {
        mFullSize = storageInfo.bytesTotal();
        mMountPoint = storageInfo.rootPath();
    }
}

qint64 DiscSpaceUtil::sizeOfPath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return 0;
    }

    if (info.isSymLink()) {
        // QFileInfo::size does not return the actual size of a symlink. #253776
        QT_STATBUF buff;
        return QT_LSTAT(QFile::encodeName(path).constData(), &buff) == 0 ? buff.st_size : 0;
    }

    if (info.isFile()) {
        return info.size();
    }

    if (info.isDir()) {
        QDirIterator it(path, QDirIterator::NoIteratorFlags);

        qint64 sum = 0;
        while (it.hasNext()) {
            it.next();
            const QFileInfo info = it.fileInfo();
            const QString name = info.fileName();

            if (name != QLatin1Char('.') && name != QLatin1String("..")) {
                sum += sizeOfPath(info.absoluteFilePath());
            }
        }

        return sum;
    }

    return 0;
}

double DiscSpaceUtil::usage(qint64 size) const
{
    if (mFullSize == 0) {
        return 0;
    }

    return (static_cast<double>(size) * 100) / static_cast<double>(mFullSize);
}

qint64 DiscSpaceUtil::size() const
{
    return mFullSize;
}

QString DiscSpaceUtil::mountPoint() const
{
    return mMountPoint;
}
