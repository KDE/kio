/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "discspaceutil.h"
#include "kiotrashdebug.h"

#include <QDirIterator>
#include <QFileInfo>

#include <kdiskfreespaceinfo.h>
#include <qplatformdefs.h> // QT_LSTAT, QT_STAT, QT_STATBUF

DiscSpaceUtil::DiscSpaceUtil(const QString &directory)
    : mDirectory(directory),
      mFullSize(0)
{
    calculateFullSize();
}

qulonglong DiscSpaceUtil::sizeOfPath(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        return 0;
    }

    if (info.isSymLink()) {
        // QFileInfo::size does not return the actual size of a symlink. #253776
        QT_STATBUF buff;
        return static_cast<qulonglong>(QT_LSTAT(QFile::encodeName(path).constData(), &buff) == 0 ? buff.st_size : 0);
    } else if (info.isFile()) {
        return info.size();
    } else if (info.isDir()) {
        QDirIterator it(path, QDirIterator::NoIteratorFlags);

        qulonglong sum = 0;
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

double DiscSpaceUtil::usage(qulonglong size) const
{
    if (mFullSize == 0) {
        return 0;
    }

    return (((double)size * 100) / (double)mFullSize);
}

qulonglong DiscSpaceUtil::size() const
{
    return mFullSize;
}

QString DiscSpaceUtil::mountPoint() const
{
    return mMountPoint;
}

void DiscSpaceUtil::calculateFullSize()
{
    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(mDirectory);
    if (info.isValid()) {
        mFullSize = info.size();
        mMountPoint = info.mountPoint();
    }
}
