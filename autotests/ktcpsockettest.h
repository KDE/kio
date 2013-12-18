/*
 * This file is part of the KDE libraries
 * Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
#ifndef KTCPSOCKETTEST_H
#define KTCPSOCKETTEST_H

#include <QtTest>

class Server;

class KTcpSocketTest : public QObject
{
    Q_OBJECT
public:
    Server *server;
    KTcpSocketTest();
    ~KTcpSocketTest();
private:
    void invokeOnServer(const char *);
private Q_SLOTS:
    void initTestCase();
    void connectDisconnect();
    void read();
    void write();
    void statesIana();
    void statesLocalHost();
    void statesManyHosts();
    void errors();
public Q_SLOTS: //auxiliary slots to check signal emission from the socket
    void states_hostFound();
};


class QTcpServer;
class QTcpSocket;

class Server : public QObject
{
    Q_OBJECT
public:
    QTcpServer *listener;
    QTcpSocket *socket;
    quint16 port;
    Server(quint16 _port);
private:
    void cleanupSocket();

public Q_SLOTS:
    void connectDisconnect();
    void read();
    void write();
    void states();
    void errors();
};

#endif
