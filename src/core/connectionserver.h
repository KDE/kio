/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
