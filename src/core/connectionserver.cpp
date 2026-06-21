/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connectionserver.h"
#include "connection_p.h"
#include "kiocoredebug.h"
#include "socketconnectionbackend_p.h"

using namespace KIO;

ConnectionServer::ConnectionServer(QObject *parent)
    : QObject(parent)
{
}

ConnectionServer::~ConnectionServer() = default;

void ConnectionServer::listenForRemote()
{
    m_backend = new SocketConnectionBackend(this);
    if (auto result = m_backend->listenForRemote(); !result.success) {
        qCWarning(KIO_CORE) << "ConnectionServer::listenForRemote failed:" << result.error;
        delete m_backend;
        m_backend = nullptr;
        return;
    }

    connect(m_backend, &SocketConnectionBackend::newConnection, this, &ConnectionServer::newConnection);
    // qDebug() << "Listening on" << d->backend->address;
}

QUrl ConnectionServer::address() const
{
    if (m_backend) {
        return m_backend->address;
    }
    return QUrl();
}

bool ConnectionServer::isListening() const
{
    return m_backend && m_backend->state == ConnectionBackend::Listening;
}

void ConnectionServer::setNextPendingConnection(Connection *conn)
{
    std::unique_ptr<ConnectionBackend> newBackend(m_backend->nextPendingConnection());
    Q_ASSERT(newBackend);

    conn->d->setBackend(std::move(newBackend));

    conn->d->dequeue();
}

#include "moc_connectionserver.cpp"
