/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "socketconnectionbackend_p.h"
#include <KLocalizedString>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <cerrno>

#include "kiocoreconnectiondebug.h"

using namespace KIO;

SocketConnectionBackend::SocketConnectionBackend(QObject *parent)
    : ConnectionBackend(parent)
    , socket(nullptr)
    , signalEmitted(false)
{
    localServer = nullptr;
}

SocketConnectionBackend::~SocketConnectionBackend()
{
}

void SocketConnectionBackend::close()
{
    if (socket) {
        socket->close();
    }
}

void SocketConnectionBackend::setSuspended(bool enable)
{
    if (state != Connected) {
        return;
    }
    Q_ASSERT(socket);
    Q_ASSERT(!localServer); // !tcpServer as well

    if (enable) {
        // qCDebug(KIO_CORE) << socket << "suspending";
        socket->setReadBufferSize(1);
    } else {
        // qCDebug(KIO_CORE) << socket << "resuming";
        // On Unix, growing the read buffer re-enables the read notifier by itself,
        // so data that arrived while suspended is picked up again.
        socket->setReadBufferSize(StandardBufferSize);
#ifdef Q_OS_WIN
        // close() shuts the socket without clearing our tracked state, so a resume can still
        // reach this point with the socket already closed. Reading from it then would only warn
        // that the device is not open, so leave a socket that is no longer connected alone.
        if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
            return;
        }
        // A Windows local socket is a named pipe. While suspended its reader fills
        // the one-byte buffer and then stops, and only a read() call makes it start
        // reading again; growing the read buffer does not. So read one byte, even
        // when none is waiting, and put it straight back to get the reader going
        // again for the data that arrived while suspended.
        QByteArray data = socket->read(socket->bytesAvailable() + 1);
        for (int i = data.size(); --i >= 0;) {
            socket->ungetChar(data[i]);
        }
        // readyRead only fires for bytes that arrive from now on, not for the ones
        // just put back, so process whatever is already buffered by hand.
        QMetaObject::invokeMethod(this, &SocketConnectionBackend::socketReadyRead, Qt::QueuedConnection);
#endif
    }
}

bool SocketConnectionBackend::connectToRemote(const QUrl &url)
{
    Q_ASSERT(state == Idle);
    Q_ASSERT(!socket);
    Q_ASSERT(!localServer); // !tcpServer as well

    QLocalSocket *sock = new QLocalSocket(this);
    QString path = url.path();
    sock->connectToServer(path);
    socket = sock;

    connect(socket, &QIODevice::readyRead, this, &SocketConnectionBackend::socketReadyRead);
    connect(socket, &QLocalSocket::disconnected, this, &SocketConnectionBackend::socketDisconnected);
    state = Connected;
    return true;
}

void SocketConnectionBackend::socketDisconnected()
{
    state = Idle;
    Q_EMIT disconnected();
}

SocketConnectionBackend::ConnectionResult SocketConnectionBackend::listenForRemote()
{
    Q_ASSERT(state == Idle);
    Q_ASSERT(!socket);
    Q_ASSERT(!localServer); // !tcpServer as well

    const QString prefix = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    static QBasicAtomicInt s_socketCounter = Q_BASIC_ATOMIC_INITIALIZER(1);
    QString appName = QCoreApplication::instance()->applicationName();
    appName.replace(QLatin1Char('/'), QLatin1Char('_')); // #357499
    QTemporaryFile socketfile(prefix + QLatin1Char('/') + appName + QStringLiteral("XXXXXX.%1.kioworker.socket").arg(s_socketCounter.fetchAndAddAcquire(1)));
    if (!socketfile.open()) {
        return {false, i18n("Unable to create KIO worker: %1", QString::fromUtf8(strerror(errno)))};
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
        return {false, errorString};
    }

    connect(localServer, &QLocalServer::newConnection, this, &SocketConnectionBackend::newConnection);

    state = Listening;
    return {true, QString()};
}

bool SocketConnectionBackend::waitForIncomingTask(int ms)
{
    Q_ASSERT(state == Connected);
    Q_ASSERT(socket);
    if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
        state = Idle;
        return false; // socket has probably closed, what do we do?
    }

    signalEmitted = false;
    if (socket->bytesAvailable()) {
        socketReadyRead();
    }
    if (signalEmitted) {
        return true; // there was enough data in the socket
    }

    // not enough data in the socket, so wait for more
    QElapsedTimer timer;
    timer.start();

    while (socket->state() == QLocalSocket::LocalSocketState::ConnectedState && (ms == -1 || timer.elapsed() < ms)) {
        if (!socket->waitForReadyRead(ms == -1 ? -1 : ms - timer.elapsed())) {
            break;
        }
        if (signalEmitted) { // socketReadyRead(), invoked via readyRead while waiting, produced a task
            break;
        }
    }

    if (signalEmitted) {
        return true;
    }
    if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
        state = Idle;
    }
    return false;
}

