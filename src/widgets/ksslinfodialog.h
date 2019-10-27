/* This file is part of the KDE project
 *
 * Copyright (C) 2000-2003 George Staikos <staikos@kde.org>
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

#ifndef _KSSLINFODIALOG_H
#define _KSSLINFODIALOG_H

#include <QDialog>

#include "kiowidgets_export.h"
#include "ktcpsocket.h" // TODO KF6 remove this include

/**
 * KDE SSL Information Dialog
 *
 * This class creates a dialog that can be used to display information about
 * an SSL session.
 *
 * There are NO GUARANTEES that KSslInfoDialog will remain binary compatible/
 * Contact staikos@kde.org for details if needed.
 *
 * @author George Staikos <staikos@kde.org>
 * @see KSSL
 * @short KDE SSL Information Dialog
 */
class KIOWIDGETS_EXPORT KSslInfoDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     *  Construct a KSSL Information Dialog
     *
     *  @param parent the parent widget
     */
    explicit KSslInfoDialog(QWidget *parent = nullptr);

    /**
     *  Destroy this dialog
     */
    virtual ~KSslInfoDialog();

    /**
     *  Tell the dialog if the connection has portions that may not be
     *  secure (ie. a mixture of secure and insecure frames)
     *
     *  @param isIt true if security is in question
     */
    void setSecurityInQuestion(bool isIt);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 64)
    /**
     *  Set information to display about the SSL connection.
     *
     *  @param certificateChain the certificate chain leading from the certificate
     *         authority to the peer.
     *  @param ip the ip of the remote host
     *  @param host the remote hostname
     *  @param sslProtocol the version of SSL in use (SSLv2, SSLv3, TLSv1)
     *  @param cipher the cipher in use
     *  @param usedBits the used bits of the key
     *  @param bits the key size of the cipher in use
     *  @param validationErrors errors validating the certificates, if any
     *  @deprecated since 5.64, use the QSslError variant
     */
    KIOCORE_DEPRECATED_VERSION(5, 64, "use the QSslError variant")
    void setSslInfo(const QList<QSslCertificate> &certificateChain,
                    const QString &ip, const QString &host,
                    const QString &sslProtocol, const QString &cipher,
                    int usedBits, int bits,
                    const QList<QList<KSslError::Error> > &validationErrors); // TODO KF6 remove
#endif

    /**
     *  Set information to display about the SSL connection.
     *
     *  @param certificateChain the certificate chain leading from the certificate
     *         authority to the peer.
     *  @param ip the ip of the remote host
     *  @param host the remote hostname
     *  @param sslProtocol the version of SSL in use (SSLv2, SSLv3, TLSv1)
     *  @param cipher the cipher in use
     *  @param usedBits the used bits of the key
     *  @param bits the key size of the cipher in use
     *  @param validationErrors errors validating the certificates, if any
     *  @since 5.64
     */
    void setSslInfo(const QList<QSslCertificate> &certificateChain,
                    const QString &ip, const QString &host,
                    const QString &sslProtocol, const QString &cipher,
                    int usedBits, int bits,
                    const QList<QList<QSslError::SslError>> &validationErrors);

    void setMainPartEncrypted(bool);
    void setAuxiliaryPartsEncrypted(bool);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 65)
    /** @deprecated since 5.65, use certificateErrorsFromString */
    KIOCORE_DEPRECATED_VERSION(5, 65, "use the QSslError variant")
    static QList<QList<KSslError::Error> > errorsFromString(const QString &s); // TODO KF6 remove
#endif
    /**
     * Converts certificate errors as provided in the "ssl_cert_errors" meta data
     * to a list of QSslError::SslError values per certificate in the certificate chain.
     * @since 5.65
     */
    static QList<QList<QSslError::SslError>> certificateErrorsFromString(const QString &errorsString);

private:
    void updateWhichPartsEncrypted();

    class KSslInfoDialogPrivate;
    KSslInfoDialogPrivate *const d;

private Q_SLOTS:
    void displayFromChain(int);
};

#endif
