/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "openorexecutefileinterface.h"

using namespace KIO;

class KIO::OpenOrExecuteFileInterfacePrivate {};

OpenOrExecuteFileInterface::OpenOrExecuteFileInterface(QObject *parent)
    : QObject(parent)
{
}

OpenOrExecuteFileInterface::~OpenOrExecuteFileInterface() = default;

void OpenOrExecuteFileInterface::promptUserOpenOrExecute(KJob *job, const QString &mimetype)
{
    Q_UNUSED(job)
    Q_UNUSED(mimetype)
    Q_EMIT canceled();
}
