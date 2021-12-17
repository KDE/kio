/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kmountpoint.h"

#include <stdlib.h>

#include <config-kmountpoint.h>
#include <kioglobal_p.h> // Defines QT_LSTAT on windows to kio_windows_lstat

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <qplatformdefs.h>

#ifdef Q_OS_WIN
#include <qt_windows.h>
static const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
static const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif

// This is the *BSD branch
#if HAVE_SYS_MOUNT_H
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
// FreeBSD has a table of names of mount-options in mount.h, which is only 
// defined (as MNTOPT_NAMES) if _WANT_MNTOPTNAMES is defined.
#define _WANT_MNTOPTNAMES
#include <sys/mount.h>
#undef _WANT_MNTOPTNAMES
#endif

#if HAVE_FSTAB_H
#include <fstab.h>
#endif

// Linux
#if HAVE_LIB_MOUNT
#include <libmount/libmount.h>
#include <blkid/blkid.h>
#endif

static bool isNetfs(const QString &mountType)
{
    // List copied from util-linux/libmount/src/utils.c
    static const std::vector<QLatin1String> netfsList{
        QLatin1String("cifs"),
        QLatin1String("smb3"),
        QLatin1String("smbfs"),
        QLatin1String("nfs"),
        QLatin1String("nfs3"),
        QLatin1String("nfs4"),
        QLatin1String("afs"),
        QLatin1String("ncpfs"),
        QLatin1String("fuse.curlftpfs"),
        QLatin1String("fuse.sshfs"),
        QLatin1String("9p"),
    };

    return std::any_of(netfsList.cbegin(), netfsList.cend(), [mountType](const QLatin1String netfs) {
        return mountType == netfs;
    });
}

class KMountPointPrivate
{
public:
    void resolveGvfsMountPoints(KMountPoint::List &result);
    void finalizePossibleMountPoint(KMountPoint::DetailsNeededFlags infoNeeded);
    void finalizeCurrentMountPoint(KMountPoint::DetailsNeededFlags infoNeeded);

    QString m_mountedFrom;
    QString m_device; // Only available when the NeedRealDeviceName flag was set.
    QString m_mountPoint;
    QString m_mountType;
    QStringList m_mountOptions;
    dev_t m_deviceId = 0;
    bool m_isNetFs = false;
};

KMountPoint::KMountPoint()
    : d(new KMountPointPrivate)
{
}

KMountPoint::~KMountPoint() = default;

#if HAVE_GETMNTINFO

#ifdef MNTOPT_NAMES
static struct mntoptnames bsdOptionNames[] = {
    MNTOPT_NAMES
};

/** @brief Get mount options from @p flags and puts human-readable version in @p list
 * 
 * Appends all positive options found in @p flags to the @p list
 * This is roughly paraphrased from FreeBSD's mount.c, prmount().
 */
static void translateMountOptions(QStringList &list, uint64_t flags)
{
    const struct mntoptnames* optionInfo = bsdOptionNames;

    // Not all 64 bits are useful option names
    flags = flags & MNT_VISFLAGMASK;
    // Chew up options as long as we're in the table and there
    // are any flags left. 
    for (; flags != 0 && optionInfo->o_opt != 0; ++optionInfo) {
        if (flags & optionInfo->o_opt) {
            list.append(QString::fromLatin1(optionInfo->o_name));
            flags &= ~optionInfo->o_opt;
        }
    }
}
#else
/** @brief Get mount options from @p flags and puts human-readable version in @p list
 * 
 * This default version just puts the hex representation of @p flags
 * in the list, because there is no human-readable version.
 */
static void translateMountOptions(QStringList &list, uint64_t flags)
{
    list.append(QStringLiteral("0x%1").arg(QString::number(flags, 16)));
}
#endif

#endif // HAVE_GETMNTINFO

