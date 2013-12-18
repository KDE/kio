/*
    This file is part of the KDE libraries

    Copyright (c) 2007 Andreas Hartmetz <ahartmetz@gmail.com>

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

#ifndef KSSLD_DBUSMETATYPES_H
#define KSSLD_DBUSMETATYPES_H

#include <qglobal.h>
#include <QDBusArgument>
#include <QDBusMetaType>

Q_DECLARE_METATYPE(KSslCertificateRule)
Q_DECLARE_METATYPE(KSslError::Error)


QDBusArgument &operator<<(QDBusArgument &argument, const QSslCertificate &cert)
{
    argument.beginStructure();
    argument << cert.toDer();
    argument.endStructure();
    return argument;
}


const QDBusArgument &operator>>(const QDBusArgument &argument, QSslCertificate &cert)
{
    QByteArray data;
    argument.beginStructure();
    argument >> data;
    argument.endStructure();
    cert = QSslCertificate(data, QSsl::Der);
    return argument;
}


QDBusArgument &operator<<(QDBusArgument &argument, const KSslCertificateRule &rule)
{
    argument.beginStructure();
    argument << rule.certificate() << rule.hostName()
             << rule.isRejected() << rule.expiryDateTime().toString(Qt::ISODate)
             << rule.ignoredErrors();
    argument.endStructure();
    return argument;
}


const QDBusArgument &operator>>(const QDBusArgument &argument, KSslCertificateRule &rule)
{
    QSslCertificate cert;
    QString hostName;
    bool isRejected;
    QString expiryStr;
    QList<KSslError::Error> ignoredErrors;
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


QDBusArgument &operator<<(QDBusArgument &argument, const KSslError::Error &error)
{
    argument.beginStructure();  //overhead ho!
    argument << static_cast<int>(error);
    argument.endStructure();
    return argument;
}


const QDBusArgument &operator>>(const QDBusArgument &argument, KSslError::Error &error)
{
    int data;
    argument.beginStructure();
    argument >> data;
    argument.endStructure();
    error = static_cast<KSslError::Error>(data);
    return argument;
}


static void registerMetaTypesForKSSLD()
{
    qDBusRegisterMetaType<QSslCertificate>();
    qDBusRegisterMetaType<KSslCertificateRule>();
    qDBusRegisterMetaType<QList<QSslCertificate> >();
    qDBusRegisterMetaType<KSslError::Error>();
    qDBusRegisterMetaType<QList<KSslError::Error> >();
}

#endif //KSSLD_DBUSMETATYPES_H
