/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000-2009 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOB_P_H
#define KIO_JOB_P_H

#include "commands_p.h"
#include "global.h"
#include "jobtracker.h"
#include "kiocoredebug.h"
#include "simplejob.h"
#include "transferjob.h"
#include "worker_p.h"
#include <KJobTrackerInterface>
#include <QDataStream>
#include <QPointer>
#include <QUrl>
#include <kio/jobuidelegateextension.h>
#include <kio/jobuidelegatefactory.h>

/* clang-format off */
#define KIO_ARGS \
    QByteArray packedArgs; \
    QDataStream stream(&packedArgs, QIODevice::WriteOnly); \
    stream
/* clang-format on */

namespace KIO
{
static constexpr filesize_t invalidFilesize = static_cast<KIO::filesize_t>(-1);

// Exported for KIOWidgets jobs
class KIOCORE_EXPORT JobPrivate
{
public:
    JobPrivate()
        : m_parentJob(nullptr)
        , m_extraFlags(0)
        , m_uiDelegateExtension(KIO::defaultJobUiDelegateExtension())
        , m_privilegeExecutionEnabled(false)
    {
    }

    virtual ~JobPrivate();

    /*!
     * Some extra storage space for jobs that don't have their own
     * private d pointer.
     */
    enum {
        EF_TransferJobAsync = (1 << 0),
        EF_TransferJobNeedData = (1 << 1),
        EF_TransferJobDataSent = (1 << 2),
        EF_ListJobUnrestricted = (1 << 3),
        EF_KillCalled = (1 << 4),
    };

    enum FileOperationType {
        ChangeAttr, // chmod(), chown(), setModificationTime()
        Copy,
        Delete,
        MkDir,
        Move,
        Rename,
        Symlink,
        Transfer, // put() and get()
        Other, // if other file operation set message, title inside the job.
    };

    // Maybe we could use the QObject parent/child mechanism instead
    // (requires a new ctor, and moving the ctor code to some init()).
    Job *m_parentJob;
    int m_extraFlags;
    MetaData m_incomingMetaData;
    MetaData m_internalMetaData;
    MetaData m_outgoingMetaData;
    JobUiDelegateExtension *m_uiDelegateExtension;
    Job *q_ptr;
    // For privilege operation
    bool m_privilegeExecutionEnabled;
    QString m_title, m_message;
    FileOperationType m_operationType;

    QByteArray privilegeOperationData();
    void slotSpeed(KJob *job, unsigned long speed);

    static void emitMoving(KIO::Job *, const QUrl &src, const QUrl &dest);
    static void emitRenaming(KIO::Job *, const QUrl &src, const QUrl &dest);
    static void emitCopying(KIO::Job *, const QUrl &src, const QUrl &dest);
    static void emitCreatingDir(KIO::Job *, const QUrl &dir);
    static void emitDeleting(KIO::Job *, const QUrl &url);
    static void emitStating(KIO::Job *, const QUrl &url);
    static void emitTransferring(KIO::Job *, const QUrl &url);
    static void emitMounting(KIO::Job *, const QString &dev, const QString &point);
    static void emitUnmounting(KIO::Job *, const QString &point);

    Q_DECLARE_PUBLIC(Job)
};

class SimpleJobPrivate : public JobPrivate
{
public:
    /*!
     * Creates a new simple job.
     * @param url the url of the job
     * @param command the command of the job
     * @param packedArgs the arguments
     */
    SimpleJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : m_worker(nullptr)
        , m_packedArgs(packedArgs)
        , m_url(url)
        , m_command(command)
        , m_schedSerial(0)
        , m_redirectionHandlingEnabled(true)
    {
    }

    QPointer<Worker> m_worker;
    QByteArray m_packedArgs;
    QUrl m_url;
    int m_command;

    // for use in KIO::Scheduler
    //
    // There are two kinds of protocol:
    // (1) The protocol of the url
    // (2) The actual protocol that the KIO worker uses.
    //
    // These two often match, but not necessarily. Most notably, they don't
    // match when doing ftp via a proxy.
    // In that case (1) is ftp, but (2) is http.
    //
    // JobData::protocol stores (2) while Job::url().protocol() returns (1).
    // The ProtocolInfoDict is indexed with (2).
    //
    // We schedule workers based on (2) but tell the worker about (1) via
    // Worker::setProtocol().
    QString m_protocol;
    int m_schedSerial;
    bool m_redirectionHandlingEnabled;

    void simpleJobInit();

    /*!
     * Called on a worker's connected signal.
     * @see connected()
     */
    void slotConnected();
    /*!
     * Forward signal from the worker.
     * @param data_size the processed size in bytes
     * @see processedSize()
     */
    void slotProcessedSize(KIO::filesize_t data_size);
    /*!
     * Forward signal from the worker.
     * @param speed the speed in bytes/s
     * @see speed()
     */
    void slotSpeed(unsigned long speed);
    /*!
     * Forward signal from the worker.
     * Can also be called by the parent job, when it knows the size.
     * @param data_size the total size
     */
    void slotTotalSize(KIO::filesize_t data_size);

