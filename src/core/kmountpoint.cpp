/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kmountpoint.h"

#include <stdlib.h>

#include <config-kmountpoint.h>

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#ifdef Q_OS_WIN
static const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
static const Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif

#if HAVE_VOLMGT
#include <volmgt.h>
#endif
#if HAVE_SYS_MNTTAB_H
#include <sys/mnttab.h>
#endif
#if HAVE_MNTENT_H
#include <mntent.h>
#elif HAVE_SYS_MNTENT_H
#include <sys/mntent.h>
#endif

// This is the *BSD branch
#if HAVE_SYS_MOUNT_H
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h>
#endif

#if HAVE_FSTAB_H
#include <fstab.h>
#endif

#if ! HAVE_GETMNTINFO
# ifdef _PATH_MOUNTED
// On some Linux, MNTTAB points to /etc/fstab !
#  undef MNTTAB
#  define MNTTAB _PATH_MOUNTED
# else
#  ifndef MNTTAB
#   ifdef MTAB_FILE
#    define MNTTAB MTAB_FILE
#   else
#    define MNTTAB "/etc/mnttab"
#   endif
#  endif
# endif
#endif

#ifdef _OS_SOLARIS_
#define FSTAB "/etc/vfstab"
#else
#define FSTAB "/etc/fstab"
#endif

class KMountPointPrivate
{
public:
    void finalizePossibleMountPoint(KMountPoint::DetailsNeededFlags infoNeeded);
    void finalizeCurrentMountPoint(KMountPoint::DetailsNeededFlags infoNeeded);

    QString m_mountedFrom;
    QString m_device; // Only available when the NeedRealDeviceName flag was set.
    QString m_mountPoint;
    QString m_mountType;
    QStringList m_mountOptions;
};

KMountPoint::KMountPoint()
    : d(new KMountPointPrivate)
{
}

KMountPoint::~KMountPoint() = default;

// There are (at least) four kind of APIs:
// setmntent + getmntent + struct mntent (linux...)
//             getmntent + struct mnttab
// getmntinfo + struct statfs&flags (BSD 4.4 and friends)
// getfsent + char* (BSD 4.3 and friends)

#if HAVE_SETMNTENT
#define SETMNTENT setmntent
#define ENDMNTENT endmntent
#define STRUCT_MNTENT struct mntent *
#define STRUCT_SETMNTENT FILE *
#define GETMNTENT(file, var) ((var = getmntent(file)) != nullptr)
#define MOUNTPOINT(var) var->mnt_dir
#define MOUNTTYPE(var) var->mnt_type
#define MOUNTOPTIONS(var) var->mnt_opts
#define FSNAME(var) var->mnt_fsname
#else
#define SETMNTENT fopen
#define ENDMNTENT fclose
#define STRUCT_MNTENT struct mnttab
#define STRUCT_SETMNTENT FILE *
#define GETMNTENT(file, var) (getmntent(file, &var) == nullptr)
#define MOUNTPOINT(var) var.mnt_mountp
#define MOUNTTYPE(var) var.mnt_fstype
#define MOUNTOPTIONS(var) var.mnt_mntopts
#define FSNAME(var) var.mnt_special
#endif

