// SPDX-License-Identifier: LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

#include <cstdlib>
#include <functional>

#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

#include <socketconnectionbackend_p.h>

class ConnectionBackendTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testJumboPackets()
    {
#if defined(Q_OS_FREEBSD)
        QSKIP("TODO testJumboPackets doesn't pass FIXME");
#endif

        KIO::SocketConnectionBackend server;
        KIO::SocketConnectionBackend clientConnection;

        QVERIFY(server.listenForRemote().success);
        auto spy = std::make_unique<QSignalSpy>(&server, &KIO::SocketConnectionBackend::newConnection);
        QVERIFY(clientConnection.connectToRemote(server.address));
        spy->wait();
        QVERIFY(!spy->isEmpty());
        auto serverConnection = std::unique_ptr<KIO::ConnectionBackend>(server.nextPendingConnection());
        QVERIFY(serverConnection);

        spy = std::make_unique<QSignalSpy>(&clientConnection, &KIO::ConnectionBackend::commandReceived);
        constexpr auto cmd = 64; // completely arbitrary value we don't actually care about the command in this test
        const auto data = randomByteArray(clientConnection.StandardBufferSize * 4L);
        QVERIFY(serverConnection->sendCommand(cmd, data));
        spy->wait();
        QVERIFY(!spy->isEmpty());

        auto task = spy->at(0).at(0).value<KIO::Task>();
        QCOMPARE(task.bytes().size(), data.size());
    }

    // Resuming a backend whose socket is not open must stay quiet. connectToRemote sets the
    // backend to Connected and returns at once, but if no server is listening the socket never
    // opens. On Windows the resume path reads one byte to kick the named pipe reader; doing that
    // on a socket that is not open would print "QIODevice::read (QLocalSocket): device not open".
    void testResumeDoesNotReadUnopenedSocket()
    {
        QTest::failOnWarning(QRegularExpression(QStringLiteral("device not open")));

        KIO::SocketConnectionBackend backend;

        const QString prefix = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        QUrl address;
        address.setScheme(QStringLiteral("local"));
        address.setPath(prefix + QStringLiteral("/connectionbackendtest-no-such-server.socket"));

        QVERIFY(backend.connectToRemote(address));

        // Resume right away, while the socket is still trying to connect, and again after the
        // connection has had a chance to fail. Neither must touch the not-open socket.
        backend.setSuspended(false);
        QTest::qWait(50);
        backend.setSuspended(false);
    }

    // A command sent around a suspend/resume cycle is delivered intact. On Unix
    // it is additionally held back until the connection resumes.
    void testSuspendResume()
    {
        KIO::SocketConnectionBackend server;
        KIO::SocketConnectionBackend clientConnection;

        QVERIFY(server.listenForRemote().success);
        auto newConnectionSpy = std::make_unique<QSignalSpy>(&server, &KIO::SocketConnectionBackend::newConnection);
        QVERIFY(clientConnection.connectToRemote(server.address));
        newConnectionSpy->wait();
        QVERIFY(!newConnectionSpy->isEmpty());
        auto serverConnection = std::unique_ptr<KIO::ConnectionBackend>(server.nextPendingConnection());
        QVERIFY(serverConnection);

        // Suspend the receiving side before anything is sent.
        clientConnection.setSuspended(true);

        QSignalSpy spy(&clientConnection, &KIO::ConnectionBackend::commandReceived);
        const auto data = randomByteArray(4096);
        bool sendOk = false;
        auto sender = startSender(serverConnection.get(), [&] {
            sendOk = serverConnection->sendCommand(64, data);
        });

        // On Unix, suspending stops the socket reading, so nothing is delivered
        // while suspended, even after the event loop has run. A Windows local
        // socket is a named pipe whose already-issued read completes regardless of
        // the shrunk read buffer, so it does not hold data back this way; there we
        // only require that the command arrives intact once resumed.
#ifndef Q_OS_WIN
        QVERIFY(!spy.wait(500));
        QVERIFY(spy.isEmpty());
#endif

        // Resuming lets the receiver drain: the command arrives intact and the
        // blocked send returns.
        clientConnection.setSuspended(false);
        QTRY_COMPARE(spy.size(), 1);
        QVERIFY(sender->wait());
        QVERIFY(sendOk);

        auto task = spy.at(0).at(0).value<KIO::Task>();
        QCOMPARE(task.bytes(), data);
    }

    // Commands keep flowing intact and in order across repeated suspend and
    // resume cycles, for payloads on either side of the header length.
    void testSuspendResumeStress()
    {
        KIO::SocketConnectionBackend server;
        KIO::SocketConnectionBackend clientConnection;

        QVERIFY(server.listenForRemote().success);
        auto newConnectionSpy = std::make_unique<QSignalSpy>(&server, &KIO::SocketConnectionBackend::newConnection);
        QVERIFY(clientConnection.connectToRemote(server.address));
        newConnectionSpy->wait();
        QVERIFY(!newConnectionSpy->isEmpty());
        auto serverConnection = std::unique_ptr<KIO::ConnectionBackend>(server.nextPendingConnection());
        QVERIFY(serverConnection);

        QSignalSpy spy(&clientConnection, &KIO::ConnectionBackend::commandReceived);

        const QList<qsizetype> sizes = {0, 1, 5, 9, 10, 11, 20, 40, 100, 1024, 4096};
        QList<QByteArray> payloads;
        for (qsizetype size : sizes) {
            payloads.append(randomByteArray(size));
        }

        auto sender = startSender(serverConnection.get(), [&] {
            for (const auto &payload : payloads) {
                serverConnection->sendCommand(64, payload);
            }
        });

        // Toggle suspend and resume while the sends are in flight. The guard just
        // stops the loop if delivery stalls so the test fails instead of hanging.
        qsizetype guard = 0;
        while (spy.size() < payloads.size() && ++guard < 5000) {
            clientConnection.setSuspended(true);
            QTest::qWait(1);
            clientConnection.setSuspended(false);
            QTest::qWait(1);
        }
        QVERIFY(sender->wait());

        QCOMPARE(spy.size(), payloads.size());
        for (qsizetype i = 0; i < payloads.size(); ++i) {
            auto task = spy.at(i).at(0).value<KIO::Task>();
            QCOMPARE(task.bytes(), payloads.at(i));
        }
    }

private:
    // Run work on its own thread with conn moved onto it. sendCommand blocks
    // until the peer drains the socket, and a suspended peer only drains once it
    // is resumed, so the send has to run off the thread that does the resuming.
    // conn is moved back to the calling thread before the worker finishes, so the
    // caller owns it again after waiting on the returned thread.
    std::unique_ptr<QThread> startSender(KIO::ConnectionBackend *conn, std::function<void()> work)
    {
        QThread *home = QThread::currentThread();
        std::unique_ptr<QThread> thread(QThread::create([conn, home, work = std::move(work)] {
            work();
            conn->moveToThread(home);
        }));
        conn->moveToThread(thread.get());
        thread->start();
        return thread;
    }

    QByteArray randomByteArray(qsizetype size)
    {
        QByteArray data;
        data.reserve(size);
        for (int i = 0; i < size; i++) {
            data.append(std::rand() % std::max(1, std::numeric_limits<char>::digits10));
        }
        return data;
    }
};

QTEST_MAIN(ConnectionBackendTest)

#include "connectionbackendtest.moc"
