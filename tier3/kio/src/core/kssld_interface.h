/*
    This file is part of the KDE libraries

    Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>

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

#ifndef KSSLDINTERFACE_H
#define KSSLDINTERFACE_H

#include <QtCore/QObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariant>
#include <QDBusConnection>
#include <QDBusAbstractInterface>
#include <QDBusReply>

#include "kssld_dbusmetatypes.h"


/*
 * Proxy class for interface org.kde.KSSLD
 */
class OrgKdeKSSLDInterface: public QDBusAbstractInterface
{
    Q_OBJECT
public:
    static inline const char *staticInterfaceName()
    {
        return "org.kde.KSSLD";
    }

public:
    OrgKdeKSSLDInterface(const QString &service, const QString &path,
                         const QDBusConnection &connection,
                         QObject *parent = 0)
     : QDBusAbstractInterface(service, path, staticInterfaceName(), connection, parent)
    {
        registerMetaTypesForKSSLD();
    }

    ~OrgKdeKSSLDInterface() {}

public Q_SLOTS: // METHODS
    Q_NOREPLY void setRule(const KSslCertificateRule &rule)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(rule);
        callWithArgumentList(QDBus::Block, QLatin1String("setRule"),
                             argumentList);
    }

    Q_NOREPLY void clearRule(const KSslCertificateRule &rule)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(rule);
        callWithArgumentList(QDBus::Block, QLatin1String("clearRule__rule"),
                             argumentList);
    }

    Q_NOREPLY void clearRule(const QSslCertificate &cert, const QString &hostName)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(cert) << qVariantFromValue(hostName);
        callWithArgumentList(QDBus::Block, QLatin1String("clearRule__certHost"),
                             argumentList);
    }

    QDBusReply<KSslCertificateRule> rule(const QSslCertificate &cert, const QString &hostName)
    {
        QList<QVariant> argumentList;
        argumentList << qVariantFromValue(cert) << qVariantFromValue(hostName);
        return callWithArgumentList(QDBus::Block, QLatin1String("rule"),
                                    argumentList);
    }
};

namespace org {
  namespace kde {
    typedef ::OrgKdeKSSLDInterface KSSLDInterface;
  }
}


#endif //KSSLDINTERFACE_H
