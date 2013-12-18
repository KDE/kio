/* This file is part of the KDE project
 *
 * Copyright (C) 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>
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

#include "ksslcertificatemanager.h"
#include "ksslcertificatemanager_p.h"

#include "ktcpsocket.h"
#include "ktcpsocket_p.h"
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>

#include <QDebug>
#include <qstandardpaths.h>
#include <QFile>
#include <QDir>
#include <QDBusConnection>
#include <QDBusReply>
#include <QDBusConnectionInterface>

#include "kssld_interface.h"

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

class KSslCertificateRulePrivate
{
public:
    QSslCertificate certificate;
    QString hostName;
    bool isRejected;
    QDateTime expiryDateTime;
    QList<KSslError::Error> ignoredErrors;
};


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


KSslCertificateRule::~KSslCertificateRule()
{
    delete d;
}


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


bool KSslCertificateRule::isErrorIgnored(KSslError::Error error) const
{
    foreach (KSslError::Error ignoredError, d->ignoredErrors)
        if (error == ignoredError)
            return true;

    return false;
}


void KSslCertificateRule::setIgnoredErrors(const QList<KSslError::Error> &errors)
{
    d->ignoredErrors.clear();
    //### Quadratic runtime, woohoo! Use a QSet if that should ever be an issue.
    foreach(KSslError::Error e, errors)
        if (!isErrorIgnored(e))
            d->ignoredErrors.append(e);
}


void KSslCertificateRule::setIgnoredErrors(const QList<KSslError> &errors)
{
    QList<KSslError::Error> el;
    foreach(const KSslError &e, errors)
        el.append(e.error());
    setIgnoredErrors(el);
}


QList<KSslError::Error> KSslCertificateRule::ignoredErrors() const
{
    return d->ignoredErrors;
}


QList<KSslError::Error> KSslCertificateRule::filterErrors(const QList<KSslError::Error> &errors) const
{
    QList<KSslError::Error> ret;
    foreach (KSslError::Error error, errors) {
        if (!isErrorIgnored(error))
            ret.append(error);
    }
    return ret;
}


QList<KSslError> KSslCertificateRule::filterErrors(const QList<KSslError> &errors) const
{
    QList<KSslError> ret;
    foreach (const KSslError &error, errors) {
        if (!isErrorIgnored(error.error()))
            ret.append(error);
    }
    return ret;
}


////////////////////////////////////////////////////////////////////

static QList<QSslCertificate> deduplicate(const QList<QSslCertificate> &certs)
{
    QSet<QByteArray> digests;
    QList<QSslCertificate> ret;
    foreach (const QSslCertificate &cert, certs) {
        QByteArray digest = cert.digest();
        if (!digests.contains(digest)) {
            digests.insert(digest);
            ret.append(cert);
        }
    }
    return ret;
}

KSslCertificateManagerPrivate::KSslCertificateManagerPrivate()
 : config(QString::fromLatin1("ksslcertificatemanager"), KConfig::SimpleConfig),
   iface(new org::kde::KSSLDInterface(QString::fromLatin1("org.kde.kded5"),
                                      QString::fromLatin1("/modules/kssld"),
                                      QDBusConnection::sessionBus())),
   isCertListLoaded(false),
   userCertDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QString::fromLatin1("/kssl/userCaCertificates/"))
{
    // set Qt's set to empty; this is protected by the lock in K_GLOBAL_STATIC.
    QSslSocket::setDefaultCaCertificates(QList<QSslCertificate>());
}

KSslCertificateManagerPrivate::~KSslCertificateManagerPrivate()
{
    delete iface;
    iface = 0;
}

void KSslCertificateManagerPrivate::loadDefaultCaCertificates()
{
    defaultCaCertificates.clear();

    QList<QSslCertificate> certs = deduplicate(QSslSocket::systemCaCertificates());

    KConfig config(QString::fromLatin1("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group("Blacklist of CA Certificates");

    certs.append(QSslCertificate::fromPath(userCertDir + QLatin1String("*"), QSsl::Pem,
                                           QRegExp::Wildcard));
    foreach (const QSslCertificate &cert, certs) {
        const QByteArray digest = cert.digest().toHex();
        if (!group.hasKey(digest.constData())) {
            defaultCaCertificates += cert;
        }
    }

    isCertListLoaded = true;
}


bool KSslCertificateManagerPrivate::addCertificate(const KSslCaCertificate &in)
{
    //qDebug() << Q_FUNC_INFO;
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
    //qDebug() << certFilename;
    QFile certFile(certFilename);
    if (certFile.open(QIODevice::ReadOnly)) {
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
    //qDebug() << Q_FUNC_INFO;
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
        foreach (const QString &certFilename, dir.entryList(QDir::Files)) {
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
    qSort(in.begin(), in.end(), certLessThan);
    qSort(old.begin(), old.end(), certLessThan);

    for (int ii = 0, oi = 0; ii < in.size() || oi < old.size(); ++ii, ++oi) {
        // look at all elements in both lists, even if we reach the end of one early.
        if (ii >= in.size()) {
            removeCertificate(old.at(oi));
            continue;
        } else if (oi >= old.size()) {
            addCertificate(in.at(ii));
            continue;
        }

        if (certLessThan (old.at(oi), in.at(ii))) {
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
    //qDebug() << Q_FUNC_INFO;
    QList<KSslCaCertificate> ret;
    foreach (const QSslCertificate &cert, deduplicate(QSslSocket::systemCaCertificates())) {
        ret += KSslCaCertificate(cert, KSslCaCertificate::SystemStore, false);
    }

    foreach (const QSslCertificate &cert, QSslCertificate::fromPath(userCertDir + QLatin1String("*"),
                                                                    QSsl::Pem, QRegExp::Wildcard)) {
        ret += KSslCaCertificate(cert, KSslCaCertificate::UserStore, false);
    }

    KConfig config(QString::fromLatin1("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group("Blacklist of CA Certificates");
    for (int i = 0; i < ret.size(); i++) {
        if (group.hasKey(ret[i].certHash.constData())) {
            ret[i].isBlacklisted = true;
            //qDebug() << "is blacklisted";
        }
    }

    return ret;
}


bool KSslCertificateManagerPrivate::updateCertificateBlacklisted(const KSslCaCertificate &cert)
{
    return setCertificateBlacklisted(cert.certHash, cert.isBlacklisted);
}


bool KSslCertificateManagerPrivate::setCertificateBlacklisted(const QByteArray &certHash,
                                                              bool isBlacklisted)
{
    //qDebug() << Q_FUNC_INFO << isBlacklisted;
    KConfig config(QString::fromLatin1("ksslcablacklist"), KConfig::SimpleConfig);
    KConfigGroup group = config.group("Blacklist of CA Certificates");
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
    // Make sure kded is running
    QDBusConnectionInterface* bus = QDBusConnection::sessionBus().interface();
    if (!bus->isServiceRegistered(QLatin1String("org.kde.kded5"))) {
        QDBusReply<void> reply = bus->startService(QLatin1String("org.kde.kded5"));
        if (!reply.isValid()) {
            qWarning() << "Couldn't start kded5 from org.kde.kded5.service:" << reply.error();
        }
    }
}


KSslCertificateManager::~KSslCertificateManager()
{
    delete d;
}


//static
KSslCertificateManager *KSslCertificateManager::self()
{
    return &g_instance()->sslCertificateManager;
}


void KSslCertificateManager::setRule(const KSslCertificateRule &rule)
{
    d->iface->setRule(rule);
}


void KSslCertificateManager::clearRule(const KSslCertificateRule &rule)
{
    d->iface->clearRule(rule);
}


void KSslCertificateManager::clearRule(const QSslCertificate &cert, const QString &hostName)
{
    d->iface->clearRule(cert, hostName);
}


KSslCertificateRule KSslCertificateManager::rule(const QSslCertificate &cert,
                                                 const QString &hostName) const
{
    return d->iface->rule(cert, hostName);
}


QList<QSslCertificate> KSslCertificateManager::caCertificates() const
{
    QMutexLocker certLocker(&d->certListMutex);
    if (!d->isCertListLoaded) {
        d->loadDefaultCaCertificates();
    }
    return d->defaultCaCertificates;
}


//static
QList<KSslError> KSslCertificateManager::nonIgnorableErrors(const QList<KSslError> &/*e*/)
{
    QList<KSslError> ret;
    // ### add filtering here...
    return ret;
}

//static
QList<KSslError::Error> KSslCertificateManager::nonIgnorableErrors(const QList<KSslError::Error> &/*e*/)
{
    QList<KSslError::Error> ret;
    // ### add filtering here...
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

#include "moc_kssld_interface.cpp"
