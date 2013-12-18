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

#include "klocalsockettest.h"

#include <QtTest/QtTest>

#include <QtCore/QFile>
#include <QtCore/QDebug>
#include <QtCore/QThread>
#include "klocalsocket.h"

static const char socketpath[] = "/tmp/testsocket";

tst_KLocalSocket::tst_KLocalSocket()
{
    server = 0;
    QFile::remove(QFile::decodeName(socketpath));
}

tst_KLocalSocket::~tst_KLocalSocket()
{
    delete server;
    QFile::remove(QFile::decodeName(socketpath));
}

#include <unistd.h>

class TimedTest: public QThread
{
public:
    KLocalSocket *socket;
    TimedTest(KLocalSocket *s)
        : socket(s)
    { }
    ~TimedTest() { wait(1000); }

    void run()
    {
        QThread::usleep(100000);
        socket->write("Hello, World!", 13);
        socket->waitForBytesWritten();
        QThread::usleep(100000);
        socket->close();
        delete socket;
    }
};

void tst_KLocalSocket::initTestCase()
{
    server = new KLocalSocketServer(this);
    QVERIFY(server->listen(socketpath));
}

void tst_KLocalSocket::connection_data()
{
    QTest::addColumn<QString>("path");

    QTest::newRow("null-path") << QString();
    QTest::newRow("empty-path") << "";
    QTest::newRow("directory") << "/tmp";
    QTest::newRow("directory2") << "/tmp/";
    QTest::newRow("non-existing") << "/tmp/nonexistingsocket";
    QTest::newRow("real") << socketpath;
}

void tst_KLocalSocket::connection()
{
    QFETCH(QString, path);
    KLocalSocket socket;
    socket.connectToPath(path);

    bool shouldSucceed = path == socketpath;
    QCOMPARE(socket.waitForConnected(1000), shouldSucceed);
    if (shouldSucceed) {
        QVERIFY(server->waitForNewConnection());
        delete server->nextPendingConnection();
    } else {
        qDebug() << socket.errorString();
    }
}

void tst_KLocalSocket::waitFor()
{
    KLocalSocket socket;
    socket.connectToPath(socketpath);
    QVERIFY(socket.waitForConnected(1000));
    QVERIFY(server->waitForNewConnection());

    // now accept:
    KLocalSocket *socket2 = server->nextPendingConnection();

    // start thread:
    TimedTest thr(socket2);
    socket2->setParent(0);
    socket2->moveToThread(&thr);
    thr.start();

    QVERIFY(socket.waitForReadyRead(500));
    QByteArray data = socket.read(512);

    QVERIFY(socket.waitForDisconnected(500));
}

void tst_KLocalSocket::reading()
{
    static const char data1[] = "Hello ",
                      data2[] = "World";
    KLocalSocket socket;
    socket.connectToPath(socketpath);
    QVERIFY(socket.waitForConnected(1000));
    QVERIFY(server->waitForNewConnection());

    // now accept and write something:
    KLocalSocket *socket2 = server->nextPendingConnection();
    socket2->write(data1, sizeof data1 - 1);
    QVERIFY(socket2->bytesToWrite() == 0 || socket2->waitForBytesWritten(200));

    QVERIFY(socket.waitForReadyRead(200));
    QByteArray read = socket.read(sizeof data1 - 1);
    QCOMPARE(read.length(), int(sizeof data1) - 1);
    QCOMPARE(read.constData(), data1);

    // write data2
    socket2->write(data2, sizeof data2 - 1);
    QVERIFY(socket2->bytesToWrite() == 0 || socket2->waitForBytesWritten(200));
    QVERIFY(socket.waitForReadyRead(200));
    read = socket.read(sizeof data2 - 1);
    QCOMPARE(read.length(), int(sizeof data2) - 1);
    QCOMPARE(read.constData(), data2);

    delete socket2;
}

