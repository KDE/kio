/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KSSLERRORUIDATA_H
#define KSSLERRORUIDATA_H

#include <kiocore_export.h>

#include <memory>

template<typename T>
class QList;
class QNetworkReply;
class QSslError;
class QSslSocket;

/**
 * This class can hold all the necessary data from a QSslSocket or QNetworkReply to ask the user
 * to continue connecting in the face of SSL errors.
 * It can be used to carry the data for the UI over time or over thread boundaries.
 *
 * @see: KSslCertificateManager::askIgnoreSslErrors()
 */
class KIOCORE_EXPORT KSslErrorUiData
{
public:
    /**
     * Default construct an instance with no useful data.
     */
    KSslErrorUiData();
    /**
     * Create an instance and initialize it with SSL error data from @p socket.
     */
    KSslErrorUiData(const QSslSocket *socket);
    /**
     * Create an instance and initialize it with SSL error data from @p reply.
     * @since 5.62
     */
    KSslErrorUiData(const QNetworkReply *reply, const QList<QSslError> &sslErrors);

    KSslErrorUiData(const KSslErrorUiData &other);
    KSslErrorUiData &operator=(const KSslErrorUiData &);
    /**
     * Destructor
     */
    ~KSslErrorUiData();

    class Private;

private:
    friend class Private;
    std::unique_ptr<Private> const d;
};

#endif // KSSLERRORUIDATA_H
