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
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 102)
class SlaveConfig;
#endif

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

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 90)
    /**
     * Changes the priority of @p job; jobs of the same priority run in the order in which
     * they were created. Jobs of lower numeric priority always run before any
     * waiting jobs of higher numeric priority. The range of priority is -10 to 10,
     * the default priority of jobs is 0.
     * @param job the job to change
     * @param priority new priority of @p job, lower runs earlier
     * @deprecated Since 5.90. Changing priorities was only used by KHTML. If you need this, please contact kde-frameworks-devel to request the feature back,
     * but as better API like Job::setPriority.
     */
    KIOCORE_DEPRECATED_VERSION(5,
                               90,
                               "Changing priorities was only used by KHTML. If you need this, please contact kde-frameworks-devel to request the feature back, "
                               "but as better API like Job::setPriority.")
    static void setJobPriority(SimpleJob *job, int priority);
#endif

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

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 101)
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
     *
     * @deprecated Since 5.101, use putWorkerOnHold(KIO::SimpleJob *, const QUrl &)
     */
    static KIOCORE_DEPRECATED_VERSION(5, 101, "Use putWorkerOnHold(KIO::SimpleJob *, const QUrl &)") void putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Removes any slave that might have been put on hold. If a slave
     * was put on hold it will be killed.
     *
     * @deprecated Since 5.101, use removeWorkerOnHold()
     */
    static KIOCORE_DEPRECATED_VERSION(5, 101, "Use removeWorkerOnHold()") void removeSlaveOnHold();
#endif

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

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 88)
    /**
     * Send the slave that was put on hold back to KLauncher. This
     * allows another process to take over the slave and resume the job
     * that was started.
     * @deprecated since 5.88, the feature of holding slaves between processes is gone, just remove the call
     */
    KIOCORE_DEPRECATED_VERSION(5, 88, "Remove this call. The feature is gone")
    static void publishSlaveOnHold();
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
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
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Port away from the connected slave feature, e.g. making a library out of the slave code.")
    static KIO::Slave *getConnectedSlave(const QUrl &url, const KIO::MetaData &config = MetaData());
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
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
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Port away from the connected slave feature, e.g. making a library out of the slave code.")
    static bool assignJobToSlave(KIO::Slave *slave, KIO::SimpleJob *job);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
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
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Port away from the connected slave feature, e.g. making a library out of the slave code.")
    static bool disconnectSlave(KIO::Slave *slave);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 103)
    /**
     * Function to connect signals emitted by the scheduler.
     *
     * @see slaveConnected()
     * @see slaveError()
     * @deprecated Since 5.103, due to no known users.
     */
    KIOCORE_DEPRECATED_VERSION(5, 103, "No known users")
    static bool connect(const char *signal, const QObject *receiver, const char *member);

    KIOCORE_DEPRECATED_VERSION(5, 103, "No known users")
    static bool connect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);

    KIOCORE_DEPRECATED_VERSION(5, 103, "No known users")
    static bool disconnect(const QObject *sender, const char *signal, const QObject *receiver, const char *member);

    KIOCORE_DEPRECATED_VERSION(5, 103, "No known users")
    bool connect(const QObject *sender, const char *signal, const char *member);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 88)
    /**
     * When true, the next job will check whether KLauncher has a slave
     * on hold that is suitable for the job.
     * @param b true when KLauncher has a job on hold
     * @deprecated since 5.88, the feature of holding slaves between processes is gone, just remove the call
     */
    KIOCORE_DEPRECATED_VERSION(5, 88, "Remove this call. The feature is gone")
    static void checkSlaveOnHold(bool b);
#endif

    static void emitReparseSlaveConfiguration();
    // KF6 TODO: rename to emitReparseWorkerConfiguration. See also T15956.

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 101)
    /**
     * Returns true if there is a slave on hold for @p url.
     *
     * @since 4.7
     * @deprecated Since 5.101, use isWorkerOnHoldFor(const QUrl &)
     */
    static KIOCORE_DEPRECATED_VERSION(5, 101, "Use isWorkerOnHoldFor(const QUrl &)") bool isSlaveOnHoldFor(const QUrl &url);
#endif

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
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
    /**
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Port away from the connected slave feature, e.g. making a library out of the slave code.")
    QT_MOC_COMPAT void slaveConnected(KIO::Slave *slave);

    /**
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Port away from the connected slave feature, e.g. making a library out of the slave code.")
    QT_MOC_COMPAT void slaveError(KIO::Slave *slave, int error, const QString &errorMsg);
#endif

    // DBUS
    Q_SCRIPTABLE void reparseSlaveConfiguration(const QString &);
    // KF6 TODO: rename to reparseWorkerConfiguration. See also T15956.

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
    /**
     * @deprecated since 5.91 Port away from the connected slave feature, e.g. making a library out of the slave code.
     */
    KIOCORE_DEPRECATED_VERSION(5, 91, "Removed for lack of usage, and due to feature removal.")
    Q_SCRIPTABLE void slaveOnHoldListChanged();
#endif

private:
    Q_DISABLE_COPY(Scheduler)
    KIOCORE_NO_EXPORT Scheduler();
    KIOCORE_NO_EXPORT ~Scheduler() override;

    KIOCORE_NO_EXPORT static Scheduler *self();

    friend class AccessManager;
    // For internal use from KIOWidgets' KIO::AccessManager, since 5.90
    static void setSimpleJobPriority(SimpleJob *job, int priority);

    // connected to D-Bus signal:
#ifndef KIO_ANDROID_STUB
    Q_PRIVATE_SLOT(d_func(), void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &))
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 91)
    Q_PRIVATE_SLOT(d_func(), void slotSlaveConnected())
    Q_PRIVATE_SLOT(d_func(), void slotSlaveError(int error, const QString &errorMsg))
#endif
private:
    friend class SchedulerPrivate;
    KIOCORE_NO_EXPORT SchedulerPrivate *d_func();
};

}
#endif
