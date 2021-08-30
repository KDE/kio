/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLD_DBUSMETATYPES_H
#define KSSLD_DBUSMETATYPES_H

#include "ksslcertificatemanager_p.h"

#include <QDBusArgument>
#include <QDBusMetaType>
#include <qglobal.h>

Q_DECLARE_METATYPE(KSslCertificateRule)
Q_DECLARE_METATYPE(QSslError::SslError)

inline QDBusArgument &operator<<(QDBusArgument &argument, const QSslCertificate &cert)
{
    argument.beginStructure();
    argument << cert.toDer();
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, QSslCertificate &cert)
{
    QByteArray data;
    argument.beginStructure();
    argument >> data;
    argument.endStructure();
    cert = QSslCertificate(data, QSsl::Der);
    return argument;
}

inline QDBusArgument &operator<<(QDBusArgument &argument, const KSslCertificateRule &rule)
{
    argument.beginStructure();
    argument << rule.certificate() << rule.hostName() << rule.isRejected() << rule.expiryDateTime().toString(Qt::ISODate)
             << rule.d->ignoredErrors; // TODO KF6: replace by a call to rule.ignoredErrors
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, KSslCertificateRule &rule)
{
    QSslCertificate cert;
    QString hostName;
    bool isRejected;
    QString expiryStr;
    QList<QSslError::SslError> ignoredErrors;
    argument.beginStructure();
    argument >> cert >> hostName >> isRejected >> expiryStr >> ignoredErrors;
    argument.endStructure();

    KSslCertificateRule ret(cert, hostName);
    ret.setRejected(isRejected);
    ret.setExpiryDateTime(QDateTime::fromString(expiryStr, Qt::ISODate));
    ret.setIgnoredErrors(ignoredErrors);
    rule = ret;
    return argument;
}

inline QDBusArgument &operator<<(QDBusArgument &argument, const QSslError::SslError &error)
{
    argument.beginStructure(); // overhead ho!
    argument << static_cast<int>(error);
    argument.endStructure();
    return argument;
}

inline const QDBusArgument &operator>>(const QDBusArgument &argument, QSslError::SslError &error)
{
    int data;
    argument.beginStructure();
    argument >> data;
    argument.endStructure();
    error = static_cast<QSslError::SslError>(data);
    return argument;
}

static void registerMetaTypesForKSSLD()
{
    qDBusRegisterMetaType<QSslCertificate>();
    qDBusRegisterMetaType<KSslCertificateRule>();
    qDBusRegisterMetaType<QList<QSslCertificate>>();
    qDBusRegisterMetaType<QSslError::SslError>();
    qDBusRegisterMetaType<QList<QSslError::SslError>>();
}

#endif // KSSLD_DBUSMETATYPES_H
