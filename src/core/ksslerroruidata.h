/* This file is part of the KDE libraries
    Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
    Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KSSLERRORUIDATA_H
#define KSSLERRORUIDATA_H

#include <kiocore_export.h>

template <typename T> class QList;
class KTcpSocket;
class QNetworkReply;
class QSslError;
class QSslSocket;

/**
 * This class can hold all the necessary data from a KTcpSocket to ask the user
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
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 65)
    /**
     * Create an instance and initialize it with SSL error data from @p socket.
     * @deprecated since 5.65, use QSslSocket variant
     */
    KIOCORE_DEPRECATED_VERSION(5, 65, "Use QSslSocket variant")
    KSslErrorUiData(const KTcpSocket *socket);
#endif
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
     * @since 4.7
     */
    ~KSslErrorUiData();

    class Private;
private:
    friend class Private;
    Private *const d;
};

#endif // KSSLERRORUIDATA_H
