/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTIONSERVER_H
#define KIO_CONNECTIONSERVER_H

#include <QObject>
#include <QUrl>

#include "connectionbackend_p.h"

namespace KIO
{
class ConnectionServerPrivate;
class Connection;

/*
 * This class provides a way to obtaining KIO::Connection connections.
 */
class ConnectionServer : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionServer(QObject *parent = nullptr);
    ~ConnectionServer() override;

    /**
     * Sets this connection to listen mode. Use address() to obtain the
     * address this is listening on.
     */
    void listenForRemote();
    bool isListening() const;

    /**
     * Returns the address for this connection if it is listening, an empty
     * address if not.
     */
    QUrl address() const;

    void setNextPendingConnection(Connection *conn);

Q_SIGNALS:
    void newConnection();

private:
    ConnectionBackend *backend = nullptr;
};

} // namespace KIO

#endif
