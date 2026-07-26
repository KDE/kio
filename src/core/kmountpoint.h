/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMOUNTPOINT_H
#define KMOUNTPOINT_H

#include "kiocore_export.h"

#include <QExplicitlySharedDataPointer>
#include <QStringList>

#include <memory>
#include <sys/types.h> // dev_t among other definitions

class KMountPointPrivate;

/*!
 * \class KMountPoint
 * \inmodule KIOCore
 *
 * \brief The KMountPoint class provides information about mounted and unmounted disks.
 *
 * It provides a system independent interface to fstab.
 */
class KIOCORE_EXPORT KMountPoint : public QSharedData
{
public:
    using Ptr = QExplicitlySharedDataPointer<KMountPoint>;

    /*!
     * List of mount points.
     * \inmodule KIOCore
     */
    class KIOCORE_EXPORT List : public QList<Ptr>
    {
    public:
        List();
        /*!
         * Find the mountpoint on which resides \a path
         * For instance if /home is a separate partition, findByPath("/home/user/blah")
         * will return /home
         *
         * \a path the path to check
         *
         * Returns the mount point of the given file
         */
        Ptr findByPath(const QString &path) const;

        /*!
         * Returns the mount point associated with \a device,
         * i.e. the one where mountedFrom() == \a device
         * (after symlink resolution).
         *
         * Returns the mountpoint, or \c nullptr if this device doesn't exist or isn't mounted
         */
        Ptr findByDevice(const QString &device) const;

        /*!
         * Returns the mount point associated with \a mountId
         *
         * Returns the mountpoint, or \c nullptr if there is no mount point associated with this ID.
         *
         * This is prefered over looking up a path because it does not have to deal with symlinks.
         *
         * \note This is only supported on Linux and returns \c nullptr for all other platforms
         * \warning mountId must not be zero
         *
         * \since 6.24
         */
        Ptr findByMountId(quint64 mountId) const;
    };

public:
    /*!
     * Flags that specify which additional details should be fetched for each mountpoint.
     *
     * \value BasicInfoNeeded Only the basic details: mountedFrom, mountPoint, mountType.
     * \value NeedMountOptions Also fetch the options used when mounting, see KMountPoint::mountOptions().
     * \value NeedRealDeviceName Also fetch the device name (with symlinks resolved), see KMountPoint::realDeviceName().
     */
    enum DetailsNeededFlag {
        BasicInfoNeeded = 0,
        NeedMountOptions = 1,
        NeedRealDeviceName = 2,
    };
    Q_DECLARE_FLAGS(DetailsNeededFlags, DetailsNeededFlag)

    /*!
     * This function gives a list of all possible mountpoints. (fstab)
     *
     * \a infoNeeded Flags that specify which additional information
     * should be fetched.
     */
    static List possibleMountPoints(DetailsNeededFlags infoNeeded = BasicInfoNeeded);

    /*!
     * Returns a list of all current mountpoints.
     *
     * \a infoNeeded Flags that specify which additional information
     * should be fetched.
     *
     * \note This method will return an empty list on Android
     */
    static List currentMountPoints(DetailsNeededFlags infoNeeded = BasicInfoNeeded);

    /*!
     * Returns the current mount point that has the given unique mount id, as
     * reported by statx() with STATX_MNT_ID_UNIQUE (for example the value stored
     * in UDSEntry::UDS_MOUNT_ID).
     *
     * The result is served from a small process-wide cache. A unique mount id is
     * never reused while the system is running and always denotes the same mount,
     * so the id-to-mount mapping is stable and the mount table is only re-read
     * when an id that is not yet cached is looked up. This avoids re-parsing
     * /proc/self/mountinfo for every item when many files on the same mount are
     * looked up in a row.
     *
     * The cached KMountPoint is a snapshot. "mount --move" (which changes
     * mountPoint()) and "mount -o remount" (which changes mountOptions()) keep the
     * same unique id, so those two fields can be out of date until the entry is
     * evicted. Fields that do not change for the life of a mount, such as the
     * filesystem type and probablySlow()/isOnNetwork(), stay correct.
     *
     * Returns the mount point, or nullptr if no current mount has this id.
     *
     * \note This is only useful on Linux; elsewhere it re-reads the mount table
     * on every call, like currentMountPoints().
     * \warning uniqueMountId must not be zero.
     *
     * \since 6.29
     */
    static Ptr currentMountPointForUniqueId(quint64 uniqueMountId);