    /*!
     * Called on a worker's info message.
     * @param s the info message
     * @see infoMessage()
     */
    void _k_slotWorkerInfoMessage(const QString &s);

    /*!
     * Called when privilegeOperationRequested() is emitted by worker.
     */
    void slotPrivilegeOperationRequested();

    /*!
     * @internal
     * Called by the scheduler when a worker gets to
     * work on this job.
     **/
    virtual void start(KIO::Worker *worker);

    /*!
     * @internal
     * Called to detach a worker from a job.
     **/
    void workerDone();

    /*!
     * Called by subclasses to restart the job after a redirection was signalled.
     * The m_redirectionURL data member can appear in several subclasses, so we have it
     * passed in. The regular URL will be set to the redirection URL which is then cleared.
     */
    void restartAfterRedirection(QUrl *redirectionUrl);

    Q_DECLARE_PUBLIC(SimpleJob)

    static inline SimpleJobPrivate *get(KIO::SimpleJob *job)
    {
        return job->d_func();
    }
    static inline SimpleJob *newJobNoUi(const QUrl &url, int command, const QByteArray &packedArgs)
    {
        SimpleJob *job = new SimpleJob(*new SimpleJobPrivate(url, command, packedArgs));
        return job;
    }
    static inline SimpleJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs, JobFlags flags = HideProgressInfo)
    {
        SimpleJob *job = new SimpleJob(*new SimpleJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            // Only delete, rename and symlink operation accept JobFlags.
            FileOperationType opType;
            switch (command) {
            case CMD_DEL:
                opType = Delete;
                break;
            case CMD_RENAME:
                opType = Rename;
                break;
            case CMD_SYMLINK:
                opType = Symlink;
                break;
            default:
                return job;
            }
            job->d_func()->m_operationType = opType;
        }
        return job;
    }
};

class TransferJobPrivate : public SimpleJobPrivate
{
public:
    inline TransferJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs, const QByteArray &_staticData)
        : SimpleJobPrivate(url, command, packedArgs)
        , m_internalSuspended(false)
        , staticData(_staticData)
        , m_isMimetypeEmitted(false)
        , m_closedBeforeStart(false)
    {
    }

    inline TransferJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs, QIODevice *ioDevice)
        : SimpleJobPrivate(url, command, packedArgs)
        , m_internalSuspended(false)
        , m_isMimetypeEmitted(false)
        , m_closedBeforeStart(false)
        , m_outgoingDataSource(QPointer<QIODevice>(ioDevice))
    {
    }

    bool m_internalSuspended;
    QByteArray staticData;
    QUrl m_redirectionURL;
    QList<QUrl> m_redirectionList;
    QString m_mimetype;
    bool m_isMimetypeEmitted;
    bool m_closedBeforeStart;
    QPointer<QIODevice> m_outgoingDataSource;
    QMetaObject::Connection m_readChannelFinishedConnection;

    /*!
     * Flow control. Suspend data processing from the worker.
     */
    void internalSuspend();
    /*!
     * Flow control. Resume data processing from the worker.
     */
    void internalResume();
    /*!
     * @internal
     * Called by the scheduler when a worker gets to
     * work on this job.
     * @param worker the worker that works on the job
     */
    void start(KIO::Worker *worker) override;
    /*!
     * @internal
     * Called when the KIO worker needs the data to send the server. This slot
     * is invoked when the data is to be sent is read from a QIODevice rather
     * instead of a QByteArray buffer.
     */
    virtual void slotDataReqFromDevice();
    void slotIODeviceClosed();
    void slotIODeviceClosedBeforeStart();
    void slotPostRedirection();

    Q_DECLARE_PUBLIC(TransferJob)
    static inline TransferJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs, const QByteArray &_staticData, JobFlags flags)
    {
        TransferJob *job = new TransferJob(*new TransferJobPrivate(url, command, packedArgs, _staticData));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            job->setFinishedNotificationHidden();
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = Transfer;
        }
        return job;
    }

    static inline TransferJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs, QIODevice *ioDevice, JobFlags flags)
    {
        TransferJob *job = new TransferJob(*new TransferJobPrivate(url, command, packedArgs, ioDevice));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            job->setFinishedNotificationHidden();
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = Transfer;
        }
        return job;
    }
};

class DirectCopyJobPrivate;
/*!
 * @internal
 * Used for direct copy from or to the local filesystem (i.e.\ WorkerBase::copy())
 */
class DirectCopyJob : public SimpleJob
{
    Q_OBJECT

public:
    DirectCopyJob(const QUrl &url, const QByteArray &packedArgs);
    ~DirectCopyJob() override;

public Q_SLOTS:
    void slotCanResume(KIO::filesize_t offset);

Q_SIGNALS:
    /*!
     * @internal
     * Emitted if the job found an existing partial file
     * and supports resuming. Used by FileCopyJob.
     */
    void canResume(KIO::Job *job, KIO::filesize_t offset);

private:
    Q_DECLARE_PRIVATE(DirectCopyJob)
};
}

#endif
