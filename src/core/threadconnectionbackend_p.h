/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_THREADCONNECTIONBACKEND_P_H
#define KIO_THREADCONNECTIONBACKEND_P_H

#include "connectionbackend_p.h"

#include <QList>
#include <QMutex>
#include <QWaitCondition>

#include <atomic>
#include <memory>
#include <utility>

namespace KIO
{
/*!
 * \internal
 *
 * Transport between the application thread and an in-process WorkerThread that
 * does not use a socket: Tasks are handed to the peer directly through a shared,
 * mutex-protected channel. The application side (which runs an event loop) is
 * woken with a queued invocation. The worker side (which blocks in
 * dispatchLoop()/waitForIncomingTask() without an event loop) is woken with a
 * wait condition. Each direction is a bounded queue: a producer blocks once the
 * consumer is HighWaterMark tasks behind, so a fast producer cannot outrun a
 * joined worker thread.
 *
 * sendCommand() does NOT copy the payload: the Task keeps a (shared) reference to
 * the caller's QByteArray and hands it to the peer asynchronously, so callers must
 * pass an OWNED QByteArray whose data outlives the task. In-process workers
 * (kio_file, kio_admin) honour this.
 */
class ThreadConnectionBackend : public ConnectionBackend
{
    Q_OBJECT

public:
    enum class Role {
        Application, // consumer runs a Qt event loop (event-driven)
        Worker, // consumer blocks in waitForIncomingTask() (polled, no event loop)
    };

    explicit ThreadConnectionBackend(Role role, QObject *parent = nullptr);
    ~ThreadConnectionBackend() override;

    /*!
     * Creates a cross-linked application/worker backend pair sharing one channel, each owned by its
     * returned unique_ptr. The application end is handed to the application's Connection, the worker
     * end is moved to the worker thread.
     */
    static std::pair<std::unique_ptr<ThreadConnectionBackend>, std::unique_ptr<ThreadConnectionBackend>> createPair();

    void setSuspended(bool suspended) override;
    void closeSocket() override;
    bool waitForIncomingTask(int ms) override;
    bool sendCommand(int command, const QByteArray &data) override;

private Q_SLOTS:
    /// Emit commandReceived() for every queued task (drives the event-loop side).
    void drainIncoming();
    /// Emit disconnected() after the peer closed its end (event-loop side).
    void handlePeerClosed();

private:
    static const int HighWaterMark = 32; // max queued tasks before a producer blocks

    // One direction of the channel: a bounded queue plus its wait conditions.
    struct Direction {
        QList<Task> queue;
        QWaitCondition dataAvailable; // wakes a blocked consumer
        QWaitCondition spaceAvailable; // wakes a back-pressured producer
        bool closed = false;
    };

    // Shared by both backends. Outlives whichever is torn down first.
    struct Channel {
        QMutex mutex;
        Direction toApplication; // worker -> application
        Direction toWorker; // application -> worker
        // Only the application end is tracked: it is the one woken by a posted invocation, so it must
        // stay reachable until torn down. The worker end is woken by a wait condition, so the channel
        // never needs to reach it. Nulled under the mutex on destruction.
        ThreadConnectionBackend *appBackend = nullptr;
        // Coalesces worker -> application wakeups: at most one drainIncoming() is queued at a
        // time, so a burst of sends costs one cross-thread event instead of one per message.
        std::atomic<bool> appDrainScheduled{false};
    };

    Direction &incoming(); // the direction this backend reads
    Direction &outgoing(); // the direction this backend writes
    void emitQueued(Direction &dir); // pop+emit commandReceived while not suspended
    void closeAllDirectionsLocked(); // mark both directions closed and wake all waiters (mutex held)

    Role m_role;
    std::shared_ptr<Channel> m_channel;
    bool m_suspended = false;
};
}

#endif
