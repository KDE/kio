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
};

} // namespace KIO

#endif // KIO_WORKERTHREAD_H
