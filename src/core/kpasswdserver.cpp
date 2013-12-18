/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2009 Michael Leupold <lemma@confuego.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) version 3, or any
 *  later version accepted by the membership of KDE e.V. (or its
 *  successor approved by the membership of KDE e.V.), which shall
 *  act as a proxy defined in Section 6 of version 3 of the license.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 * 
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kpasswdserver_p.h"

#include <kio/authinfo.h>
#include <QtCore/QByteArray>
#include <QtCore/QEventLoop>

#include "kpasswdserverloop_p.h"
#include "kpasswdserver_interface.h"

namespace KIO
{

KPasswdServer::KPasswdServer()
    : m_interface(new OrgKdeKPasswdServerInterface("org.kde.kded5",
                                                   "/modules/kpasswdserver",
                                                   QDBusConnection::sessionBus()))
{
}

KPasswdServer::~KPasswdServer()
{
    delete m_interface;
}

bool KPasswdServer::checkAuthInfo(KIO::AuthInfo &info, qlonglong windowId,
                                  qlonglong usertime)
{
    //qDebug() << "window-id=" << windowId << "url=" << info.url;

    // special handling for kioslaves which aren't QCoreApplications
    if (!QCoreApplication::instance()) {
        qWarning() << "kioslave is not a QCoreApplication!";
        return legacyCheckAuthInfo(info, windowId, usertime);
    }
    
    // create the loop for waiting for a result before sending the request
    KPasswdServerLoop loop;
    QObject::connect(m_interface, SIGNAL(checkAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)),
                     &loop, SLOT(slotQueryResult(qlonglong,qlonglong,KIO::AuthInfo)));
            
    QDBusReply<qlonglong> reply = m_interface->checkAuthInfoAsync(info, windowId,
                                                                  usertime);
    if (!reply.isValid()) {
        if (reply.error().type() == QDBusError::UnknownMethod) {
            if (legacyCheckAuthInfo(info, windowId, usertime)) {
                return true;
            }
        }

        qWarning() << "Can't communicate with kded_kpasswdserver (for checkAuthInfo)!";
        //qDebug() << reply.error().name() << reply.error().message();
        return false;
    }

    if (!loop.waitForResult(reply.value())) {
        qWarning() << "kded_kpasswdserver died while waiting for reply!";
        return false;
    }

    if (loop.authInfo().isModified()) {
        //qDebug() << "username=" << info.username << "password=[hidden]";
        info = loop.authInfo();
        return true;
    }

    return false;
}

bool KPasswdServer::legacyCheckAuthInfo(KIO::AuthInfo &info, qlonglong windowId,
                                             qlonglong usertime)
{
    qWarning() << "Querying old kded_kpasswdserver.";
    
    QByteArray params;
    QDataStream stream(&params, QIODevice::WriteOnly);
    stream << info;
    QDBusReply<QByteArray> reply = m_interface->checkAuthInfo(params, windowId,
                                                              usertime);
    if (reply.isValid()) {
        AuthInfo authResult;
        QDataStream stream2(reply.value());
        stream2 >> authResult;
        if (authResult.isModified()) {
            info = authResult;
            return true;
        }
    }
    return false;
}

qlonglong KPasswdServer::queryAuthInfo(KIO::AuthInfo &info, const QString &errorMsg,
                                       qlonglong windowId, qlonglong seqNr,
                                       qlonglong usertime)
{
    //qDebug() << "window-id=" << windowId;

    // special handling for kioslaves which aren't QCoreApplications
    if (!QCoreApplication::instance()) {
        qWarning() << "kioslave is not a QCoreApplication!";
        return legacyQueryAuthInfo(info, errorMsg, windowId, seqNr, usertime);
    }
    
    // create the loop for waiting for a result before sending the request
    KPasswdServerLoop loop;
    QObject::connect(m_interface, SIGNAL(queryAuthInfoAsyncResult(qlonglong,qlonglong,KIO::AuthInfo)),
                     &loop, SLOT(slotQueryResult(qlonglong,qlonglong,KIO::AuthInfo)));

    QDBusReply<qlonglong> reply = m_interface->queryAuthInfoAsync(info, errorMsg,
                                                                  windowId, seqNr,
                                                                  usertime);
    if (!reply.isValid()) {
        // backwards compatibility for old kpasswdserver
        if (reply.error().type() == QDBusError::UnknownMethod) {
            qlonglong res = legacyQueryAuthInfo(info, errorMsg, windowId, seqNr,
                                                usertime);
            if (res > 0) {
                return res;
            }
        }

        qWarning() << "Can't communicate with kded_kpasswdserver (for queryAuthInfo)!";
        //qDebug() << reply.error().name() << reply.error().message();
        return -1;
    }

    if (!loop.waitForResult(reply.value())) {
        qWarning() << "kded_kpasswdserver died while waiting for reply!";
        return -1;
    }

    info = loop.authInfo();

    //qDebug() << "username=" << info.username << "password=[hidden]";

    return loop.seqNr();
}

qlonglong KPasswdServer::legacyQueryAuthInfo(KIO::AuthInfo &info, const QString &errorMsg,
                                             qlonglong windowId, qlonglong seqNr,
                                             qlonglong usertime)
{
    qWarning() << "Querying old kded_kpasswdserver.";
    
    QByteArray params;
    QDataStream stream(&params, QIODevice::WriteOnly);
    stream << info;
    QDBusPendingReply<QByteArray, qlonglong> reply = m_interface->queryAuthInfo(params, errorMsg,
                                                                                windowId, seqNr,
                                                                                usertime);
    reply.waitForFinished();
    if (reply.isValid()) {
        AuthInfo authResult;
        QDataStream stream2(reply.argumentAt<0>());
        stream2 >> authResult;
        if (authResult.isModified()) {
            info = authResult;
        }
        return reply.argumentAt<1>();
    }
    return -1;
}

void KPasswdServer::addAuthInfo(const KIO::AuthInfo &info, qlonglong windowId)
{
    QDBusReply<void> reply = m_interface->addAuthInfo(info, windowId);
    if (!reply.isValid() && reply.error().type() == QDBusError::UnknownMethod) {
        legacyAddAuthInfo(info, windowId);
    }
}

void KPasswdServer::legacyAddAuthInfo(const KIO::AuthInfo &info, qlonglong windowId)
{
    qWarning() << "Querying old kded_kpasswdserver.";
    
    QByteArray params;
    QDataStream stream(&params, QIODevice::WriteOnly);
    stream << info;
    m_interface->addAuthInfo(params, windowId);
}

void KPasswdServer::removeAuthInfo(const QString &host, const QString &protocol,
                                   const QString &user)
{
    m_interface->removeAuthInfo(host, protocol, user);
}

}
