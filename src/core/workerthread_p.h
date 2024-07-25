/*
    SPDX-FileCopyrightText: 2022 David Faure <faure@kde.org>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_WORKERTHREAD_H
#define KIO_WORKERTHREAD_H

#include <QMutex>
#include <QThread>

namespace KIO
{

class WorkerBase;
class WorkerFactory;
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(QObject *parent, WorkerFactory *factory, const QByteArray &appSocket);
    ~WorkerThread() override;

    void abort();

protected:
    void run() override;

private:
    void setWorker(KIO::WorkerBase *worker);

    WorkerFactory *m_factory; // set by constructor, no mutex needed
    QByteArray m_appSocket; // set by constructor, no mutex needed

    QMutex m_workerMutex; // protects m_worker, accessed by both threads
    KIO::WorkerBase *m_worker = nullptr;
};

} // namespace KIO

#endif // KIO_WORKERTHREAD_H
