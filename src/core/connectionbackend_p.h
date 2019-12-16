/* This file is part of the KDE libraries
    Copyright (C) 2007 Thiago Macieira <thiago@kde.org>

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
    void commandReceived(const Task &task);
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
