/*
    This file is part of the KDE
    SPDX-FileCopyrightText: 2009 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: GPL-2.0-only
*/

#include <QCoreApplication>
#include <QThread>

#include "kinterprocesslock.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    KInterProcessLock lock(QStringLiteral("mytrash"));
    qDebug("retrieve lock...");
    lock.lock();
    qDebug("waiting...");
    lock.waitForLockGranted();
    qDebug("retrieved lock");
    qDebug("sleeping...");
#ifdef Q_OS_WIN
    Sleep(10 * 1000);
#else
    sleep(10);
#endif

    if (argc != 2) {
        lock.unlock();
        qDebug("release lock");
    }

    return 0;
}
