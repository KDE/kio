/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000-2003 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2000 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KSSLINFODIALOG_H
#define _KSSLINFODIALOG_H

#include <QDialog>
#include <QSslError>

#include "kiowidgets_export.h"

#include <memory>

/*!
 * \class KSslInfoDialog
 * \inmodule KIOWidgets
 *
 * \brief KDE SSL Information Dialog.
 *
 * This class creates a dialog that can be used to display information about
 * an SSL session.
 */
class KIOWIDGETS_EXPORT KSslInfoDialog : public QDialog
{
    Q_OBJECT
public:
    /*!
     *  Construct a KSSL Information Dialog
     *
     *  \a parent the parent widget
     */
    explicit KSslInfoDialog(QWidget *parent = nullptr);

    ~KSslInfoDialog() override;

    /*!
     * Set information to display about the SSL connection.
     *
     * \a certificateChain the certificate chain leading from the certificate
     *         authority to the peer.
     *
     * \a ip the ip of the remote host
     *
     * \a host the remote hostname
     *
     * \a sslProtocol the version of SSL in use (SSLv2, SSLv3, TLSv1)
     *
     * \a cipher the cipher in use
     *
     * \a usedBits the used bits of the key
     *
     * \a bits the key size of the cipher in use
     *
     * \a validationErrors errors validating the certificates, if any
     *
     * \since 5.64
     */
    void setSslInfo(const QList<QSslCertificate> &certificateChain,
                    const QString &ip,
                    const QString &host,
                    const QString &sslProtocol,
                    const QString &cipher,
                    int usedBits,
                    int bits,
                    const QList<QList<QSslError::SslError>> &validationErrors);

    /*!
     *
     */
    void setMainPartEncrypted(bool);

    /*!
     *
     */
    void setAuxiliaryPartsEncrypted(bool);

    /*!
     * Converts certificate errors as provided in the "ssl_cert_errors" meta data
     * to a list of QSslError::SslError values per certificate in the certificate chain.
     * \since 5.65
     */
    static QList<QList<QSslError::SslError>> certificateErrorsFromString(const QString &errorsString);

private:
    KIOWIDGETS_NO_EXPORT void updateWhichPartsEncrypted();

    class KSslInfoDialogPrivate;
    std::unique_ptr<KSslInfoDialogPrivate> const d;

private Q_SLOTS:
    KIOWIDGETS_NO_EXPORT void displayFromChain(int);
};

#endif
