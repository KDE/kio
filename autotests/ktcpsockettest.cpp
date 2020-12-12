/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2007 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ktcpsockettest.h"
#include <QDebug>
#include <QTcpServer>
#include <QThread>
#include "ktcpsocket.h"

/* TODO items:
 - test errors including error strings
 - test overriding errors
 - test the most important SSL operations (full coverage is very hard)
 - test readLine()
 - test nonblocking, signal based usage
 - test that waitForDisconnected() writes out all buffered data
 - (test local and peer address and port getters)
 - test isValid(). Its documentation is less than clear :(
 */

static const quint16 testPort = 22342;

KTcpSocketTest::KTcpSocketTest()
{
    server = nullptr;
}

KTcpSocketTest::~KTcpSocketTest()
{
}

void KTcpSocketTest::invokeOnServer(const char *method)
{
    QMetaObject::invokeMethod(server, method, Qt::QueuedConnection);
    QTest::qWait(1); //Enter the event loop
}

Server::Server(quint16 _port)
    : listener(new QTcpServer(this)),
      socket(nullptr),
      port(_port)
{
    listener->listen(QHostAddress(QStringLiteral("127.0.0.1")), testPort);
}

Server::~Server()
{
}

void Server::cleanupSocket()
{
    Q_ASSERT(socket);
    socket->close();
    socket->deleteLater();
    socket = nullptr;
}

void KTcpSocketTest::initTestCase()
{
    m_thread = new QThread();
    server = new Server(testPort);
    server->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, server, &QObject::deleteLater);
    m_thread->start();
}

