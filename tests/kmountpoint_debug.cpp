/*
    SPDX-FileCopyrightText: 2002, 2003 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2003 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <kmountpoint.h>
#include <QDir>
#include <QUrl>
#include <QCoreApplication>
#include <QDebug>

// This is a test program for KMountPoint

// Call it with either a device path or a mount point.
// It will try both, so obviously one will fail.

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QUrl url;
    if (argc > 1) {
        url = QUrl::fromUserInput(argv[1]);
    } else {
        url = QUrl::fromLocalFile(QDir::currentPath());
    }

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();

    KMountPoint::Ptr mp = mountPoints.findByDevice(url.toLocalFile());
    if (!mp) {
        qDebug() << "no mount point for device" << url << "found";
    } else {
        qDebug() << mp->mountPoint() << "is the mount point for device" << url;
    }

    mp = mountPoints.findByPath(url.toLocalFile());
    if (!mp) {
        qDebug() << "no mount point for path" << url << "found";
    } else {
        qDebug() << mp->mountPoint() << "is the mount point for path" << url;
        qDebug() << url << "is probably" << (mp->probablySlow() ? "slow" : "normal") << "mounted";
    }

    url = QUrl::fromLocalFile(QDir::homePath());

    mp = mountPoints.findByPath(url.toLocalFile());
    if (!mp) {
        qDebug() << "no mount point for path" << url << "found";
    } else {
        qDebug() << mp->mountPoint() << "is the mount point for path" << url;
    }

    return 0;
}

