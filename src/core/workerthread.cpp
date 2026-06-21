/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kiocoredebug.h"
#include "slavebase.h"
#include "threadconnectionbackend_p.h"
#include "workerbase.h"
#include "workerbase_p.h"
#include "workerfactory.h"
#include "workerthread_p.h"

#include <QPluginLoader>

namespace KIO
{

WorkerThread::WorkerThread(QObject *parent, WorkerFactory *factory, std::unique_ptr<ThreadConnectionBackend> workerBackend, QPluginLoader *pluginLoader)
    : QThread(parent)
    , m_factory(factory)
    , m_workerBackend(std::move(workerBackend))
    , m_pluginLoader(pluginLoader)
{
    // Give the backend this thread's affinity before it starts, so its signals are
    // delivered to the worker-side Connection on the worker thread.
    m_workerBackend->moveToThread(this);
}

WorkerThread::~WorkerThread()
{
    wait();
    if (m_pluginLoader) {
        m_pluginLoader->unload();
        delete m_pluginLoader;
    }
}

void WorkerThread::abort()
{
    QMutexLocker locker(&m_workerMutex);
    if (m_worker) { // not deleted yet
        m_worker->exit();
    }
}

void WorkerThread::run()
{
    qCDebug(KIO_CORE) << QThread::currentThreadId() << "Creating threaded worker";

    auto worker = m_factory->createWorker({}, {});
    SlaveBase *base = &(worker->d->bridge);

    // Adopt the pre-paired thread backend instead of connecting to a socket address.
    // Ownership passes to the worker-side Connection (which parents it).
    base->setConnectionBackend(std::move(m_workerBackend));

    base->setRunInThread(true);
    setWorker(base);

    base->dispatchLoop();

    setWorker(nullptr); // before the actual deletion
}

void WorkerThread::setWorker(SlaveBase *worker)
{
    QMutexLocker locker(&m_workerMutex);
    m_worker = worker;
}

} // namespace KIO

#include "moc_workerthread_p.cpp"
