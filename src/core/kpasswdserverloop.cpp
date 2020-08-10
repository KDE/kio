/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kpasswdserverloop_p.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>

KPasswdServerLoop::KPasswdServerLoop() : m_seqNr(-1)
{
    QDBusServiceWatcher *watcher = new QDBusServiceWatcher(QStringLiteral("org.kde.kpasswdserver"), QDBusConnection::sessionBus(),
            QDBusServiceWatcher::WatchForUnregistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &KPasswdServerLoop::kdedServiceUnregistered);
}

KPasswdServerLoop::~KPasswdServerLoop()
{
}

bool KPasswdServerLoop::waitForResult(qlonglong requestId)
{
    m_requestId = requestId;
    m_seqNr = -1;
    m_authInfo = KIO::AuthInfo();
    return (exec() == 0);
}

qlonglong KPasswdServerLoop::seqNr() const
{
    return m_seqNr;
}

const KIO::AuthInfo &KPasswdServerLoop::authInfo() const
{
    return m_authInfo;
}

void KPasswdServerLoop::slotQueryResult(qlonglong requestId, qlonglong seqNr,
                                        const KIO::AuthInfo &authInfo)
{
    if (m_requestId == requestId) {
        m_seqNr = seqNr;
        m_authInfo = authInfo;
        exit(0);
    }
}

void KPasswdServerLoop::kdedServiceUnregistered()
{
    exit(-1);
}

#include "moc_kpasswdserverloop_p.cpp"
