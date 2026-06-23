/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTIONBACKEND_P_H
#define KIO_CONNECTIONBACKEND_P_H

#include <QObject>
#include <QUrl>

#include <atomic>

#ifdef BUILD_TESTING
#include "kiocore_export.h"
#endif

class QLocalServer;
class QLocalSocket;

class QTcpServer;

namespace KIO
{
struct Task {
    int cmd = -1;
    long len = 0;
    QByteArray data{};
};

class ConnectionBackend : public QObject
{
    Q_OBJECT

public:
    enum {
        Idle,
        Listening,
        Connected
    } state;
    QUrl address;
    QString errorString;

    static const int HeaderSize = 10;
    static const int StandardBufferSize = 32 * 1024;

private:
    QLocalSocket *socket;
    QLocalServer *localServer;
    std::optional<Task> pendingTask = std::nullopt;
    bool signalEmitted;
#ifdef Q_OS_UNIX
    std::atomic<qintptr> m_socketFd = -1; // cached for abortConnection() to ::shutdown cross-thread
#endif
#ifdef BUILD_TESTING
    static std::atomic<bool> s_testBlockSocketClose;
#endif

Q_SIGNALS:
    void disconnected();
    void commandReceived(const KIO::Task &task);
    void newConnection();

public:
    explicit ConnectionBackend(QObject *parent = nullptr);
    ~ConnectionBackend() override;

    struct ConnectionResult {
        bool success = true;
        QString error;
    };

    void setSuspended(bool enable);
    void closeSocket();
    // Thread-safe: ::shutdown the connected socket so a blocking waitForIncomingTask() returns.
    void abortConnection();
    bool connectToRemote(const QUrl &url);

#ifdef BUILD_TESTING
    // Test hook: make closeSocket() a no-op, so a blocked worker can only be woken by abortConnection().
    KIOCORE_EXPORT static void setTestBlockSocketClose(bool block);
#endif
    ConnectionResult listenForRemote();
    bool waitForIncomingTask(int ms);
    bool sendCommand(int command, const QByteArray &data) const;
    ConnectionBackend *nextPendingConnection();

public Q_SLOTS:
    void socketReadyRead();
    void socketDisconnected();
};
}

#endif
