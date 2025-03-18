/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ksslcertificatemanager.h"
#include "ksslcertificatemanager_p.h"

#ifdef WITH_QTDBUS
#include "kssld_interface.h"
#endif

#include "ksslerroruidata_p.h"

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSslConfiguration>
#include <QStandardPaths>

#include <set>

/*
  Config file format:
[<MD5-Digest>]
<Host> = <Date> <List of ignored errors>
#for example
#mail.kdab.net =  ExpireUTC 2008-08-20T18:22:14, SelfSigned, Expired
#very.old.com =  ExpireUTC 2008-08-20T18:22:14, TooWeakEncryption <- not actually planned to implement
#clueless.admin.com =  ExpireUTC 2008-08-20T18:22:14, HostNameMismatch
#
#Wildcard syntax
#* = ExpireUTC 2008-08-20T18:22:14, SelfSigned
#*.kdab.net = ExpireUTC 2008-08-20T18:22:14, SelfSigned
#mail.kdab.net = ExpireUTC 2008-08-20T18:22:14, All <- not implemented
#* = ExpireUTC 9999-12-31T23:59:59, Reject  #we know that something is wrong with that certificate
CertificatePEM = <PEM-encoded certificate> #host entries are all lowercase, thus no clashes

 */

// TODO GUI for managing exception rules

KSslCertificateRule::KSslCertificateRule(const QSslCertificate &cert, const QString &hostName)
    : d(new KSslCertificateRulePrivate())
{
    d->certificate = cert;
    d->hostName = hostName;
    d->isRejected = false;
}

KSslCertificateRule::KSslCertificateRule(const KSslCertificateRule &other)
    : d(new KSslCertificateRulePrivate())
{
    *d = *other.d;
}

KSslCertificateRule::~KSslCertificateRule() = default;

KSslCertificateRule &KSslCertificateRule::operator=(const KSslCertificateRule &other)
{
    *d = *other.d;
    return *this;
}

QSslCertificate KSslCertificateRule::certificate() const
{
    return d->certificate;
}

QString KSslCertificateRule::hostName() const
{
    return d->hostName;
}

void KSslCertificateRule::setExpiryDateTime(const QDateTime &dateTime)
{
    d->expiryDateTime = dateTime;
}

QDateTime KSslCertificateRule::expiryDateTime() const
{
    return d->expiryDateTime;
}

void KSslCertificateRule::setRejected(bool rejected)
{
    d->isRejected = rejected;
}

bool KSslCertificateRule::isRejected() const
{
    return d->isRejected;
}

bool KSslCertificateRule::isErrorIgnored(QSslError::SslError error) const
{
    return d->ignoredErrors.contains(error);
}

void KSslCertificateRule::setIgnoredErrors(const QList<QSslError> &errors)
{
    d->ignoredErrors.clear();
    for (const QSslError &error : errors) {
        if (!isErrorIgnored(error.error())) {
            d->ignoredErrors.append(error.error());
        }
    }
}

void KSslCertificateRule::setIgnoredErrors(const QList<QSslError::SslError> &errors)
{
    d->ignoredErrors.clear();
    for (QSslError::SslError error : errors) {
        if (!isErrorIgnored(error)) {
            d->ignoredErrors.append(error);
        }
    }
}

QList<QSslError::SslError> KSslCertificateRule::ignoredErrors() const
{
    return d->ignoredErrors;
}

QList<QSslError> KSslCertificateRule::filterErrors(const QList<QSslError> &errors) const
{
    QList<QSslError> ret;
    for (const QSslError &error : errors) {
        if (!isErrorIgnored(error.error())) {
            ret.append(error);
        }
    }
    return ret;
}

////////////////////////////////////////////////////////////////////

static QList<QSslCertificate> deduplicate(const QList<QSslCertificate> &certs)
{
    std::set<QByteArray> digests;
    QList<QSslCertificate> ret;
    for (const QSslCertificate &cert : certs) {
        QByteArray digest = cert.digest();
        const auto [it, isInserted] = digests.insert(digest);
        if (isInserted) {
            ret.append(cert);
        }
    }
    return ret;
}

KSslCertificateManagerPrivate::KSslCertificateManagerPrivate()
    : config(QStringLiteral("ksslcertificatemanager"), KConfig::SimpleConfig)
#ifdef WITH_QTDBUS
    , iface(new org::kde::KSSLDInterface(QStringLiteral("org.kde.kssld6"), QStringLiteral("/modules/kssld"), QDBusConnection::sessionBus()))
#endif
    , isCertListLoaded(false)
    , userCertDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kssl/userCaCertificates/"))
{
}

KSslCertificateManagerPrivate::~KSslCertificateManagerPrivate()
{
#ifdef WITH_QTDBUS
    delete iface;
    iface = nullptr;
#endif
}

