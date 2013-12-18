/*
 *  Copyright (C) 2002, 2003 Stephan Kulow <coolo@kde.org>
 *  Copyright (C) 2003       David Faure   <faure@kde.org>
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
    if (argc > 1)
      url = QUrl::fromUserInput(argv[1]);
    else
      url = QUrl::fromLocalFile(QDir::currentPath());

    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();

    KMountPoint::Ptr mp = mountPoints.findByDevice(url.toLocalFile());
    if (!mp) {
        qDebug() << "no mount point for device" << url << "found";
    } else
        qDebug() << mp->mountPoint() << "is the mount point for device" << url;

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
    } else
        qDebug() << mp->mountPoint() << "is the mount point for path" << url;

    return 0;
}