void KMountPointPrivate::finalizePossibleMountPoint(KMountPoint::DetailsNeededFlags infoNeeded)
{
    QString potentialDevice;
    if (const auto tag = QLatin1String("UUID="); m_mountedFrom.startsWith(tag)) {
        potentialDevice = QFile::symLinkTarget(QLatin1String("/dev/disk/by-uuid/") + QStringView(m_mountedFrom).mid(tag.size()));
    } else if (const auto tag = QLatin1String("LABEL="); m_mountedFrom.startsWith(tag)) {
        potentialDevice = QFile::symLinkTarget(QLatin1String("/dev/disk/by-label/") + QStringView(m_mountedFrom).mid(tag.size()));
    }

    if (QFile::exists(potentialDevice)) {
        m_mountedFrom = potentialDevice;
    }

    if (infoNeeded & KMountPoint::NeedRealDeviceName) {
        if (m_mountedFrom.startsWith(QLatin1Char('/'))) {
            m_device = QFileInfo(m_mountedFrom).canonicalFilePath();
        }
    }

    // Chop trailing slash
    if (m_mountedFrom.endsWith(QLatin1Char('/'))) {
        m_mountedFrom.chop(1);
    }
}

void KMountPointPrivate::finalizeCurrentMountPoint(KMountPoint::DetailsNeededFlags infoNeeded)
{
    if (infoNeeded & KMountPoint::NeedRealDeviceName) {
        if (m_mountedFrom.startsWith(QLatin1Char('/'))) {
            m_device = QFileInfo(m_mountedFrom).canonicalFilePath();
        }
    }
}

KMountPoint::List KMountPoint::possibleMountPoints(DetailsNeededFlags infoNeeded)
{
    KMountPoint::List result;

#ifdef Q_OS_WIN
    result = KMountPoint::currentMountPoints(infoNeeded);

#elif HAVE_LIB_MOUNT
    if (struct libmnt_table *table = mnt_new_table()) {
        // By default parses "/etc/fstab"
        if (mnt_table_parse_fstab(table, nullptr) == 0) {
            struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
            struct libmnt_fs *fs;

            while (mnt_table_next_fs(table, itr, &fs) == 0) {
                const char *fsType = mnt_fs_get_fstype(fs);
                if (qstrcmp(fsType, "swap") == 0) {
                    continue;
                }

                Ptr mp(new KMountPoint);
                mp->d->m_mountType = QFile::decodeName(fsType);
                const char *target = mnt_fs_get_target(fs);
                mp->d->m_mountPoint = QFile::decodeName(target);

                if (QT_STATBUF buff; QT_LSTAT(target, &buff) == 0) {
                    mp->d->m_deviceId = buff.st_dev;
                }

                // First field in /etc/fstab, e.g. /dev/sdXY, LABEL=, UUID=, /some/bind/mount/dir
                // or some network mount
                if (const char *source = mnt_fs_get_source(fs)) {
                    mp->d->m_mountedFrom = QFile::decodeName(source);
                }

                if (infoNeeded & NeedMountOptions) {
                    mp->d->m_mountOptions = QFile::decodeName(mnt_fs_get_options(fs)).split(QLatin1Char(','));
                }

                mp->d->finalizePossibleMountPoint(infoNeeded);
                result.append(mp);
            }
            mnt_free_iter(itr);
        }

        mnt_free_table(table);
    }
#elif HAVE_FSTAB_H

    QFile f{QLatin1String(FSTAB)};
    if (!f.open(QIODevice::ReadOnly)) {
        return result;
    }

    QTextStream t(&f);
    QString s;

    while (!t.atEnd()) {
        s = t.readLine().simplified();
        if (s.isEmpty() || (s[0] == QLatin1Char('#'))) {
            continue;
        }

        // not empty or commented out by '#'
        const QStringList item = s.split(QLatin1Char(' '));

        if (item.count() < 4) {
            continue;
        }

        Ptr mp(new KMountPoint);

        int i = 0;
        mp->d->m_mountedFrom = item[i++];
        mp->d->m_mountPoint = item[i++];
        mp->d->m_mountType = item[i++];
        if (mp->d->m_mountType == QLatin1String("swap")) {
            continue;
        }
        QString options = item[i++];

        if (infoNeeded & NeedMountOptions) {
            mp->d->m_mountOptions = options.split(QLatin1Char(','));
        }

        mp->d->finalizePossibleMountPoint(infoNeeded);

        result.append(mp);
    } // while

    f.close();
#endif

    return result;
}

