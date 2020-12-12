/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KTCPSOCKETTEST_H
#define KTCPSOCKETTEST_H

#include <QTest>

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
    void cleanupTestCase();
    void connectDisconnect();
    void read();
    void write();
    void statesIana();
    void statesLocalHost();
    void statesManyHosts();
    void errors();
public Q_SLOTS: //auxiliary slots to check signal emission from the socket
    void states_hostFound();
private:
    QThread *m_thread;
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
    explicit Server(quint16 _port);
    ~Server();
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