bool SocketConnectionBackend::sendCommand(int cmd, const QByteArray &data)
{
    Q_ASSERT(state == Connected);
    Q_ASSERT(socket);

    // The header holds the size in six hex digits, so that is as much as one message can carry.
    if (data.size() > 0xffffff) {
        qCWarning(KIO_CORE_CONNECTION) << "Message of" << data.size() << "bytes is too much for one command";
        return false;
    }

    char buffer[HeaderSize + 2];
    sprintf(buffer, "%6zx_%2x_", static_cast<size_t>(data.size()), cmd);
    socket->write(buffer, HeaderSize);
    socket->write(data);

    // qCDebug(KIO_CORE) << this << "Sending command" << hex << cmd << "of"
    //         << data.size() << "bytes (" << socket->bytesToWrite()
    //         << "bytes left to write )";

    // blocking mode:
    while (socket->bytesToWrite() > 0 && socket->state() == QLocalSocket::LocalSocketState::ConnectedState) {
        socket->waitForBytesWritten(-1);
    }

    if (socket->state() != QLocalSocket::LocalSocketState::ConnectedState) {
        qCWarning(KIO_CORE_CONNECTION) << "Socket not connected" << socket->error();
    }

    return socket->state() == QLocalSocket::LocalSocketState::ConnectedState;
}

ConnectionBackend *SocketConnectionBackend::nextPendingConnection()
{
    Q_ASSERT(state == Listening);
    Q_ASSERT(localServer);
    Q_ASSERT(!socket);

    qCDebug(KIO_CORE_CONNECTION) << "Got a new connection";

    QLocalSocket *newSocket = localServer->nextPendingConnection();

    if (!newSocket) {
        qCDebug(KIO_CORE_CONNECTION) << "... nevermind";
        return nullptr; // there was no connection...
    }

    SocketConnectionBackend *result = new SocketConnectionBackend();
    result->state = Connected;
    result->socket = newSocket;
    newSocket->setParent(result);
    connect(newSocket, &QIODevice::readyRead, result, &SocketConnectionBackend::socketReadyRead);
    connect(newSocket, &QLocalSocket::disconnected, result, &SocketConnectionBackend::socketDisconnected);

    return result;
}

void SocketConnectionBackend::socketReadyRead()
{
    bool shouldReadAnother;
    do {
        if (!socket)
        // might happen if the invokeMethods were delivered after we disconnected
        {
            return;
        }

        qCDebug(KIO_CORE_CONNECTION) << this << "Got" << socket->bytesAvailable() << "bytes";
        if (!pendingTask.has_value()) {
            // We have to read the header
            char buffer[HeaderSize];

            if (socket->bytesAvailable() < HeaderSize) {
                return; // wait for more data
            }

            socket->read(buffer, sizeof buffer);
            buffer[6] = 0;
            buffer[9] = 0;

            const char *p = buffer;
            while (*p == ' ') {
                p++;
            }
            auto len = strtol(p, nullptr, 16);

            p = buffer + 7;
            while (*p == ' ') {
                p++;
            }
            auto cmd = strtol(p, nullptr, 16);

            pendingTask = Task{.cmd = static_cast<int>(cmd)};
            pendingLen = len;
            pendingData.clear();
            pendingData.reserve(len);

            qCDebug(KIO_CORE_CONNECTION) << this << "Beginning of command" << pendingTask->cmd << "of size" << pendingLen;
        }

        QPointer<ConnectionBackend> that = this;

        const auto alreadyRead = pendingData.size();
        const auto toRead = std::min<off_t>(socket->bytesAvailable(), pendingLen - alreadyRead);
        qCDebug(KIO_CORE_CONNECTION) << socket << "Want to read" << toRead << "bytes; appending to already existing bytes" << alreadyRead;
        // Read into the buffer the command is collected in, rather than into one of its own.
        pendingData.resize(alreadyRead + toRead);
        const auto wasRead = socket->read(pendingData.data() + alreadyRead, toRead);
        pendingData.resize(alreadyRead + std::max<qint64>(wasRead, 0));

        if (pendingData.size() == pendingLen) { // read all data of this task -> emit it and reset
            signalEmitted = true;
            qCDebug(KIO_CORE_CONNECTION) << "emitting task" << pendingTask->cmd << pendingData.size();
            pendingTask->payload = std::move(pendingData);
            Q_EMIT commandReceived(pendingTask.value());

            pendingTask = {};
            pendingData.clear();
        }

        // If we're dead, better don't try anything.
        if (that.isNull()) {
            return;
        }

        // Do we have enough for an another read?
        if (!pendingTask.has_value()) {
            shouldReadAnother = socket->bytesAvailable() >= HeaderSize;
        } else { // NOTE: if we don't have data pending we may still have a pendingTask that gets resumed when we get more data!
            shouldReadAnother = socket->bytesAvailable();
        }
    } while (shouldReadAnother);
}

#include "moc_socketconnectionbackend_p.cpp"
