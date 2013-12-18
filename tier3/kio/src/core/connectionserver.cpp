/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "connectionserver.h"
#include "connection_p.h"
#include "socketconnectionbackend_p.h"

using namespace KIO;

class KIO::ConnectionServerPrivate
{
public:
    inline ConnectionServerPrivate()
        : backend(0)
    { }

    ConnectionServer *q;
    AbstractConnectionBackend *backend;
};

ConnectionServer::ConnectionServer(QObject *parent)
    : QObject(parent), d(new ConnectionServerPrivate)
{
    d->q = this;
}

ConnectionServer::~ConnectionServer()
{
    delete d;
}

void ConnectionServer::listenForRemote()
{
#ifdef Q_OS_WIN
    d->backend = new SocketConnectionBackend(SocketConnectionBackend::TcpSocketMode, this);
#else
    d->backend = new SocketConnectionBackend(SocketConnectionBackend::LocalSocketMode, this);
#endif
    if (!d->backend->listenForRemote()) {
        delete d->backend;
        d->backend = 0;
        return;
    }

    connect(d->backend, SIGNAL(newConnection()), SIGNAL(newConnection()));
    //qDebug() << "Listening on" << d->backend->address;
}

QUrl ConnectionServer::address() const
{
    if (d->backend)
        return d->backend->address;
    return QUrl();
}

bool ConnectionServer::isListening() const
{
    return d->backend && d->backend->state == AbstractConnectionBackend::Listening;
}

void ConnectionServer::close()
{
    delete d->backend;
    d->backend = 0;
}

Connection *ConnectionServer::nextPendingConnection()
{
    if (!isListening())
        return 0;

    AbstractConnectionBackend *newBackend = d->backend->nextPendingConnection();
    if (!newBackend)
        return 0;               // no new backend...

    Connection *result = new Connection;
    result->d->setBackend(newBackend);
    newBackend->setParent(result);

    return result;
}

void ConnectionServer::setNextPendingConnection(Connection *conn)
{
    AbstractConnectionBackend *newBackend = d->backend->nextPendingConnection();
    Q_ASSERT(newBackend);

    conn->d->backend = newBackend;
    conn->d->setBackend(newBackend);
    newBackend->setParent(conn);

    conn->d->dequeue();
}
