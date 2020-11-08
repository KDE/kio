/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kssld.h"

#include "ksslcertificatemanager.h"
#include "ksslcertificatemanager_p.h"
#include "kssld_adaptor.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDate>
#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(KSSLD, "kssld.json")

class KSSLDPrivate
{
public:
    KSSLDPrivate()
        : config(QStringLiteral("ksslcertificatemanager"), KConfig::SimpleConfig)
    {
        struct strErr {
            const char *str;
            QSslError::SslError err;
        };

        //hmmm, looks like these are all of the errors where it is possible to continue.
        // TODO for Qt > 5.14 QSslError::SslError is a Q_ENUM, and we can therefore replace this manual mapping table
        const static strErr strError[] = {
            {"NoError", QSslError::NoError},
            {"UnknownError", QSslError::UnspecifiedError},
            {"InvalidCertificateAuthority", QSslError::InvalidCaCertificate},
            {"InvalidCertificate", QSslError::UnableToDecodeIssuerPublicKey},
            {"CertificateSignatureFailed", QSslError::CertificateSignatureFailed},
            {"SelfSignedCertificate", QSslError::SelfSignedCertificate},
            {"RevokedCertificate", QSslError::CertificateRevoked},
            {"InvalidCertificatePurpose", QSslError::InvalidPurpose},
            {"RejectedCertificate", QSslError::CertificateRejected},
            {"UntrustedCertificate", QSslError::CertificateUntrusted},
            {"ExpiredCertificate", QSslError::CertificateExpired},
            {"HostNameMismatch", QSslError::HostNameMismatch},
            {"UnableToGetLocalIssuerCertificate", QSslError::UnableToGetLocalIssuerCertificate},
            {"InvalidNotBeforeField", QSslError::InvalidNotBeforeField},
            {"InvalidNotAfterField", QSslError::InvalidNotAfterField},
            {"CertificateNotYetValid", QSslError::CertificateNotYetValid},
            {"SubjectIssuerMismatch", QSslError::SubjectIssuerMismatch},
            {"AuthorityIssuerSerialNumberMismatch", QSslError::AuthorityIssuerSerialNumberMismatch},
            {"SelfSignedCertificateInChain", QSslError::SelfSignedCertificateInChain},
            {"UnableToVerifyFirstCertificate", QSslError::UnableToVerifyFirstCertificate},
            {"UnableToDecryptCertificateSignature", QSslError::UnableToDecryptCertificateSignature},
            {"UnableToGetIssuerCertificate", QSslError::UnableToGetIssuerCertificate}
        };

        for (const strErr &row : strError) {
            QString s = QString::fromLatin1(row.str);
            stringToSslError.insert(s, row.err);
            sslErrorToString.insert(row.err, s);
        }
    }

    KConfig config;
    QHash<QString, QSslError::SslError> stringToSslError;
    QHash<QSslError::SslError, QString> sslErrorToString;
};

KSSLD::KSSLD(QObject *parent, const QVariantList &)
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

    QString dtString = QStringLiteral("ExpireUTC ");
    dtString.append(rule.expiryDateTime().toString(Qt::ISODate));
    sl.append(dtString);

    if (rule.isRejected()) {
        sl.append(QStringLiteral("Reject"));
    } else {
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 64)
        for (QSslError::SslError e : qAsConst(rule.d->ignoredErrors)) {
#else
        const auto ignoredErrors = rule.ignoredErrors();
        for (QSslError::SslError e : ignoredErrors) {
#endif
            sl.append(d->sslErrorToString.value(e));
        }
    }

    if (!group.hasKey("CertificatePEM")) {
        group.writeEntry("CertificatePEM", rule.certificate().toPem());
    }
#ifdef PARANOIA
    else if (group.readEntry("CertificatePEM") != rule.certificate().toPem()) {
        return;
    }
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
    const QStringList groupNames = d->config.groupList();
    for (const QString &groupName : groupNames) {
        QByteArray certDigest = groupName.toLatin1();
        const QStringList keys = d->config.group(groupName).keyList();
        for (const QString &key : keys) {
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

    QList<QSslError::SslError> ignoredErrors;
    bool isRejected = false;
    for (const QString &s : qAsConst(sl)) {
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
