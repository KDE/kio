/*
    kdiskfreespaceinfo.h
    SPDX-FileCopyrightText: 2008 Sebastian Trug <trueg@kde.org>

    Based on kdiskfreespace.h
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2008 Dirk Mueller <mueller@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kdiskfreespaceinfo.h"

#include <QSharedData>
#include <QFile>

#include <kmountpoint.h>

#ifdef Q_OS_WIN
#include <QDir>
#include <qt_windows.h>
#else
#include <sys/statvfs.h>
#endif

class Q_DECL_HIDDEN KDiskFreeSpaceInfo::Private : public QSharedData
{
public:
    Private()
        : valid(false),
          size(0),
          available(0)
    {
    }

    bool valid;
    QString mountPoint;
    KIO::filesize_t size;
    KIO::filesize_t available;
};

KDiskFreeSpaceInfo::KDiskFreeSpaceInfo()
    : d(new Private())
{
}

KDiskFreeSpaceInfo::KDiskFreeSpaceInfo(const KDiskFreeSpaceInfo &other)
{
    d = other.d;
}

KDiskFreeSpaceInfo::~KDiskFreeSpaceInfo()
{
}

KDiskFreeSpaceInfo &KDiskFreeSpaceInfo::operator=(const KDiskFreeSpaceInfo &other)
{
    d = other.d;
    return *this;
}

bool KDiskFreeSpaceInfo::isValid() const
{
    return d->valid;
}

QString KDiskFreeSpaceInfo::mountPoint() const
{
    return d->mountPoint;
}

KIO::filesize_t KDiskFreeSpaceInfo::size() const
{
    return d->size;
}

KIO::filesize_t KDiskFreeSpaceInfo::available() const
{
    return d->available;
}

KIO::filesize_t KDiskFreeSpaceInfo::used() const
{
    return d->size - d->available;
}

KDiskFreeSpaceInfo KDiskFreeSpaceInfo::freeSpaceInfo(const QString &path)
{
    KDiskFreeSpaceInfo info;

    // determine the mount point
    KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(path);
    if (mp) {
        info.d->mountPoint = mp->mountPoint();
    }

#ifdef Q_OS_WIN
    quint64 availUser;
    QFileInfo fi(info.d->mountPoint);
    QString dir = QDir::toNativeSeparators(fi.absoluteDir().canonicalPath());

    if (GetDiskFreeSpaceExW((LPCWSTR)dir.utf16(),
                            (PULARGE_INTEGER)&availUser,
                            (PULARGE_INTEGER)&info.d->size,
                            (PULARGE_INTEGER)&info.d->available) != 0) {
        info.d->valid = true;
    }
#else
    struct statvfs statvfs_buf;

    // Prefer mountPoint if available, so that it even works with non-existing files.
    const QString pathArg = info.d->mountPoint.isEmpty() ? path : info.d->mountPoint;
    if (!statvfs(QFile::encodeName(pathArg).constData(), &statvfs_buf)) {
        const quint64 blksize = quint64(statvfs_buf.f_frsize); // cast to avoid overflow
        info.d->available = statvfs_buf.f_bavail * blksize;
        info.d->size = statvfs_buf.f_blocks * blksize;
        info.d->valid = true;
    }
#endif

    return info;
}