void KTcpSocketTest::cleanupTestCase()
{
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

void KTcpSocketTest::connectDisconnect()
{
    invokeOnServer("connectDisconnect");

    KTcpSocket *s = new KTcpSocket(this);
    QCOMPARE(s->openMode(), QIODevice::NotOpen);
    QCOMPARE(s->error(), KTcpSocket::UnknownError);

    s->connectToHost(QStringLiteral("127.0.0.1"), testPort);
    QCOMPARE(s->state(), KTcpSocket::ConnectingState);
    QVERIFY(s->openMode() & QIODevice::ReadWrite);
    const bool connected = s->waitForConnected(150);
    QVERIFY(connected);
    QCOMPARE(s->state(), KTcpSocket::ConnectedState);

    s->waitForDisconnected(150);
    //ClosingState occurs only when there is buffered data
    QCOMPARE(s->state(), KTcpSocket::UnconnectedState);

    s->deleteLater();
}

void Server::connectDisconnect()
{
    listener->waitForNewConnection(10000, nullptr);
    socket = listener->nextPendingConnection();

    cleanupSocket();
}

#define TESTDATA QByteArray("things and stuff and a bag of chips")

void KTcpSocketTest::read()
{
    invokeOnServer("read");

    KTcpSocket *s = new KTcpSocket(this);
    s->connectToHost(QStringLiteral("127.0.0.1"), testPort);
    s->waitForConnected(40);
    s->waitForReadyRead(40);
    QCOMPARE((int)s->bytesAvailable(), TESTDATA.size());
    QCOMPARE(s->readAll(), TESTDATA);
    s->deleteLater();
}

void Server::read()
{
    listener->waitForNewConnection(10000, nullptr);
    socket = listener->nextPendingConnection();

    socket->write(TESTDATA);
    socket->waitForBytesWritten(150);
    cleanupSocket();
}

void KTcpSocketTest::write()
{
    invokeOnServer("write");

    KTcpSocket *s = new KTcpSocket(this);
    s->connectToHost(QStringLiteral("127.0.0.1"), testPort);
    s->waitForConnected(40);
    s->write(TESTDATA);
    QCOMPARE((int)s->bytesToWrite(), TESTDATA.size());
    s->waitForReadyRead(150);
    QCOMPARE((int)s->bytesAvailable(), TESTDATA.size());
    QCOMPARE(s->readAll(), TESTDATA);

    s->write(TESTDATA);
    QCOMPARE((int)s->bytesToWrite(), TESTDATA.size());
    s->disconnectFromHost();
    //Test closing with pending data to transmit (pending rx data comes later)
    QCOMPARE(s->state(), KTcpSocket::ClosingState);
    s->waitForDisconnected(150);
    QCOMPARE(s->state(), KTcpSocket::UnconnectedState);

    s->deleteLater();
}

void Server::write()
{
    listener->waitForNewConnection(10000, nullptr);
    socket = listener->nextPendingConnection();

    socket->waitForReadyRead(40);
    socket->write(socket->readAll()); //echo
    socket->waitForBytesWritten(150);

    socket->waitForReadyRead(40);
    socket->write(socket->readAll());
    cleanupSocket();
}

static QString stateToString(KTcpSocket::State state)
{
    switch (state) {
    case KTcpSocket::UnconnectedState:
        return QStringLiteral("UnconnectedState");
    case KTcpSocket::HostLookupState:
        return QStringLiteral("HostLookupState");
    case KTcpSocket::ConnectingState:
        return QStringLiteral("ConnectingState");
    case KTcpSocket::ConnectedState:
        return QStringLiteral("ConnectedState");
    case KTcpSocket::BoundState:
        return QStringLiteral("BoundState");
    case KTcpSocket::ListeningState:
        return QStringLiteral("ListeningState");
    case KTcpSocket::ClosingState:
        return QStringLiteral("ClosingState");
    }
    return QStringLiteral("ERROR");
}

#define HTTPREQUEST QByteArray("GET / HTTP/1.1\nHost: www.example.com\n\n")
// I assume that example.com, hosted by the IANA, will exist indefinitely.
// It is a nice test site because it serves a very small HTML page that should
// fit into a TCP packet or two.

void KTcpSocketTest::statesIana()
{
    QSKIP("Too unreliable");
    //A connection to a real internet host
    KTcpSocket *s = new KTcpSocket(this);
    connect(s, &KTcpSocket::hostFound, this, &KTcpSocketTest::states_hostFound);
    QCOMPARE(s->state(), KTcpSocket::UnconnectedState);
    s->connectToHost(QStringLiteral("www.iana.org"), 80);
    QCOMPARE(s->state(), KTcpSocket::HostLookupState);
    s->write(HTTPREQUEST);
    QCOMPARE(s->state(), KTcpSocket::HostLookupState);
    s->waitForBytesWritten(2500);
    QCOMPARE(s->state(), KTcpSocket::ConnectedState);

    // Try to ensure that inbound data in the next part of the test is really from the second request;
    // it is not *guaranteed* that this reads all data, e.g. if the connection is very slow (so too many
    // of the waitForReadyRead() time out), or if the reply packets are extremely fragmented (so 50 reads
    // are not enough to receive all of them). I don't know the details of fragmentation so the latter
    // problem could be nonexistent.
    QByteArray received;
    for (int i = 0; i < 50; i++) {
        s->waitForReadyRead(50);
        received.append(s->readAll());
    }
    QVERIFY(received.size() > 200);

    // Here, the connection should neither have data in its write buffer nor inbound packets in flight

    // Now reuse the connection for another request / reply pair

    s->write(HTTPREQUEST);
    s->waitForReadyRead();
    // After waitForReadyRead(), the write buffer should be empty because the server has to wait for the
    // end of the request before sending a reply.
    // The socket can then shut down without having to wait for draining the write buffer.
    // Incoming data cannot delay the transition to UnconnectedState, as documented in
    // QAbstractSocket::disconnectFromHost(). close() just wraps disconnectFromHost().
    s->close();
    QCOMPARE((int)s->state(), (int)KTcpSocket::UnconnectedState);

    delete s;
}

void KTcpSocketTest::statesLocalHost()
{
    //Now again an internal connection
    invokeOnServer("states");

    KTcpSocket *s = new KTcpSocket(this);
    connect(s, &KTcpSocket::hostFound, this, &KTcpSocketTest::states_hostFound);
    s->connectToHost(QStringLiteral("127.0.0.1"), testPort);
    QCOMPARE(s->state(), KTcpSocket::ConnectingState);
    s->waitForConnected(40);
    QCOMPARE(s->state(), KTcpSocket::ConnectedState);

    s->write(HTTPREQUEST);
    s->waitForReadyRead();
    QCOMPARE((int)s->bytesAvailable(), HTTPREQUEST.size()); //for good measure...
    QCOMPARE(s->state(), KTcpSocket::ConnectedState);

    s->waitForDisconnected(40);
    QCOMPARE(s->state(), KTcpSocket::UnconnectedState);

    disconnect(s, SIGNAL(hostFound()));
    delete s;
}

void KTcpSocketTest::statesManyHosts()
{
    KTcpSocket *s = new KTcpSocket(this);
    QByteArray requestProlog("GET /  HTTP/1.1\r\n"         //exact copy of a real HTTP query
                             "Connection: Keep-Alive\r\n"  //not really...
                             "User-Agent: Mozilla/5.0 (compatible; Konqueror/3.96; Linux) "
                             "KHTML/3.96.0 (like Gecko)\r\n"
                             "Pragma: no-cache\r\n"
                             "Cache-control: no-cache\r\n"
                             "Accept: text/html, image/jpeg, image/png, text/*, image/*, */*\r\n"
                             "Accept-Encoding: x-gzip, x-deflate, gzip, deflate\r\n"
                             "Accept-Charset: utf-8, utf-8;q=0.5, *;q=0.5\r\n"
                             "Accept-Language: en-US, en\r\n"
                             "Host: ");
    QByteArray requestEpilog("\r\n\r\n");
    //Test rapid connection and disconnection to different hosts
    static const char *hosts[] = {"www.google.de", "www.spiegel.de", "www.stern.de", "www.google.com"};
    static const int numHosts = 4;
    for (int i = 0; i < numHosts * 5; i++) {
        qDebug("\nNow trying %s...", hosts[i % numHosts]);
        QCOMPARE(s->state(), KTcpSocket::UnconnectedState);
        s->connectToHost(hosts[i % numHosts], 80);
        bool skip = false;
        KTcpSocket::State expectedState = KTcpSocket::ConnectingState;

        if (i < numHosts) {
            expectedState = KTcpSocket::HostLookupState;
        } else {
            expectedState = KTcpSocket::ConnectingState;
        }

        if (!skip) {
            QCOMPARE(stateToString(s->state()), stateToString(expectedState));
        } else { // let's make sure it's at least one of the two expected states
            QVERIFY(stateToString(s->state()) == stateToString(KTcpSocket::HostLookupState) ||
                    stateToString(s->state()) == stateToString(KTcpSocket::ConnectingState));

        }

        //weave the host address into the HTTP request
        QByteArray request(requestProlog);
        request.append(hosts[i % numHosts]);
        request.append(requestEpilog);
        s->write(request);

        if (!skip) {
            QCOMPARE(stateToString(s->state()), stateToString(expectedState));
        }

        s->waitForBytesWritten(-1);
        QCOMPARE(s->state(), KTcpSocket::ConnectedState);
        int tries = 0;
        while (s->bytesAvailable() <= 100 && ++tries < 10) {
            s->waitForReadyRead(-1);
        }
        QVERIFY(s->bytesAvailable() > 100);
        if (i % (numHosts + 1)) {
            s->readAll();
            QVERIFY(s->bytesAvailable() == 0);
        } else {
            char dummy[4];
            s->read(dummy, 1);
            QVERIFY(s->bytesAvailable() > 100 - 1);
        }
        s->disconnectFromHost();
        if (s->state() != KTcpSocket::UnconnectedState) {
            s->waitForDisconnected(-1);
        }
        if (i % 2) {
            s->close();    //close() is not very well defined for sockets so just check that it
                           //does no harm
        }
    }

    s->deleteLater();
}

void KTcpSocketTest::states_hostFound()
{
    QCOMPARE(static_cast<KTcpSocket *>(sender())->state(), KTcpSocket::ConnectingState);
}

void Server::states()
{
    listener->waitForNewConnection(10000, nullptr);
    socket = listener->nextPendingConnection();

    socket->waitForReadyRead(40);
    socket->write(socket->readAll()); //echo
    socket->waitForBytesWritten(150);

    cleanupSocket();
}

void KTcpSocketTest::errors()
{
    //invokeOnServer("errors");
}

void Server::errors()
{
    listener->waitForNewConnection(10000, nullptr);
    socket = listener->nextPendingConnection();

    cleanupSocket();
}

QTEST_MAIN(KTcpSocketTest)
