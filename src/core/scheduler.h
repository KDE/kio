// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _kio_scheduler_h
#define _kio_scheduler_h

#include "simplejob.h"
#include <QMap>
#include <QTimer>

namespace KIO
{
class Slave;

class SchedulerPrivate;
/**
 * @class KIO::Scheduler scheduler.h <KIO/Scheduler>
 *
 * The KIO::Scheduler manages KIO workers for the application.
 * It also queues jobs and assigns the job to a worker when one
 * becomes available.
 *
 * There are 3 possible ways for a job to get a worker:
 *
 * <h3>1. Direct</h3>
 * This is the default. When you create a job the
 * KIO::Scheduler will be notified and will find either an existing
 * worker that is idle or it will create a new worker for the job.
 *
 * Example:
 * \code
 *    TransferJob *job = KIO::get(QUrl("https://www.kde.org"));
 * \endcode
 *
 *
 * <h3>2. Scheduled</h3>
 * If you create a lot of jobs, you might want not want to have a
 * worker for each job. If you schedule a job, a maximum number
 * of workers will be created. When more jobs arrive, they will be
 * queued. When a worker is finished with a job, it will be assigned
 * a job from the queue.
 *
 * Example:
 * \code
 *    TransferJob *job = KIO::get(QUrl("https://www.kde.org"));
 *    KIO::Scheduler::setJobPriority(job, 1);
 * \endcode
 *
 * <h3>3. Connection Oriented (TODO KF6 remove this section)</h3>
 * For some operations it is important that multiple jobs use
 * the same connection. This can only be ensured if all these jobs
 * use the same slave.
 *
 * You can ask the scheduler to open a slave for connection oriented
 * operations. You can then use the scheduler to assign jobs to this
 * slave. The jobs will be queued and the slave will handle these jobs
 * one after the other.
 *
 * Example:
 * \code
 *    Slave *slave = KIO::Scheduler::getConnectedSlave(
 *            QUrl("pop3://bastian:password@mail.kde.org"));
 *    TransferJob *job1 = KIO::get(
 *            QUrl("pop3://bastian:password@mail.kde.org/msg1"));
 *    KIO::Scheduler::assignJobToSlave(slave, job1);
 *    TransferJob *job2 = KIO::get(
 *            QUrl("pop3://bastian:password@mail.kde.org/msg2"));
 *    KIO::Scheduler::assignJobToSlave(slave, job2);
 *    TransferJob *job3 = KIO::get(
 *            QUrl("pop3://bastian:password@mail.kde.org/msg3"));
 *    KIO::Scheduler::assignJobToSlave(slave, job3);
 *
 *    // ... Wait for jobs to finish...
 *
 *    KIO::Scheduler::disconnectSlave(slave);
 * \endcode
 *
 * Note that you need to explicitly disconnect the slave when the
 * connection goes down, so your error handler should contain:
 * \code
 *    if (error == KIO::ERR_CONNECTION_BROKEN)
 *        KIO::Scheduler::disconnectSlave(slave);
 * \endcode
 *
 * @see KIO::Slave
 * @see KIO::Job
 **/

class KIOCORE_EXPORT Scheduler : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KIO.Scheduler")
public:
    /**
     * Register @p job with the scheduler.
     * The default is to create a new worker for the job if no worker
     * is available. This can be changed by calling setJobPriority.
     * @param job the job to register
     */
    static void doJob(SimpleJob *job);

    /**
     * Stop the execution of a job.
     * @param job the job to cancel
     */
    static void cancelJob(SimpleJob *job);

    /**
     * Called when a job is done.
     * @param job the finished job
     * @param slave the slave that executed the @p job
     */
    static void jobFinished(KIO::SimpleJob *job, KIO::Slave *slave);

    /**
     * Puts a worker on notice. A next job may reuse this worker if it
     * requests the same URL.
     *
     * A job can be put on hold after it has emit'ed its mimetype() signal.
     * Based on the MIME type, the program can give control to another
     * component in the same process which can then resume the job
     * by simply asking for the same URL again.
     * @param job the job that should be stopped
     * @param url the URL that is handled by the @p url
     *
     * @since 5.101
     */
    static void putWorkerOnHold(KIO::SimpleJob *job, const QUrl &url);

    /**
     * Removes any worker that might have been put on hold. If a worker
     * was put on hold it will be killed.
     *
     * @since 5.101
     */
    static void removeWorkerOnHold();

    static void emitReparseSlaveConfiguration();
    // KF6 TODO: rename to emitReparseWorkerConfiguration. See also T15956.

    /**
     * Returns true if there is a worker on hold for @p url.
     *
     * @since 5.101
     */
    static bool isWorkerOnHoldFor(const QUrl &url);

    /**
     * Updates the internal metadata from job.
     *
     * @since 4.6.5
     */
    static void updateInternalMetaData(SimpleJob *job);

Q_SIGNALS:

    // DBUS
    Q_SCRIPTABLE void reparseSlaveConfiguration(const QString &);
    // KF6 TODO: rename to reparseWorkerConfiguration. See also T15956.

private:
    Q_DISABLE_COPY(Scheduler)
    Scheduler();
    ~Scheduler() override;

    static Scheduler *self();

    friend class AccessManager;
    // For internal use, since 5.90
    static void setSimpleJobPriority(SimpleJob *job, int priority);

    // connected to D-Bus signal:
#ifndef KIO_ANDROID_STUB
    Q_PRIVATE_SLOT(d_func(), void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &))
#endif

private:
    friend class SchedulerPrivate;
    SchedulerPrivate *d_func();
};

}
#endif
