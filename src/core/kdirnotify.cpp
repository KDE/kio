/* This file is part of the KDE libraries

    Copyright (c) 2000-2012 David Faure <faure@kde.org>
    Copyright (c) 2006 Thiago Macieira <thiago@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kdirnotify.h"
#include <kdbusconnectionpool.h> // HAND-EDIT
#include <QUrl>

/*
 * Implementation of interface class OrgKdeKDirNotifyInterface
 */

OrgKdeKDirNotifyInterface::OrgKdeKDirNotifyInterface(const QString &service, const QString &path, const QDBusConnection &connection, QObject *parent)
    : QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
{
}

OrgKdeKDirNotifyInterface::~OrgKdeKDirNotifyInterface()
{
}

static void emitSignal(const QString &signalName, const QVariantList &args)
{
    QDBusMessage message =
        QDBusMessage::createSignal(QStringLiteral("/"), QLatin1String(org::kde::KDirNotify::staticInterfaceName()), signalName);
    message.setArguments(args);
    QDBusConnection::sessionBus().send(message);
}

void OrgKdeKDirNotifyInterface::emitFileRenamed(const QUrl &src, const QUrl &dst)
{
    emitSignal(QStringLiteral("FileRenamed"), QVariantList{ src.toString(), dst.toString() });
    emitSignal(QStringLiteral("FileRenamedWithLocalPath"), QVariantList{ src.toString(), dst.toString(), QString() });
}

void OrgKdeKDirNotifyInterface::emitFileRenamedWithLocalPath(const QUrl &src, const QUrl &dst, const QString &dstPath)
{
    emitSignal(QStringLiteral("FileRenamed"), QVariantList{ src.toString(), dst.toString() });
    emitSignal(QStringLiteral("FileRenamedWithLocalPath"), QVariantList{ src.toString(), dst.toString(), dstPath });
}

void OrgKdeKDirNotifyInterface::emitFileMoved(const QUrl &src, const QUrl &dst)
{
    emitSignal(QStringLiteral("FileMoved"), QVariantList{ src.toString(), dst.toString() });
}

void OrgKdeKDirNotifyInterface::emitFilesAdded(const QUrl &directory)
{
    emitSignal(QStringLiteral("FilesAdded"), QVariantList() << directory.toString());
}

void OrgKdeKDirNotifyInterface::emitFilesChanged(const QList<QUrl> &fileList)
{
    emitSignal(QStringLiteral("FilesChanged"), QVariantList() << QVariant(QUrl::toStringList(fileList)));
}

void OrgKdeKDirNotifyInterface::emitFilesRemoved(const QList<QUrl> &fileList)
{
    emitSignal(QStringLiteral("FilesRemoved"), QVariantList() << QVariant(QUrl::toStringList(fileList)));
}

void OrgKdeKDirNotifyInterface::emitEnteredDirectory(const QUrl &url)
{
    emitSignal(QStringLiteral("enteredDirectory"), QVariantList() << url.toString());
}

void OrgKdeKDirNotifyInterface::emitLeftDirectory(const QUrl &url)
{
    emitSignal(QStringLiteral("leftDirectory"), QVariantList() << url.toString());
}

