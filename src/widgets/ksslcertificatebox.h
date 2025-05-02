/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLCERTIFICATEBOX_H
#define KSSLCERTIFICATEBOX_H

#include "kiowidgets_export.h"

#include <QWidget>

#include <memory>

class QSslCertificate;

class KSslCertificateBoxPrivate;

/*!
 * \class KSslCertificateBox
 * \inmodule KIOWidgets
 */
class KIOWIDGETS_EXPORT KSslCertificateBox : public QWidget
{
    Q_OBJECT
public:
    /*!
     * \value Subject
     * \value Issuer
     */
    enum CertificateParty {
        Subject = 0,
        Issuer,
    };

    explicit KSslCertificateBox(QWidget *parent = nullptr);
    ~KSslCertificateBox() override;

    /*!
     *
     */
    void setCertificate(const QSslCertificate &cert, CertificateParty party);

    /*!
     *
     */
    void clear();

    std::unique_ptr<KSslCertificateBoxPrivate> const d;
};

#endif // KSSLCERTIFICATEBOX_H
