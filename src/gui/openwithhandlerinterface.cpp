/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "openwithhandlerinterface.h"

#include <QFile>
#include <QSaveFile>
#include "kiocoredebug.h"

using namespace KIO;

class KIO::OpenWithHandlerInterfacePrivate {};

OpenWithHandlerInterface::OpenWithHandlerInterface(QObject *parent)
    : QObject(parent)
{
}

OpenWithHandlerInterface::~OpenWithHandlerInterface() = default;

void OpenWithHandlerInterface::promptUserForApplication(KJob *job, const QList<QUrl> &urls, const QString &mimeType)
{
    Q_UNUSED(job)
    Q_UNUSED(urls)
    Q_UNUSED(mimeType)
    Q_EMIT canceled();
}
