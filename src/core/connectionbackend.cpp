/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionbackend_p.h"
#include <errno.h>
#include <QCoreApplication>
#include <KLocalizedString>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QPointer>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>

#include "kiocoredebug.h"

using namespace KIO;

ConnectionBackend::ConnectionBackend(QObject *parent)
    : QObject(parent),
      state(Idle),
      socket(nullptr),
      len(-1),
      cmd(0),
      signalEmitted(false)
{
    localServer = nullptr;
}

ConnectionBackend::~ConnectionBackend()
{
}

void ConnectionBackend::setSuspended(bool enable)
{
    if (state != Connected) {
        return;
    }
    Q_ASSERT(socket);
    Q_ASSERT(!localServer);     // !tcpServer as well

    if (enable) {
        //qCDebug(KIO_CORE) << socket << "suspending";
        socket->setReadBufferSize(1);
    } else {
        //qCDebug(KIO_CORE) << socket << "resuming";
        // Calling setReadBufferSize from a readyRead slot leads to a bug in Qt, fixed in 13c246ee119
        socket->setReadBufferSize(StandardBufferSize);
        if (socket->bytesAvailable() >= HeaderSize) {
            // there are bytes available
            QMetaObject::invokeMethod(this, "socketReadyRead", Qt::QueuedConnection);
        }

        // We read all bytes here, but we don't use readAll() because we need
        // to read at least one byte (even if there isn't any) so that the
        // socket notifier is reenabled
        QByteArray data = socket->read(socket->bytesAvailable() + 1);
        for (int i = data.size(); --i >= 0;) {
            socket->ungetChar(data[i]);
        }
        // Workaround Qt5 bug, readyRead isn't always emitted here...
        QMetaObject::invokeMethod(this, "socketReadyRead", Qt::QueuedConnection);
    }
}

bool ConnectionBackend::connectToRemote(const QUrl &url)
{
    Q_ASSERT(state == Idle);
    Q_ASSERT(!socket);
    Q_ASSERT(!localServer);     // !tcpServer as well

    QLocalSocket *sock = new QLocalSocket(this);
    QString path = url.path();
    sock->connectToServer(path);
    socket = sock;

    connect(socket, &QIODevice::readyRead, this, &ConnectionBackend::socketReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &ConnectionBackend::socketDisconnected);
    state = Connected;
    return true;
}

void ConnectionBackend::socketDisconnected()
{
    state = Idle;
    Q_EMIT disconnected();
}

bool ConnectionBackend::listenForRemote()
{
    Q_ASSERT(state == Idle);
    Q_ASSERT(!socket);
    Q_ASSERT(!localServer);     // !tcpServer as well

    const QString prefix = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    static QBasicAtomicInt s_socketCounter = Q_BASIC_ATOMIC_INITIALIZER(1);
    QString appName = QCoreApplication::instance()->applicationName();
    appName.replace(QLatin1Char('/'), QLatin1Char('_')); // #357499
    QTemporaryFile socketfile(prefix + QLatin1Char('/') + appName + QStringLiteral("XXXXXX.%1.slave-socket").arg(s_socketCounter.fetchAndAddAcquire(1)));
    if (!socketfile.open()) {
        errorString = i18n("Unable to create io-slave: %1", QString::fromUtf8(strerror(errno)));
        return false;
    }

    QString sockname = socketfile.fileName();
    address.clear();
    address.setScheme(QStringLiteral("local"));
    address.setPath(sockname);
    socketfile.setAutoRemove(false);
    socketfile.remove(); // can't bind if there is such a file

    localServer = new QLocalServer(this);
    if (!localServer->listen(sockname)) {
        errorString = localServer->errorString();
        delete localServer;
        localServer = nullptr;
        return false;
    }

    connect(localServer, &QLocalServer::newConnection, this, &ConnectionBackend::newConnection);

    state = Listening;
    return true;
}

