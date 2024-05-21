/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _INCLUDE_KSSLCERTIFICATEMANAGER_H
#define _INCLUDE_KSSLCERTIFICATEMANAGER_H

#include <QDate>
#include <QSslCertificate>
#include <QSslError>
#include <QString>
#include <QStringList>

#include "kiocore_export.h"

#include <memory>

class QDBusArgument;
class KSslCertificateRulePrivate;
class KSslCertificateManagerPrivate;

// ### document this... :/
/** Certificate rule. */
class KIOCORE_EXPORT KSslCertificateRule
{
public:
    KSslCertificateRule(const QSslCertificate &cert = QSslCertificate(), const QString &hostName = QString());
    KSslCertificateRule(const KSslCertificateRule &other);
    ~KSslCertificateRule();
    KSslCertificateRule &operator=(const KSslCertificateRule &other);

    QSslCertificate certificate() const;
    QString hostName() const;
    void setExpiryDateTime(const QDateTime &dateTime);
    QDateTime expiryDateTime() const;
    void setRejected(bool rejected);
    bool isRejected() const;
    /**
     * Returns whether @p error is ignored for this certificate.
     * @since 5.64
     */
    bool isErrorIgnored(QSslError::SslError error) const;
    /**
     * Set the ignored errors for this certificate.
     * @since 5.64
     */
    void setIgnoredErrors(const QList<QSslError> &errors);
    /**
     * Set the ignored errors for this certificate.
     * @since 5.64
     */
    void setIgnoredErrors(const QList<QSslError::SslError> &errors);
    QList<QSslError::SslError> ignoredErrors() const;
    /**
     * Filter out errors that are already ignored.
     * @since 5.64
     */
    QList<QSslError> filterErrors(const QList<QSslError> &errors) const;

private:
    std::unique_ptr<KSslCertificateRulePrivate> const d;
};

// ### document this too... :/
/** Certificate manager. */
class KIOCORE_EXPORT KSslCertificateManager
{
public:
    static KSslCertificateManager *self();

    // TODO: the rule functions are not working if there is no DBus with a working kiod
    void setRule(const KSslCertificateRule &rule);
    void clearRule(const KSslCertificateRule &rule);
    void clearRule(const QSslCertificate &cert, const QString &hostName);
    KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName) const;

    QList<QSslCertificate> caCertificates() const;

    /**
     * Returns the subset of @p errors that cannot be ignored, ie. that is considered fatal.
     * @since 5.64
     */
    static QList<QSslError> nonIgnorableErrors(const QList<QSslError> &errors);

private:
    friend class KSslCertificateManagerContainer;
    friend class KSslCertificateManagerPrivate;
    KIOCORE_NO_EXPORT KSslCertificateManager();
    KIOCORE_NO_EXPORT ~KSslCertificateManager();

    std::unique_ptr<KSslCertificateManagerPrivate> d;
};

#endif
