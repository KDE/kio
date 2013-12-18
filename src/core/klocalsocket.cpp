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

#include "klocalsocket.h"
#include "klocalsocket_p.h"

#include <QtCore/QSocketNotifier>


//#define LocalSocket (QAbstractSocket::SocketType(int(QAbstractSocket::UdpSocket) + 1))

void KLocalSocketPrivate::emitError(QAbstractSocket::SocketError error, const QString &errorString)
{
    q->setSocketState(QAbstractSocket::UnconnectedState);
    q->setSocketError(error);
    q->setErrorString(errorString);
    emit q->stateChanged(QAbstractSocket::UnconnectedState);
    emit q->error(error);
}

void KLocalSocketServerPrivate::emitError(QAbstractSocket::SocketError an_error, const QString &an_errorString)
{
    error = an_error;
    errorString = an_errorString;
}

KLocalSocket::KLocalSocket(QObject *parent)
    : QTcpSocket(parent), d(new KLocalSocketPrivate(this))
{
}

KLocalSocket::~KLocalSocket()
{
    delete d;
    // parent's destructor closes the socket
}

void KLocalSocket::connectToPath(const QString &path, OpenMode mode)
{
    // cheat:
    connectToHost(path, UnixSocket, mode);
}

void KLocalSocket::connectToPath(const QString &path, LocalSocketType type, OpenMode mode)
{
    // cheat:
    connectToHost(path, type, mode);
}

void KLocalSocket::connectToHostImplementation(const QString &path, quint16 type, OpenMode mode)
{
    if (state() == ConnectedState || state() == ConnectingState)
        return;

    d->localPath.clear();
    d->peerPath.clear();

    setSocketState(ConnectingState);
    emit stateChanged(ConnectingState);

    d->connectToPath(path, LocalSocketType(type), mode);
}

void KLocalSocket::disconnectFromHostImplementation()
{
    QTcpSocket::disconnectFromHost();

    d->peerPath.clear();
    d->localPath.clear();
    d->type = UnknownLocalSocketType;
}

void KLocalSocket::disconnectFromPath()
{
    // cheat:
    disconnectFromHost();
}

KLocalSocket::LocalSocketType KLocalSocket::localSocketType() const
{
    return d->type;
}

QString KLocalSocket::localPath() const
{
    return d->localPath;
}

QString KLocalSocket::peerPath() const
{
    return d->peerPath;
}

KLocalSocketServerPrivate::KLocalSocketServerPrivate(KLocalSocketServer *qq)
    : q(qq), descriptor(-1), maxPendingConnections(30),
      state(QAbstractSocket::UnconnectedState),
      error(QAbstractSocket::UnknownSocketError),
      type(KLocalSocket::UnknownLocalSocketType),
      readNotifier(0)
{
}

KLocalSocketServer::KLocalSocketServer(QObject *parent)
    : QObject(parent), d(new KLocalSocketServerPrivate(this))
{
}

KLocalSocketServer::~KLocalSocketServer()
{
    close();
    delete d;
}

bool KLocalSocketServer::isListening() const
{
    return d->state == QAbstractSocket::ListeningState;
}

bool KLocalSocketServer::listen(const QString &path, KLocalSocket::LocalSocketType type)
{
    if (d->state == QAbstractSocket::ListeningState)
        return false;           // already created

    if (!d->listen(path, type)) {
        // the private set the error code
        return false;
    }

    d->localPath = path;
    return true;
}

void KLocalSocketServer::close()
{
    d->close();
}

void KLocalSocketServer::setMaxPendingConnections(int numConnections)
{
    if (numConnections >= 0) {
        d->maxPendingConnections = numConnections;
        d->readNotifier->setEnabled(d->pendingConnections.size() < d->maxPendingConnections);
    } else {
        qWarning("KLocalSocketServer::setMaxPendingConnections: cannot set to a negative number");
    }
}

int KLocalSocketServer::maxPendingConnections() const
{
    return d->maxPendingConnections;
}

KLocalSocket::LocalSocketType KLocalSocketServer::localSocketType() const
{
    return d->type;
}

QString KLocalSocketServer::localPath() const
{
    return d->localPath;
}

bool KLocalSocketServer::waitForNewConnection(int msec, bool *timedOut)
{
    if (!isListening())
        return false;           // can't wait if we're not not listening

    return d->waitForNewConnection(msec, timedOut);
}

bool KLocalSocketServer::hasPendingConnections() const
{
    return !d->pendingConnections.isEmpty();
}

KLocalSocket *KLocalSocketServer::nextPendingConnection()
{
    if (hasPendingConnections()) {
        d->readNotifier->setEnabled((d->pendingConnections.size() - 1) < d->maxPendingConnections);
        return d->pendingConnections.dequeue();
    }
    return 0;
}

void KLocalSocketServer::incomingConnection(int descriptor)
{
    KLocalSocket *socket = new KLocalSocket(this);
    KLocalSocketPrivate *socket_d = KLocalSocketPrivate::d(socket);
    socket_d->localPath = d->localPath;
    socket_d->type = d->type;

    socket->setSocketDescriptor(descriptor, QAbstractSocket::ConnectedState, QIODevice::ReadWrite);
    d->pendingConnections.enqueue(socket);

    emit newConnection();
}

QAbstractSocket::SocketError KLocalSocketServer::serverError() const
{
    return d->error;
}

QString KLocalSocketServer::errorString() const
{
    return d->errorString;
}

#include "moc_klocalsocket.cpp"
