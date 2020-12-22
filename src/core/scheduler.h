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
#include <QTimer>
#include <QMap>

namespace KIO
{

class Slave;
class SlaveConfig;

class SchedulerPrivate;
/**
 * @class KIO::Scheduler scheduler.h <KIO/Scheduler>
 *
 * The KIO::Scheduler manages io-slaves for the application.
 * It also queues jobs and assigns the job to a slave when one
 * becomes available.
 *
 * There are 3 possible ways for a job to get a slave:
 *
 * <h3>1. Direct</h3>
 * This is the default. When you create a job the
 * KIO::Scheduler will be notified and will find either an existing
 * slave that is idle or it will create a new slave for the job.
 *
 * Example:
 * \code
 *    TransferJob *job = KIO::get(QUrl("http://www.kde.org"));
 * \endcode
 *
 *
 * <h3>2. Scheduled</h3>
 * If you create a lot of jobs, you might want not want to have a
 * slave for each job. If you schedule a job, a maximum number
 * of slaves will be created. When more jobs arrive, they will be
 * queued. When a slave is finished with a job, it will be assigned
 * a job from the queue.
 *
 * Example:
 * \code
 *    TransferJob *job = KIO::get(QUrl("http://www.kde.org"));
 *    KIO::Scheduler::setJobPriority(job, 1);
 * \endcode
 *
 * <h3>3. Connection Oriented</h3>
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
     * The default is to create a new slave for the job if no slave
     * is available. This can be changed by calling setJobPriority.
     * @param job the job to register
     */
    static void doJob(SimpleJob *job);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 5)
    /**
     * Schedules @p job scheduled for later execution.
     * This method is deprecated and just sets the job's priority to 1. It is
     * recommended to replace calls to scheduleJob(job) with setJobPriority(job, 1).
     * @param job the job to schedule
     * @deprecated Since 4.5, use setJobPriority(SimpleJob *job, int priority)
     */
    KIOCORE_DEPRECATED_VERSION(4, 5, "Use Scheduler::setJobPriority(SimpleJob *, int )")
    static void scheduleJob(SimpleJob *job);
#endif

    /**
     * Changes the priority of @p job; jobs of the same priority run in the order in which
     * they were created. Jobs of lower numeric priority always run before any
     * waiting jobs of higher numeric priority. The range of priority is -10 to 10,
     * the default priority of jobs is 0.
     * @param job the job to change
     * @param priority new priority of @p job, lower runs earlier
     */
    static void setJobPriority(SimpleJob *job, int priority);

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
     * Puts a slave on notice. A next job may reuse this slave if it
     * requests the same URL.
     *
     * A job can be put on hold after it has emit'ed its mimetype() signal.
     * Based on the MIME type, the program can give control to another
     * component in the same process which can then resume the job
     * by simply asking for the same URL again.
     * @param job the job that should be stopped
     * @param url the URL that is handled by the @p url
     */
    static void putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url);

    /**
     * Removes any slave that might have been put on hold. If a slave
     * was put on hold it will be killed.
     */
    static void removeSlaveOnHold();

    /**
     * Send the slave that was put on hold back to KLauncher. This
     * allows another process to take over the slave and resume the job
     * that was started.
     */
    static void publishSlaveOnHold();

    /**
     * Requests a slave for use in connection-oriented mode.
     *
     * @param url This defines the username,password,host & port to
     *            connect with.
     * @param config Configuration data for the slave.
     *
     * @return A pointer to a connected slave or @c nullptr if an error occurred.
     * @see assignJobToSlave()
     * @see disconnectSlave()
     */
    static KIO::Slave *getConnectedSlave(const QUrl &url,
                                         const KIO::MetaData &config = MetaData());

    /**
     * Uses @p slave to do @p job.
     * This function should be called immediately after creating a Job.
     *
     * @param slave The slave to use. The slave must have been obtained
     *              with a call to getConnectedSlave and must not
     *              be currently assigned to any other job.
     * @param job The job to do.
     *
     * @return true is successful, false otherwise.
     *
     * @see getConnectedSlave()
     * @see disconnectSlave()
     * @see slaveConnected()
     * @see slaveError()
     */
    static bool assignJobToSlave(KIO::Slave *slave, KIO::SimpleJob *job);

    /**
     * Disconnects @p slave.
     *
     * @param slave The slave to disconnect. The slave must have been
     *              obtained with a call to getConnectedSlave
     *              and must not be assigned to any job.
     *
     * @return true is successful, false otherwise.
     *
     * @see getConnectedSlave
     * @see assignJobToSlave
     */
    static bool disconnectSlave(KIO::Slave *slave);

    /**
     * Function to connect signals emitted by the scheduler.
     *
     * @see slaveConnected()
     * @see slaveError()
     */
    // KDE5: those methods should probably be removed, ugly and only marginally useful
    static bool connect(const char *signal, const QObject *receiver,
                        const char *member);

    static bool connect(const QObject *sender, const char *signal,
                        const QObject *receiver, const char *member);

    static bool disconnect(const QObject *sender, const char *signal,
                           const QObject *receiver, const char *member);

    bool connect(const QObject *sender, const char *signal,
                 const char *member);

    /**
     * When true, the next job will check whether KLauncher has a slave
     * on hold that is suitable for the job.
     * @param b true when KLauncher has a job on hold
     */
    static void checkSlaveOnHold(bool b);

    static void emitReparseSlaveConfiguration();

    /**
     * Returns true if there is a slave on hold for @p url.
     *
     * @since 4.7
     */
    static bool isSlaveOnHoldFor(const QUrl &url);

    /**
     * Updates the internal metadata from job.
     *
     * @since 4.6.5
     */
    static void updateInternalMetaData(SimpleJob *job);

Q_SIGNALS:
    void slaveConnected(KIO::Slave *slave);
    void slaveError(KIO::Slave *slave, int error, const QString &errorMsg);

    // DBUS
    Q_SCRIPTABLE void reparseSlaveConfiguration(const QString &);
    Q_SCRIPTABLE void slaveOnHoldListChanged();

private:
    Q_DISABLE_COPY(Scheduler)
    Scheduler();
    ~Scheduler();

    static Scheduler *self();

    Q_PRIVATE_SLOT(d_func(), void slotSlaveDied(KIO::Slave *slave))
    Q_PRIVATE_SLOT(d_func(), void slotSlaveStatus(qint64 pid, const QByteArray &protocol,
                   const QString &host, bool connected))

    // connected to D-Bus signal:
    Q_PRIVATE_SLOT(d_func(), void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &))
    Q_PRIVATE_SLOT(d_func(), void slotSlaveOnHoldListChanged())

    Q_PRIVATE_SLOT(d_func(), void slotSlaveConnected())
    Q_PRIVATE_SLOT(d_func(), void slotSlaveError(int error, const QString &errorMsg))
private:
    friend class SchedulerPrivate;
    SchedulerPrivate *d_func();
};

}
#endif
