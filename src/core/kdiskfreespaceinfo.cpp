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

class KDiskFreeSpaceInfoPrivate : public QSharedData
{
public:
    bool m_valid = false;
    QString m_mountPoint;
    KIO::filesize_t m_size = 0;
    KIO::filesize_t m_available = 0;
};

KDiskFreeSpaceInfo::KDiskFreeSpaceInfo()
    : d(new KDiskFreeSpaceInfoPrivate)
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
    return d->m_valid;
}

QString KDiskFreeSpaceInfo::mountPoint() const
{
    return d->m_mountPoint;
}

KIO::filesize_t KDiskFreeSpaceInfo::size() const
{
    return d->m_size;
}

KIO::filesize_t KDiskFreeSpaceInfo::available() const
{
    return d->m_available;
}

KIO::filesize_t KDiskFreeSpaceInfo::used() const
{
    return d->m_size - d->m_available;
}

KDiskFreeSpaceInfo KDiskFreeSpaceInfo::freeSpaceInfo(const QString &path)
{
    KDiskFreeSpaceInfo info;

    // determine the mount point
    KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByPath(path);
    if (mp) {
        info.d->m_mountPoint = mp->mountPoint();
    }

#ifdef Q_OS_WIN
    quint64 availUser;
    QFileInfo fi(info.d->m_mountPoint);
    QString dir = QDir::toNativeSeparators(fi.absoluteDir().canonicalPath());

    if (GetDiskFreeSpaceExW((LPCWSTR)dir.utf16(),
                            (PULARGE_INTEGER)&availUser,
                            (PULARGE_INTEGER)&info.d->m_size,
                            (PULARGE_INTEGER)&info.d->m_available) != 0) {
        info.d->m_valid = true;
    }
#else
    struct statvfs statvfs_buf;

    // Prefer mountPoint if available, so that it even works with non-existing files.
    const QString pathArg = info.d->m_mountPoint.isEmpty() ? path : info.d->m_mountPoint;
    if (!statvfs(QFile::encodeName(pathArg).constData(), &statvfs_buf)) {
        const quint64 blksize = quint64(statvfs_buf.f_frsize); // cast to avoid overflow
        info.d->m_available = statvfs_buf.f_bavail * blksize;
        info.d->m_size = statvfs_buf.f_blocks * blksize;
        info.d->m_valid = true;
    }
#endif

    return info;
}
