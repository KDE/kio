/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_WORKERTHREAD_H
#define KIO_WORKERTHREAD_H

#include <QMutex>
#include <QThread>

#ifdef Q_OS_UNIX
#include <pthread.h>
#endif

#ifdef BUILD_TESTING
#include "kiocore_export.h"
#include <QSemaphore>
#endif

class QPluginLoader;

namespace KIO
{

class SlaveBase;
class WorkerFactory;
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(QObject *parent, WorkerFactory *factory, const QByteArray &appSocket, QPluginLoader *pluginLoader = nullptr);
    ~WorkerThread() override;

    void abort();

#ifdef BUILD_TESTING
    // Test hooks to force the deterministic Worker::deref() deadlock (see
    // KIOThreadTest::cancelJobWhileWorkerIsBlocked). When the exit gate is enabled, a worker
    // thread blocks on its way out of run() until the main thread releases it through its
    // event loop - mirroring a worker that is mid-transfer and can only finish once the main
    // thread keeps draining its socket. A synchronous join in deref() never lets the main
    // thread reach its event loop to release the gate, so both threads wedge.
    KIOCORE_EXPORT static void setTestExitGateEnabled(bool enabled);
    KIOCORE_EXPORT static void releaseTestExitGate();
#endif

protected:
    void run() override;

private:
    void setWorker(KIO::SlaveBase *worker);

    WorkerFactory *m_factory; // set by constructor, no mutex needed
    QByteArray m_appSocket; // set by constructor, no mutex needed
    QPluginLoader *m_pluginLoader; // set by constructor, owned, unloaded in destructor

    QMutex m_workerMutex; // protects m_worker, accessed by both threads
    KIO::SlaveBase *m_worker = nullptr;
#ifdef Q_OS_UNIX
    pthread_t m_nativeHandle = {};
#endif
#ifdef BUILD_TESTING
    static bool s_testExitGateEnabled;
    static QSemaphore s_testExitGate;
#endif
};

} // namespace KIO

#endif // KIO_WORKERTHREAD_H