void tst_KLocalSocket::writing()
{
    static const char data1[] = "Hello ",
                      data2[] = "World";
    KLocalSocket socket;
    socket.connectToPath(socketpath);
    QVERIFY(socket.waitForConnected(1000));
    QVERIFY(server->waitForNewConnection());

    // now accept and write something:
    KLocalSocket *socket2 = server->nextPendingConnection();

    QCOMPARE(socket.write(data1, sizeof data1 - 1), Q_INT64_C(sizeof data1 - 1));
    QVERIFY(socket.bytesToWrite() == 0 || socket.waitForBytesWritten(100));
    QVERIFY(socket2->waitForReadyRead());

    QByteArray read = socket2->read(sizeof data1 - 1);
    QCOMPARE(read.length(), int(sizeof data1) - 1);
    QCOMPARE(read.constData(), data1);

    // write data2
    QCOMPARE(socket.write(data2, sizeof data2 - 1), Q_INT64_C(sizeof data2 - 1));
    QVERIFY(socket.bytesToWrite() == 0 || socket.waitForBytesWritten(100));
    QVERIFY(socket2->waitForReadyRead());
    read = socket2->read(sizeof data2 - 1);
    QCOMPARE(read.length(), int(sizeof data2) - 1);
    QCOMPARE(read.constData(), data2);

    delete socket2;
}

void tst_KLocalSocket::state()
{
    KLocalSocket socket;

    // sanity check:
    QCOMPARE(int(socket.localSocketType()), int(KLocalSocket::UnknownLocalSocketType));
    QVERIFY(socket.localPath().isEmpty());
    QVERIFY(socket.peerPath().isEmpty());
    QCOMPARE(int(socket.state()), int(QAbstractSocket::UnconnectedState));

    // now connect and accept
    socket.connectToPath(socketpath);
    QVERIFY(socket.waitForConnected(1000));
    QVERIFY(server->waitForNewConnection());
    KLocalSocket *socket2 = server->nextPendingConnection();

    QCOMPARE(socket.peerPath(), QString(socketpath));
    QCOMPARE(socket2->localPath(), QString(socketpath));
    QCOMPARE(int(socket.state()), int(QAbstractSocket::ConnectedState));
    QCOMPARE(int(socket2->state()), int(QAbstractSocket::ConnectedState));
    QCOMPARE(int(socket.localSocketType()), int(KLocalSocket::UnixSocket));
    QCOMPARE(int(socket2->localSocketType()), int(KLocalSocket::UnixSocket));

    // now close one of the sockets:
    socket.close();

    // it must have reset its state:
    QCOMPARE(int(socket.localSocketType()), int(KLocalSocket::UnknownLocalSocketType));
    QVERIFY(socket.peerPath().isEmpty());
    QCOMPARE(int(socket.state()), int(QAbstractSocket::UnconnectedState));

    // but the other one mustn't have yet:
    QCOMPARE(int(socket2->state()), int(QAbstractSocket::ConnectedState));
    QVERIFY(!socket2->localPath().isEmpty());
    QCOMPARE(int(socket2->localSocketType()), int(KLocalSocket::UnixSocket));

    // wait for disconnected:
    QVERIFY(socket2->waitForDisconnected());

    // now it must have:
    QCOMPARE(int(socket2->state()), int(QAbstractSocket::UnconnectedState));
    QVERIFY(socket2->localPath().isEmpty());
    QCOMPARE(int(socket2->localSocketType()), int(KLocalSocket::UnknownLocalSocketType));

    delete socket2;
}

void tst_KLocalSocket::connected()
{
    KLocalSocket socket;
    socket.connectToPath(socketpath);
    QEXPECT_FAIL("", "Will fix later", Continue);
    QVERIFY(!socket.isOpen());

    QSignalSpy spy(&socket, SIGNAL(connected()));
    QTest::qWait(100);

    QEXPECT_FAIL("", "Will fix later", Continue);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(tst_KLocalSocket)

