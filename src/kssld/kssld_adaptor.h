/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLD_ADAPTOR_H
#define KSSLD_ADAPTOR_H

#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusAbstractAdaptor>
#include <QDBusArgument>

#include "kssld_dbusmetatypes.h"

class KSSLDAdaptor: public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KSSLD")

public:
    explicit KSSLDAdaptor(KSSLD *parent)
        : QDBusAbstractAdaptor(parent)
    {
        Q_ASSERT(parent);
        registerMetaTypesForKSSLD();
    }

private:
    inline KSSLD *p()
    {
        return static_cast<KSSLD *>(parent());
    }

public Q_SLOTS:
    inline Q_NOREPLY void setRule(const KSslCertificateRule &rule)
    {
        p()->setRule(rule);
    }

    inline Q_NOREPLY void clearRule__rule(const KSslCertificateRule &rule)
    {
        p()->clearRule(rule);
    }

    inline Q_NOREPLY void clearRule__certHost(const QSslCertificate &cert, const QString &hostName)
    {
        p()->clearRule(cert, hostName);
    }

    inline KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName)
    {
        return p()->rule(cert, hostName);
    }
};

#endif //KSSLD_ADAPTOR_H
