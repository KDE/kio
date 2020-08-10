/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOB_BASE_H
#define KIO_JOB_BASE_H

#include <KCompositeJob>
#include <kio/metadata.h>

namespace KIO
{

class JobUiDelegateExtension;

class JobPrivate;
/**
 * @class KIO::Job job_base.h <KIO/Job>
 *
 * The base class for all jobs.
 * For all jobs created in an application, the code looks like
 *
 * \code
 *   KIO::Job* job = KIO::someoperation(some parameters);
 *   connect(job, &KJob::result, this, &MyClass::slotResult);
 * \endcode
 *   (other connects, specific to the job)
 *
 * And slotResult is usually at least:
 *
 * \code
 * void MyClass::slotResult(KJob *job)
 * {
 *   if (job->error()) {
 *     job->uiDelegate()->showErrorMessage();
 *   }
 * }
 * \endcode
 * @see KIO::Scheduler
 */
class KIOCORE_EXPORT Job : public KCompositeJob
{
    Q_OBJECT

protected:
    Job();
    Job(JobPrivate &dd);

public:
    virtual ~Job();
    void start() override {} // Since KIO autostarts its jobs

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * Retrieves the UI delegate of this job.
     *
     * @return the delegate used by the job to communicate with the UI
     *
     * @deprecated since 5.0, can now be replaced with uiDelegate()
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Use KJob::uiDelegate()")
    KJobUiDelegate *ui() const;
#endif

    /**
     * Retrieves the UI delegate extension used by this job.
     * @since 5.0
     */
    JobUiDelegateExtension *uiDelegateExtension() const;

    /**
     * Sets the UI delegate extension to be used by this job.
     * The default UI delegate extension is KIO::defaultJobUiDelegateExtension()
     */
    void setUiDelegateExtension(JobUiDelegateExtension *extension);

protected:
    /**
     * Abort this job.
     * This kills all subjobs and deletes the job.
     *
     */
    bool doKill() override;

    /**
     * Suspend this job
     * @see resume
     */
    bool doSuspend() override;

    /**
     * Resume this job
     * @see suspend
     */
    bool doResume() override;

public:
    /**
     * Converts an error code and a non-i18n error message into an
     * error message in the current language. The low level (non-i18n)
     * error message (usually a url) is put into the translated error
     * message using %1.
     *
     * Example for errid == ERR_CANNOT_OPEN_FOR_READING:
     * \code
     *   i18n( "Could not read\n%1" ).arg( errortext );
     * \endcode
     * Use this to display the error yourself, but for a dialog box
     * use uiDelegate()->showErrorMessage(). Do not call it if error()
     * is not 0.
     * @return the error message and if there is no error, a message
     *         telling the user that the app is broken, so check with
     *         error() whether there is an error
     */
    QString errorString() const override;

    /**
     * Converts an error code and a non-i18n error message into i18n
     * strings suitable for presentation in a detailed error message box.
     *
     * @param reqUrl the request URL that generated this error message
     * @param method the method that generated this error message
     * (unimplemented)
     * @return the following strings: caption, error + description,
     *         causes+solutions
     */
    QStringList detailedErrorStrings(const QUrl *reqUrl = nullptr,
                                     int method = -1) const;

    /**
     * Set the parent Job.
     * One example use of this is when FileCopyJob calls RenameDialog::open,
     * it must pass the correct progress ID of the parent CopyJob
     * (to hide the progress dialog).
     * You can set the parent job only once. By default a job does not
     * have a parent job.
     * @param parentJob the new parent job
     */
    void setParentJob(Job *parentJob);

    /**
     * Returns the parent job, if there is one.
     * @return the parent job, or @c nullptr if there is none
     * @see setParentJob
     */
    Job *parentJob() const;

    /**
     * Set meta data to be sent to the slave, replacing existing
     * meta data.
     * @param metaData the meta data to set
     * @see addMetaData()
     * @see mergeMetaData()
     */
    void setMetaData(const KIO::MetaData &metaData);