bool ConnectionBackend::waitForIncomingTask(int ms)
{
    Q_ASSERT(state == Connected);
    Q_ASSERT(socket);
    if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
        state = Idle;
        return false;           // socket has probably closed, what do we do?
    }

    signalEmitted = false;
    if (socket->bytesAvailable()) {
        socketReadyRead();
    }
    if (signalEmitted) {
        return true;    // there was enough data in the socket
    }

    // not enough data in the socket, so wait for more
    QElapsedTimer timer;
    timer.start();

    while (socket->state() == QLocalSocket::LocalSocketState::ConnectedState && !signalEmitted &&
            (ms == -1 || timer.elapsed() < ms))
        if (!socket->waitForReadyRead(ms == -1 ? -1 : ms - timer.elapsed())) {
            break;
        }

    if (signalEmitted) {
        return true;
    }
    if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
        state = Idle;
    }
    return false;
}

bool ConnectionBackend::sendCommand(int cmd, const QByteArray &data) const
{
    Q_ASSERT(state == Connected);
    Q_ASSERT(socket);

    char buffer[HeaderSize + 2];
    sprintf(buffer, "%6x_%2x_", data.size(), cmd);
    socket->write(buffer, HeaderSize);
    socket->write(data);

    //qCDebug(KIO_CORE) << this << "Sending command" << hex << cmd << "of"
    //         << data.size() << "bytes (" << socket->bytesToWrite()
    //         << "bytes left to write )";

    // blocking mode:
    while (socket->bytesToWrite() > 0 && socket->state() == QLocalSocket::LocalSocketState::ConnectedState) {
        socket->waitForBytesWritten(-1);
    }

    return socket->state() == QLocalSocket::LocalSocketState::ConnectedState;
}

ConnectionBackend *ConnectionBackend::nextPendingConnection()
{
    Q_ASSERT(state == Listening);
    Q_ASSERT(localServer);
    Q_ASSERT(!socket);

    //qCDebug(KIO_CORE) << "Got a new connection";

    QLocalSocket *newSocket = localServer->nextPendingConnection();

    if (!newSocket) {
        return nullptr;    // there was no connection...
    }

    ConnectionBackend *result = new ConnectionBackend();
    result->state = Connected;
    result->socket = newSocket;
    newSocket->setParent(result);
    connect(newSocket, &QIODevice::readyRead, result, &ConnectionBackend::socketReadyRead);
    connect(newSocket, &QLocalSocket::disconnected, result, &ConnectionBackend::socketDisconnected);

    return result;
}

void ConnectionBackend::socketReadyRead()
{
    bool shouldReadAnother;
    do {
        if (!socket)
            // might happen if the invokeMethods were delivered after we disconnected
        {
            return;
        }

        //qCDebug(KIO_CORE) << this << "Got" << socket->bytesAvailable() << "bytes";
        if (len == -1) {
            // We have to read the header
            char buffer[HeaderSize];

            if (socket->bytesAvailable() < HeaderSize) {
                return;             // wait for more data
            }

            socket->read(buffer, sizeof buffer);
            buffer[6] = 0;
            buffer[9] = 0;

            char *p = buffer;
            while (*p == ' ') {
                p++;
            }
            len = strtol(p, nullptr, 16);

            p = buffer + 7;
            while (*p == ' ') {
                p++;
            }
            cmd = strtol(p, nullptr, 16);

            //qCDebug(KIO_CORE) << this << "Beginning of command" << hex << cmd << "of size" << len;
        }

        QPointer<ConnectionBackend> that = this;

        //qCDebug(KIO_CORE) << socket << "Want to read" << len << "bytes";
        if (socket->bytesAvailable() >= len) {
            Task task;
            task.cmd = cmd;
            if (len) {
                task.data = socket->read(len);
            }
            len = -1;

            signalEmitted = true;
            Q_EMIT commandReceived(task);
        } else if (len > StandardBufferSize) {
            qCDebug(KIO_CORE) << socket << "Jumbo packet of" << len << "bytes";
            // Calling setReadBufferSize from a readyRead slot leads to a bug in Qt, fixed in 13c246ee119
            socket->setReadBufferSize(len + 1);
        }

        // If we're dead, better don't try anything.
        if (that.isNull()) {
            return;
        }

        // Do we have enough for an another read?
        if (len == -1) {
            shouldReadAnother = socket->bytesAvailable() >= HeaderSize;
        } else {
            shouldReadAnother = socket->bytesAvailable() >= len;
        }
    } while (shouldReadAnother);
}

