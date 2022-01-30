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

class SlaveBase;
class WorkerFactory;
class WorkerThread : public QThread
{
    Q_OBJECT
public:
    WorkerThread(WorkerFactory *factory, const QByteArray &appSocket);

    void abort();

protected:
    void run() override;

private:
    void setWorker(KIO::SlaveBase *worker);

    WorkerFactory *m_factory; // set by constructor, no mutex needed
    QByteArray m_appSocket; // set by constructor, no mutex needed

    QMutex m_workerMutex; // protects m_worker, accessed by both threads
    KIO::SlaveBase *m_worker = nullptr;
};

} // namespace KIO

#endif // KIO_WORKERTHREAD_H
