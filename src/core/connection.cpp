/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2024 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connection_p.h"
#include "connectionbackend_p.h"
#include "kiocoredebug.h"
#include <QDebug>

#include <cerrno>

using namespace KIO;

void ConnectionPrivate::dequeue()
{
    if (!backend || suspended) {
        return;
    }

    for (const Task &task : std::as_const(outgoingTasks)) {
        q->sendnow(task.cmd, task.data);
    }
    outgoingTasks.clear();

    if (!incomingTasks.isEmpty()) {
        Q_EMIT q->readyRead();
    }
}

void ConnectionPrivate::commandReceived(const Task &task)
{
    // qDebug() << this << "Command" << task.cmd << "added to the queue";
    if (!suspended && incomingTasks.isEmpty() && readMode == Connection::ReadMode::EventDriven) {
        auto dequeueFunc = [this]() {
            dequeue();
        };
        QMetaObject::invokeMethod(q, dequeueFunc, Qt::QueuedConnection);
    }
    incomingTasks.append(task);
}

void ConnectionPrivate::disconnected()
{
    q->close();
    if (readMode == Connection::ReadMode::EventDriven) {
        QMetaObject::invokeMethod(q, &Connection::readyRead, Qt::QueuedConnection);
    }
}

void ConnectionPrivate::setBackend(ConnectionBackend *b)
{
    delete backend;
    backend = b;
    if (backend) {
        q->connect(backend, &ConnectionBackend::commandReceived, q, [this](const Task &task) {
            commandReceived(task);
        });
        q->connect(backend, &ConnectionBackend::disconnected, q, [this]() {
            disconnected();
        });
        backend->setSuspended(suspended);
    }
}

Connection::Connection(Type type, QObject *parent)
    : QObject(parent)
    , d(new ConnectionPrivate)
    , m_type(type)
{
    d->q = this;
}

Connection::~Connection()
{
    close();
}

void Connection::suspend()
{
    // qDebug() << this << "Suspended";
    d->suspended = true;
    if (d->backend) {
        d->backend->setSuspended(true);
    }
}

void Connection::resume()
{
    // send any outgoing or incoming commands that may be in queue
    if (d->readMode == Connection::ReadMode::EventDriven) {
        auto dequeueFunc = [this]() {
            d->dequeue();
        };
        QMetaObject::invokeMethod(this, dequeueFunc, Qt::QueuedConnection);
    }

    // qDebug() << this << "Resumed";
    d->suspended = false;
    if (d->backend) {
        d->backend->setSuspended(false);
    }
}

void Connection::close()
{
    if (d->backend) {
        d->backend->disconnect(this);
        d->backend->deleteLater();
        d->backend = nullptr;
    }
    d->outgoingTasks.clear();
    d->incomingTasks.clear();
}

bool Connection::isConnected() const
{
    return d->backend && d->backend->state == ConnectionBackend::Connected;
}

bool Connection::inited() const
{
    return d->backend;
}

bool Connection::suspended() const
{
    return d->suspended;
}

void Connection::connectToRemote(const QUrl &address)
{
    // qDebug() << "Connection requested to" << address;
    const QString scheme = address.scheme();

    if (scheme == QLatin1String("local")) {
        d->setBackend(new ConnectionBackend(this));
    } else {
        qCWarning(KIO_CORE) << "Unknown protocol requested:" << scheme << "(" << address << ")";
        Q_ASSERT(0);
        return;
    }

    // connection succeeded
    if (!d->backend->connectToRemote(address)) {
        // qCWarning(KIO_CORE) << "could not connect to" << address << "using scheme" << scheme;
        delete d->backend;
        d->backend = nullptr;
        return;
    }

    d->dequeue();
}

bool Connection::send(int cmd, const QByteArray &data)
{
    // Remember that a Connection instance exists in the Application and the Worker. If the application terminates
    // we potentially get disconnected while looping on data to send in the worker, terminate the worker when this
    // happens. Specifically while reading a possible answer from the Application we may get socketDisconnected()
    // we'll never get an answer in that case.
    if (m_type == Type::Worker && !inited()) {
        qCWarning(KIO_CORE) << "Connection::send() called with connection not inited";
        return false;
    }
    if (!inited() || !d->outgoingTasks.isEmpty()) {
        Task task;
        task.cmd = cmd;
        task.data = data;
        d->outgoingTasks.append(std::move(task));
        return true;
    } else {
        return sendnow(cmd, data);
    }
}

bool Connection::sendnow(int cmd, const QByteArray &data)
{
    if (!d->backend) {
        qCWarning(KIO_CORE) << "Connection::sendnow has no backend";
        return false;
    }

    if (data.size() > 0xffffff) {
        qCWarning(KIO_CORE) << "Connection::sendnow too much data";
        return false;
    }

    if (!isConnected()) {
        qCWarning(KIO_CORE) << "Connection::sendnow not connected";
        return false;
    }

    // qDebug() << this << "Sending command" << cmd << "of size" << data.size();
    return d->backend->sendCommand(cmd, data);
}

bool Connection::hasTaskAvailable() const
{
    return !d->incomingTasks.isEmpty();
}

bool Connection::waitForIncomingTask(int ms)
{
    if (!isConnected()) {
        return false;
    }

    if (d->backend) {
        return d->backend->waitForIncomingTask(ms);
    }
    return false;
}

int Connection::read(int *_cmd, QByteArray &data)
{
    // if it's still empty, then it's an error
    if (d->incomingTasks.isEmpty()) {
        // qCWarning(KIO_CORE) << this << "Task list is empty!";
        return -1;
    }
    const Task &task = d->incomingTasks.constFirst();
    // qDebug() << this << "Command" << task.cmd << "removed from the queue (size" << task.data.size() << ")";
    *_cmd = task.cmd;
    data = task.data;

    d->incomingTasks.removeFirst();

    // if we didn't empty our reading queue, emit again
    if (!d->suspended && !d->incomingTasks.isEmpty() && d->readMode == Connection::ReadMode::EventDriven) {
        auto dequeueFunc = [this]() {
            d->dequeue();
        };
        QMetaObject::invokeMethod(this, dequeueFunc, Qt::QueuedConnection);
    }

    return data.size();
}

void Connection::setReadMode(ReadMode readMode)
{
    d->readMode = readMode;
}

#include "moc_connection_p.cpp"
