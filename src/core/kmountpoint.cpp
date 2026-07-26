/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kmountpoint.h"

#include <stdlib.h>

#include "../utils_p.h"
#include <config-kmountpoint.h>
#include <kioglobal_p.h> // Defines QT_LSTAT on windows to kio_windows_lstat

#include <QCache>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGlobalStatic>
#include <QMutex>
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
#include <sys/sysmacros.h>
#if HAVE_STATX_MNT_ID
#include <fcntl.h>
#include <sys/stat.h>
#endif
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

    return std::ranges::any_of(netfsList, [mountType](const QLatin1String &netfs) {
        return mountType == netfs;
    });
}

static bool isPseudoFs(const QString &mountType)
{
    // List copied from util-linux/libmount/src/utils.c mnt_fstype_is_pseudofs
    static const std::vector<QLatin1String> pseudofsList{
        QLatin1String("anon_inodefs"),
        QLatin1String("apparmorfs"),
        QLatin1String("autofs"),
        QLatin1String("bdev"),
        QLatin1String("binder"),
        QLatin1String("binfmt_misc"),
        QLatin1String("bpf"),
        QLatin1String("cgroup"),
        QLatin1String("cgroup2"),
        QLatin1String("configfs"),
        QLatin1String("cpuset"),
        QLatin1String("debugfs"),
        QLatin1String("devfs"),
        QLatin1String("devpts"),
        QLatin1String("devtmpfs"),
        QLatin1String("dlmfs"),
        QLatin1String("dmabuf"),
        QLatin1String("drm"),
        QLatin1String("efivarfs"),
        QLatin1String("fuse"),
        QLatin1String("fuse.archivemount"),
        QLatin1String("fuse.avfsd"),
        QLatin1String("fuse.dumpfs"),
        QLatin1String("fuse.encfs"),
        QLatin1String("fuse.gvfs-fuse-daemon"),
        QLatin1String("fuse.gvfsd-fuse"),
        QLatin1String("fuse.kio-fuse"),
        QLatin1String("fuse.lxcfs"),
        QLatin1String("fuse.portal"),
        QLatin1String("fuse.rofiles-fuse"),
        QLatin1String("fuse.vmware-vmblock"),
        QLatin1String("fuse.xwmfs"),
        QLatin1String("fusectl"),
        QLatin1String("hugetlbfs"),
        QLatin1String("ipathfs"),
        QLatin1String("mqueue"),
        QLatin1String("nfsd"),
        QLatin1String("none"),
        QLatin1String("nsfs"),
        QLatin1String("overlay"),
        QLatin1String("pidfs"),
        QLatin1String("pipefs"),
        QLatin1String("proc"),
        QLatin1String("pstore"),
        QLatin1String("ramfs"),
        QLatin1String("resctrl"),
        QLatin1String("rootfs"),
        QLatin1String("rpc_pipefs"),
        QLatin1String("securityfs"),
        QLatin1String("selinuxfs"),
        QLatin1String("smackfs"),
        QLatin1String("sockfs"),
        QLatin1String("spufs"),
        QLatin1String("sysfs"),
        QLatin1String("tmpfs"),
        QLatin1String("tracefs"),
        QLatin1String("vboxsf"),
        QLatin1String("virtiofs"),
    };

    return std::ranges::any_of(pseudofsList, [mountType](const QLatin1String &pseudofs) {
        return mountType == pseudofs;
    });
}

class KMountPointPrivate
{
public:
    void resolveGvfsMountPoints(KMountPoint::List &result);
    void finalizePossibleMountPoint(KMountPoint::DetailsNeededFlags infoNeeded);
    void setAdditionalDetails(KMountPoint::DetailsNeededFlags infoNeeded);

    QString m_mountedFrom;
    QString m_device; // Only available when the NeedRealDeviceName flag was set.
    QString m_mountPoint;
    QString m_mountType;
    QStringList m_mountOptions;
    dev_t m_deviceId = 0;
    // The unique mount id (STATX_MNT_ID_UNIQUE) when the kernel provides it, 0 otherwise.
    // The reusable STATX_MNT_ID is not stored, so a non-zero value always identifies the
    // same mount for the life of the system and is safe to cache.
    quint64 m_mountId = 0;
    bool m_isNetFs = false;
    bool m_isPseudoFs = false;
};

