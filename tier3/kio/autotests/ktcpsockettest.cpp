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

#include "ktcpsockettest.h"
#include <QtCore/QDebug>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>
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

class ServerThread : public QThread
{
public:
    Server *volatile server;
    ServerThread()
     : server(0) {}
    ~ServerThread() { wait(100); }
protected:
    virtual void run();
};

KTcpSocketTest::KTcpSocketTest()
{
    server = 0;
}

KTcpSocketTest::~KTcpSocketTest()
{
    delete server;
}

void KTcpSocketTest::invokeOnServer(const char *method)
{
    QMetaObject::invokeMethod(server, method, Qt::QueuedConnection);
    QTest::qWait(1); //Enter the event loop
}


void ServerThread::run()
{
    server = new Server(testPort);
    exec(); //Start the event loop; this won't return.
}


Server::Server(quint16 _port)
 : listener(0),
   socket(0),
   port(_port)
{
    listener = new QTcpServer();
    listener->listen(QHostAddress("127.0.0.1"), testPort);
}

void Server::cleanupSocket()
{
    Q_ASSERT(socket);
    socket->close();
    socket->deleteLater();
    socket = 0;
}


void KTcpSocketTest::initTestCase()
{
    ServerThread *st = new ServerThread();
    st->start();
    while (!st->server)
        ;
    //Let the other thread initialize its event loop or whatever; there were problems...
    QTest::qWait(200);
    server = st->server;
}


void KTcpSocketTest::connectDisconnect()
{
    invokeOnServer("connectDisconnect");

    KTcpSocket *s = new KTcpSocket(this);
    QCOMPARE(s->openMode(), QIODevice::NotOpen);
    QCOMPARE(s->error(), KTcpSocket::UnknownError);

    s->connectToHost("127.0.0.1", testPort);
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
    listener->waitForNewConnection(10000, 0);
    socket = listener->nextPendingConnection();

    cleanupSocket();
}


#define TESTDATA QByteArray("things and stuff and a bag of chips")

void KTcpSocketTest::read()
{
    invokeOnServer("read");

    KTcpSocket *s = new KTcpSocket(this);
    s->connectToHost("127.0.0.1", testPort);
    s->waitForConnected(40);
    s->waitForReadyRead(40);
    QCOMPARE((int)s->bytesAvailable(), TESTDATA.size());
    QCOMPARE(s->readAll(), TESTDATA);
    s->deleteLater();
}

void Server::read()
{
    listener->waitForNewConnection(10000, 0);
    socket = listener->nextPendingConnection();

    socket->write(TESTDATA);
    socket->waitForBytesWritten(150);
    cleanupSocket();
}


void KTcpSocketTest::write()
{
    invokeOnServer("write");

    KTcpSocket *s = new KTcpSocket(this);
    s->connectToHost("127.0.0.1", testPort);
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
    listener->waitForNewConnection(10000, 0);
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
    switch(state) {
    case KTcpSocket::UnconnectedState:
        return "UnconnectedState";
    case KTcpSocket::HostLookupState:
        return "HostLookupState";
    case KTcpSocket::ConnectingState:
        return "ConnectingState";
    case KTcpSocket::ConnectedState:
        return "ConnectedState";
    case KTcpSocket::BoundState:
        return "BoundState";
    case KTcpSocket::ListeningState:
        return "ListeningState";
    case KTcpSocket::ClosingState:
        return "ClosingState";
    }
    return "ERROR";
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
    connect(s, SIGNAL(hostFound()), this, SLOT(states_hostFound()));
    QCOMPARE(s->state(), KTcpSocket::UnconnectedState);
    s->connectToHost("www.iana.org", 80);
    QCOMPARE(s->state(), KTcpSocket::HostLookupState);
    s->write(HTTPREQUEST);
    QCOMPARE(s->state(), KTcpSocket::HostLookupState);
    s->waitForBytesWritten(2500) ;
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
    connect(s, SIGNAL(hostFound()), this, SLOT(states_hostFound()));
    s->connectToHost("127.0.0.1", testPort);
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
    static const char *hosts[] = {"www.google.de", "www.spiegel.de", "www.stern.de", "www.laut.de"};
    static const int numHosts = 4;
    for (int i = 0; i < numHosts * 5; i++) {
        qDebug("\nNow trying %s...", hosts[i % numHosts]);
        QCOMPARE(s->state(), KTcpSocket::UnconnectedState);
        s->connectToHost(hosts[i % numHosts], 80);
        bool skip = false;
        KTcpSocket::State expectedState = KTcpSocket::ConnectingState;
#if QT_VERSION > 0x040701
        // Since Qt 4.6.3 the Qt-internal DNS cache returns a result (if cached) immediately
        // but it was unreliable (when called from QTcpSocket) until 4.7.2
        if (i < numHosts) {
            expectedState = KTcpSocket::HostLookupState;
        } else {
            expectedState = KTcpSocket::ConnectingState;
        }
#elif QT_VERSION < 0x040603
        // Previously there was no caching
        expectedState = KTcpSocket::HostLookupState;
#else   // 4.6.3 to 4.7.1: unreliable results, skip test
        skip = true;
        qDebug() << "Skipping test on state(), because DNS caching is unreliable in this Qt version";
#endif
        if (!skip)
            QCOMPARE(stateToString(s->state()), stateToString(expectedState));
        else { // let's make sure it's at least one of the two expected states
            QVERIFY(stateToString(s->state()) == stateToString(KTcpSocket::HostLookupState) ||
                    stateToString(s->state()) == stateToString(KTcpSocket::ConnectingState));

        }

        //weave the host address into the HTTP request
        QByteArray request(requestProlog);
        request.append(hosts[i % numHosts]);
        request.append(requestEpilog);
        s->write(request);

        if (!skip)
            QCOMPARE(stateToString(s->state()), stateToString(expectedState));

        s->waitForBytesWritten(-1);
        QCOMPARE(s->state(), KTcpSocket::ConnectedState);
        s->waitForReadyRead(-1);
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
        if (s->state() != KTcpSocket::UnconnectedState)
            s->waitForDisconnected(-1);
        if (i % 2)
            s->close();     //close() is not very well defined for sockets so just check that it
                            //does no harm
    }

    s->deleteLater();
}

void KTcpSocketTest::states_hostFound()
{
    QCOMPARE(static_cast<KTcpSocket *>(sender())->state(), KTcpSocket::ConnectingState);
}

void Server::states()
{
    listener->waitForNewConnection(10000, 0);
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
    listener->waitForNewConnection(10000, 0);
    socket = listener->nextPendingConnection();

    cleanupSocket();
}


QTEST_MAIN(KTcpSocketTest)

