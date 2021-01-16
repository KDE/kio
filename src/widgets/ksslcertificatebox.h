/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLCERTIFICATEBOX_H
#define KSSLCERTIFICATEBOX_H

#include "kiowidgets_export.h"

#include <QWidget>

class QSslCertificate;

class KSslCertificateBoxPrivate;

class KIOWIDGETS_EXPORT KSslCertificateBox : public QWidget
{
    Q_OBJECT
public:
    enum CertificateParty {
        Subject = 0,
        Issuer,
    };

    explicit KSslCertificateBox(QWidget *parent = nullptr);
    ~KSslCertificateBox();

    void setCertificate(const QSslCertificate &cert, CertificateParty party);
    void clear();

    KSslCertificateBoxPrivate *const d;
};

#endif // KSSLCERTIFICATEBOX_H
