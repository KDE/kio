/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>
    Copyright (C) 2007 Thiago Macieira <thiago@kde.org>

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

#include "connection_p.h"
#include <QDebug>
#include "socketconnectionbackend_p.h"

#include <errno.h>

#include <QQueue>
#include <QCoreApplication>

using namespace KIO;


void ConnectionPrivate::dequeue()
{
    if (!backend || suspended)
        return;

    while (!outgoingTasks.isEmpty()) {
       const Task task = outgoingTasks.dequeue();
       q->sendnow(task.cmd, task.data);
    }

    if (!incomingTasks.isEmpty())
        emit q->readyRead();
}

void ConnectionPrivate::commandReceived(const Task &task)
{
    //qDebug() << this << "Command " << task.cmd << " added to the queue";
    if (!suspended && incomingTasks.isEmpty())
        QMetaObject::invokeMethod(q, "dequeue", Qt::QueuedConnection);
    incomingTasks.enqueue(task);
}

void ConnectionPrivate::disconnected()
{
    q->close();
    QMetaObject::invokeMethod(q, "readyRead", Qt::QueuedConnection);
}

void ConnectionPrivate::setBackend(AbstractConnectionBackend *b)
{
    backend = b;
    if (backend) {
        q->connect(backend, SIGNAL(commandReceived(Task)), SLOT(commandReceived(Task)));
        q->connect(backend, SIGNAL(disconnected()), SLOT(disconnected()));
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
    if (d->backend)
        d->backend->setSuspended(true);
}

void Connection::resume()
{
    // send any outgoing or incoming commands that may be in queue
    QMetaObject::invokeMethod(this, "dequeue", Qt::QueuedConnection);

    //qDebug() << this << "Resumed";
    d->suspended = false;
    if (d->backend)
        d->backend->setSuspended(false);
}

void Connection::close()
{
    if (d->backend) {
        d->backend->disconnect(this);
        d->backend->deleteLater();
        d->backend = 0;
    }
    d->outgoingTasks.clear();
    d->incomingTasks.clear();
}

bool Connection::isConnected() const
{
    return d->backend && d->backend->state == AbstractConnectionBackend::Connected;
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
        d->setBackend(new SocketConnectionBackend(SocketConnectionBackend::LocalSocketMode, this));
    } else if (scheme == QLatin1String("tcp")) {
        d->setBackend(new SocketConnectionBackend(SocketConnectionBackend::TcpSocketMode, this));
    } else {
        qWarning() << "Unknown protocol requested:" << scheme << "(" << address << ")";
        Q_ASSERT(0);
        return;
    }

    // connection succeeded
    if (!d->backend->connectToRemote(address)) {
        //kWarning(7017) << "could not connect to" << address << "using scheme" << scheme ;
        delete d->backend;
        d->backend = 0;
        return;
    }

    d->dequeue();
}

QString Connection::errorString() const
{
    if (d->backend)
        return d->backend->errorString;
    return QString();
}

bool Connection::send(int cmd, const QByteArray& data)
{
    if (!inited() || !d->outgoingTasks.isEmpty()) {
        Task task;
        task.cmd = cmd;
        task.data = data;
        d->outgoingTasks.enqueue(task);
        return true;
    } else {
        return sendnow(cmd, data);
    }
}

bool Connection::sendnow(int _cmd, const QByteArray &data)
{
    if (data.size() > 0xffffff)
        return false;

    if (!isConnected())
        return false;

    //qDebug() << this << "Sending command " << _cmd << " of size " << data.size();
    Task task;
    task.cmd = _cmd;
    task.data = data;
    return d->backend->sendCommand(task);
}

bool Connection::hasTaskAvailable() const
{
    return !d->incomingTasks.isEmpty();
}

bool Connection::waitForIncomingTask(int ms)
{
    if (!isConnected())
        return false;

    if (d->backend)
        return d->backend->waitForIncomingTask(ms);
    return false;
}

int Connection::read( int* _cmd, QByteArray &data )
{
    // if it's still empty, then it's an error
    if (d->incomingTasks.isEmpty()) {
        //kWarning() << this << "Task list is empty!";
        return -1;
    }
    const Task task = d->incomingTasks.dequeue();
    //qDebug() << this << "Command " << task.cmd << " removed from the queue (size "
    //         << task.data.size() << ")";
    *_cmd = task.cmd;
    data = task.data;

    // if we didn't empty our reading queue, emit again
    if (!d->suspended && !d->incomingTasks.isEmpty())
        QMetaObject::invokeMethod(this, "dequeue", Qt::QueuedConnection);

    return data.size();
}

#include "moc_connection_p.cpp"
