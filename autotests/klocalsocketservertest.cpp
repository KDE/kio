/*
 * This file is part of the KDE libraries
 * Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
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

#include "klocalsocketservertest.h"
#include <QSignalSpy>
#include <QFile>

#include <qtest.h>
#include <QtCore/QThread>
#include "klocalsocket.h"

static const char afile[] = "/tmp/afile";
static const char asocket[] = "/tmp/asocket";

tst_KLocalSocketServer::tst_KLocalSocketServer()
{
    QFile f(QFile::encodeName(afile));
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
}

tst_KLocalSocketServer::~tst_KLocalSocketServer()
{
    QFile::remove(afile);
}

class TimedConnection: public QThread
{
public:
    ~TimedConnection() { wait(); }
protected:
    void run()
    {
        KLocalSocket socket;
        QThread::usleep(200);
        socket.connectToPath(asocket);
        socket.waitForConnected();
    }
};

void tst_KLocalSocketServer::cleanup()
{
    QFile::remove(asocket);
}

void tst_KLocalSocketServer::listen_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("success");

    QTest::newRow("null") << QString() << false;
    QTest::newRow("empty") << "" << false;
    QTest::newRow("a-dir") << "/tmp/" << false;
    QTest::newRow("not-a-dir") << QString(afile + QLatin1String("/foo")) << false;
    QTest::newRow("not-permitted") << "/root/foo" << false;
    QTest::newRow("valid") << asocket << true;
}

void tst_KLocalSocketServer::listen()
{
    QFETCH(QString, path);
    KLocalSocketServer server;
    QTEST(server.listen(path), "success");
}

void tst_KLocalSocketServer::waitForConnection()
{
    KLocalSocketServer server;
    QVERIFY(server.listen(asocket));
    QVERIFY(!server.hasPendingConnections());

    {
        KLocalSocket socket;
        socket.connectToPath(asocket);
        QVERIFY(socket.waitForConnected());

        // make sure we can accept that connection
        QVERIFY(server.waitForNewConnection());
        QVERIFY(server.hasPendingConnections());
        delete server.nextPendingConnection();
    }

    // test a timeout now
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!server.waitForNewConnection(0));
    QVERIFY(!server.waitForNewConnection(200));

    {
        // now try a timed connection
        TimedConnection conn;
        conn.start();
        QVERIFY(server.waitForNewConnection(500));
        QVERIFY(server.hasPendingConnections());
        delete server.nextPendingConnection();
    }
}

void tst_KLocalSocketServer::newConnection()
{
    KLocalSocketServer server;
    QVERIFY(server.listen(asocket));
    QVERIFY(!server.hasPendingConnections());

    // catch the signal
    QSignalSpy spy(&server, SIGNAL(newConnection()));

    KLocalSocket socket;
    socket.connectToPath(asocket);
    QVERIFY(socket.waitForConnected());

    // let the events be processed
    QTest::qWait(100);

    QVERIFY(spy.count() == 1);
}

void tst_KLocalSocketServer::accept()
{
    KLocalSocketServer server;
    QVERIFY(server.listen(asocket));
    QVERIFY(!server.hasPendingConnections());

    KLocalSocket socket;
    socket.connectToPath(asocket);
    QVERIFY(socket.waitForConnected());
    QVERIFY(server.waitForNewConnection());
    QVERIFY(server.hasPendingConnections());

    KLocalSocket *socket2 = server.nextPendingConnection();
    QVERIFY(!server.hasPendingConnections());
    QCOMPARE(socket.state(), QAbstractSocket::ConnectedState);
    QCOMPARE(socket2->state(), QAbstractSocket::ConnectedState);

    delete socket2;
}

void tst_KLocalSocketServer::state()
{
    KLocalSocketServer server;

    // sanity check of the initial state:
    QVERIFY(!server.isListening());
    QVERIFY(server.localPath().isEmpty());
    QCOMPARE(int(server.localSocketType()), int(KLocalSocket::UnknownLocalSocketType));
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!server.nextPendingConnection());

    // it's not connected, so it shouldn't change timedOut
    bool timedOut = true;
    QVERIFY(!server.waitForNewConnection(0, &timedOut));
    QVERIFY(timedOut);
    timedOut = false;
    QVERIFY(!server.waitForNewConnection(0, &timedOut));
    QVERIFY(!timedOut);

    // start listening:
    QVERIFY(server.listen(asocket));
    QVERIFY(server.isListening());
    QCOMPARE(server.localPath(), QString(asocket));
    QCOMPARE(int(server.localSocketType()), int(KLocalSocket::UnixSocket));
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!server.nextPendingConnection());

    // it must timeout now:
    timedOut = false;
    QVERIFY(!server.waitForNewConnection(0, &timedOut));
    QVERIFY(timedOut);

    // make a connection:
    KLocalSocket socket;
    socket.connectToPath(asocket);
    QVERIFY(socket.waitForConnected());

    // it mustn't time out now:
    timedOut = true;
    QVERIFY(server.waitForNewConnection(0, &timedOut));
    QVERIFY(!timedOut);

    QVERIFY(server.hasPendingConnections());
    KLocalSocket *socket2 = server.nextPendingConnection();
    QVERIFY(socket2);
    delete socket2;

    // close:
    server.close();

    // verify state:
    QVERIFY(!server.isListening());
    QVERIFY(server.localPath().isEmpty());
    QCOMPARE(int(server.localSocketType()), int(KLocalSocket::UnknownLocalSocketType));
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!server.nextPendingConnection());
}

void tst_KLocalSocketServer::setMaxPendingConnections()
{
    KLocalSocketServer server;
    QVERIFY(server.listen(asocket));
    QVERIFY(!server.hasPendingConnections());
    server.setMaxPendingConnections(0); // we don't want to receive

    // check if the event loop won't cause a connection to accepted
    KLocalSocket socket;
    socket.connectToPath(asocket);
    QTest::qWait(100);          // 100 ms doing absolutely nothing
    QVERIFY(!server.hasPendingConnections());

    // now check if we get that conenction
    server.setMaxPendingConnections(1);
    QTest::qWait(100);
    QVERIFY(server.hasPendingConnections());
    delete server.nextPendingConnection();
    QVERIFY(socket.waitForDisconnected());

    // check if we receive only one of the two pending connections
    KLocalSocket socket2;
    socket.connectToPath(asocket);
    socket2.connectToPath(asocket);
    QTest::qWait(100);

    QVERIFY(server.hasPendingConnections());
    delete server.nextPendingConnection();
    QVERIFY(!server.hasPendingConnections());
    QVERIFY(!server.nextPendingConnection());
}

void tst_KLocalSocketServer::abstractUnixSocket_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<bool>("success");

    QTest::newRow("null") << QString() << false;
    QTest::newRow("empty") << "" << false;
#if 0
    // apparently, we are allowed to put sockets there, even if we don't have permission to
    QTest::newRow("a-dir") << "/tmp/" << false;
    QTest::newRow("not-a-dir") << afile + QLatin1String("/foo") << false;
    QTest::newRow("not-permitted") << "/root/foo" << false;
#endif
    QTest::newRow("valid") << asocket << true;
}

void tst_KLocalSocketServer::abstractUnixSocket()
{
    QFETCH(QString, path);
    QFETCH(bool, success);

    if (success)
        QVERIFY(!QFile::exists(path));

    KLocalSocketServer server;
    QCOMPARE(server.listen(path, KLocalSocket::AbstractUnixSocket), success);

    if (success) {
        // the socket must not exist in the filesystem
        QVERIFY(!QFile::exists(path));

        // now try to connect to it
        KLocalSocket socket;
        socket.connectToPath(path, KLocalSocket::AbstractUnixSocket);
        QVERIFY(socket.waitForConnected(100));
        QVERIFY(server.waitForNewConnection(100));
        QVERIFY(server.hasPendingConnections());

        // the socket must still not exist in the filesystem
        QVERIFY(!QFile::exists(path));

        // verify that they can exchange data too:
        KLocalSocket *socket2 = server.nextPendingConnection();
        QByteArray data("Hello");
        socket2->write(data);
        QVERIFY(socket2->bytesToWrite() == 0 || socket2->waitForBytesWritten(100));
        QVERIFY(socket.waitForReadyRead(100));
        QCOMPARE(socket.read(data.length()), data);

        socket.write(data);
        QVERIFY(socket.bytesToWrite() == 0 || socket.waitForBytesWritten(100));
        QVERIFY(socket2->waitForReadyRead(100));
        QCOMPARE(socket2->read(data.length()), data);

        delete socket2;
        QVERIFY(socket.waitForDisconnected(100));
    }
}

QTEST_MAIN(tst_KLocalSocketServer)

