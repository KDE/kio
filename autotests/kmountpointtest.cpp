/*
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kmountpointtest.h"

#include "kmountpoint.h"
#include "kmountpoint_p.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <limits>
#include <qplatformdefs.h>

#ifdef Q_OS_LINUX
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif

#ifdef Q_OS_LINUX
namespace
{
void writeSysfsAttribute(const QString &path, const QByteArray &value)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(value + '\n');
}

// A block device at @p devicePath under devices/, with its link in class/block like the kernel makes it.
void addBlockDevice(const QString &sysRoot, const QString &devicePath, const QString &name)
{
    QVERIFY(QDir().mkpath(sysRoot + QLatin1String("/devices/") + devicePath));
    QVERIFY(QDir().mkpath(sysRoot + QLatin1String("/class/block")));
    QVERIFY(QFile::link(sysRoot + QLatin1String("/devices/") + devicePath, sysRoot + QLatin1String("/class/block/") + name));
}

// A fake sysfs with the devices of the testRemovableBlockDevice() rows.
void createSysfs(const QString &sysRoot)
{
    const QString devices = sysRoot + QLatin1String("/devices/");

    // A USB stick that reports no removable media, plugged into a port that can be reached from outside.
    const QString usbStick = QStringLiteral("pci0000:00/0000:00:14.0/usb2/2-1/2-1:1.0/host6/target6:0:0/6:0:0:0/block/sdb");
    writeSysfsAttribute(devices + QLatin1String("pci0000:00/0000:00:14.0/usb2/2-1/removable"), "removable");
    addBlockDevice(sysRoot, usbStick, QStringLiteral("sdb"));
    writeSysfsAttribute(devices + usbStick + QLatin1String("/removable"), "0");
    addBlockDevice(sysRoot, usbStick + QLatin1String("/sdb1"), QStringLiteral("sdb1"));
    writeSysfsAttribute(devices + usbStick + QLatin1String("/sdb1/partition"), "1");

    // An internal SATA disk.
    const QString sata = QStringLiteral("pci0000:00/0000:00:17.0/ata1/host0/target0:0:0/0:0:0:0/block/sda");
    writeSysfsAttribute(devices + QLatin1String("pci0000:00/0000:00:17.0/removable"), "fixed");
    addBlockDevice(sysRoot, sata, QStringLiteral("sda"));
    writeSysfsAttribute(devices + sata + QLatin1String("/removable"), "0");
    addBlockDevice(sysRoot, sata + QLatin1String("/sda1"), QStringLiteral("sda1"));
    writeSysfsAttribute(devices + sata + QLatin1String("/sda1/partition"), "1");

    // A card reader on an internal USB port, which reports removable media.
    const QString cardReader = QStringLiteral("pci0000:00/0000:00:14.0/usb1/1-5/1-5:1.0/host7/target7:0:0/7:0:0:0/block/sdc");
    writeSysfsAttribute(devices + QLatin1String("pci0000:00/0000:00:14.0/usb1/1-5/removable"), "fixed");
    addBlockDevice(sysRoot, cardReader, QStringLiteral("sdc"));
    writeSysfsAttribute(devices + cardReader + QLatin1String("/removable"), "1");

    // An SD card and an eMMC, both on an MMC host.
    const QString sdCard = QStringLiteral("platform/fe320000.mmc/mmc_host/mmc0/mmc0:aaaa/block/mmcblk0");
    writeSysfsAttribute(devices + QLatin1String("platform/fe320000.mmc/mmc_host/mmc0/mmc0:aaaa/type"), "SD");
    addBlockDevice(sysRoot, sdCard, QStringLiteral("mmcblk0"));
    writeSysfsAttribute(devices + sdCard + QLatin1String("/removable"), "0");
    addBlockDevice(sysRoot, sdCard + QLatin1String("/mmcblk0p1"), QStringLiteral("mmcblk0p1"));
    writeSysfsAttribute(devices + sdCard + QLatin1String("/mmcblk0p1/partition"), "1");
    const QString emmc = QStringLiteral("platform/fe330000.mmc/mmc_host/mmc1/mmc1:0001/block/mmcblk1");
    writeSysfsAttribute(devices + QLatin1String("platform/fe330000.mmc/mmc_host/mmc1/mmc1:0001/type"), "MMC");
    addBlockDevice(sysRoot, emmc, QStringLiteral("mmcblk1"));
    writeSysfsAttribute(devices + emmc + QLatin1String("/removable"), "0");

    // LUKS volumes, one on the USB stick and one on the internal disk.
    for (const auto &[dm, under] : {std::pair{QStringLiteral("dm-0"), QStringLiteral("sdb1")}, std::pair{QStringLiteral("dm-1"), QStringLiteral("sda1")}}) {
        const QString dmPath = QStringLiteral("virtual/block/") + dm;
        addBlockDevice(sysRoot, dmPath, dm);
        QVERIFY(QDir().mkpath(devices + dmPath + QLatin1String("/slaves")));
        QVERIFY(QFile::link(sysRoot + QLatin1String("/class/block/") + under, devices + dmPath + QLatin1String("/slaves/") + under));
    }
}
}
#endif

void KMountPointTest::testRemovableBlockDevice_data()
{
    QTest::addColumn<QString>("deviceName");
    QTest::addColumn<bool>("removable");

    QTest::newRow("USB stick without removable media") << QStringLiteral("sdb") << true;
    QTest::newRow("partition of that USB stick") << QStringLiteral("sdb1") << true;
    QTest::newRow("internal disk") << QStringLiteral("sda") << false;
    QTest::newRow("partition of the internal disk") << QStringLiteral("sda1") << false;
    QTest::newRow("card reader with removable media") << QStringLiteral("sdc") << true;
    QTest::newRow("SD card partition") << QStringLiteral("mmcblk0p1") << true;
    QTest::newRow("eMMC") << QStringLiteral("mmcblk1") << false;
    QTest::newRow("LUKS on the USB stick") << QStringLiteral("dm-0") << true;
    QTest::newRow("LUKS on the internal disk") << QStringLiteral("dm-1") << false;
    QTest::newRow("unknown device") << QStringLiteral("sdz") << false;
    QTest::newRow("no device") << QString() << false;
}

void KMountPointTest::testRemovableBlockDevice()
{
#ifndef Q_OS_LINUX
    QSKIP("The removable state is read from sysfs, which only Linux has");
#else
    QFETCH(QString, deviceName);
    QFETCH(bool, removable);

    QTemporaryDir sysRoot;
    QVERIFY(sysRoot.isValid());
    createSysfs(sysRoot.path());
    if (QTest::currentTestFailed()) {
        return;
    }
    QCOMPARE(KMountPointPrivateSysfs::isRemovableBlockDevice(sysRoot.path(), deviceName), removable);
#endif
}

// A network filesystem has no block device, so it is not on a removable device.
void KMountPointTest::testNetworkMountIsNotRemovable()
{
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();
    for (const KMountPoint::Ptr &mountPoint : mountPoints) {
        if (mountPoint->isOnNetwork() || mountPoint->isPseudoFs()) {
            QVERIFY2(!KIOPrivate::isOnRemovableDevice(*mountPoint), qPrintable(mountPoint->mountPoint()));
        }
    }
}

QTEST_MAIN(KMountPointTest)

void KMountPointTest::initTestCase()
{
}

void KMountPointTest::testCurrentMountPoints()
{
    // on Windows we just create pseudo mount points for drives, that will fail the tests below
#ifdef Q_OS_WIN
    QSKIP("Skipping test on Windows, KMountPoint only has pseudo mounts for drive letters");
#endif

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName);
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("mtab is empty");
        return;
    }
    KMountPoint::Ptr mountWithDevice;
    for (KMountPoint::Ptr mountPoint : mountPoints) {
        qDebug() << mountPoint;
        QVERIFY(!mountPoint->mountedFrom().isEmpty());
        QVERIFY(!mountPoint->mountPoint().isEmpty());
        QVERIFY(!mountPoint->mountType().isEmpty());
        // old bug, happened because KMountPoint called KStandardDirs::realPath instead of realFilePath
        if (mountPoint->realDeviceName().startsWith(QLatin1String("/dev"))) { // skip this check for cifs mounts for instance
            QVERIFY(!mountPoint->realDeviceName().endsWith('/'));
        }

        // keep one (any) mountpoint with a device name for the test below
        if (!mountPoint->realDeviceName().isEmpty() && !mountWithDevice) {
            mountWithDevice = mountPoint;
        }

        QVERIFY(mountPoints.findByPath(mountPoint->mountPoint()) != nullptr);
    }

    if (!mountWithDevice) {
        // This happens on build.kde.org (LXC virtualization, mtab points to non-existing device paths)
        qWarning() << "Couldn't find any mountpoint with a valid device?";
    } else {
        // Check findByDevice
        KMountPoint::Ptr found = mountPoints.findByDevice(mountWithDevice->mountedFrom());
        QVERIFY(found);
        QCOMPARE(found->mountPoint(), mountWithDevice->mountPoint());
        found = mountPoints.findByDevice(QStringLiteral("/I/Dont/Exist")); // krazy:exclude=spelling
        QVERIFY(!found);
    }

    // Check findByPath
#ifdef Q_OS_UNIX
    const KMountPoint::Ptr rootMountPoint = mountPoints.findByPath(QStringLiteral("/"));
    QVERIFY(rootMountPoint);
    QCOMPARE(rootMountPoint->mountPoint(), QStringLiteral("/"));
    QVERIFY(!rootMountPoint->isOnRemoteStorage());

    QT_STATBUF rootStatBuff;
    QCOMPARE(QT_STAT("/", &rootStatBuff), 0);
    QT_STATBUF homeStatBuff;
    if (QT_STAT("/home", &homeStatBuff) == 0) {
        bool sameDevice = rootStatBuff.st_dev == homeStatBuff.st_dev;
        const KMountPoint::Ptr homeMountPoint = mountPoints.findByPath(QStringLiteral("/home"));
        QVERIFY(homeMountPoint);
        // qDebug() << "Checking the home mount point, sameDevice=" << sameDevice;
        if (sameDevice) {
            QCOMPARE(homeMountPoint->mountPoint(), QStringLiteral("/"));
        } else {
            QCOMPARE(homeMountPoint->mountPoint(), QDir(QStringLiteral("/home")).canonicalPath());
        }

        QCOMPARE(homeMountPoint->deviceId(), homeStatBuff.st_dev);
    } else {
        qDebug() << "/home doesn't seem to exist, skipping test";
    }
#endif
}

void KMountPointTest::testCurrentMountPointOptions()
{
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName | KMountPoint::NeedMountOptions);
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("No mountpoints available.");
        return;
    }

    KMountPoint::Ptr anyZfsMount;
    KMountPoint::Ptr mountWithDevice;
    KMountPoint::Ptr mountWithOptions;
    for (KMountPoint::Ptr mountPoint : mountPoints) {
        // keep one (first) mountpoint with a device name
        if (!mountWithDevice && !mountPoint->realDeviceName().isEmpty()) {
            mountWithDevice = mountPoint;
        }

        // keep one (first) ZFS mountpoint
        if (!anyZfsMount && mountPoint->mountType() == QLatin1String("zfs")) {
            anyZfsMount = mountPoint;
        }

        // keep one (first) mountpoint with any options
        if (!mountWithOptions && !mountPoint->mountOptions().empty()) {
            mountWithOptions = mountPoint;
        }
    }

    if (!anyZfsMount) {
        qDebug() << "No ZFS mounts, skipping test";
    } else {
        // A ZFS mount doesn't have a "real device" because it comes from a pool
        QVERIFY(anyZfsMount->realDeviceName().isEmpty());
        // But it does always have a "local" option
        QVERIFY(!anyZfsMount->mountOptions().isEmpty());
        QVERIFY(anyZfsMount->mountOptions().contains(QLatin1String("local")));
        qDebug() << "ZFS mount options" << anyZfsMount->mountOptions();
    }

    if (!mountWithDevice) {
        qDebug() << "No mountpoint from real device, skipping test";
    } else {
        // Double-check
        QVERIFY(!mountWithDevice->realDeviceName().isEmpty());
        qDebug() << "Device mount options" << mountWithDevice->mountOptions();
    }

    if (!mountWithOptions) {
        qDebug() << "No mount with options, skipping test";
    } else {
        QVERIFY(!mountWithOptions->mountOptions().isEmpty());
        qDebug() << "Options mount options" << mountWithOptions->mountOptions();
    }
}

void KMountPointTest::testPossibleMountPoints()
{
    // on Windows we just create pseudo mount points for drives, that will fail the tests below
#ifdef Q_OS_WIN
    QSKIP("Skipping test on Windows, KMountPoint only has pseudo mounts for drive letters");
#endif

    const KMountPoint::List mountPoints = KMountPoint::possibleMountPoints(KMountPoint::NeedRealDeviceName | KMountPoint::NeedMountOptions);
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("fstab is empty");
        return;
    }
    KMountPoint::Ptr mountWithDevice;
    for (KMountPoint::Ptr mountPoint : mountPoints) {
        qDebug() << "Possible mount: " << mountPoint->mountedFrom() << " (" << mountPoint->realDeviceName() << ") " << mountPoint->mountPoint() << " "
                 << mountPoint->mountType() << " options:" << mountPoint->mountOptions();
        QVERIFY(!mountPoint->mountedFrom().isEmpty());
        QVERIFY(!mountPoint->mountPoint().isEmpty());
        QVERIFY(!mountPoint->mountType().isEmpty());
        QVERIFY(!mountPoint->mountOptions().isEmpty());
        // old bug, happened because KMountPoint called KStandardDirs::realPath instead of realFilePath
        QVERIFY(!mountPoint->realDeviceName().endsWith('/'));

        // keep one (any) mountpoint with a device name for the test below
        if (!mountPoint->realDeviceName().isEmpty()) {
            mountWithDevice = mountPoint;
        }
    }

    if (!mountWithDevice) {
        qDebug() << "No mountpoint (" << mountPoints.size() << "checked ) has a non-empty real-device-name";
    }
    QVERIFY(mountWithDevice);

    // BSD CI runs in a container without '/' in fstab, so skip this
#if defined(Q_OS_UNIX) && !defined(Q_OS_FREEBSD)
    const KMountPoint::Ptr rootMountPoint = mountPoints.findByPath(QStringLiteral("/"));
    QVERIFY(rootMountPoint);
    QCOMPARE(rootMountPoint->mountPoint(), QStringLiteral("/"));
    QVERIFY(rootMountPoint->realDeviceName().startsWith(QLatin1String("/"))); // Usually /dev, but can be /host/ubuntu/disks/root.disk...
    QVERIFY(!rootMountPoint->mountOptions().contains(QLatin1String("noauto"))); // how would this work?
    QVERIFY(!rootMountPoint->isOnRemoteStorage());
#endif
}

void KMountPointTest::testCachedMountPointLookup()
{
#ifdef Q_OS_WIN
    QSKIP("Skipping test on Windows, KMountPoint only has pseudo mounts for drive letters");
#endif

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("mtab is empty");
    }

    const KMountPoint::Ptr root = mountPoints.findByPath(QStringLiteral("/"));
    QVERIFY(root);

    // The cached path lookup agrees with findByPath(), and a second call (served
    // from the cache) gives the same answer.
    const KMountPoint::Ptr byPath = KMountPoint::currentMountPointForPath(QStringLiteral("/"));
    QVERIFY(byPath);
    QCOMPARE(byPath->mountPoint(), QStringLiteral("/"));
    const KMountPoint::Ptr byPathAgain = KMountPoint::currentMountPointForPath(QStringLiteral("/"));
    QVERIFY(byPathAgain);
    QCOMPARE(byPathAgain->mountPoint(), QStringLiteral("/"));

    const quint64 rootId = root->mountId();
    if (rootId == 0) {
        // The kernel is older than STATX_MNT_ID_UNIQUE, so there is no unique id to
        // look up or cache; the path lookup above still works via the fallback.
        QSKIP("No unique mount id available on this kernel");
    }

    // Zero is never a valid id.
    QVERIFY(!KMountPoint::currentMountPointForUniqueId(0));

    // Looking up the root's unique id returns the root mount, and repeated lookups
    // (cache hits) stay consistent.
    for (int i = 0; i < 3; ++i) {
        const KMountPoint::Ptr byId = KMountPoint::currentMountPointForUniqueId(rootId);
        QVERIFY(byId);
        QCOMPARE(byId->mountPoint(), QStringLiteral("/"));
        QCOMPARE(byId->mountId(), rootId);
    }

    // The path and id accessors resolve to the same mount for "/".
    const KMountPoint::Ptr byPathId = KMountPoint::currentMountPointForPath(QStringLiteral("/"));
    QVERIFY(byPathId);
    QCOMPARE(byPathId->mountId(), rootId);

    // An id that does not belong to any current mount has no mount point.
    QVERIFY(!KMountPoint::currentMountPointForUniqueId(std::numeric_limits<quint64>::max()));
}

// currentMountPointForUniqueId() reads a mount with statmount() where the kernel has it, and
// currentMountPoints() reads the table with libmount, so each mount has to come out the same from both.
void KMountPointTest::testMountPointForUniqueIdMatchesTable()
{
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();
    int compared = 0;
    for (const KMountPoint::Ptr &fromTable : mountPoints) {
        if (fromTable->mountId() == 0) {
            continue;
        }
        const KMountPoint::Ptr byId = KMountPoint::currentMountPointForUniqueId(fromTable->mountId());
        QVERIFY2(byId, qPrintable(fromTable->mountPoint()));
        QCOMPARE(byId->mountPoint(), fromTable->mountPoint());
        QCOMPARE(byId->mountType(), fromTable->mountType());
        QCOMPARE(byId->mountedFrom(), fromTable->mountedFrom());
        QCOMPARE(byId->deviceId(), fromTable->deviceId());
        QCOMPARE(byId->mountId(), fromTable->mountId());
        QCOMPARE(byId->isOnRemoteStorage(), fromTable->isOnRemoteStorage());
        QCOMPARE(byId->isPseudoFs(), fromTable->isPseudoFs());
        ++compared;
    }
    if (compared == 0) {
        QSKIP("No unique mount id available on this kernel");
    }
}

// The list of filesystems that can share the blocks of a file with a copy of it is written out by
// hand, so it is checked against the filesystem the test itself is running on: whatever the kernel
// agrees to clone has to be a filesystem the list names.
void KMountPointTest::testFileCloningFlagMatchesTheKernel()
{
#ifndef Q_OS_LINUX
    QSKIP("Cloning a file is asked for with a Linux ioctl");
#else
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile source(dir.filePath(QStringLiteral("source")));
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write(QByteArray(64 * 1024, 'a')), 64 * 1024);
    source.close();
    QVERIFY(source.open(QIODevice::ReadOnly));

    QFile clone(dir.filePath(QStringLiteral("clone")));
    QVERIFY(clone.open(QIODevice::WriteOnly));
    const bool kernelClonedIt = ::ioctl(clone.handle(), FICLONE, source.handle()) == 0;

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("mtab is empty");
    }
    const KMountPoint::Ptr mountPoint = mountPoints.findByPath(dir.path());
    QVERIFY(mountPoint);
    const bool flag = mountPoint->testFileSystemFlag(KMountPoint::SupportsFileCloning);

    qDebug() << "cloning a file on" << mountPoint->mountType() << (kernelClonedIt ? "worked" : "was refused") << ", the flag says"
             << (flag ? "it is supported" : "it is not");
    if (kernelClonedIt) {
        QVERIFY2(flag, "the kernel cloned a file on a filesystem the flag does not name");
    }
    // The other way around says nothing: a filesystem of the right kind still refuses to clone
    // when it was made without the feature, which is why the flag is documented as it is.
#endif
}

#include "moc_kmountpointtest.cpp"