void KSslCertificateManagerPrivate::loadDefaultCaCertificates()
{
    defaultCaCertificates.clear();

    QList<QSslCertificate> certs = deduplicate(QSslConfiguration::systemCaCertificates());

    KConfig config(QStringLiteral("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group(QStringLiteral("Blacklist of CA Certificates"));

    certs.append(QSslCertificate::fromPath(userCertDir + QLatin1Char('*'), QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard));

    for (const QSslCertificate &cert : std::as_const(certs)) {
        const QByteArray digest = cert.digest().toHex();
        if (!group.hasKey(digest.constData())) {
            defaultCaCertificates += cert;
        }
    }

    isCertListLoaded = true;
}

bool KSslCertificateManagerPrivate::addCertificate(const KSslCaCertificate &in)
{
    // qDebug() << Q_FUNC_INFO;
    // cannot add a certificate to the system store
    if (in.store == KSslCaCertificate::SystemStore) {
        Q_ASSERT(false);
        return false;
    }
    if (knownCerts.contains(in.certHash)) {
        Q_ASSERT(false);
        return false;
    }

    QString certFilename = userCertDir + QString::fromLatin1(in.certHash);

    QFile certFile(certFilename);
    if (!QDir().mkpath(userCertDir) || certFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    if (!certFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (certFile.write(in.cert.toPem()) < 1) {
        return false;
    }
    knownCerts.insert(in.certHash);

    updateCertificateBlacklisted(in);

    return true;
}

bool KSslCertificateManagerPrivate::removeCertificate(const KSslCaCertificate &old)
{
    // qDebug() << Q_FUNC_INFO;
    // cannot remove a certificate from the system store
    if (old.store == KSslCaCertificate::SystemStore) {
        Q_ASSERT(false);
        return false;
    }

    if (!QFile::remove(userCertDir + QString::fromLatin1(old.certHash))) {
        // suppose somebody copied a certificate file into userCertDir without changing the
        // filename to the digest.
        // the rest of the code will work fine because it loads all certificate files from
        // userCertDir without asking for the name, we just can't remove the certificate using
        // its digest as filename - so search the whole directory.
        // if the certificate was added with the digest as name *and* with a different name, we
        // still fail to remove it completely at first try - BAD USER! BAD!

        bool removed = false;
        QDir dir(userCertDir);
        const QStringList dirList = dir.entryList(QDir::Files);
        for (const QString &certFilename : dirList) {
            const QString certPath = userCertDir + certFilename;
            QList<QSslCertificate> certs = QSslCertificate::fromPath(certPath);

            if (!certs.isEmpty() && certs.at(0).digest().toHex() == old.certHash) {
                if (QFile::remove(certPath)) {
                    removed = true;
                } else {
                    // maybe the file is readable but not writable
                    return false;
                }
            }
        }
        if (!removed) {
            // looks like the file is not there
            return false;
        }
    }

    // note that knownCerts *should* need no updating due to the way setAllCertificates() works -
    // it should never call addCertificate and removeCertificate for the same cert in one run

    // clean up the blacklist
    setCertificateBlacklisted(old.certHash, false);

    return true;
}

static bool certLessThan(const KSslCaCertificate &cacert1, const KSslCaCertificate &cacert2)
{
    if (cacert1.store != cacert2.store) {
        // SystemStore is numerically smaller so the system certs come first; this is important
        // so that system certificates come first in case the user added an already-present
        // certificate as a user certificate.
        return cacert1.store < cacert2.store;
    }
    return cacert1.certHash < cacert2.certHash;
}

void KSslCertificateManagerPrivate::setAllCertificates(const QList<KSslCaCertificate> &certsIn)
{
    Q_ASSERT(knownCerts.isEmpty());
    QList<KSslCaCertificate> in = certsIn;
    QList<KSslCaCertificate> old = allCertificates();
    std::sort(in.begin(), in.end(), certLessThan);
    std::sort(old.begin(), old.end(), certLessThan);

    for (int ii = 0, oi = 0; ii < in.size() || oi < old.size(); ++ii, ++oi) {
        // look at all elements in both lists, even if we reach the end of one early.
        if (ii >= in.size()) {
            removeCertificate(old.at(oi));
            continue;
        } else if (oi >= old.size()) {
            addCertificate(in.at(ii));
            continue;
        }

        if (certLessThan(old.at(oi), in.at(ii))) {
            // the certificate in "old" is not in "in". only advance the index of "old".
            removeCertificate(old.at(oi));
            ii--;
        } else if (certLessThan(in.at(ii), old.at(oi))) {
            // the certificate in "in" is not in "old". only advance the index of "in".
            addCertificate(in.at(ii));
            oi--;
        } else { // in.at(ii) "==" old.at(oi)
            if (in.at(ii).cert != old.at(oi).cert) {
                // hash collision, be prudent(?) and don't do anything.
            } else {
                knownCerts.insert(old.at(oi).certHash);
                if (in.at(ii).isBlacklisted != old.at(oi).isBlacklisted) {
                    updateCertificateBlacklisted(in.at(ii));
                }
            }
        }
    }
    knownCerts.clear();
    QMutexLocker certListLocker(&certListMutex);
    isCertListLoaded = false;
    loadDefaultCaCertificates();
}

QList<KSslCaCertificate> KSslCertificateManagerPrivate::allCertificates() const
{
    // qDebug() << Q_FUNC_INFO;
    QList<KSslCaCertificate> ret;
    const QList<QSslCertificate> list = deduplicate(QSslConfiguration::systemCaCertificates());
    for (const QSslCertificate &cert : list) {
        ret += KSslCaCertificate(cert, KSslCaCertificate::SystemStore, false);
    }

    const QList<QSslCertificate> userList = QSslCertificate::fromPath(userCertDir + QLatin1Char('*'), QSsl::Pem, QSslCertificate::PatternSyntax::Wildcard);
    for (const QSslCertificate &cert : userList) {
        ret += KSslCaCertificate(cert, KSslCaCertificate::UserStore, false);
    }

    KConfig config(QStringLiteral("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group(QStringLiteral("Blacklist of CA Certificates"));
    for (KSslCaCertificate &cert : ret) {
        if (group.hasKey(cert.certHash.constData())) {
            cert.isBlacklisted = true;
            // qDebug() << "is blacklisted";
        }
    }

    return ret;
}

bool KSslCertificateManagerPrivate::updateCertificateBlacklisted(const KSslCaCertificate &cert)
{
    return setCertificateBlacklisted(cert.certHash, cert.isBlacklisted);
}

bool KSslCertificateManagerPrivate::setCertificateBlacklisted(const QByteArray &certHash, bool isBlacklisted)
{
    // qDebug() << Q_FUNC_INFO << isBlacklisted;
    KConfig config(QStringLiteral("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group(QStringLiteral("Blacklist of CA Certificates"));
    if (isBlacklisted) {
        // TODO check against certificate list ?
        group.writeEntry(certHash.constData(), QString());
    } else {
        if (!group.hasKey(certHash.constData())) {
            return false;
        }
        group.deleteEntry(certHash.constData());
    }

    return true;
}

class KSslCertificateManagerContainer
{
public:
    KSslCertificateManager sslCertificateManager;
};

Q_GLOBAL_STATIC(KSslCertificateManagerContainer, g_instance)

KSslCertificateManager::KSslCertificateManager()
    : d(new KSslCertificateManagerPrivate())
{
}

KSslCertificateManager::~KSslCertificateManager() = default;

// static
KSslCertificateManager *KSslCertificateManager::self()
{
    return &g_instance()->sslCertificateManager;
}

void KSslCertificateManager::setRule(const KSslCertificateRule &rule)
{
#ifdef WITH_QTDBUS
    d->iface->setRule(rule);
#endif
}

void KSslCertificateManager::clearRule(const KSslCertificateRule &rule)
{
#ifdef WITH_QTDBUS
    d->iface->clearRule(rule);
#endif
}

void KSslCertificateManager::clearRule(const QSslCertificate &cert, const QString &hostName)
{
#ifdef WITH_QTDBUS
    d->iface->clearRule(cert, hostName);
#endif
}

KSslCertificateRule KSslCertificateManager::rule(const QSslCertificate &cert, const QString &hostName) const
{
#ifdef WITH_QTDBUS
    return d->iface->rule(cert, hostName);
#else
    return KSslCertificateRule();
#endif
}

QList<QSslCertificate> KSslCertificateManager::caCertificates() const
{
    QMutexLocker certLocker(&d->certListMutex);
    if (!d->isCertListLoaded) {
        d->loadDefaultCaCertificates();
    }
    return d->defaultCaCertificates;
}

QList<QSslError> KSslCertificateManager::nonIgnorableErrors(const QList<QSslError> &errors)
{
    QList<QSslError> ret;
    // errors not handled in KSSLD
    std::copy_if(errors.begin(), errors.end(), std::back_inserter(ret), [](const QSslError &e) {
        return e.error() == QSslError::NoPeerCertificate || e.error() == QSslError::PathLengthExceeded || e.error() == QSslError::NoSslSupport;
    });
    return ret;
}

QList<KSslCaCertificate> _allKsslCaCertificates(KSslCertificateManager *cm)
{
    return KSslCertificateManagerPrivate::get(cm)->allCertificates();
}

void _setAllKsslCaCertificates(KSslCertificateManager *cm, const QList<KSslCaCertificate> &certsIn)
{
    KSslCertificateManagerPrivate::get(cm)->setAllCertificates(certsIn);
}

#ifdef WITH_QTDBUS
#include "moc_kssld_interface.cpp"
#endif
