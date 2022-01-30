/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kiocoredebug.h"
#include "slavebase.h"
#include "workerfactory.h"
#include "workerthread_p.h"

namespace KIO
{

WorkerThread::WorkerThread(WorkerFactory *factory, const QByteArray &appSocket)
    : m_factory(factory)
    , m_appSocket(appSocket)
{
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
    std::unique_ptr<KIO::SlaveBase> worker = m_factory->createWorker(QByteArray(), m_appSocket);
    worker->setRunInThread(true);
    setWorker(worker.get());

    worker->dispatchLoop();

    setWorker(nullptr); // before the actual deletion
}

void WorkerThread::setWorker(SlaveBase *worker)
{
    QMutexLocker locker(&m_workerMutex);
    m_worker = worker;
}

} // namespace KIO