    /**
     * Add key/value pair to the meta data that is sent to the slave.
     * @param key the key of the meta data
     * @param value the value of the meta data
     * @see setMetaData()
     * @see mergeMetaData()
     */
    void addMetaData(const QString &key, const QString &value);

    /**
     * Add key/value pairs to the meta data that is sent to the slave.
     * If a certain key already existed, it will be overridden.
     * @param values the meta data to add
     * @see setMetaData()
     * @see mergeMetaData()
     */
    void addMetaData(const QMap<QString, QString> &values);

    /**
     * Add key/value pairs to the meta data that is sent to the slave.
     * If a certain key already existed, it will remain unchanged.
     * @param values the meta data to merge
     * @see setMetaData()
     * @see addMetaData()
     */
    void mergeMetaData(const QMap<QString, QString> &values);

    /**
     * @internal. For the scheduler. Do not use.
     */
    MetaData outgoingMetaData() const;

    /**
     * Get meta data received from the slave.
     * (Valid when first data is received and/or slave is finished)
     * @return the job's meta data
     */
    MetaData metaData() const;

    /**
     * Query meta data received from the slave.
     * (Valid when first data is received and/or slave is finished)
     * @param key the key of the meta data to retrieve
     * @return the value of the meta data, or QString() if the
     *         @p key does not exist
     */
    QString queryMetaData(const QString &key);

protected:

Q_SIGNALS:
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    /**
     * Emitted when the job is canceled.
     * Signal result() is emitted as well, and error() is,
     * in this case, ERR_USER_CANCELED.
     * @param job the job that emitted this signal
     * @deprecated Since 5.0. Don't use !
     */
    KIOCORE_DEPRECATED_VERSION(5, 0, "Do not use")
    void canceled(KJob *job);
#endif

    /**
     * Emitted when the slave successfully connected to the host.
     * There is no guarantee the slave will send this, and this is
     * currently unused (in the applications).
     * @param job the job that emitted this signal
     */
    void connected(KIO::Job *job);

protected:
    /**
     * Add a job that has to be finished before a result
     * is emitted. This has obviously to be called before
     * the finish signal is emitted by the slave.
     *
     * @param job the subjob to add
     */
    bool addSubjob(KJob *job) override;

    /**
     * Mark a sub job as being done.
     *
     * Note that this does not terminate the parent job, even if @p job
     * is the last subjob.  emitResult must be called to indicate that
     * the job is complete.
     *
     * @param job the subjob to remove
     */
    bool removeSubjob(KJob *job) override;

protected:
    JobPrivate *const d_ptr;

private:
    Q_DECLARE_PRIVATE(Job)
};

/**
 * Flags for the job properties.
 * Not all flags are supported in all cases. Please see documentation of
 * the calling function!
 * @see JobFlags
 */
enum JobFlag {
    /**
     * Show the progress info GUI, no Resume and no Overwrite
     */
    DefaultFlags = 0,

    /**
     * Hide progress information dialog, i.e. don't show a GUI.
     */
    HideProgressInfo = 1,

    /**
     * When set, automatically append to the destination file if it exists already.
     * WARNING: this is NOT the builtin support for offering the user to resume a previous
     * partial download. The Resume option is much less used, it allows to append
     * to an existing file.
     * This is used by KIO::put(), KIO::file_copy(), KIO::file_move().
     */
    Resume = 2,

    /**
     * When set, automatically overwrite the destination if it exists already.
     * This is used by KIO::rename(), KIO::put(), KIO::file_copy(), KIO::file_move(), KIO::symlink().
     * Otherwise the operation will fail with ERR_FILE_ALREADY_EXIST or ERR_DIR_ALREADY_EXIST.
     */
    Overwrite = 4,

    /**
     * When set, notifies the slave that application/job does not want privilege execution.
     * So in case of failure due to insufficient privileges show an error without attempting
     * to run the operation as root first.
     *
     * @since 5.43
     */
    NoPrivilegeExecution = 8,
};
/**
 * Stores a combination of #JobFlag values.
 */
Q_DECLARE_FLAGS(JobFlags, JobFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(JobFlags)

enum LoadType { Reload, NoReload };

}

#endif