void KMountPointPrivate::resolveGvfsMountPoints(KMountPoint::List &result)
{
    if (m_mountedFrom == QLatin1String("gvfsd-fuse")) {
        const QDir gvfsDir(m_mountPoint);
        const QStringList mountDirs = gvfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &mountDir : mountDirs) {
            const QString type = mountDir.section(QLatin1Char(':'), 0, 0);
            if (type.isEmpty()) {
                continue;
            }

            KMountPoint::Ptr gvfsmp(new KMountPoint);
            gvfsmp->d->m_mountedFrom = m_mountedFrom;
            gvfsmp->d->m_mountPoint = m_mountPoint + QLatin1Char('/') + mountDir;
            gvfsmp->d->m_mountType = type;
            result.append(gvfsmp);
        }
    }
}

KMountPoint::List KMountPoint::currentMountPoints(DetailsNeededFlags infoNeeded)
{
    KMountPoint::List result;

#if HAVE_GETMNTINFO

#if GETMNTINFO_USES_STATVFS
    struct statvfs *mounted;
#else
    struct statfs *mounted;
#endif

    int num_fs = getmntinfo(&mounted, MNT_NOWAIT);

    result.reserve(num_fs);

    for (int i = 0; i < num_fs; i++) {
        Ptr mp(new KMountPoint);
        mp->d->m_mountedFrom = QFile::decodeName(mounted[i].f_mntfromname);
        mp->d->m_mountPoint = QFile::decodeName(mounted[i].f_mntonname);
        mp->d->m_mountType = QFile::decodeName(mounted[i].f_fstypename);

        if (QT_STATBUF buff; QT_LSTAT(mounted[i].f_mntonname, &buff) == 0) {
            mp->d->m_deviceId = buff.st_dev;
        }

        if (infoNeeded & NeedMountOptions) {
            struct fstab *ft = getfsfile(mounted[i].f_mntonname);
            if (ft != nullptr) {
                QString options = QFile::decodeName(ft->fs_mntops);
                mp->d->m_mountOptions = options.split(QLatin1Char(','));
            } else {
                translateMountOptions(mp->d->m_mountOptions, mounted[i].f_flags);
            }
        }

        mp->d->finalizeCurrentMountPoint(infoNeeded);
        // TODO: Strip trailing '/' ?
        result.append(mp);
    }

#elif defined(Q_OS_WIN)
    // nothing fancy with infoNeeded but it gets the job done
    DWORD bits = GetLogicalDrives();
    if (!bits) {
        return result;
    }

    for (int i = 0; i < 26; i++) {
        if (bits & (1 << i)) {
            Ptr mp(new KMountPoint);
            mp->d->m_mountPoint = QString(QLatin1Char('A' + i) + QLatin1String(":/"));
            result.append(mp);
        }
    }

#elif HAVE_LIB_MOUNT
    if (struct libmnt_table *table = mnt_new_table()) {
        // if "/etc/mtab" is a regular file,
        // "/etc/mtab" is used by default instead of "/proc/self/mountinfo" file.
        // This leads to NTFS mountpoints being hidden.
        if (mnt_table_parse_mtab(table, "/proc/self/mountinfo") == 0) {
            struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_FORWARD);
            struct libmnt_fs *fs;

            while (mnt_table_next_fs(table, itr, &fs) == 0) {
                Ptr mp(new KMountPoint);
                mp->d->m_mountedFrom = QFile::decodeName(mnt_fs_get_source(fs));
                mp->d->m_mountPoint = QFile::decodeName(mnt_fs_get_target(fs));
                mp->d->m_mountType = QFile::decodeName(mnt_fs_get_fstype(fs));
                mp->d->m_isNetFs = mnt_fs_is_netfs(fs) == 1;
                mp->d->m_deviceId = mnt_fs_get_devno(fs);

                if (infoNeeded & NeedMountOptions) {
                    mp->d->m_mountOptions = QFile::decodeName(mnt_fs_get_options(fs)).split(QLatin1Char(','));
                }

                if (infoNeeded & NeedRealDeviceName) {
                    if (mp->d->m_mountedFrom.startsWith(QLatin1Char('/'))) {
                        mp->d->m_device = mp->d->m_mountedFrom;
                    }
                }

                mp->d->resolveGvfsMountPoints(result);

                mp->d->finalizeCurrentMountPoint(infoNeeded);
                result.push_back(mp);
            }

            mnt_free_iter(itr);
        }

        mnt_free_table(table);
    }
