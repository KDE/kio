/* This file is part of the KDE libraries
    Copyright (C) 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

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

#include "ksslerroruidata.h"
#include "ksslerroruidata_p.h"
#include "ktcpsocket.h"

#include <QHostAddress>
#include <QNetworkReply>
#include <QSslCipher>

KSslErrorUiData::KSslErrorUiData()
    : d(new Private())
{
    d->usedBits = 0;
    d->bits = 0;
}

KSslErrorUiData::KSslErrorUiData(const KTcpSocket *socket)
    : d(new Private())
{
    d->certificateChain = socket->peerCertificateChain();
    const auto ksslErrors = socket->sslErrors();
    d->sslErrors.reserve(ksslErrors.size());
    for (const auto &error : ksslErrors) {
        d->sslErrors.push_back(error.sslError());
    }
    d->ip = socket->peerAddress().toString();
    d->host = socket->peerName();
    d->sslProtocol = socket->negotiatedSslVersionName();
    d->cipher = socket->sessionCipher().name();
    d->usedBits = socket->sessionCipher().usedBits();
    d->bits = socket->sessionCipher().supportedBits();
}

KSslErrorUiData::KSslErrorUiData(const QSslSocket *socket)
    : d(new Private())
{
    d->certificateChain = socket->peerCertificateChain();
#if (QT_VERSION < QT_VERSION_CHECK(5, 15, 0))
    d->sslErrors = socket->sslErrors();
#else
    d->sslErrors = socket->sslHandshakeErrors();
#endif
    d->ip = socket->peerAddress().toString();
    d->host = socket->peerName();
    if (socket->isEncrypted()) {
        d->sslProtocol = socket->sessionCipher().protocolString();
    }
    d->cipher = socket->sessionCipher().name();
    d->usedBits = socket->sessionCipher().usedBits();
    d->bits = socket->sessionCipher().supportedBits();
}

KSslErrorUiData::KSslErrorUiData(const QNetworkReply *reply, const QList<QSslError> &sslErrors)
    : d(new Private())
{
    const auto sslConfig = reply->sslConfiguration();
    d->certificateChain = sslConfig.peerCertificateChain();
    d->sslErrors = sslErrors;
    d->host = reply->request().url().host();
    d->sslProtocol = sslConfig.sessionCipher().protocolString();
    d->cipher = sslConfig.sessionCipher().name();
    d->usedBits = sslConfig.sessionCipher().usedBits();
    d->bits = sslConfig.sessionCipher().supportedBits();
}

KSslErrorUiData::KSslErrorUiData(const KSslErrorUiData &other)
    : d(new Private(*other.d))
{}

KSslErrorUiData::~KSslErrorUiData()
{
    delete d;
}

KSslErrorUiData &KSslErrorUiData::operator=(const KSslErrorUiData &other)
{
    *d = *other.d;
    return *this;
}

