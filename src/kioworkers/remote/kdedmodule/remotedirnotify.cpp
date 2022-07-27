/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2004 Kévin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "remotedirnotify.h"

#include <KDirNotify>
#include <KDirWatch>

#include <QStandardPaths>
#include <QUrl>

RemoteDirNotify::RemoteDirNotify()
{
    const QString path = QStringLiteral("%1/remoteview").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));

    m_dirWatch = new KDirWatch(this);
    m_dirWatch->addDir(path, KDirWatch::WatchFiles);

    connect(m_dirWatch, &KDirWatch::created, this, &RemoteDirNotify::slotRemoteChanged);
    connect(m_dirWatch, &KDirWatch::deleted, this, &RemoteDirNotify::slotRemoteChanged);
    connect(m_dirWatch, &KDirWatch::dirty, this, &RemoteDirNotify::slotRemoteChanged);
}

void RemoteDirNotify::slotRemoteChanged()
{
    org::kde::KDirNotify::emitFilesAdded(QUrl(QStringLiteral("remote:/")));
}
