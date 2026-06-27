/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "threadconnectionbackend_p.h"

#include "kiocoredebug.h"

using namespace KIO;

ThreadConnectionBackend::ThreadConnectionBackend(Role role, QObject *parent)
    : ConnectionBackend(parent)
    , m_role(role)
{
}

ThreadConnectionBackend::~ThreadConnectionBackend()
{
    if (!m_channel) {
        return;
    }
    QMutexLocker lock(&m_channel->mutex);
    if (m_role == Role::Application) {
        m_channel->appBackend = nullptr;
    }
    closeAllDirectionsLocked();
}

// Mark both directions closed and wake everyone so neither peer keeps blocking on us. The caller
// must hold m_channel->mutex.
void ThreadConnectionBackend::closeAllDirectionsLocked()
{
    for (Direction *dir : {&m_channel->toApplication, &m_channel->toWorker}) {
        dir->closed = true;
        dir->dataAvailable.wakeAll();
        dir->spaceAvailable.wakeAll();
    }
}

std::pair<std::unique_ptr<ThreadConnectionBackend>, std::unique_ptr<ThreadConnectionBackend>> ThreadConnectionBackend::createPair()
{
    auto channel = std::make_shared<Channel>();
    auto app = std::make_unique<ThreadConnectionBackend>(Role::Application, nullptr);
    auto worker = std::make_unique<ThreadConnectionBackend>(Role::Worker, nullptr); // parentless: moved to the worker thread
    app->m_channel = channel;
    worker->m_channel = channel;

    QMutexLocker lock(&channel->mutex);
    channel->appBackend = app.get();
    app->state = Connected;
    worker->state = Connected;
    return {std::move(app), std::move(worker)};
}

ThreadConnectionBackend::Direction &ThreadConnectionBackend::incoming()
{
    return m_role == Role::Application ? m_channel->toApplication : m_channel->toWorker;
}

ThreadConnectionBackend::Direction &ThreadConnectionBackend::outgoing()
{
    return m_role == Role::Application ? m_channel->toWorker : m_channel->toApplication;
}

void ThreadConnectionBackend::emitQueued(Direction &dir)
{
    // Drain whole tasks one at a time, emitting outside the lock so the consumer
    // can re-enter (e.g. read() then wait again) without self-deadlocking.
    while (true) {
        Task task;
        {
            QMutexLocker lock(&m_channel->mutex);
            if (m_suspended || dir.queue.isEmpty()) {
                return;
            }
            task = dir.queue.takeFirst();
            dir.spaceAvailable.wakeOne(); // freed a slot for the single back-pressured producer
        }
        Q_EMIT commandReceived(task);
    }
}

void ThreadConnectionBackend::drainIncoming()
{
    if (m_channel) {
        // Clear before draining so a send racing in during the drain re-arms the wakeup.
        m_channel->appDrainScheduled.store(false);
        emitQueued(incoming());
    }
}

bool ThreadConnectionBackend::sendCommand(int cmd, const QByteArray &data)
{
    if (!m_channel) {
        return false;
    }

    QMutexLocker lock(&m_channel->mutex);
    Direction &out = outgoing();
    // Bounded queue with back-pressure, but only worker -> application (the high-volume
    // direction): block while the application is HighWaterMark tasks behind. The
    // application -> worker direction is left unbounded on purpose - the application
    // produces few commands and must never block in send(), otherwise it would stop
    // draining the worker's data and the two could deadlock against each other.
    if (m_role == Role::Worker) {
        while (out.queue.size() >= HighWaterMark && !out.closed) {
            out.spaceAvailable.wait(&m_channel->mutex);
        }
    }
    if (out.closed) {
        return false;
    }
    // Share the payload (no copy): in-process workers must hand us an owned QByteArray,
    // since we deliver it to the application asynchronously (after the worker has moved
    // on). Its refcount keeps the bytes alive until the application has consumed them.
    out.queue.append(Task{.cmd = cmd, .data = data});
    out.dataAvailable.wakeOne(); // one consumer per direction

    // Post the wakeup to the event-loop consumer while still holding the mutex: the application
    // backend's destructor clears appBackend under this same mutex, so it cannot be freed between
    // this read and the post (and Qt drops the queued call if it is destroyed afterwards). Posting
    // outside the lock would race delete this -> ~ThreadConnectionBackend and target freed memory.
    // Coalesce a burst of sends into a single drainIncoming().
    if (m_role == Role::Worker && m_channel->appBackend && !m_channel->appDrainScheduled.exchange(true)) {
        QMetaObject::invokeMethod(m_channel->appBackend, &ThreadConnectionBackend::drainIncoming, Qt::QueuedConnection);
    }
    return true;
}

bool ThreadConnectionBackend::waitForIncomingTask(int ms)
{
    if (!m_channel) {
        return false;
    }
    {
        QMutexLocker lock(&m_channel->mutex);
        Direction &in = incoming();
        if (in.queue.isEmpty() && !in.closed) {
            if (ms < 0) {
                in.dataAvailable.wait(&m_channel->mutex);
            } else {
                in.dataAvailable.wait(&m_channel->mutex, static_cast<unsigned long>(ms));
            }
        }
        if (in.queue.isEmpty()) {
            if (in.closed) {
                state = Idle; // let dispatchLoop() see the disconnection and exit
            }
            return false;
        }
    }
    emitQueued(incoming());
    return true;
}

void ThreadConnectionBackend::closeSocket()
{
    if (!m_channel) {
        return;
    }
    QMutexLocker lock(&m_channel->mutex);
    closeAllDirectionsLocked();
    state = Idle;
    // Post under the mutex, for the same reason as sendCommand(): the application backend's
    // destructor clears appBackend under this mutex, so it cannot be freed between the read and
    // the post.
    if (m_role == Role::Worker && m_channel->appBackend) {
        QMetaObject::invokeMethod(m_channel->appBackend, &ThreadConnectionBackend::handlePeerClosed, Qt::QueuedConnection);
    }
}

void ThreadConnectionBackend::handlePeerClosed()
{
    if (state != Idle) {
        state = Idle;
        Q_EMIT disconnected();
    }
}

void ThreadConnectionBackend::setSuspended(bool suspended)
{
    if (!m_channel) {
        return;
    }
    {
        QMutexLocker lock(&m_channel->mutex);
        m_suspended = suspended;
    }
    if (!suspended) {
        emitQueued(incoming()); // flush whatever queued up while suspended
    }
}

#include "moc_threadconnectionbackend_p.cpp"