#endif

    return result;
}

QString KMountPoint::mountedFrom() const
{
    return d->m_mountedFrom;
}

dev_t KMountPoint::deviceId() const
{
    return d->m_deviceId;
}

bool KMountPoint::isOnNetwork() const
{
    return d->m_isNetFs || isNetfs(d->m_mountType);
}

QString KMountPoint::realDeviceName() const
{
    return d->m_device;
}

QString KMountPoint::mountPoint() const
{
    return d->m_mountPoint;
}

QString KMountPoint::mountType() const
{
    return d->m_mountType;
}

QStringList KMountPoint::mountOptions() const
{
    return d->m_mountOptions;
}

KMountPoint::List::List()
    : QList<Ptr>()
{
}

KMountPoint::Ptr KMountPoint::List::findByPath(const QString &path) const
{
#ifdef Q_OS_WIN
    const QString realPath = QDir::fromNativeSeparators(QDir(path).absolutePath());
#else
    /* If the path contains symlinks, get the real name */
    QFileInfo fileinfo(path);
    // canonicalFilePath won't work unless file exists
    const QString realPath = fileinfo.exists() ? fileinfo.canonicalFilePath() : fileinfo.absolutePath();
#endif

    KMountPoint::Ptr result;

    if (QT_STATBUF buff; QT_LSTAT(QFile::encodeName(realPath).constData(), &buff) == 0) {
        auto it = std::find_if(this->cbegin(), this->cend(), [&buff, &realPath](const KMountPoint::Ptr &mountPtr) {
            // For a bind mount, the deviceId() is that of the base mount point, e.g. /mnt/foo,
            // however the path we're looking for, e.g. /home/user/bar, doesn't start with the
            // mount point of the base device, so we go on searching
            return mountPtr->deviceId() == buff.st_dev && realPath.startsWith(mountPtr->mountPoint());
        });

        if (it != this->cend()) {
            result = *it;
        }
    }

    return result;
}

KMountPoint::Ptr KMountPoint::List::findByDevice(const QString &device) const
{
    const QString realDevice = QFileInfo(device).canonicalFilePath();
    if (realDevice.isEmpty()) { // d->m_device can be empty in the loop below, don't match empty with it
        return Ptr();
    }
    for (const KMountPoint::Ptr &mountPoint : *this) {
        if (realDevice.compare(mountPoint->d->m_device, cs) == 0 || realDevice.compare(mountPoint->d->m_mountedFrom, cs) == 0) {
            return mountPoint;
        }
    }
    return Ptr();
}

bool KMountPoint::probablySlow() const
{
    /* clang-format off */
    return isOnNetwork()
        || d->m_mountType == QLatin1String("autofs")
        || d->m_mountType == QLatin1String("subfs")
        // Technically KIOFUSe mounts local slaves as well,
        // such as recents:/, but better safe than sorry...
        || d->m_mountType == QLatin1String("fuse.kio-fuse");
    /* clang-format on */
}

bool KMountPoint::testFileSystemFlag(FileSystemFlag flag) const
{
    /* clang-format off */
    const bool isMsDos = d->m_mountType == QLatin1String("msdos")
                         || d->m_mountType == QLatin1String("fat")
                         || d->m_mountType == QLatin1String("vfat");

    const bool isNtfs = d->m_mountType.contains(QLatin1String("fuse.ntfs"))
                        || d->m_mountType.contains(QLatin1String("fuseblk.ntfs"))
                        // fuseblk could really be anything. But its most common use is for NTFS mounts, these days.
                        || d->m_mountType == QLatin1String("fuseblk");

    const bool isSmb = d->m_mountType == QLatin1String("cifs")
                       || d->m_mountType == QLatin1String("smb3")
                       || d->m_mountType == QLatin1String("smbfs")
                       // gvfs-fuse mounted SMB share
                       || d->m_mountType == QLatin1String("smb-share");
    /* clang-format on */

    switch (flag) {
    case SupportsChmod:
    case SupportsChown:
    case SupportsUTime:
    case SupportsSymlinks:
        return !isMsDos && !isNtfs && !isSmb; // it's amazing the number of things Microsoft filesystems don't support :)
    case CaseInsensitive:
        return isMsDos;
    }
    return false;
}
