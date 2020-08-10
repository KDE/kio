/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Alex Richardson <arichardson.kde@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#include "kioglobal_p.h"

#include <QFile>
#include <unistd.h>
#include <signal.h>

KIOCORE_EXPORT bool KIOPrivate::isProcessAlive(qint64 pid)
{
    return ::kill(pid, 0) == 0;
}

KIOCORE_EXPORT void KIOPrivate::sendTerminateSignal(qint64 pid)
{
    ::kill(pid, SIGTERM);
}

KIOCORE_EXPORT bool KIOPrivate::createSymlink(const QString &source, const QString &destination, SymlinkType type)
{
    Q_UNUSED(type)
    return ::symlink(QFile::encodeName(source).constData(), QFile::encodeName(destination).constData()) == 0;
}

KIOCORE_EXPORT bool KIOPrivate::changeOwnership(const QString& file, KUserId newOwner, KGroupId newGroup)
{
    return ::chown(QFile::encodeName(file).constData(), newOwner.nativeId(), newGroup.nativeId()) == 0;
}
