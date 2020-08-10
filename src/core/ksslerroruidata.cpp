/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
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

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 65)
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
#endif

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

