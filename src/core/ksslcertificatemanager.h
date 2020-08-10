/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _INCLUDE_KSSLCERTIFICATEMANAGER_H
#define _INCLUDE_KSSLCERTIFICATEMANAGER_H

#include "ktcpsocket.h" // TODO KF6 remove include

#include <QSslCertificate>
#include <QSslError>
#include <QString>
#include <QStringList>
#include <QDate>

class QDBusArgument;
class QSslCertificate;
class KSslCertificateRulePrivate;
class KSslCertificateManagerPrivate;

//### document this... :/
/** Certificate rule. */
class KIOCORE_EXPORT KSslCertificateRule
{
public:
    KSslCertificateRule(const QSslCertificate &cert = QSslCertificate(),
                        const QString &hostName = QString());
    KSslCertificateRule(const KSslCertificateRule &other);
    ~KSslCertificateRule();
    KSslCertificateRule &operator=(const KSslCertificateRule &other);

    QSslCertificate certificate() const;
    QString hostName() const;
    void setExpiryDateTime(const QDateTime &dateTime);
    QDateTime expiryDateTime() const;
    void setRejected(bool rejected);
    bool isRejected() const;
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 64)
    /** @deprecated since 5.64, use the QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateRule::isErrorIgnored(QSslError::SslError)")
    bool isErrorIgnored(KSslError::Error error) const;
#endif
    /**
     * Returns whether @p error is ignored for this certificate.
     * @since 5.64
     */
    bool isErrorIgnored(QSslError::SslError error) const;
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 64)
    /** @deprecated since 5.64, use the QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateRule::setIgnoredErrors(const QList<QSslError> &)")
    void setIgnoredErrors(const QList<KSslError::Error> &errors);
    /** @deprecated since 5.64, use the QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateRule::setIgnoredErrors(const QList<QSslError> &)")
    void setIgnoredErrors(const QList<KSslError> &errors);
#endif
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
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 64)
    QList<KSslError::Error> ignoredErrors() const;
#else
    QList<QSslError::SslError> ignoredErrors() const;
#endif
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 64)
    /** @deprecated since 5.64, use the QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateRule::filterErrors(const QList<QSslError> &)")
    QList<KSslError::Error> filterErrors(const QList<KSslError::Error> &errors) const;
    /** @deprecated since 5.64, use the QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateRule::filterErrors(const QList<QSslError> &)")
    QList<KSslError> filterErrors(const QList<KSslError> &errors) const;
#endif
    /**
     * Filter out errors that are already ignored.
     * @since 5.64
     */
    QList<QSslError> filterErrors(const QList<QSslError> &errors) const;
private:
    friend QDBusArgument &operator<<(QDBusArgument &argument, const KSslCertificateRule &rule); // TODO KF6 remove
    friend class KSSLD; // TODO KF6 remove
    KSslCertificateRulePrivate *const d;
};

//### document this too... :/
/** Certificate manager. */
class KIOCORE_EXPORT KSslCertificateManager
{
public:
    static KSslCertificateManager *self();
    void setRule(const KSslCertificateRule &rule);
    void clearRule(const KSslCertificateRule &rule);
    void clearRule(const QSslCertificate &cert, const QString &hostName);
    KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName) const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 6)
    /** @deprecated Since 4.6, use caCertificates() instead */
    KIOCORE_DEPRECATED_VERSION(4, 6, "Use KSslCertificateManager::caCertificates()")
    QList<QSslCertificate> rootCertificates() const
    {
        return caCertificates();
    }
#endif

    QList<QSslCertificate> caCertificates() const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 64)
    /** @deprecated since 5.64, use the corresponding QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateManager::nonIgnorableErrors(const QList<QSslError> &)")
    static QList<KSslError> nonIgnorableErrors(const QList<KSslError> &errors);
    /** @deprecated since 5.64, use the corresponding QSslError variant. */
    KIOCORE_DEPRECATED_VERSION(5, 64, "Use KSslCertificateManager::nonIgnorableErrors(const QList<QSslError> &)")
    static QList<KSslError::Error> nonIgnorableErrors(const QList<KSslError::Error> &errors);
#endif
    /**
     * Returns the subset of @p errors that cannot be ignored, ie. that is considered fatal.
     * @since 5.64
     */
    static QList<QSslError> nonIgnorableErrors(const QList<QSslError> &errors);

private:
    friend class KSslCertificateManagerContainer;
    friend class KSslCertificateManagerPrivate;
    KSslCertificateManager();
    ~KSslCertificateManager();

    KSslCertificateManagerPrivate *const d;
};

#endif
