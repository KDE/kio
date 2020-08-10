/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionserver.h"
#include "connection_p.h"
#include "connectionbackend_p.h"

using namespace KIO;

class KIO::ConnectionServerPrivate
{
public:
    inline ConnectionServerPrivate()
        : backend(nullptr)
    { }

    ConnectionServer *q;
    ConnectionBackend *backend;
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
    d->backend = new ConnectionBackend(this);
    if (!d->backend->listenForRemote()) {
        delete d->backend;
        d->backend = nullptr;
        return;
    }

    connect(d->backend, &ConnectionBackend::newConnection, this, &ConnectionServer::newConnection);
    //qDebug() << "Listening on" << d->backend->address;
}

QUrl ConnectionServer::address() const
{
    if (d->backend) {
        return d->backend->address;
    }
    return QUrl();
}

bool ConnectionServer::isListening() const
{
    return d->backend && d->backend->state == ConnectionBackend::Listening;
}

void ConnectionServer::close()
{
    delete d->backend;
    d->backend = nullptr;
}

Connection *ConnectionServer::nextPendingConnection()
{
    if (!isListening()) {
        return nullptr;
    }

    ConnectionBackend *newBackend = d->backend->nextPendingConnection();
    if (!newBackend) {
        return nullptr;    // no new backend...
    }

    Connection *result = new Connection;
    result->d->setBackend(newBackend);
    newBackend->setParent(result);

    return result;
}

void ConnectionServer::setNextPendingConnection(Connection *conn)
{
    ConnectionBackend *newBackend = d->backend->nextPendingConnection();
    Q_ASSERT(newBackend);

    conn->d->setBackend(newBackend);
    newBackend->setParent(conn);

    conn->d->dequeue();
}
