/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 Michael Leupold <lemma@confuego.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kpasswdserverclient.h"
#include "kiocoredebug.h"

#include <kio/global.h>
#include <kio/authinfo.h>

#include "kpasswdserverloop_p.h"
#include "kpasswdserver_interface.h"

class KPasswdServerClientPrivate
{
public:
    KPasswdServerClientPrivate()
      : seqNr(0) {}

    qlonglong seqNr;
    QString lastHost;
};

KPasswdServerClient::KPasswdServerClient()
    : m_interface(new OrgKdeKPasswdServerInterface(QStringLiteral("org.kde.kpasswdserver"),
                  QStringLiteral("/modules/kpasswdserver"),
                  QDBusConnection::sessionBus())),
      d(new KPasswdServerClientPrivate)
{
}

KPasswdServerClient::~KPasswdServerClient()
{
    delete m_interface;
    delete d;
}

bool KPasswdServerClient::checkAuthInfo(KIO::AuthInfo *info, qlonglong windowId,
                                  qlonglong usertime)
{
    //qDebug() << "window-id=" << windowId << "url=" << info.url;

    if (!QCoreApplication::instance()) {
        qCWarning(KIO_CORE) << "kioslave is not a QCoreApplication! This is required for checkAuthInfo.";
        return false;
    }

    // create the loop for waiting for a result before sending the request
    KPasswdServerLoop loop;
    QObject::connect(m_interface, &OrgKdeKPasswdServerInterface::checkAuthInfoAsyncResult,
                     &loop, &KPasswdServerLoop::slotQueryResult);

    QDBusReply<qlonglong> reply = m_interface->checkAuthInfoAsync(*info, windowId,
                                  usertime);
    if (!reply.isValid()) {
        qCWarning(KIO_CORE) << "Can't communicate with kiod_kpasswdserver (for checkAuthInfo)!";
        //qDebug() << reply.error().name() << reply.error().message();
        return false;
    }

    if (!loop.waitForResult(reply.value())) {
        qCWarning(KIO_CORE) << "kiod_kpasswdserver died while waiting for reply!";
        return false;
    }

    if (loop.authInfo().isModified()) {
        //qDebug() << "username=" << info.username << "password=[hidden]";
        *info = loop.authInfo();
        return true;
    }

    return false;
}

int KPasswdServerClient::queryAuthInfo(KIO::AuthInfo *info, const QString &errorMsg,
                                             qlonglong windowId,
                                             qlonglong usertime)
{
    if (info->url.host() != d->lastHost) { // see kpasswdserver/DESIGN
        d->lastHost = info->url.host();
        d->seqNr = 0;
    }

    //qDebug() << "window-id=" << windowId;

    if (!QCoreApplication::instance()) {
        qCWarning(KIO_CORE) << "kioslave is not a QCoreApplication! This is required for queryAuthInfo.";
        return KIO::ERR_PASSWD_SERVER;
    }

    // create the loop for waiting for a result before sending the request
    KPasswdServerLoop loop;
    QObject::connect(m_interface, &OrgKdeKPasswdServerInterface::queryAuthInfoAsyncResult,
                     &loop, &KPasswdServerLoop::slotQueryResult);

    QDBusReply<qlonglong> reply = m_interface->queryAuthInfoAsync(*info, errorMsg,
                                  windowId, d->seqNr,
                                  usertime);
    if (!reply.isValid()) {
        qCWarning(KIO_CORE) << "Can't communicate with kiod_kpasswdserver (for queryAuthInfo)!";
        //qDebug() << reply.error().name() << reply.error().message();
        return KIO::ERR_PASSWD_SERVER;
    }

    if (!loop.waitForResult(reply.value())) {
        qCWarning(KIO_CORE) << "kiod_kpasswdserver died while waiting for reply!";
        return KIO::ERR_PASSWD_SERVER;
    }

    *info = loop.authInfo();

    //qDebug() << "username=" << info->username << "password=[hidden]";

    const qlonglong newSeqNr = loop.seqNr();

    if (newSeqNr > 0) {
        d->seqNr = newSeqNr;
        if (info->isModified()) {
            return KJob::NoError;
        }
    }

    return KIO::ERR_USER_CANCELED;
}

void KPasswdServerClient::addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId)
{
    m_interface->addAuthInfo(info, windowId);
}

void KPasswdServerClient::removeAuthInfo(const QString &host, const QString &protocol,
                                   const QString &user)
{
    m_interface->removeAuthInfo(host, protocol, user);
}
