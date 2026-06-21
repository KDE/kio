/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_WORKERTHREAD_H
#define KIO_WORKERTHREAD_H

#include <QMutex>
#include <QThread>

class QPluginLoader;

namespace KIO
{

class SlaveBase;
class ThreadConnectionBackend;
class WorkerFactory;
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(QObject *parent, WorkerFactory *factory, std::unique_ptr<ThreadConnectionBackend> workerBackend, QPluginLoader *pluginLoader = nullptr);
    ~WorkerThread() override;

    void abort();

protected:
    void run() override;

private:
    void setWorker(KIO::SlaveBase *worker);

    WorkerFactory *m_factory; // set by constructor, no mutex needed
    std::unique_ptr<ThreadConnectionBackend> m_workerBackend; // owned until adopted by the worker in run()
    QPluginLoader *m_pluginLoader; // set by constructor, owned, unloaded in destructor

    QMutex m_workerMutex; // protects m_worker, accessed by both threads
    KIO::SlaveBase *m_worker = nullptr;
};

} // namespace KIO

#endif // KIO_WORKERTHREAD_H
