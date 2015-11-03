/* This file is part of the KDE project
 *
 * Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>
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

#include "ksslcertificatebox.h"

#include "ui_certificateparty.h"

#include <QtNetwork/QSslCertificate>

class KSslCertificateBoxPrivate
{
public:
    Ui::CertificateParty ui;
};

KSslCertificateBox::KSslCertificateBox(QWidget *parent)
    : QWidget(parent),
      d(new KSslCertificateBoxPrivate())
{
    d->ui.setupUi(this);
    // No fooling us with html tags
    Q_FOREACH (QLabel *label, findChildren<QLabel *>()) {
        label->setTextFormat(Qt::PlainText);
    }
}

KSslCertificateBox::~KSslCertificateBox()
{
    delete d;
}

void KSslCertificateBox::setCertificate(const QSslCertificate &cert, CertificateParty party)
{
    if (party == Subject)  {
        d->ui.commonName->setText(cert.subjectInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")));
        d->ui.organization->setText(cert.subjectInfo(QSslCertificate::Organization).join(QStringLiteral(", ")));
        d->ui.organizationalUnit
        ->setText(cert.subjectInfo(QSslCertificate::OrganizationalUnitName).join(QStringLiteral(", ")));
        d->ui.country->setText(cert.subjectInfo(QSslCertificate::CountryName).join(QStringLiteral(", ")));
        d->ui.state->setText(cert.subjectInfo(QSslCertificate::StateOrProvinceName).join(QStringLiteral(", ")));
        d->ui.city->setText(cert.subjectInfo(QSslCertificate::LocalityName).join(QStringLiteral(", ")));
    } else if (party == Issuer) {
        d->ui.commonName->setText(cert.issuerInfo(QSslCertificate::CommonName).join(QStringLiteral(", ")));
        d->ui.organization->setText(cert.issuerInfo(QSslCertificate::Organization).join(QStringLiteral(", ")));
        d->ui.organizationalUnit
        ->setText(cert.issuerInfo(QSslCertificate::OrganizationalUnitName).join(QStringLiteral(", ")));
        d->ui.country->setText(cert.issuerInfo(QSslCertificate::CountryName).join(QStringLiteral(", ")));
        d->ui.state->setText(cert.issuerInfo(QSslCertificate::StateOrProvinceName).join(QStringLiteral(", ")));
        d->ui.city->setText(cert.issuerInfo(QSslCertificate::LocalityName).join(QStringLiteral(", ")));
    }
}

void KSslCertificateBox::clear()
{
    d->ui.commonName->clear();
    d->ui.organization->clear();
    d->ui.organizationalUnit->clear();
    d->ui.country->clear();
    d->ui.state->clear();
    d->ui.city->clear();
}

