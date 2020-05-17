/* This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "openwithhandlerinterface.h"

#include <QFile>
#include <QSaveFile>
#include "kiocoredebug.h"

using namespace KIO;

class KIO::OpenWithHandlerInterfacePrivate {};

OpenWithHandlerInterface::OpenWithHandlerInterface() = default;

OpenWithHandlerInterface::~OpenWithHandlerInterface() = default;

void OpenWithHandlerInterface::promptUserForApplication(OpenUrlJob *job, const QList<QUrl> &urls, const QString &mimeType)
{
    Q_UNUSED(job)
    Q_UNUSED(urls)
    Q_UNUSED(mimeType)
    Q_EMIT canceled();
}
