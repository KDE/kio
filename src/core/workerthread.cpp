/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kiocoredebug.h"
#include "slavebase.h"
#include "workerbase.h"
#include "workerbase_p.h"
#include "workerfactory.h"
#include "workerthread_p.h"

namespace KIO
{

WorkerThread::WorkerThread(QObject *parent, WorkerFactory *factory, const QByteArray &appSocket)
    : QThread(parent)
    , m_factory(factory)
    , m_appSocket(appSocket)
{
}

WorkerThread::~WorkerThread()
{
    wait();
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

    auto worker = m_factory->createWorker({}, m_appSocket);
    SlaveBase *base = &(worker->d->bridge);

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
