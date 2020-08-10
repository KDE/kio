/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007, 2008, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLD_H
#define KSSLD_H

#include <KDEDModule>
#include <QList>
#include <QVariant>
class QString;
class QSslCertificate;

class KSslCertificateRule;

class KSSLDPrivate;
class KSSLD : public KDEDModule
{
    Q_OBJECT
public:
    KSSLD(QObject *parent, const QVariantList &);
    ~KSSLD();

    void setRule(const KSslCertificateRule &rule);
    void clearRule(const KSslCertificateRule &rule);
    void clearRule(const QSslCertificate &cert, const QString &hostName);
    void pruneExpiredRules();
    KSslCertificateRule rule(const QSslCertificate &cert, const QString &hostName) const;

private:
    //AFAICS we don't need the d-pointer technique here but it makes the code look
    //more like the rest of kdelibs and it can be reused anywhere in kdelibs.
    KSSLDPrivate *const d;
};

#endif //KSSLD_H
