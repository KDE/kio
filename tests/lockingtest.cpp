/*
    This file is part of the KDE

    Copyright (C) 2009 Tobias Koenig (tokoe@kde.org)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 2 as published by the Free Software Foundation.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this library; see the file COPYING. If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include "kinterprocesslock.h"

#include <unistd.h>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    KInterProcessLock lock("mytrash");
    qDebug("retrieve lock...");
    lock.lock();
    qDebug("waiting...");
    lock.waitForLockGranted();
    qDebug("retrieved lock");
    qDebug("sleeping...");
#ifdef Q_OS_WIN
    Sleep(10*1000);
#else
    sleep(10);
#endif

    if (argc != 2) {
        lock.unlock();
        qDebug("release lock");
    }

    return 0;
}
