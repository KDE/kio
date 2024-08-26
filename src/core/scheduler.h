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
class Worker;

class SchedulerPrivate;
/*
 *
 * The KIO::Scheduler manages KIO workers for the application.
 * It also queues jobs and assigns the job to a worker when one
 * becomes available.
 *
 * There are two possible ways for a job to get a worker:
 *
 * <h3>1. Direct</h3>
 * This is the default. When you create a job the
 * KIO::Scheduler will be notified and will find either an existing
 * worker that is idle or it will create a new worker for the job.
 *
 *
 * <h3>2. Scheduled</h3>
 * If you create a lot of jobs, you might want not want to have a
 * worker for each job. If you schedule a job, a maximum number
 * of workers will be created. When more jobs arrive, they will be
 * queued. When a worker is finished with a job, it will be assigned
 * a job from the queue.
 *
 * @see KIO::Job
 */
class Scheduler : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.KIO.Scheduler")
public:
    /**
     * Register @p job with the scheduler.
     * The default is to create a new worker for the job if no worker
     * is available.
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
     * @param worker the worker that executed the @p job
     */
    static void jobFinished(KIO::SimpleJob *job, KIO::Worker *worker);

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
    KIOCORE_NO_EXPORT Scheduler();
    KIOCORE_NO_EXPORT ~Scheduler() override;

    KIOCORE_NO_EXPORT static Scheduler *self();

    // connected to D-Bus signal:
#ifdef WITH_QTDBUS
    Q_PRIVATE_SLOT(d_func(), void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &))
#endif

private:
    friend class SchedulerPrivate;
    KIOCORE_NO_EXPORT SchedulerPrivate *d_func();
};

}
#endif