void KMountPointPrivate::finalizePossibleMountPoint(KMountPoint::DetailsNeededFlags infoNeeded)
{
    if (m_mountedFrom.startsWith(QLatin1String("UUID="))) {
        const QStringRef uuid = m_mountedFrom.midRef(5);
        const QString potentialDevice = QFile::symLinkTarget(QLatin1String("/dev/disk/by-uuid/") + uuid);
        if (QFile::exists(potentialDevice)) {
            m_mountedFrom = potentialDevice;
        }
    }
    if (m_mountedFrom.startsWith(QLatin1String("LABEL="))) {
        const QStringRef label = m_mountedFrom.midRef(6);
        const QString potentialDevice = QFile::symLinkTarget(QLatin1String("/dev/disk/by-label/") + label);
        if (QFile::exists(potentialDevice)) {
            m_mountedFrom = potentialDevice;
        }
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
#ifdef Q_OS_WIN
    return KMountPoint::currentMountPoints(infoNeeded);
#endif

    KMountPoint::List result;

#if HAVE_SETMNTENT
    STRUCT_SETMNTENT fstab;
    if ((fstab = SETMNTENT(FSTAB, "r")) == nullptr) {
        return result;
    }

    STRUCT_MNTENT fe;
    while (GETMNTENT(fstab, fe)) {
        if (QByteArray(MOUNTTYPE(fe)) == "swap") {
            continue;
        }
        Ptr mp(new KMountPoint);
        mp->d->m_mountedFrom = QFile::decodeName(FSNAME(fe));

        mp->d->m_mountPoint = QFile::decodeName(MOUNTPOINT(fe));
        mp->d->m_mountType = QFile::decodeName(MOUNTTYPE(fe));

        if (infoNeeded & NeedMountOptions) {
            QString options = QFile::decodeName(MOUNTOPTIONS(fe));
            mp->d->m_mountOptions = options.split(QLatin1Char(','));
        }

        mp->d->finalizePossibleMountPoint(infoNeeded);

        result.append(mp);
    }
    ENDMNTENT(fstab);
#else
    QFile f(QLatin1String(FSTAB));
    if (!f.open(QIODevice::ReadOnly)) {
        return result;
    }

    QTextStream t(&f);
    QString s;

    while (! t.atEnd()) {
        s = t.readLine().simplified();
        if (s.isEmpty() || (s[0] == QLatin1Char('#'))) {
            continue;
        }

        // not empty or commented out by '#'
        const QStringList item = s.split(QLatin1Char(' '));

#ifdef _OS_SOLARIS_
        if (item.count() < 5) {
            continue;
        }
#else
        if (item.count() < 4) {
            continue;
        }
#endif

        Ptr mp(new KMountPoint);

        int i = 0;
        mp->d->m_mountedFrom = item[i++];
#ifdef _OS_SOLARIS_
        //device to fsck
        i++;
#endif
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
    } //while

    f.close();
#endif
    return result;
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

    for (int i = 0; i < num_fs; i++) {
        Ptr mp(new KMountPoint);
        mp->d->m_mountedFrom = QFile::decodeName(mounted[i].f_mntfromname);
        mp->d->m_mountPoint = QFile::decodeName(mounted[i].f_mntonname);

#ifdef __osf__
        mp->d->m_mountType = QFile::decodeName(mnt_names[mounted[i].f_type]);
#else
        mp->d->m_mountType = QFile::decodeName(mounted[i].f_fstypename);
#endif

        if (infoNeeded & NeedMountOptions) {
            struct fstab *ft = getfsfile(mounted[i].f_mntonname);
            if (ft != nullptr) {
                QString options = QFile::decodeName(ft->fs_mntops);
                mp->d->m_mountOptions = options.split(QLatin1Char(','));
            } else {
                // TODO: get mount options if not mounted via fstab, see mounted[i].f_flags
            }
        }

        mp->d->finalizeCurrentMountPoint(infoNeeded);
        // TODO: Strip trailing '/' ?
        result.append(mp);
    }

#elif defined(Q_OS_WIN)
    //nothing fancy with infoNeeded but it gets the job done
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

#elif !defined(Q_OS_ANDROID)
    STRUCT_SETMNTENT mnttab;
    if ((mnttab = SETMNTENT(MNTTAB, "r")) == nullptr) {
        return result;
    }

    STRUCT_MNTENT fe;
    while (GETMNTENT(mnttab, fe)) {
        Ptr mp(new KMountPoint);
        mp->d->m_mountedFrom = QFile::decodeName(FSNAME(fe));

        mp->d->m_mountPoint = QFile::decodeName(MOUNTPOINT(fe));
        mp->d->m_mountType = QFile::decodeName(MOUNTTYPE(fe));

        if (infoNeeded & NeedMountOptions) {
            QString options = QFile::decodeName(MOUNTOPTIONS(fe));
            mp->d->m_mountOptions = options.split(QLatin1Char(','));
        }

        // Resolve gvfs mountpoints
        if (mp->d->m_mountedFrom == QLatin1String("gvfsd-fuse")) {
            const QDir gvfsDir(mp->d->m_mountPoint);
            const QStringList mountDirs = gvfsDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QString &mountDir : mountDirs) {
                const QString type = mountDir.section(QLatin1Char(':'), 0, 0);
                if (type.isEmpty()) {
                    continue;
                }

                Ptr gvfsmp(new KMountPoint);
                gvfsmp->d->m_mountedFrom = mp->d->m_mountedFrom;
                gvfsmp->d->m_mountPoint = mp->d->m_mountPoint + QLatin1Char('/') + mountDir;
                gvfsmp->d->m_mountType = type;
                result.append(gvfsmp);
            }
        }

        mp->d->finalizeCurrentMountPoint(infoNeeded);

        result.append(mp);
    }
    ENDMNTENT(mnttab);
#endif
    return result;
}

