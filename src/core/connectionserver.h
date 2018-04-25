/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>

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

#ifndef KIO_CONNECTIONSERVER_H
#define KIO_CONNECTIONSERVER_H

#include "kiocore_export.h"

#include <QUrl>
#include <QObject>

namespace KIO
{

class ConnectionServerPrivate;
class Connection;

/**
 * @private
 * @internal
 *
 * This class provides a way to obtaining KIO::Connection connections.
 * Used by klauncher.
 * Do not use outside KIO and klauncher!
 */
class KIOCORE_EXPORT ConnectionServer : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionServer(QObject *parent = nullptr);
    ~ConnectionServer();

    /**
     * Sets this connection to listen mode. Use address() to obtain the
     * address this is listening on.
     */
    void listenForRemote();
    bool isListening() const;
    /// Closes the connection.
    void close();

    /**
     * Returns the address for this connection if it is listening, an empty
     * address if not.
     */
    QUrl address() const;

    Connection *nextPendingConnection();
    void setNextPendingConnection(Connection *conn);

Q_SIGNALS:
    void newConnection();

private:
    friend class ConnectionServerPrivate;
    ConnectionServerPrivate *const d;
};

} // namespace KIO

#endif
