/* This file is part of the KDE project
 *
 * Copyright (C) 2010 Andreas Hartmetz <ahartmetz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KSSLCERTIFICATEMANAGER_P_H
#define KSSLCERTIFICATEMANAGER_P_H

#include <QMutex>

#include "kconfig.h"

struct KSslCaCertificate
{
    enum Store {
        SystemStore = 0,
        UserStore
    };

    // TODO see if we can get rid of the .toHex() for storage and comparison; requires
    //      several changes in KSslCertificateManager and CaCertificatesPage!
    KSslCaCertificate(const QSslCertificate &c, Store s, bool _isBlacklisted)
     : cert(c),
       certHash(c.digest().toHex()),
       store(s),
       isBlacklisted(_isBlacklisted) { }
    const QSslCertificate cert;
    const QByteArray certHash;
    const Store store;
    bool isBlacklisted;
    // the synthesized version without the const_casts doesn't compile
    const KSslCaCertificate &operator=(const KSslCaCertificate &other)
    {
        const_cast<QSslCertificate &>(cert) = other.cert;
        const_cast<QByteArray &>(certHash) = other.certHash;
        const_cast<Store &>(store) = other.store;
        isBlacklisted = other.isBlacklisted;
        return *this;
    }
};

class OrgKdeKSSLDInterface; // aka org::kde::KSSLDInterface
namespace org { namespace kde {
typedef ::OrgKdeKSSLDInterface KSSLDInterface;
}}

class KSslCertificateManagerPrivate
{
public:
    KSslCertificateManagerPrivate();
    ~KSslCertificateManagerPrivate();

    static KSslCertificateManagerPrivate *get(KSslCertificateManager *q)
        { return q->d; }

    void loadDefaultCaCertificates();

    // helpers for setAllCertificates()
    bool addCertificate(const KSslCaCertificate &in);
    bool removeCertificate(const KSslCaCertificate &old);
    bool updateCertificateBlacklisted(const KSslCaCertificate &cert);
    bool setCertificateBlacklisted(const QByteArray &certHash, bool isBlacklisted);

    void setAllCertificates(const QList<KSslCaCertificate> &certsIn);
    QList<KSslCaCertificate> allCertificates() const;

    KConfig config;
    org::kde::KSSLDInterface *iface;
    QHash<QString, KSslError::Error> stringToSslError;
    QHash<KSslError::Error, QString> sslErrorToString;

    QList<QSslCertificate> defaultCaCertificates;

    // for use in setAllCertificates() only
    QSet<QByteArray> knownCerts;
    QMutex certListMutex;
    bool isCertListLoaded;
    QString userCertDir;
};

// don't export KSslCertificateManagerPrivate to avoid unnecessary symbols
KIOCORE_EXPORT QList<KSslCaCertificate> _allKsslCaCertificates(KSslCertificateManager *cm);
KIOCORE_EXPORT void _setAllKsslCaCertificates(KSslCertificateManager *cm,
                                              const QList<KSslCaCertificate> &certsIn);

#endif //KSSLCERTIFICATEMANAGER_P_H