KMountPoint::KMountPoint()
    : d(new KMountPointPrivate)
{
}

KMountPoint::~KMountPoint() = default;

#if HAVE_GETMNTINFO

#ifdef MNTOPT_NAMES
static struct mntoptnames bsdOptionNames[] = {MNTOPT_NAMES};

/* Get mount options from flags and puts human-readable version in list
 *
 * Appends all positive options found in flags to the list
 * This is roughly paraphrased from FreeBSD's mount.c, prmount().
 */
static void translateMountOptions(QStringList &list, uint64_t flags)
{
    const struct mntoptnames *optionInfo = bsdOptionNames;

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
/* Get mount options from flags and puts human-readable version in list
 *
 * This default version just puts the hex representation of flags
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

    setAdditionalDetails(infoNeeded);

    // Chop trailing slash
    Utils::removeTrailingSlash(m_mountedFrom);
}

void KMountPointPrivate::setAdditionalDetails(KMountPoint::DetailsNeededFlags infoNeeded)
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
                mp->d->m_isNetFs = isNetfs(mp->d->m_mountType);
                mp->d->m_isPseudoFs = ::isPseudoFs(mp->d->m_mountType);
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
        mp->d->m_isNetFs = isNetfs(mp->d->m_mountType);
        mp->d->m_isPseudoFs = ::isPseudoFs(mp->d->m_mountType);
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
            gvfsmp->d->m_isNetFs = true;
            gvfsmp->d->m_isPseudoFs = true;
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
        mp->d->m_isNetFs = isNetfs(mp->d->m_mountType);
        mp->d->m_isPseudoFs = ::isPseudoFs(mp->d->m_mountType);

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

        mp->d->setAdditionalDetails(infoNeeded);
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
        if (
#if QT_VERSION_CHECK(LIBMOUNT_MAJOR_VERSION, LIBMOUNT_MINOR_VERSION, LIBMOUNT_PATCH_VERSION) >= QT_VERSION_CHECK(2, 39, 0)
            mnt_table_parse_mtab(table, nullptr)
#else // backwards compat, the documentation advises to use nullptr so lets do that whenever possible
            mnt_table_parse_mtab(table, "/proc/self/mountinfo")
#endif
            == 0) {
            struct libmnt_iter *itr = mnt_new_iter(MNT_ITER_BACKWARD);
            struct libmnt_fs *fs;

            while (mnt_table_next_fs(table, itr, &fs) == 0) {
                Ptr mp(new KMountPoint);
                const char *target = mnt_fs_get_target(fs);
                mp->d->m_mountPoint = QFile::decodeName(target);
                mp->d->m_mountedFrom = QFile::decodeName(mnt_fs_get_source(fs));
                mp->d->m_mountType = QFile::decodeName(mnt_fs_get_fstype(fs));
                mp->d->m_isNetFs = mnt_fs_is_netfs(fs) == 1;
                mp->d->m_isPseudoFs = mnt_fs_is_pseudofs(fs) == 1 || mp->d->m_mountType == QLatin1String("fuse.kio-fuse");

                uint mask_mnt_id = 0;
#if HAVE_STATX_MNT_ID_UNIQUE
                // Only the unique mount id, which is never reused while the system is up and
                // is therefore safe to cache. A non-zero m_mountId always means a unique id;
                // on a kernel too old to provide it statx clears the bit and m_mountId stays 0.
                mask_mnt_id = STATX_MNT_ID_UNIQUE;
#elif HAVE_STATX_MNT_ID
                mask_mnt_id = STATX_MNT_ID;
#endif

                if (struct statx buff; statx(AT_FDCWD, target, AT_STATX_DONT_SYNC | AT_NO_AUTOMOUNT, STATX_INO | mask_mnt_id, &buff) == 0) {
                    mp->d->m_deviceId = makedev(buff.stx_dev_major, buff.stx_dev_minor);
#if HAVE_STATX_MNT_ID
                    if (buff.stx_mask & mask_mnt_id) {
                        mp->d->m_mountId = (quint64)buff.stx_mnt_id;
                    }
#endif
                }

                if (infoNeeded & NeedMountOptions) {
                    mp->d->m_mountOptions = QFile::decodeName(mnt_fs_get_options(fs)).split(QLatin1Char(','));
                }

                mp->d->resolveGvfsMountPoints(result);

                mp->d->setAdditionalDetails(infoNeeded);
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

quint64 KMountPoint::mountId() const
{
    return d->m_mountId;
}

bool KMountPoint::isOnNetwork() const
{
    return d->m_isNetFs;
}

bool KMountPoint::isPseudoFs() const
{
    return d->m_isPseudoFs;
}

bool KMountPoint::isEncryptedFs() const
{
    return d->m_mountType == QLatin1String("fuse.gocryptfs")
        || d->m_mountType == QLatin1String("fuse.encfs");
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
    const QString realPath = fileinfo.exists() && !fileinfo.isSymLink() ? fileinfo.canonicalFilePath() : fileinfo.absolutePath();
#endif

    KMountPoint::Ptr result;
#if HAVE_STATX_MNT_ID
    uint mask = STATX_MNT_ID;
#if HAVE_STATX_MNT_ID_UNIQUE
    mask |= STATX_MNT_ID_UNIQUE;
#endif
    // If we have statx, there is no need to guess. take the mount id from statx and get the mountpoint.
    if (struct statx buff; statx(0, QFile::encodeName(path).constData(), AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, mask, &buff) == 0 && (buff.stx_mask & mask)) {
        auto it = std::find_if(this->cbegin(), this->cend(), [&buff](const KMountPoint::Ptr &mountPtr) {
            return mountPtr->d->m_mountId == buff.stx_mnt_id;
        });

        if (it != this->cend()) {
            return *it;
        }
    }
#endif


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

KMountPoint::Ptr KMountPoint::List::findByMountId(quint64 mountId) const
{
#if HAVE_STATX_MNT_ID
    Q_ASSERT(mountId);
    for (const KMountPoint::Ptr &mountPoint : *this) {
        if (mountPoint->d->m_mountId == mountId) {
            return mountPoint;
        }
    }
#else
    Q_UNUSED(mountId)
#endif
    return Ptr();
}

namespace
{
// Small process-wide cache of unique-mount-id -> mount point. A STATX_MNT_ID_UNIQUE
// id is never reused while the system is running and always denotes the same mount, so
// a cached entry never points at a different mount; a miss means a mount we have not seen
// yet and the table is re-read once. QCache bounds the size and drops the least recently
// used entries. The cache only ever holds unique ids, so on a kernel without
// STATX_MNT_ID_UNIQUE it stays empty and every lookup falls through to a reparse.
//
// A mount can, however, keep its unique id while its mountPoint()/mountOptions() change
// under it ("mount --move", "mount -o remount"). To avoid serving a stale snapshot, a
// libmount monitor watches the kernel mount table and the entries still in the cache are
// refreshed from a single re-read whenever anything has changed since the last lookup.
struct MountIdCache {
    QMutex mutex;
    QCache<quint64, KMountPoint::Ptr> byUniqueId;
    // A copy touches its source and destination mount; browsing spans a few mounts.
    MountIdCache()
        : byUniqueId(16)
    {
    }

// The monitor only matters when the cache can hold entries, which needs a unique
// mount id; without STATX_MNT_ID_UNIQUE the cache stays empty and there is nothing
// to invalidate, so do not compile or arm it.
#if HAVE_LIB_MOUNT && HAVE_STATX_MNT_ID_UNIQUE
    struct libmnt_monitor *monitor = nullptr;
    bool monitorSetUp = false;

    ~MountIdCache()
    {
        if (monitor) {
            mnt_unref_monitor(monitor);
        }
    }

    // Called with mutex held. When the mount table has changed since the last check,
    // refresh the entries we currently hold from a single re-read: a mount keeps its
    // unique id across "mount --move"/"mount -o remount", so its mountPoint()/
    // mountOptions() may have changed, and a mount that went away is dropped. Only
    // bothers when the cache actually stores entries.
    void refreshCacheIfMountsChanged()
    {
        if (byUniqueId.maxCost() <= 1) {
            return;
        }
        if (!monitorSetUp) {
            monitorSetUp = true;
            monitor = mnt_new_monitor();
            if (monitor && mnt_monitor_enable_kernel(monitor, 1) < 0) {
                mnt_unref_monitor(monitor);
                monitor = nullptr;
            }
        }
        if (!monitor) {
            return;
        }
        // Non-blocking poll: has anything changed since we last drained the monitor?
        if (mnt_monitor_wait(monitor, 0) <= 0) {
            return;
        }
        // Drain the queued events so the monitor is armed for the next change.
        const char *filename = nullptr;
        int eventType = 0;
        while (mnt_monitor_next_change(monitor, &filename, &eventType) == 0) { }
        // Re-read the table once and update only the ids we still hold.
        const QList<quint64> cachedIds = byUniqueId.keys();
        if (cachedIds.isEmpty()) {
            return;
        }
        const KMountPoint::List mounts = KMountPoint::currentMountPoints();
        for (const quint64 id : cachedIds) {
            if (const KMountPoint::Ptr fresh = mounts.findByMountId(id)) {
                byUniqueId.insert(id, new KMountPoint::Ptr(fresh));
            } else {
                byUniqueId.remove(id);
            }
        }
    }
#else
    void refreshCacheIfMountsChanged()
    {
    }
#endif
};
}
Q_GLOBAL_STATIC(MountIdCache, s_mountIdCache)

KMountPoint::Ptr KMountPoint::currentMountPointForUniqueId(quint64 uniqueMountId)
{
    if (uniqueMountId == 0) {
        return Ptr();
    }
#if HAVE_STATX_MNT_ID_UNIQUE
    MountIdCache *cache = s_mountIdCache();
    {
        QMutexLocker locker(&cache->mutex);
        cache->refreshCacheIfMountsChanged();
        if (Ptr *hit = cache->byUniqueId.object(uniqueMountId)) {
            return *hit;
        }
    }
    // Miss: re-read the mount table once (done outside the lock) and look the id up.
    const List mounts = currentMountPoints();
    const Ptr mp = mounts.findByMountId(uniqueMountId);
    if (mp) {
        // m_mountId only ever holds a unique id (see currentMountPoints()), so a match
        // here will keep denoting this same mount and is safe to cache.
        QMutexLocker locker(&cache->mutex);
        cache->byUniqueId.insert(uniqueMountId, new Ptr(mp));
    }
    return mp;
#else
    return currentMountPoints().findByMountId(uniqueMountId);
#endif
}

KMountPoint::Ptr KMountPoint::currentMountPointForPath(const QString &path)
{
#if HAVE_STATX_MNT_ID_UNIQUE
    // Resolve the path's unique mount id with a single statx and serve it from the id
    // cache, so repeated lookups under the same mount do not re-read the mount table.
    if (struct statx buff; statx(AT_FDCWD, QFile::encodeName(path).constData(), AT_SYMLINK_NOFOLLOW | AT_NO_AUTOMOUNT, STATX_MNT_ID_UNIQUE, &buff) == 0
        && (buff.stx_mask & STATX_MNT_ID_UNIQUE)) {
        return currentMountPointForUniqueId(buff.stx_mnt_id);
    }
#endif
    return currentMountPoints().findByPath(path);
}

bool KMountPoint::probablySlow() const
{
    /* clang-format off */
    return isOnNetwork()
        || d->m_mountType == QLatin1String("autofs")
        || d->m_mountType == QLatin1String("subfs")
        // Technically KIOFUSe mounts local workers as well,
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

KIOCORE_EXPORT QDebug operator<<(QDebug debug, const KMountPoint::Ptr &mp)
{
    QDebugStateSaver saver(debug);
    if (!mp) {
        debug << "QDebug operator<< called on a null KMountPoint::Ptr";
        return debug;
    }

    // clang-format off
    debug.nospace() << "KMountPoint ["
                    << "Mounted from: "  << mp->d->m_mountedFrom
                    << ", device name: " << mp->d->m_device
                    << ", mount point: " << mp->d->m_mountPoint
                    << ", mount type: "  << mp->d->m_mountType
                    << ", device id: "   << mp->d->m_deviceId
                    << ", mount id: "    << mp->d->m_mountId
                    <<']';

    // clang-format on
    return debug;
}