    /*!
     * Returns the current mount point that \a path resides on, using the same cache
     * as currentMountPointForUniqueId(): the path's unique mount id is resolved with
     * a single statx() and looked up in the cache, so repeated lookups under the same
     * mount do not re-read /proc/self/mountinfo. Falls back to
     * currentMountPoints().findByPath() when the kernel does not provide a unique
     * mount id.
     *
     * Returns the mount point, or nullptr if none matches.
     *
     * \since 6.29
     */
    static Ptr currentMountPointForPath(const QString &path);

    /*!
     * Where this filesystem gets mounted from.
     * This can refer to a device, a remote server or something else.
     */
    QString mountedFrom() const;

    /*!
     * Returns \c true if this mount point represents a network filesystem (e.g.\ NFS,
     * CIFS, etc.), otherwise returns \c false.
     *
     * \since 5.86
     */
    bool isOnNetwork() const;

    /*!
     * Returns \c true if this mount points to a pseudo filesystem (e.g. devfs, proc,
     * tmpfs, fuse, etc.), otherwise returns \c false.
     *
     * \since 6.24
     */
    bool isPseudoFs() const;

    /*!
     * Returns \c true if this mount points to a filesystem that is encrypting its
     * content (e.g. gocryptfs), otherwise returns \c false.
     *
     * \since 6.24
     */
    bool isEncryptedFs() const;

    /*!
     * Returns the device ID (dev_t, major, minor) of this mount point. This
     * ID is unique per device (including network mounts).
     *
     * \since 5.86
     */
    dev_t deviceId() const;

    /*!
     * Returns the mount ID of this mount point.
     *
     * If the system supports MNT_ID_UNIQUE, it will be returned. It is
     * guaranteed to be unique until the next boot.
     * If the system doesn't support that, the MNT_ID property will be returned.
     * It is unique for a point in time, but is not guaranteed to be unique
     * until next boot.
     *
     * On systems without statmount() (exists since Linux 6.8) this will return 0.
     *
     * \since 6.24
     */
    quint64 mountId() const;

    /*!
     * Canonical name of the device where the filesystem got mounted from.
     * (Or empty, if not a device)
     * Only available when the NeedRealDeviceName flag was set.
     */
    QString realDeviceName() const;

    /*!
     * Path where the filesystem is mounted (if you used currentMountPoints()),
     * or can be mounted (if you used possibleMountPoints()).
     */
    QString mountPoint() const;

    /*!
     * Type of filesystem
     */
    QString mountType() const;

    /*!
     * Options used to mount the filesystem.
     * Only available if the NeedMountOptions flag was set.
     */
    QStringList mountOptions() const;

    /*!
     * Returns \c true if the filesystem is "probably" slow, e.g. a network mount,
     * \c false otherwise.
     */
    bool probablySlow() const;

    /*!
     * \value SupportsChmod
     * \value SupportsChown
     * \value SupportsUTime
     * \value SupportsSymlinks
     * \value CaseInsensitive
     */
    enum FileSystemFlag {
        SupportsChmod,
        SupportsChown,
        SupportsUTime,
        SupportsSymlinks,
        CaseInsensitive,
    };

    /*!
     * Checks the capabilities of the filesystem.
     *
     * \a flag the flag to check
     *
     * Returns \c true if the filesystem has that flag, false if not
     *
     * The available flags are:
     * \list
     * \li SupportsChmod: returns true if the filesystem supports chmod
     * (e.g. msdos filesystems return false)
     * \li SupportsChown: returns true if the filesystem supports chown
     * (e.g. msdos filesystems return false)
     * \li SupportsUtime: returns true if the filesystems supports utime
     * (e.g. msdos filesystems return false)
     * \li SupportsSymlinks: returns true if the filesystems supports symlinks
     * (e.g. msdos filesystems return false)
     * \li CaseInsensitive: returns true if the filesystem treats
     * "foo" and "FOO" as being the same file (true for msdos filesystems)
     * \endlist
     */
    bool testFileSystemFlag(FileSystemFlag flag) const;

    ~KMountPoint();

private:
    KIOCORE_NO_EXPORT KMountPoint();

    friend KIOCORE_EXPORT QDebug operator<<(QDebug debug, const Ptr &mp);

    friend KMountPointPrivate;
    std::unique_ptr<KMountPointPrivate> d;
};

KIOCORE_EXPORT QDebug operator<<(QDebug debug, const KMountPoint::Ptr &mp);

Q_DECLARE_OPERATORS_FOR_FLAGS(KMountPoint::DetailsNeededFlags)

#endif // KMOUNTPOINT_H
