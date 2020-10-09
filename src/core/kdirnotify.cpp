/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2012 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirnotify.h"
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
    emitSignal(QStringLiteral("FilesAdded"), QVariantList{QVariant(directory.toString())});
}

void OrgKdeKDirNotifyInterface::emitFilesChanged(const QList<QUrl> &fileList)
{
    emitSignal(QStringLiteral("FilesChanged"), QVariantList{QVariant(QUrl::toStringList(fileList))});
}

void OrgKdeKDirNotifyInterface::emitFilesRemoved(const QList<QUrl> &fileList)
{
    emitSignal(QStringLiteral("FilesRemoved"), QVariantList{QVariant(QUrl::toStringList(fileList))});
}

void OrgKdeKDirNotifyInterface::emitEnteredDirectory(const QUrl &url)
{
    emitSignal(QStringLiteral("enteredDirectory"), QVariantList{QVariant(url.toString())});
}

void OrgKdeKDirNotifyInterface::emitLeftDirectory(const QUrl &url)
{
    emitSignal(QStringLiteral("leftDirectory"), QVariantList{QVariant(url.toString())});
}

