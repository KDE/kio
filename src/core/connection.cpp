/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "connection_p.h"
#include <QDebug>
#include "connectionbackend_p.h"
#include "kiocoredebug.h"

#include <errno.h>


using namespace KIO;

void ConnectionPrivate::dequeue()
{
    if (!backend || suspended) {
        return;
    }

    for (const Task &task : qAsConst(outgoingTasks)) {
        q->sendnow(task.cmd, task.data);
    }
    outgoingTasks.clear();

    if (!incomingTasks.isEmpty()) {
        Q_EMIT q->readyRead();
    }
}

void ConnectionPrivate::commandReceived(const Task &task)
{
    //qDebug() << this << "Command " << task.cmd << " added to the queue";
    if (!suspended && incomingTasks.isEmpty() && readMode == Connection::ReadMode::EventDriven) {
        QMetaObject::invokeMethod(q, "dequeue", Qt::QueuedConnection);
    }
    incomingTasks.append(task);
}

void ConnectionPrivate::disconnected()
{
    q->close();
    if (readMode == Connection::ReadMode::EventDriven) {
        QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
    }
}

void ConnectionPrivate::setBackend(ConnectionBackend *b)
{
    delete backend;
    backend = b;
    if (backend) {
        q->connect(backend, &ConnectionBackend::commandReceived, q, [this](const Task &task) { commandReceived(task); });
        q->connect(backend, &ConnectionBackend::disconnected, q, [this]() { disconnected(); });
        backend->setSuspended(suspended);
    }
}

Connection::Connection(QObject *parent)
    : QObject(parent), d(new ConnectionPrivate)
{
    d->q = this;
}

Connection::~Connection()
{
    close();
    delete d;
}

void Connection::suspend()
{
    //qDebug() << this << "Suspended";
    d->suspended = true;
    if (d->backend) {
        d->backend->setSuspended(true);
    }
}

void Connection::resume()
{
    // send any outgoing or incoming commands that may be in queue
    if (d->readMode == Connection::ReadMode::EventDriven) {
        QMetaObject::invokeMethod(this, "dequeue", Qt::QueuedConnection);
    }

    //qDebug() << this << "Resumed";
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
    //qDebug() << "Connection requested to " << address;
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
        //kWarning(7017) << "could not connect to" << address << "using scheme" << scheme ;
        delete d->backend;
        d->backend = nullptr;
        return;
    }

    d->dequeue();
}

QString Connection::errorString() const
{
    if (d->backend) {
        return d->backend->errorString;
    }
    return QString();
}

bool Connection::send(int cmd, const QByteArray &data)
{
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
    if (!d->backend || data.size() > 0xffffff || !isConnected()) {
        return false;
    }

    //qDebug() << this << "Sending command " << _cmd << " of size " << data.size();
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
        //kWarning() << this << "Task list is empty!";
        return -1;
    }
    const Task& task = d->incomingTasks.constFirst();
    //qDebug() << this << "Command " << task.cmd << " removed from the queue (size "
    //         << task.data.size() << ")";
    *_cmd = task.cmd;
    data = task.data;

    d->incomingTasks.removeFirst();

    // if we didn't empty our reading queue, emit again
    if (!d->suspended && !d->incomingTasks.isEmpty() && d->readMode == Connection::ReadMode::EventDriven) {
        QMetaObject::invokeMethod(this, "dequeue", Qt::QueuedConnection);
    }

    return data.size();
}

void Connection::setReadMode(ReadMode readMode)
{
    d->readMode = readMode;
}

#include "moc_connection_p.cpp"
