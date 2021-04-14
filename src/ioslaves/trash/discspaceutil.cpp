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
    : mDirectory(directory)
    , mFullSize(0)
{
    calculateFullSize();
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
    } else if (info.isFile()) {
        return info.size();
    } else if (info.isDir()) {
        QDirIterator it(path, QDirIterator::NoIteratorFlags);

        qint64 sum = 0;
        while (it.hasNext()) {
            const QFileInfo info = it.next();

            if (info.fileName() != QLatin1String(".") && info.fileName() != QLatin1String("..")) {
                sum += sizeOfPath(info.absoluteFilePath());
            }
        }

        return sum;
    } else {
        return 0;
    }
}

double DiscSpaceUtil::usage(qint64 size) const
{
    if (mFullSize == 0) {
        return 0;
    }

    return (((double)size * 100) / (double)mFullSize);
}

qint64 DiscSpaceUtil::size() const
{
    return mFullSize;
}

QString DiscSpaceUtil::mountPoint() const
{
    return mMountPoint;
}

void DiscSpaceUtil::calculateFullSize()
{
    QStorageInfo storageInfo(mDirectory);
    if (storageInfo.isValid() && storageInfo.isReady()) {
        mFullSize = storageInfo.bytesTotal();
        mMountPoint = storageInfo.rootPath();
    }
}
