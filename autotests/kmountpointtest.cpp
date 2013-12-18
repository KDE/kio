/*
 *  Copyright (C) 2006 David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "kmountpointtest.h"

#include <QtTest/QtTest>
#include "kmountpoint.h"
#include <QDebug>
#include <qplatformdefs.h>

QTEST_MAIN( KMountPointTest )

void KMountPointTest::initTestCase()
{

}

void KMountPointTest::testCurrentMountPoints()
{
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints(KMountPoint::NeedRealDeviceName);
    QVERIFY(!mountPoints.isEmpty());
    KMountPoint::Ptr mountWithDevice;
    foreach(KMountPoint::Ptr mountPoint, mountPoints) {
        qDebug() << "Mount: " << mountPoint->mountedFrom()
          << " (" << mountPoint->realDeviceName() << ") "
          << mountPoint->mountPoint() << " " << mountPoint->mountType() << endl;
        QVERIFY(!mountPoint->mountedFrom().isEmpty());
        QVERIFY(!mountPoint->mountPoint().isEmpty());
        QVERIFY(!mountPoint->mountType().isEmpty());
        // old bug, happened because KMountPoint called KStandardDirs::realPath instead of realFilePath
        if (mountPoint->realDeviceName().startsWith("/dev")) { // skip this check for cifs mounts for instance
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
        found = mountPoints.findByDevice("/I/Dont/Exist"); // krazy:exclude=spelling
        QVERIFY(!found);
    }

    // Check findByPath
#ifdef Q_OS_UNIX
    const KMountPoint::Ptr rootMountPoint = mountPoints.findByPath("/");
    QVERIFY(rootMountPoint);
    QCOMPARE(rootMountPoint->mountPoint(), QString("/"));
    QVERIFY(!rootMountPoint->probablySlow());

    QT_STATBUF rootStatBuff;
    QCOMPARE( QT_STAT( "/", &rootStatBuff ), 0 );
    QT_STATBUF homeStatBuff;
    if ( QT_STAT( "/home", &homeStatBuff ) == 0 ) {
        bool sameDevice = rootStatBuff.st_dev == homeStatBuff.st_dev;
        const KMountPoint::Ptr homeMountPoint = mountPoints.findByPath("/home");
        QVERIFY(homeMountPoint);
        //qDebug() << "Checking the home mount point, sameDevice=" << sameDevice;
        if (sameDevice)
            QCOMPARE(homeMountPoint->mountPoint(), QString("/"));
        else
            QCOMPARE(homeMountPoint->mountPoint(), QString("/home"));
    } else {
        qDebug() << "/home doesn't seem to exist, skipping test";
    }
#endif
}

void KMountPointTest::testPossibleMountPoints()
{
    const KMountPoint::List mountPoints = KMountPoint::possibleMountPoints(KMountPoint::NeedRealDeviceName|KMountPoint::NeedMountOptions);
    if (mountPoints.isEmpty()) { // can happen in chroot jails
        QSKIP("fstab is empty");
        return;
    }
    KMountPoint::Ptr mountWithDevice;
    foreach(KMountPoint::Ptr mountPoint, mountPoints) {
        qDebug() << "Possible mount: " << mountPoint->mountedFrom()
          << " (" << mountPoint->realDeviceName() << ") "
          << mountPoint->mountPoint() << " " << mountPoint->mountType()
                 << " options:" << mountPoint->mountOptions() << endl;
        QVERIFY(!mountPoint->mountedFrom().isEmpty());
        QVERIFY(!mountPoint->mountPoint().isEmpty());
        QVERIFY(!mountPoint->mountType().isEmpty());
        QVERIFY(!mountPoint->mountOptions().isEmpty());
        // old bug, happened because KMountPoint called KStandardDirs::realPath instead of realFilePath
        QVERIFY(!mountPoint->realDeviceName().endsWith('/'));

        // keep one (any) mountpoint with a device name for the test below
        if (!mountPoint->realDeviceName().isEmpty())
            mountWithDevice = mountPoint;
    }

    QVERIFY(mountWithDevice);

#ifdef Q_OS_UNIX
    const KMountPoint::Ptr rootMountPoint = mountPoints.findByPath("/");
    QVERIFY(rootMountPoint);
    QCOMPARE(rootMountPoint->mountPoint(), QString("/"));
    QVERIFY(rootMountPoint->realDeviceName().startsWith(QLatin1String("/"))); // Usually /dev, but can be /host/ubuntu/disks/root.disk...
    QVERIFY(!rootMountPoint->mountOptions().contains("noauto")); // how would this work?
    QVERIFY(!rootMountPoint->probablySlow());
#endif
}

