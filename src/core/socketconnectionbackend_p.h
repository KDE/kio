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

#ifndef KIO_CONNECTION_P_H
#define KIO_CONNECTION_P_H

#include <QUrl>
#include <QObject>
class KLocalSocketServer;
class QTcpServer;
class QTcpSocket;

namespace KIO {
    struct Task {
        int cmd;
        QByteArray data;
    };

    class AbstractConnectionBackend: public QObject
    {
        Q_OBJECT
    public:
        QUrl address;
        QString errorString;
        enum { Idle, Listening, Connected } state;

        explicit AbstractConnectionBackend(QObject *parent = 0);
        ~AbstractConnectionBackend();

        virtual void setSuspended(bool enable) = 0;
        virtual bool connectToRemote(const QUrl &url) = 0;
        virtual bool listenForRemote() = 0;
        virtual bool waitForIncomingTask(int ms) = 0;
        virtual bool sendCommand(const Task &task) = 0;
        virtual AbstractConnectionBackend *nextPendingConnection() = 0;

    Q_SIGNALS:
        void disconnected();
        void commandReceived(const Task &task);
        void newConnection();
    };

    class SocketConnectionBackend: public AbstractConnectionBackend
    {
        Q_OBJECT
    public:
        enum Mode { LocalSocketMode, TcpSocketMode };

    private:
        enum { HeaderSize = 10, StandardBufferSize = 32*1024 };

        QTcpSocket *socket;
        union {
            KLocalSocketServer *localServer;
            QTcpServer *tcpServer;
        };
        long len;
        int cmd;
        int port;
        bool signalEmitted;
        quint8 mode;

    public:
        explicit SocketConnectionBackend(Mode m, QObject *parent = 0);
        ~SocketConnectionBackend();

        void setSuspended(bool enable);
        bool connectToRemote(const QUrl &url);
        bool listenForRemote();
        bool waitForIncomingTask(int ms);
        bool sendCommand(const Task &task);
        AbstractConnectionBackend *nextPendingConnection();
    public Q_SLOTS:
        void socketReadyRead();
        void socketDisconnected();
    };
}

#endif
