/*
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kmountpointtest.h"

#include "kmountpoint.h"
#include <QDebug>
#include <QTest>
#include <qplatformdefs.h>

QTEST_MAIN(KMountPointTest)

void KMountPointTest::initTestCase()
{
}

void KMountPointTest::testCurrentMountPoints()
{
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName);
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("mtab is empty");
        return;
    }
    KMountPoint::Ptr mountWithDevice;
    for (KMountPoint::Ptr mountPoint : mountPoints) {
        qDebug().nospace() << "Mounted from: " << mountPoint->mountedFrom() << ", device name: " << mountPoint->realDeviceName()
                           << ", mount point: " << mountPoint->mountPoint() << ", mount type: " << mountPoint->mountType();
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
    QVERIFY(!rootMountPoint->probablySlow());

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
    QVERIFY(!rootMountPoint->probablySlow());
#endif
}
