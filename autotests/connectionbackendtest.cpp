// SPDX-License-Identifier: LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

#include <cstdlib>

#include <QSignalSpy>
#include <QTest>

#include <connectionbackend_p.h>

class ConnectionBackendTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testJumboPackets()
    {
        QSKIP("TODO testJumboPackets doesn't pass FIXME");

        KIO::ConnectionBackend server;
        KIO::ConnectionBackend clientConnection;

        QVERIFY(server.listenForRemote().success);
        auto spy = std::make_unique<QSignalSpy>(&server, &KIO::ConnectionBackend::newConnection);
        QVERIFY(clientConnection.connectToRemote(server.address));
        spy->wait();
        QVERIFY(!spy->isEmpty());
        auto serverConnection = std::unique_ptr<KIO::ConnectionBackend>(server.nextPendingConnection());
        QVERIFY(serverConnection);

        spy = std::make_unique<QSignalSpy>(&clientConnection, &KIO::ConnectionBackend::commandReceived);
        constexpr auto cmd = 64; // completely arbitrary value we don't actually care about the command in this test
        const auto data = randomByteArray(clientConnection.StandardBufferSize * 4L);
        serverConnection->sendCommand(cmd, data);
        spy->wait();
        QVERIFY(!spy->isEmpty());

        auto task = spy->at(0).at(0).value<KIO::Task>();
        QCOMPARE(task.data.size(), data.size());
    }

private:
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
