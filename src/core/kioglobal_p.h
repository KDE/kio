/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Alex Richardson <arichardson.kde@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_KIOGLOBAL_P_H
#define KIO_KIOGLOBAL_P_H

#include "kiocore_export.h"
#include <qplatformdefs.h>

#include <KUser>
#include <Solid/SolidNamespace>

#ifdef Q_OS_WIN
// windows just sets the mode_t access rights bits to the same value for user+group+other.
// This means using the Linux values here is fine.
#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IRGRP
#define S_IRGRP 0040
#endif
#ifndef S_IROTH
#define S_IROTH 0004
#endif

#ifndef S_IWUSR
#define S_IWUSR 0200
#endif
#ifndef S_IWGRP
#define S_IWGRP 0020
#endif
#ifndef S_IWOTH
#define S_IWOTH 0002
#endif

#ifndef S_IXUSR
#define S_IXUSR 0100
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001
#endif

#ifndef S_IRWXU
#define S_IRWXU S_IRUSR | S_IWUSR | S_IXUSR
#endif
#ifndef S_IRWXG
#define S_IRWXG S_IRGRP | S_IWGRP | S_IXGRP
#endif
#ifndef S_IRWXO
#define S_IRWXO S_IROTH | S_IWOTH | S_IXOTH
#endif
Q_STATIC_ASSERT(S_IRUSR == _S_IREAD && S_IWUSR == _S_IWRITE && S_IXUSR == _S_IEXEC);

// these three will never be set in st_mode
#ifndef S_ISUID
#define S_ISUID 04000 // SUID bit does not exist on windows
#endif
#ifndef S_ISGID
#define S_ISGID 02000 // SGID bit does not exist on windows
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000 // sticky bit does not exist on windows
#endif

// Windows does not have S_IFBLK and S_IFSOCK, just use the Linux values, they won't conflict
#ifndef S_IFBLK
#define S_IFBLK 0060000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
/** performs a QT_STAT and add QT_STAT_LNK to st_mode if the path is a symlink */
KIOCORE_EXPORT int kio_windows_lstat(const char *path, QT_STATBUF *buffer);

#ifndef QT_LSTAT
#define QT_LSTAT kio_windows_lstat
#endif

#ifndef QT_STAT_LNK
#define QT_STAT_LNK 0120000
#endif // QT_STAT_LNK

#endif // Q_OS_WIN

namespace Solid
{
class Device;
}

namespace KIOPrivate
{
/** @return true if the process with given PID is currently running */
KIOCORE_EXPORT bool isProcessAlive(qint64 pid);
/** Send a terminate signal (SIGTERM on UNIX) to the process with given PID. */
KIOCORE_EXPORT void sendTerminateSignal(qint64 pid);

enum SymlinkType {
    GuessSymlinkType,
    FileSymlink,
    DirectorySymlink,
};

/** Creates a symbolic link at @p destination pointing to @p source
 * Unlike UNIX, Windows needs to know whether the symlink points to a file or a directory
 * when creating the link. This information can be passed in @p type. If @p type is not given
 * the windows code will guess the type based on the source file.
 * @note On Windows this requires the current user to have the SeCreateSymbolicLink privilege which
 * is usually only given to administrators.
 * @return true on success, false on error
 */
KIOCORE_EXPORT bool createSymlink(const QString &source, const QString &destination, SymlinkType type = GuessSymlinkType);

/** Changes the ownership of @p file (like chown()) */
KIOCORE_EXPORT bool changeOwnership(const QString &file, KUserId newOwner, KGroupId newGroup);

class KFileItemIconCache : public QObject
{
public:
    static KFileItemIconCache *instance();

    /**
     * Gets the corresponding icon name for a mount point.
     *
     * @param localDirectory the path of the local directory
     * @return the icon name for the mount point
     * @see initializeMountPointsMap
     */
    QString iconForMountPoint(const QString &localDirectory);

    /**
     * Returns an icon name for a standard path, e.g. folder-pictures for any path in
     * QStandardPaths::PicturesLocation.
     *
     * @param localDirectory the path of the local directory
     * @return the icon name for the standard path
     * @see initializeStandardLocationsMap
     */
    QString iconForStandardPath(const QString &localDirectory);

private Q_SLOTS:
    /**
     * The function serves as a signal receiver of Solid::DeviceNotifier::deviceAdded.
     *
     * @param udi the UDI of the volume
     * @see refreshStorageAccess
     */
    void slotDeviceAdded(const QString &udi);

    /**
     * The function serves as a signal receiver of Solid::DeviceNotifier::deviceRemoved.
     *
     * @param udi the UDI of the volume
     * @see slotUpdateMountPointsMap
     */
    void slotDeviceRemoved(const QString &udi);

    /**
     * Updates a mount point and stores the corresponding icon name to the map if
     * a mount point is mounted, or removes the stored icon name from the map if a
     * mount point is unmounted.
     *
     * @param error type of error that occurred, if any
     * @param errorData more information about the error, if any
     * @param udi the UDI of the volume
     */
    void slotUpdateMountPointsMap(Solid::ErrorType error, const QVariant &errorData, const QString &udi);

private:
    KFileItemIconCache();
    ~KFileItemIconCache() = default;
    KFileItemIconCache(const KFileItemIconCache &instance);
    const KFileItemIconCache &operator=(const KFileItemIconCache &instance);

    /**
     * Retrieves devices on which StorageAccess is available, and prepares for
     * updating the mount points map.
     *
     * @see iconForMountPoint
     */
    void initializeMountPointsMap();

    /**
     * Retrieves standard locations of the system, and prepares for updating
     * the standard locations map.
     *
     * @see iconForStandardPath
     */
    void initializeStandardLocationsMap();

    /**
     * When a device appears in the underlying system, if the device type is
     * Solid::StorageAccess, when setting up of this device is completed,
     * or when tearing down of this device is completed, update the mount points
     * map.
     *
     * @param device the device for the given UDI
     */
    void refreshStorageAccess(const Solid::Device &device);

    QMap<QString /* mount point */, QString /* icon name */> m_mountPointToIconProxyMap;
    QMap<QString /* standard location */, QString /* icon name */> m_standardLocationsMap;

    /**
     * This map serves as a proxy from a UDI to a mount point. Because removable devices can
     * be forcedly disconnected from the host, the storageAccess pointer could be nullptr, so
     * it's required to store an item with the key UDI and a value of the mount point.
     */
    QMap<QString /* udi */, QString /* mount point */> m_udiToMountPointProxyMap;
};
}

#endif // KIO_KIOGLOBAL_P_H
