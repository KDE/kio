/*
   This file is part of the KDE libraries

   Copyright (c) 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

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

#include "kssld.h"

#include "ksslcertificatemanager.h"
#include "kssld_adaptor.h"

#include <kconfig.h>
#include <kconfiggroup.h>
#include <QtCore/QFile>

#include <QtCore/QDate>
#include <kpluginfactory.h>
#include <kpluginloader.h>

K_PLUGIN_FACTORY(KSSLDFactory, registerPlugin<KSSLD>();)
//KDECORE_EXPORT void *__kde_do_unload; // TODO re-add support for this?


class KSSLDPrivate
{
public:
    KSSLDPrivate()
     : config(QString::fromLatin1("ksslcertificatemanager"), KConfig::SimpleConfig)
    {
        struct strErr {
            const char *str;
            KSslError::Error err;
        };

        //hmmm, looks like these are all of the errors where it is possible to continue.
        const static strErr strError[] = {
            {"NoError", KSslError::NoError},
            {"UnknownError", KSslError::UnknownError},
            {"InvalidCertificateAuthority", KSslError::InvalidCertificateAuthorityCertificate},
            {"InvalidCertificate", KSslError::InvalidCertificate},
            {"CertificateSignatureFailed", KSslError::CertificateSignatureFailed},
            {"SelfSignedCertificate", KSslError::SelfSignedCertificate},
            {"RevokedCertificate", KSslError::RevokedCertificate},
            {"InvalidCertificatePurpose", KSslError::InvalidCertificatePurpose},
            {"RejectedCertificate", KSslError::RejectedCertificate},
            {"UntrustedCertificate", KSslError::UntrustedCertificate},
            {"ExpiredCertificate", KSslError::ExpiredCertificate},
            {"HostNameMismatch", KSslError::HostNameMismatch}
        };

        for (int i = 0; i < int(sizeof(strError)/sizeof(strErr)); i++) {
            QString s = QString::fromLatin1(strError[i].str);
            KSslError::Error e = strError[i].err;
            stringToSslError.insert(s, e);
            sslErrorToString.insert(e, s);
        }
    }

    KConfig config;
    QHash<QString, KSslError::Error> stringToSslError;
    QHash<KSslError::Error, QString> sslErrorToString;
};



KSSLD::KSSLD(QObject* parent, const QVariantList&)
 : KDEDModule(parent),
   d(new KSSLDPrivate())
{
    new KSSLDAdaptor(this);
    pruneExpiredRules();
}


KSSLD::~KSSLD()
{
    delete d;
}


void KSSLD::setRule(const KSslCertificateRule &rule)
{
    if (rule.hostName().isEmpty()) {
        return;
    }
    KConfigGroup group = d->config.group(rule.certificate().digest().toHex());

    QStringList sl;

    QString dtString = QString::fromLatin1("ExpireUTC ");
    dtString.append(rule.expiryDateTime().toString(Qt::ISODate));
    sl.append(dtString);

    if (rule.isRejected()) {
        sl.append(QString::fromLatin1("Reject"));
    } else {
        foreach (KSslError::Error e, rule.ignoredErrors())
            sl.append(d->sslErrorToString.value(e));
    }

    if (!group.hasKey("CertificatePEM"))
        group.writeEntry("CertificatePEM", rule.certificate().toPem());
#ifdef PARANOIA
    else
        if (group.readEntry("CertificatePEM") != rule.certificate().toPem())
            return;
#endif
    group.writeEntry(rule.hostName(), sl);
    group.sync();
}


void KSSLD::clearRule(const KSslCertificateRule &rule)
{
    clearRule(rule.certificate(), rule.hostName());
}


void KSSLD::clearRule(const QSslCertificate &cert, const QString &hostName)
{
    KConfigGroup group = d->config.group(cert.digest().toHex());
    group.deleteEntry(hostName);
    if (group.keyList().size() < 2) {
        group.deleteGroup();
    }
    group.sync();
}


void KSSLD::pruneExpiredRules()
{
    // expired rules are deleted when trying to load them, so we just try to load all rules.
    // be careful about iterating over KConfig(Group) while changing it
    foreach (const QString &groupName, d->config.groupList()) {
        QByteArray certDigest = groupName.toLatin1();
        foreach (const QString &key, d->config.group(groupName).keyList()) {
            if (key == QLatin1String("CertificatePEM")) {
                continue;
            }
            KSslCertificateRule r = rule(QSslCertificate(certDigest), key);
        }
    }
}


// check a domain name with subdomains for well-formedness and count the dot-separated parts
static QString normalizeSubdomains(const QString &hostName, int *namePartsCount)
{
    QString ret;
    int partsCount = 0;
    bool wasPrevDot = true; // -> allow no dot at the beginning and count first name part
    const int length = hostName.length();
    for (int i = 0; i < length; i++) {
        const QChar c = hostName.at(i);
        if (c == QLatin1Char('.')) {
            if (wasPrevDot || (i + 1 == hostName.length())) {
                // consecutive dots or a dot at the end are forbidden
                partsCount = 0;
                ret.clear();
                break;
            }
            wasPrevDot = true;
        } else {
            if (wasPrevDot) {
                partsCount++;
            }
            wasPrevDot = false;
        }
        ret.append(c);
    }

    *namePartsCount = partsCount;
    return ret;
}


KSslCertificateRule KSSLD::rule(const QSslCertificate &cert, const QString &hostName) const
{
    const QByteArray certDigest = cert.digest().toHex();
    KConfigGroup group = d->config.group(certDigest);

    KSslCertificateRule ret(cert, hostName);
    bool foundHostName = false;

    int needlePartsCount;
    QString needle = normalizeSubdomains(hostName, &needlePartsCount);

    // Find a rule for the hostname, either...
    if (group.hasKey(needle)) {
      // directly (host, site.tld, a.site.tld etc)
      if (needlePartsCount >= 1) {
        foundHostName = true;
      }
    } else {
        // or with wildcards
        //   "tld" <- "*." and "site.tld" <- "*.tld" are not valid matches,
        //   "a.site.tld" <- "*.site.tld" is
        while (--needlePartsCount >= 2) {
            const int dotIndex = needle.indexOf(QLatin1Char('.'));
            Q_ASSERT(dotIndex > 0); // if this fails normalizeSubdomains() failed
            needle.remove(0, dotIndex - 1);
            needle[0] = QChar::fromLatin1('*');
            if (group.hasKey(needle)) {
                foundHostName = true;
                break;
            }
            needle.remove(0, 2);    // remove "*."
        }
    }

    if (!foundHostName) {
        //Don't make a rule with the failed wildcard pattern - use the original hostname.
        return KSslCertificateRule(cert, hostName);
    }

    //parse entry of the format "ExpireUTC <date>, Reject" or
    //"ExpireUTC <date>, HostNameMismatch, ExpiredCertificate, ..."
    QStringList sl = group.readEntry(needle, QStringList());

    QDateTime expiryDt;
    // the rule is well-formed if it contains at least the expire date and one directive
    if (sl.size() >= 2) {
        QString dtString = sl.takeFirst();
        if (dtString.startsWith(QLatin1String("ExpireUTC "))) {
            dtString.remove(0, 10/* length of "ExpireUTC " */);
            expiryDt = QDateTime::fromString(dtString, Qt::ISODate);
        }
    }

    if (!expiryDt.isValid() || expiryDt < QDateTime::currentDateTime()) {
        //the entry is malformed or expired so we remove it
        group.deleteEntry(needle);
        //the group is useless once only the CertificatePEM entry left
        if (group.keyList().size() < 2) {
            group.deleteGroup();
        }
        return ret;
    }

    QList<KSslError::Error> ignoredErrors;
    bool isRejected = false;
    foreach (const QString &s, sl) {
        if (s == QLatin1String("Reject")) {
            isRejected = true;
            ignoredErrors.clear();
            break;
        }
        if (!d->stringToSslError.contains(s)) {
            continue;
        }
        ignoredErrors.append(d->stringToSslError.value(s));
    }

    //Everything is checked and we can make ret valid
    ret.setExpiryDateTime(expiryDt);
    ret.setRejected(isRejected);
    ret.setIgnoredErrors(ignoredErrors);
    return ret;
}


#include "moc_kssld.cpp"
#include "moc_kssld_adaptor.cpp"
#include "kssld.moc"
