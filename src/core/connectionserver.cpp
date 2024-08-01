/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionserver.h"
#include "connection_p.h"
#include "connectionbackend_p.h"
#include "kiocoredebug.h"

using namespace KIO;

ConnectionServer::ConnectionServer(QObject *parent)
    : QObject(parent)
{
}

ConnectionServer::~ConnectionServer() = default;

void ConnectionServer::listenForRemote()
{
    backend = new ConnectionBackend(this);
    if (auto result = backend->listenForRemote(); !result.success) {
        qCWarning(KIO_CORE) << "ConnectionServer::listenForRemote failed:" << result.error;
        delete backend;
        backend = nullptr;
        return;
    }

    connect(backend, &ConnectionBackend::newConnection, this, &ConnectionServer::newConnection);
    // qDebug() << "Listening on" << d->backend->address;
}

QUrl ConnectionServer::address() const
{
    if (backend) {
        return backend->address;
    }
    return QUrl();
}

bool ConnectionServer::isListening() const
{
    return backend && backend->state == ConnectionBackend::Listening;
}

void ConnectionServer::setNextPendingConnection(Connection *conn)
{
    ConnectionBackend *newBackend = backend->nextPendingConnection();
    Q_ASSERT(newBackend);

    conn->d->setBackend(newBackend);
    newBackend->setParent(conn);

    conn->d->dequeue();
}

#include "moc_connectionserver.cpp"
