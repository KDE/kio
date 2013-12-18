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

#ifndef KSSLD_ADAPTOR_H
#define KSSLD_ADAPTOR_H

#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMetaType>
#include <QtDBus/QDBusAbstractAdaptor>
#include <QtDBus/QDBusArgument>

#include "kssld_dbusmetatypes.h"


class KSSLDAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KSSLD")

public:
    KSSLDAdaptor(KSSLD *parent)
     : QDBusAbstractAdaptor(parent)
    {
        Q_ASSERT(parent);
        registerMetaTypesForKSSLD();
    }

private:
    inline KSSLD *p()
        { return static_cast<KSSLD *>(parent()); }

public Q_SLOTS:
    inline Q_NOREPLY void setRule(const KSslCertificateRule &rule)
        { return p()->setRule(rule); }

    inline Q_NOREPLY void clearRule__rule(const KSslCertificateRule &rule)
        { return p()->clearRule(rule); }

    inline Q_NOREPLY void clearRule__certHost(const QSslCertificate &cert, const QString &hostName)
        { return p()->clearRule(cert, hostName); }

    inline KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName)
        { return p()->rule(cert, hostName); }
};

#endif //KSSLD_ADAPTOR_H
