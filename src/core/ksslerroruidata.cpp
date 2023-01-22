/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksslerroruidata.h"
#include "ksslerroruidata_p.h"

#include <QHostAddress>
#include <QNetworkReply>
#include <QSslCipher>

KSslErrorUiData::KSslErrorUiData()
    : d(new Private())
{
    d->usedBits = 0;
    d->bits = 0;
}

KSslErrorUiData::KSslErrorUiData(const QSslSocket *socket)
    : d(new Private())
{
    d->certificateChain = socket->peerCertificateChain();
    d->sslErrors = socket->sslHandshakeErrors();
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
{
}

KSslErrorUiData::~KSslErrorUiData() = default;

KSslErrorUiData &KSslErrorUiData::operator=(const KSslErrorUiData &other)
{
    *d = *other.d;
    return *this;
}
