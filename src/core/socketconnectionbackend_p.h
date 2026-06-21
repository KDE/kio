/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_SOCKETCONNECTIONBACKEND_P_H
#define KIO_SOCKETCONNECTIONBACKEND_P_H

#include "connectionbackend_p.h"

#include <optional>

class QLocalServer;
class QLocalSocket;

namespace KIO
{
/*!
 * \internal
 *
 * ConnectionBackend talking to a peer over a QLocalSocket. Used for
 * out-of-process workers (and historically for in-process ones too).
 */
class SocketConnectionBackend : public ConnectionBackend
{
    Q_OBJECT

public:
    static const int HeaderSize = 10;
    static const int StandardBufferSize = 32 * 1024;

    struct ConnectionResult {
        bool success = true;
        QString error;
    };

    explicit SocketConnectionBackend(QObject *parent = nullptr);
    ~SocketConnectionBackend() override;

    void setSuspended(bool enable) override;
    void closeSocket() override;
    bool waitForIncomingTask(int ms) override;
    bool sendCommand(int command, const QByteArray &data) override;

    // Socket-specific: a paired transport (ThreadConnectionBackend) has no notion of connecting
    // to an address or listening/accepting, so these live only on the socket backend.
    bool connectToRemote(const QUrl &url);
    ConnectionResult listenForRemote();
    ConnectionBackend *nextPendingConnection();

private:
    QLocalSocket *socket;
    QLocalServer *localServer;
    std::optional<Task> pendingTask = std::nullopt;
    bool signalEmitted;

Q_SIGNALS:
    void newConnection();

public Q_SLOTS:
    void socketReadyRead();
    void socketDisconnected();
};
}

#endif