QString KMountPoint::mountedFrom() const
{
    return d->m_mountedFrom;
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

static bool pathsAreParentAndChildOrEqual(const QString &parent, const QString &child)
{
    const QLatin1Char slash('/');
    if (child.startsWith(parent, cs)) {
        // Check if either
        // (a) both paths are equal, or
        // (b) parent ends with '/', or
        // (c) the first character of child that is not shared with parent is '/'.
        //     Note that child is guaranteed to be longer than parent if (a) is false.
        //
        // This prevents that we incorrectly consider "/books" a child of "/book".
        return parent.compare(child, cs) == 0 || parent.endsWith(slash) || child.at(parent.length()) == slash;
    } else {
        // Note that "/books" is a child of "/books/".
        return parent.endsWith(slash) && (parent.length() == child.length() + 1) && parent.startsWith(child, cs);
    }
}

KMountPoint::Ptr KMountPoint::List::findByPath(const QString &path) const
{
#ifndef Q_OS_WIN
    /* If the path contains symlinks, get the real name */
    QFileInfo fileinfo(path);
    const QString realname = fileinfo.exists()
                             ? fileinfo.canonicalFilePath()
                             : fileinfo.absolutePath(); //canonicalFilePath won't work unless file exists
#else
    const QString realname = QDir::fromNativeSeparators(QDir(path).absolutePath());
#endif

    int max = 0;
    KMountPoint::Ptr result;
    for (const KMountPoint::Ptr &mp : *this) {
        const QString mountpoint = mp->d->m_mountPoint;
        const int length = mountpoint.length();
        if (length > max && pathsAreParentAndChildOrEqual(mountpoint, realname)) {
            max = length;
            result = mp;
            // keep iterating to check for a better match (bigger max)
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
        if (realDevice.compare(mountPoint->d->m_device, cs) == 0 ||
                realDevice.compare(mountPoint->d->m_mountedFrom, cs) == 0) {
            return mountPoint;
        }
    }
    return Ptr();
}

bool KMountPoint::probablySlow() const
{
    return d->m_mountType == QLatin1String("nfs")
        || d->m_mountType == QLatin1String("nfs4")
        || d->m_mountType == QLatin1String("cifs")
        || d->m_mountType == QLatin1String("autofs")
        || d->m_mountType == QLatin1String("subfs")
        // Technically KIOFUSe mounts local slaves as well,
        // such as recents:/, but better safe than sorry...
        || d->m_mountType == QLatin1String("fuse.kio-fuse");
}

bool KMountPoint::testFileSystemFlag(FileSystemFlag flag) const
{
    const bool isMsDos = (d->m_mountType == QLatin1String("msdos") || d->m_mountType == QLatin1String("fat") || d->m_mountType == QLatin1String("vfat"));
    const bool isNtfs = d->m_mountType.contains(QLatin1String("fuse.ntfs")) || d->m_mountType.contains(QLatin1String("fuseblk.ntfs"))
                        // fuseblk could really be anything. But its most common use is for NTFS mounts, these days.
                        || d->m_mountType == QLatin1String("fuseblk");
    const bool isSmb = d->m_mountType == QLatin1String("cifs")
                    || d->m_mountType == QLatin1String("smbfs")
                    // gvfs-fuse mounted SMB share
                    || d->m_mountType == QLatin1String("smb-share");

    switch (flag)  {
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
