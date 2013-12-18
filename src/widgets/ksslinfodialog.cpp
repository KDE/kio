/* This file is part of the KDE project
 *
 * Copyright (C) 2000,2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2000 Malte Starostik <malte@kde.org>
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

#include "ksslinfodialog.h"
#include "ui_sslinfo.h"
#include "ksslcertificatebox.h"

#include <QProcess>

#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>

#include <QtNetwork/QSslCertificate>

#include <klocalizedstring.h>
#include <kiconloader.h> // BarIcon

#include "ktcpsocket.h"


class KSslInfoDialog::KSslInfoDialogPrivate
{
public:
    QList<QSslCertificate> certificateChain;
    QList<QList<KSslError::Error> > certificateErrors;

    bool isMainPartEncrypted;
    bool auxPartsEncrypted;

    Ui::SslInfo ui;
    KSslCertificateBox *subject;
    KSslCertificateBox *issuer;
};



KSslInfoDialog::KSslInfoDialog(QWidget *parent)
 : QDialog(parent),
   d(new KSslInfoDialogPrivate)
{
    setWindowTitle(i18n("KDE SSL Information"));
    setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);

    QWidget *mainWidget = new QWidget(this);
    d->ui.setupUi(mainWidget);
    layout->addWidget(mainWidget);

    d->subject = new KSslCertificateBox(d->ui.certParties);
    d->issuer = new KSslCertificateBox(d->ui.certParties);
    d->ui.certParties->addTab(d->subject, i18nc("The receiver of the SSL certificate", "Subject"));
    d->ui.certParties->addTab(d->issuer, i18nc("The authority that issued the SSL certificate", "Issuer"));

    d->isMainPartEncrypted = true;
    d->auxPartsEncrypted = true;
    updateWhichPartsEncrypted();

    QDialogButtonBox *buttonBox = new QDialogButtonBox(this);
    buttonBox->setStandardButtons(QDialogButtonBox::Close);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    layout->addWidget(buttonBox);

#if 0
    if (QSslSocket::supportsSsl()) {
        if (d->m_secCon) {
            d->pixmap->setPixmap(BarIcon("security-high"));
            d->info->setText(i18n("Current connection is secured with SSL."));
        } else {
            d->pixmap->setPixmap(BarIcon("security-low"));
            d->info->setText(i18n("Current connection is not secured with SSL."));
        }
    } else {
        d->pixmap->setPixmap(BarIcon("security-low"));
        d->info->setText(i18n("SSL support is not available in this build of KDE."));
    }
#endif
}


KSslInfoDialog::~KSslInfoDialog()
{
    delete d;
}


//slot
void KSslInfoDialog::launchConfig()
{
    QProcess::startDetached("kcmshell4", QStringList() << "crypto");
}


void KSslInfoDialog::setMainPartEncrypted(bool mainEncrypted)
{
    d->isMainPartEncrypted = mainEncrypted;
    updateWhichPartsEncrypted();
}


void KSslInfoDialog::setAuxiliaryPartsEncrypted(bool auxEncrypted)
{
    d->auxPartsEncrypted = auxEncrypted;
    updateWhichPartsEncrypted();
}


void KSslInfoDialog::updateWhichPartsEncrypted()
{
    if (d->isMainPartEncrypted) {
        if (d->auxPartsEncrypted) {
            d->ui.encryptionIndicator->setPixmap(BarIcon("security-high"));
            d->ui.explanation->setText(i18n("Current connection is secured with SSL."));
        } else {
            d->ui.encryptionIndicator->setPixmap(BarIcon("security-medium"));
            d->ui.explanation->setText(i18n("The main part of this document is secured "
                                            "with SSL, but some parts are not."));
        }
    } else {
        if (d->auxPartsEncrypted) {
            d->ui.encryptionIndicator->setPixmap(BarIcon("security-medium"));
            d->ui.explanation->setText(i18n("Some of this document is secured with SSL, "
                                            "but the main part is not."));
        } else {
            d->ui.encryptionIndicator->setPixmap(BarIcon("security-low"));
            d->ui.explanation->setText(i18n("Current connection is not secured with SSL."));
        }
    }
}


void KSslInfoDialog::setSslInfo(const QList<QSslCertificate> &certificateChain,
                                const QString &ip, const QString &host,
                                const QString &sslProtocol, const QString &cipher,
                                int usedBits, int bits,
                                const QList<QList<KSslError::Error> > &validationErrors) {

    d->certificateChain = certificateChain;
    d->certificateErrors = validationErrors;

    d->ui.certSelector->clear();
    for (int i = 0; i < certificateChain.size(); i++) {
        const QSslCertificate &cert = certificateChain[i];
        QString name;
        static const QSslCertificate::SubjectInfo si[] = {
            QSslCertificate::CommonName,
            QSslCertificate::Organization,
            QSslCertificate::OrganizationalUnitName
        };
        for (int j = 0; j < 3 && name.isEmpty(); j++)
#warning QT5 PORT TO NEW API
            name = cert.subjectInfo(si[j]).first();
        d->ui.certSelector->addItem(name);
    }
    if (certificateChain.size() < 2) {
        d->ui.certSelector->setEnabled(false);
    }
    connect(d->ui.certSelector, SIGNAL(currentIndexChanged(int)),
            this, SLOT(displayFromChain(int)));
    if (d->certificateChain.isEmpty())
        d->certificateChain.append(QSslCertificate());
    displayFromChain(0);

    d->ui.ip->setText(ip);
    d->ui.address->setText(host);
    d->ui.sslVersion->setText(sslProtocol);

    const QStringList cipherInfo = cipher.split('\n', QString::SkipEmptyParts);
    if (cipherInfo.size() >= 4) {
        d->ui.encryption->setText(i18nc("%1, using %2 bits of a %3 bit key", "%1, %2 %3", cipherInfo[0],
                                  i18ncp("Part of: %1, using %2 bits of a %3 bit key",
                                  "using %1 bit", "using %1 bits", usedBits),
                                  i18ncp("Part of: %1, using %2 bits of a %3 bit key",
                                  "of a %1 bit key", "of a %1 bit key", bits)));
        d->ui.details->setText(QString("Auth = %1, Kx = %2, MAC = %3")
                                      .arg(cipherInfo[1], cipherInfo[2],
                                           cipherInfo[3]));
    } else {
        d->ui.encryption->setText("");
        d->ui.details->setText("");
    }
}


void KSslInfoDialog::displayFromChain(int i)
{
    const QSslCertificate &cert = d->certificateChain[i];

    QString trusted;
    if (!d->certificateErrors[i].isEmpty()) {
        trusted = i18nc("The certificate is not trusted", "NO, there were errors:");
        foreach (KSslError::Error e, d->certificateErrors[i]) {
            KSslError classError(e);
            trusted.append('\n');
            trusted.append(classError.errorString());
        }
    } else {
        trusted = i18nc("The certificate is trusted", "Yes");
    }
    d->ui.trusted->setText(trusted);

    QString vp = i18nc("%1 is the effective date of the certificate, %2 is the expiry date", "%1 to %2",
                cert.effectiveDate().toString(),
                cert.expiryDate().toString());
    d->ui.validityPeriod->setText(vp);

    d->ui.serial->setText(cert.serialNumber());
    d->ui.digest->setText(cert.digest().toHex());
    d->ui.sha1Digest->setText(cert.digest(QCryptographicHash::Sha1).toHex());

    d->subject->setCertificate(cert, KSslCertificateBox::Subject);
    d->issuer->setCertificate(cert, KSslCertificateBox::Issuer);
}


//static
QList<QList<KSslError::Error> > KSslInfoDialog::errorsFromString(const QString &es)
{
    QStringList sl = es.split('\n', QString::KeepEmptyParts);
    QList<QList<KSslError::Error> > ret;
    foreach (const QString &s, sl) {
        QList<KSslError::Error> certErrors;
        QStringList sl2 = s.split('\t', QString::SkipEmptyParts);
        foreach (const QString &s2, sl2) {
            bool didConvert;
            KSslError::Error error = static_cast<KSslError::Error>(s2.toInt(&didConvert));
            if (didConvert) {
                certErrors.append(error);
            }
        }
        ret.append(certErrors);
    }
    return ret;
}

