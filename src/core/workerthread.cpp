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

std::variant<std::unique_ptr<KIO::SlaveBase>, std::unique_ptr<KIO::WorkerBase>> makeWorker(const QByteArray &appSocket, WorkerFactory *factory)
{
    if (auto workerFactory = qobject_cast<RealWorkerFactory *>(factory)) {
        return workerFactory->createRealWorker({}, appSocket);
    }
    return factory->createWorker({}, appSocket);
}

void WorkerThread::run()
{
    qCDebug(KIO_CORE) << QThread::currentThreadId() << "Creating threaded worker";

    auto slaveOrWorker = makeWorker(m_appSocket, m_factory);
    SlaveBase *base = nullptr;
    if (std::holds_alternative<std::unique_ptr<KIO::WorkerBase>>(slaveOrWorker)) {
        auto &worker = std::get<std::unique_ptr<KIO::WorkerBase>>(slaveOrWorker);
        base = &(worker->d->bridge);
    } else {
        Q_ASSERT(std::holds_alternative<std::unique_ptr<KIO::SlaveBase>>(slaveOrWorker));
        base = std::get<std::unique_ptr<KIO::SlaveBase>>(slaveOrWorker).get();
    }

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
