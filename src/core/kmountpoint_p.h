/*
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMOUNTPOINT_P_H
#define KMOUNTPOINT_P_H

#include "kiocore_export.h"

#include <QString>

class KMountPoint;

// \internal not installed, exported for the autotests
namespace KMountPointPrivateSysfs
{
/*
 * Whether the block device @p deviceName, as named in /sys/class/block ("sdb1", "dm-2"), is on a device
 * that can be unplugged: removable media, a disk behind a USB port or a PCIe slot that the kernel marks
 * removable, or an SD card. A device-mapper or RAID device is removable when a device under it is.
 * @p sysRoot is where sysfs is mounted, "/sys" outside of tests.
 */
KIOCORE_EXPORT bool isRemovableBlockDevice(const QString &sysRoot, const QString &deviceName);
}

// \internal not installed, exported for the file worker
namespace KIOPrivate
{
/*
 * Whether @p mountPoint is on a device that can be unplugged: removable media such as a USB stick or a
 * memory card, or a disk behind a USB port or a PCIe slot that the system marks as removable. An
 * encrypted or LVM volume is on a removable device when the disk under it is.
 * Linux only, from sysfs, read at each call. Elsewhere, and for a mount that is not on a block device,
 * such as a network filesystem, it is false.
 */
KIOCORE_EXPORT bool isOnRemovableDevice(const KMountPoint &mountPoint);
}

#endif
