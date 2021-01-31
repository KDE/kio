/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTIONBACKEND_P_H
#define KIO_CONNECTIONBACKEND_P_H

#include <QUrl>
#include <QObject>

class QLocalServer;
class QLocalSocket;

class QTcpServer;

namespace KIO
{
struct Task {
    int cmd;
    QByteArray data;
};


class ConnectionBackend: public QObject
{
    Q_OBJECT

public:
    enum { Idle, Listening, Connected } state;
    QUrl address;
    QString errorString;

private:

    QLocalSocket *socket;
    QLocalServer *localServer;
    long len;
    int cmd;
    int port;
    bool signalEmitted;
    quint8 mode;

    static const int HeaderSize = 10;
    static const int StandardBufferSize = 32 * 1024;

Q_SIGNALS:
    void disconnected();
    // TODO KF6: fix clazy warning by using fully-qualified signal argument
    void commandReceived(const Task &task); // clazy:exclude=fully-qualified-moc-types
    void newConnection();

public:
    explicit ConnectionBackend(QObject *parent = nullptr);
    ~ConnectionBackend();

    void setSuspended(bool enable);
    bool connectToRemote(const QUrl &url);
    bool listenForRemote();
    bool waitForIncomingTask(int ms);
    bool sendCommand(int command, const QByteArray &data) const;
    ConnectionBackend *nextPendingConnection();

public Q_SLOTS:
    void socketReadyRead();
    void socketDisconnected();
};
}

#endif
