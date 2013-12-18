// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KIO_JOBCLASSES_H
#define KIO_JOBCLASSES_H

#include <QtCore/QObject>
#include <QtCore/QStringList>

#include <kio/global.h> // filesize_t
#include <kio/kiocore_export.h>
#include <kio/metadata.h>
#include <kio/udsentry.h>

#include <kcompositejob.h>

namespace KIO {

    /**
     * Flags for the job properties.
     * Not all flags are supported in all cases. Please see documentation of
     * the calling function!
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
      Overwrite = 4
    };
    Q_DECLARE_FLAGS(JobFlags, JobFlag)
    Q_DECLARE_OPERATORS_FOR_FLAGS(JobFlags)

    class JobUiDelegateExtension;

    class JobPrivate;
    /**
     * The base class for all jobs.
     * For all jobs created in an application, the code looks like
     *
     * \code
     *   KIO::Job * job = KIO::someoperation( some parameters );
     *   connect( job, SIGNAL( result( KJob * ) ),
     *            this, SLOT( slotResult( KJob * ) ) );
     * \endcode
     *   (other connects, specific to the job)
     *
     * And slotResult is usually at least:
     *
     * \code
     *  if ( job->error() )
     *      job->ui()->showErrorMessage();
     * \endcode
     * @see KIO::Scheduler
     */
    class KIOCORE_EXPORT Job : public KCompositeJob {
        Q_OBJECT

    protected:
        Job();
        Job(JobPrivate &dd);

    public:
        virtual ~Job();
        void start() {} // Since KIO autostarts its jobs

        /**
         * Retrieves the UI delegate of this job.
         *
         * @deprecated since 5.0, can now be replaced with uiDelegate()
         *
         * @return the delegate used by the job to communicate with the UI
         */
        KJobUiDelegate *ui() const;

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
        virtual bool doKill();

        /**
         * Suspend this job
         * @see resume
         */
        virtual bool doSuspend();

        /**
         * Resume this job
         * @see suspend
         */
        virtual bool doResume();

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
         * use ui()->showErrorMessage(). Do not call it if error()
         * is not 0.
         * @return the error message and if there is no error, a message
         *         telling the user that the app is broken, so check with
         *         error() whether there is an error
         */
        QString errorString() const;

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
        QStringList detailedErrorStrings(const QUrl *reqUrl = 0L,
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
        void setParentJob( Job* parentJob );

        /**
         * Returns the parent job, if there is one.
         * @return the parent job, or 0 if there is none
         * @see setParentJob
         */
        Job* parentJob() const;

        /**
         * Set meta data to be sent to the slave, replacing existing
         * meta data.
         * @param metaData the meta data to set
         * @see addMetaData()
         * @see mergeMetaData()
         */
        void setMetaData( const KIO::MetaData &metaData);

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
        void addMetaData(const QMap<QString,QString> &values);

        /**
         * Add key/value pairs to the meta data that is sent to the slave.
         * If a certain key already existed, it will remain unchanged.
         * @param values the meta data to merge
         * @see setMetaData()
         * @see addMetaData()
         */
        void mergeMetaData(const QMap<QString,QString> &values);

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
        /**
         * @deprecated. Don't use !
         * Emitted when the job is canceled.
         * Signal result() is emitted as well, and error() is,
         * in this case, ERR_USER_CANCELED.
         * @param job the job that emitted this signal
         */
        void canceled( KJob *job );

        /**
         * Emitted when the slave successfully connected to the host.
         * There is no guarantee the slave will send this, and this is
         * currently unused (in the applications).
         * @param job the job that emitted this signal
         */
        void connected( KIO::Job *job );

    protected:
        /**
         * Add a job that has to be finished before a result
         * is emitted. This has obviously to be called before
         * the finish signal is emitted by the slave.
         *
         * @param job the subjob to add
         */
        virtual bool addSubjob( KJob *job );

        /**
         * Mark a sub job as being done.
         *
         * KDE4 change: this doesn't terminate the parent job anymore, call emitResult to do that.
         *
         * @param job the subjob to remove
         */
        virtual bool removeSubjob( KJob *job );

    protected:
        JobPrivate *const d_ptr;

    private:
        /**
         * Forward signal from subjob.
         * @param job the subjob
         * @param speed the speed in bytes/s
         * @see speed()
         */
        Q_PRIVATE_SLOT(d_func(), void slotSpeed( KJob *job, unsigned long speed ))
        Q_DECLARE_PRIVATE(Job)
    };

    class SimpleJobPrivate;
    /**
     * A simple job (one url and one command).
     * This is the base class for all jobs that are scheduled.
     * Other jobs are high-level jobs (CopyJob, DeleteJob, FileCopyJob...)
     * that manage subjobs but aren't scheduled directly.
     */
    class KIOCORE_EXPORT SimpleJob : public KIO::Job {
    Q_OBJECT

    public:
        ~SimpleJob();

    protected:
        /**
         * Suspend this job
         * @see resume
         */
        virtual bool doSuspend();

        /**
         * Resume this job
         * @see suspend
         */
        virtual bool doResume();

        /**
         * Abort job.
         * This kills all subjobs and deletes the job.
         */
        virtual bool doKill();

    public:
        /**
         * Returns the SimpleJob's URL
         * @return the url
         */
        const QUrl& url() const;

        /**
         * Abort job.
         * Suspends slave to be reused by another job for the same request.
         */
        virtual void putOnHold();

        /**
         * Discard suspended slave.
         */
        static void removeOnHold();

        /**
         * Returns true when redirections are handled internally, the default.
         *
         * @since 4.4
         */
        bool isRedirectionHandlingEnabled() const;

        /**
         * Set @p handle to false to prevent the internal handling of redirections.
         *
         * When this flag is set, redirection requests are simply forwarded to the
         * caller instead of being handled internally.
         *
         * @since 4.4
         */
        void setRedirectionHandlingEnabled(bool handle);

    public Q_SLOTS:
        /**
         * @internal
         * Called on a slave's error.
         * Made public for the scheduler.
         */
        void slotError( int , const QString & );

    protected Q_SLOTS:
        /**
         * Called when the slave marks the job
         * as finished.
         */
        virtual void slotFinished( );

        /**
         * @internal
         * Called on a slave's warning.
         */
        virtual void slotWarning( const QString & );

        /**
         * MetaData from the slave is received.
         * @param _metaData the meta data
         * @see metaData()
         */
        virtual void slotMetaData( const KIO::MetaData &_metaData);

    protected:
        /*
         * Allow jobs that inherit SimpleJob and are aware
         * of redirections to store the SSL session used.
         * Retrieval is handled by SimpleJob::start
         * @param m_redirectionURL Reference to redirection URL,
         * used instead of m_url if not empty
         */
        void storeSSLSessionFromJob(const QUrl &m_redirectionURL);

        /**
         * Creates a new simple job. You don't need to use this constructor,
         * unless you create a new job that inherits from SimpleJob.
         */
        SimpleJob(SimpleJobPrivate &dd);
    private:
        Q_PRIVATE_SLOT(d_func(), void slotConnected())
        Q_PRIVATE_SLOT(d_func(), void slotProcessedSize( KIO::filesize_t data_size ))
        Q_PRIVATE_SLOT(d_func(), void slotSpeed( unsigned long speed ))
        Q_PRIVATE_SLOT(d_func(), void slotTotalSize( KIO::filesize_t data_size ))
        Q_PRIVATE_SLOT(d_func(), void _k_slotSlaveInfoMessage(const QString&))

        Q_DECLARE_PRIVATE(SimpleJob)
    };

    class StatJobPrivate;
    /**
     * A KIO job that retrieves information about a file or directory.
     * @see KIO::stat()
     */
    class KIOCORE_EXPORT StatJob : public SimpleJob {

    Q_OBJECT

    public:
        enum StatSide {
            SourceSide,
            DestinationSide
        };

        ~StatJob();

        /**
         * A stat() can have two meanings. Either we want to read from this URL,
         * or to check if we can write to it. First case is "source", second is "dest".
         * It is necessary to know what the StatJob is for, to tune the kioslave's behavior
         * (e.g. with FTP).
         * By default it is SourceSide.
         * @param side SourceSide or DestinationSide
         */
        void setSide(StatSide side);

        /**
         * A stat() can have two meanings. Either we want to read from this URL,
         * or to check if we can write to it. First case is "source", second is "dest".
         * It is necessary to know what the StatJob is for, to tune the kioslave's behavior
         * (e.g. with FTP).
         * @param source true for "source" mode, false for "dest" mode
         * @deprecated use setSide(StatSide side).
         */
#ifndef KDE_NO_DEPRECATED
        KIOCORE_DEPRECATED void setSide( bool source );
#endif

        /**
         * Selects the level of @p details we want.
         * By default this is 2 (all details wanted, including modification time, size, etc.),
         * setDetails(1) is used when deleting: we don't need all the information if it takes
         * too much time, no need to follow symlinks etc.
         * setDetails(0) is used for very simple probing: we'll only get the answer
         * "it's a file or a directory, or it doesn't exist". This is used by KRun.
         * @param details 2 for all details, 1 for simple, 0 for very simple
         */
        void setDetails( short int details );

        /**
         * @brief Result of the stat operation.
         * Call this in the slot connected to result,
         * and only after making sure no error happened.
         * @return the result of the stat
         */
        const UDSEntry & statResult() const;

        /**
         * @brief most local URL
         * Call this in the slot connected to result,
         * and only after making sure no error happened.
         * @return the most local URL for the URL we were stat'ing.
         *
         * Sample usage:
         * <code>
         * KIO::StatJob* job = KIO::mostLocalUrl("desktop:/foo");
         * job->ui()->setWindow(this);
         * connect(job, SIGNAL(result(KJob*)), this, SLOT(slotMostLocalUrlResult(KJob*)));
         * [...]
         * // and in the slot
         * if (job->error()) {
         *    [...] // doesn't exist
         * } else {
         *    const QUrl localUrl = job->mostLocalUrl();
         *    // localUrl = file:///$HOME/Desktop/foo
         *    [...]
         * }
         *
         * \since 4.4
         */
        QUrl mostLocalUrl() const;

    Q_SIGNALS:
        /**
         * Signals a redirection.
         * Use to update the URL shown to the user.
         * The redirection itself is handled internally.
         * @param job the job that is redirected
         * @param url the new url
         */
        void redirection( KIO::Job *job, const QUrl &url );

        /**
         * Signals a permanent redirection.
         * The redirection itself is handled internally.
         * @param job the job that is redirected
         * @param fromUrl the original URL
         * @param toUrl the new URL
         */
        void permanentRedirection( KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl );

    protected Q_SLOTS:
        virtual void slotFinished();
        virtual void slotMetaData( const KIO::MetaData &_metaData);
    protected:
        StatJob(StatJobPrivate &dd);

    private:
        Q_PRIVATE_SLOT(d_func(), void slotStatEntry( const KIO::UDSEntry & entry ))
        Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &url))
        Q_DECLARE_PRIVATE(StatJob)
    };

    class FileCopyJobPrivate;
    class TransferJobPrivate;
    /**
     * The transfer job pumps data into and/or out of a Slave.
     * Data is sent to the slave on request of the slave ( dataReq).
     * If data coming from the slave can not be handled, the
     * reading of data from the slave should be suspended.
     */
    class KIOCORE_EXPORT TransferJob : public SimpleJob {
    Q_OBJECT

    public:
        ~TransferJob();

        /**
         * Sets the modification time of the file to be created (by KIO::put)
         * Note that some kioslaves might ignore this.
         */
        void setModificationTime( const QDateTime& mtime );

        /**
         * Checks whether we got an error page. This currently only happens
         * with HTTP urls. Call this from your slot connected to result().
         *
         * @return true if we got an (HTML) error page from the server
         * instead of what we asked for.
         */
        bool isErrorPage() const;

        /**
         * Enable the async data mode.
         * When async data is enabled, data should be provided to the job by
         * calling sendAsyncData() instead of returning data in the
         * dataReq() signal.
         */
        void setAsyncDataEnabled(bool enabled);

        /**
         * Provide data to the job when async data is enabled.
         * Should be called exactly once after receiving a dataReq signal
         * Sending an empty block indicates end of data.
         */
        void sendAsyncData(const QByteArray &data);

        /**
         * When enabled, the job reports the amount of data that has been sent,
         * instead of the amount of data that that has been received.
         * @see slotProcessedSize
         * @see slotSpeed
         * @deprecated not needed, this is false for KIO::get and true for KIO::put,
         *             automatically since KDE-4.2.1
         */
#ifndef KDE_NO_DEPRECATED
        KIOCORE_DEPRECATED void setReportDataSent(bool enabled);
#endif

        /**
         *  Returns whether the job reports the amount of data that has been
         *  sent (true), or whether the job reports the amount of data that
         * has been received (false)
         * @deprecated not needed, this is false for KIO::get and true for KIO::put,
         *             automatically since KDE-4.2.1 (and not useful as public API)
         */
#ifndef KDE_NO_DEPRECATED
        KIOCORE_DEPRECATED bool reportDataSent() const;
#endif

        /**
         * Call this in the slot connected to result,
         * and only after making sure no error happened.
         * @return the mimetype of the URL
         */
        QString mimetype() const;

        /**
         * After the job has finished, it will return the final url in case a redirection
         * has happened.
         * @return the final url that can be empty in case no redirection has happened.
         * @since 5.0
         */
        QUrl redirectUrl() const;

        /**
         * Set the total size of data that we are going to send
         * in a put job. Helps getting proper progress information.
         * @since 4.2.1
         */
        void setTotalSize(KIO::filesize_t bytes);

    protected:
        /**
         * Called when m_subJob finishes.
         * @param job the job that finished
         */
        virtual void slotResult( KJob *job );

        /**
         * Reimplemented for internal reasons
         */
        virtual bool doResume();

    Q_SIGNALS:
        /**
         * Data from the slave has arrived.
         * @param job the job that emitted this signal
         * @param data data received from the slave.
         *
         * End of data (EOD) has been reached if data.size() == 0, however, you
         * should not be certain of data.size() == 0 ever happening (e.g. in case
         * of an error), so you should rely on result() instead.
         */
        void data( KIO::Job *job, const QByteArray &data );

        /**
         * Request for data.
         * Please note, that you shouldn't put too large chunks
         * of data in it as this requires copies within the frame
         * work, so you should rather split the data you want
         * to pass here in reasonable chunks (about 1MB maximum)
         *
         * @param job the job that emitted this signal
         * @param data buffer to fill with data to send to the
         * slave. An empty buffer indicates end of data. (EOD)
         */
        void dataReq( KIO::Job *job, QByteArray &data );

        /**
         * Signals a redirection.
         * Use to update the URL shown to the user.
         * The redirection itself is handled internally.
         * @param job the job that emitted this signal
         * @param url the new URL
         */
        void redirection( KIO::Job *job, const QUrl &url );

        /**
         * Signals a permanent redirection.
         * The redirection itself is handled internally.
         * @param job the job that emitted this signal
         * @param fromUrl the original URL
         * @param toUrl the new URL
         */
        void permanentRedirection( KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl );

        /**
         * Mimetype determined.
         * @param job the job that emitted this signal
         * @param type the mime type
         */
        void mimetype( KIO::Job *job, const QString &type );

        /**
         * @internal
         * Emitted if the "put" job found an existing partial file
         * (in which case offset is the size of that file)
         * and emitted by the "get" job if it supports resuming to
         * the given offset - in this case @p offset is unused)
         */
        void canResume( KIO::Job *job, KIO::filesize_t offset );


    protected Q_SLOTS:
        virtual void slotRedirection(const QUrl &url);
        virtual void slotFinished();
        virtual void slotData( const QByteArray &data);
        virtual void slotDataReq();
        virtual void slotMimetype( const QString &mimetype );
        virtual void slotMetaData( const KIO::MetaData &_metaData);

    protected:
        TransferJob(TransferJobPrivate &dd);
    private:
        Q_PRIVATE_SLOT(d_func(), void slotErrorPage())
        Q_PRIVATE_SLOT(d_func(), void slotCanResume( KIO::filesize_t offset ))
        Q_PRIVATE_SLOT(d_func(), void slotPostRedirection())
        Q_PRIVATE_SLOT(d_func(), void slotNeedSubUrlData())
        Q_PRIVATE_SLOT(d_func(), void slotSubUrlData(KIO::Job*, const QByteArray &))
        Q_PRIVATE_SLOT(d_func(), void slotDataReqFromDevice())
        Q_DECLARE_PRIVATE(TransferJob)

        // A FileCopyJob may control one or more TransferJobs
        friend class FileCopyJob;
        friend class FileCopyJobPrivate;
    };

    class StoredTransferJobPrivate;
    /**
     * StoredTransferJob is a TransferJob (for downloading or uploading data) that
     * also stores a QByteArray with the data, making it simpler to use than the
     * standard TransferJob.
     *
     * For KIO::storedGet it puts the data into the member QByteArray, so the user
     * of this class can get hold of the whole data at once by calling data()
     * when the result signal is emitted.
     * You should only use StoredTransferJob to download data if you cannot
     * process the data by chunks while it's being downloaded, since storing
     * everything in a QByteArray can potentially require a lot of memory.
     *
     * For KIO::storedPut the user of this class simply provides the bytearray from
     * the start, and the job takes care of uploading it.
     * You should only use StoredTransferJob to upload data if you cannot
     * provide the in chunks while it's being uploaded, since storing
     * everything in a QByteArray can potentially require a lot of memory.
     */
    class KIOCORE_EXPORT StoredTransferJob : public KIO::TransferJob {
        Q_OBJECT

    public:
        ~StoredTransferJob();

        /**
         * Set data to be uploaded. This is for put jobs.
         * Automatically called by KIO::storedPut(const QByteArray &, ...),
         * do not call this yourself.
         */
        void setData( const QByteArray& arr );

        /**
         * Get hold of the downloaded data. This is for get jobs.
         * You're supposed to call this only from the slot connected to the result() signal.
         */
        QByteArray data() const;

    protected:
        StoredTransferJob(StoredTransferJobPrivate &dd);
    private:
        Q_PRIVATE_SLOT(d_func(), void slotStoredData( KIO::Job *job, const QByteArray &data ))
        Q_PRIVATE_SLOT(d_func(), void slotStoredDataReq( KIO::Job *job, QByteArray &data ))

        Q_DECLARE_PRIVATE(StoredTransferJob)
    };

    class MultiGetJobPrivate;
    /**
     * The MultiGetJob is a TransferJob that allows you to get
     * several files from a single server. Don't create directly,
     * but use KIO::multi_get() instead.
     * @see KIO::multi_get()
     */
    class KIOCORE_EXPORT MultiGetJob : public TransferJob {
    Q_OBJECT

    public:
        virtual ~MultiGetJob();

        /**
         * Get an additional file.
         *
         * @param id the id of the file
         * @param url the url of the file to get
         * @param metaData the meta data for this request
         */
        void get(long id, const QUrl &url, const MetaData &metaData);

    Q_SIGNALS:
        /**
         * Data from the slave has arrived.
         * @param id the id of the request
         * @param data data received from the slave.
         * End of data (EOD) has been reached if data.size() == 0
         */
        void data( long id, const QByteArray &data);

        /**
         * Mimetype determined
         * @param id the id of the request
         * @param type the mime type
         */
        void mimetype( long id, const QString &type );

        /**
         * File transfer completed.
         *
         * When all files have been processed, result(KJob *) gets
         * emitted.
         * @param id the id of the request
         */
        void result( long id);

    protected Q_SLOTS:
        virtual void slotRedirection(const QUrl &url);
        virtual void slotFinished();
        virtual void slotData( const QByteArray &data);
        virtual void slotMimetype( const QString &mimetype );

    protected:
        MultiGetJob(MultiGetJobPrivate &dd);
    private:
        Q_DECLARE_PRIVATE(MultiGetJob)
    };

    class MimetypeJobPrivate;
    /**
     * A MimetypeJob is a TransferJob that  allows you to get
     * the mime type of an URL. Don't create directly,
     * but use KIO::mimetype() instead.
     * @see KIO::mimetype()
     */
    class KIOCORE_EXPORT MimetypeJob : public TransferJob {
    Q_OBJECT

    public:
        ~MimetypeJob();

    protected Q_SLOTS:
        virtual void slotFinished( );
    protected:
        MimetypeJob(MimetypeJobPrivate &dd);
    private:
        Q_DECLARE_PRIVATE(MimetypeJob)
    };

    /**
     * The FileCopyJob copies data from one place to another.
     * @see KIO::file_copy()
     * @see KIO::file_move()
     */
    class KIOCORE_EXPORT FileCopyJob : public Job {
    Q_OBJECT

    public:
        ~FileCopyJob();
        /**
         * If you know the size of the source file, call this method
         * to inform this job. It will be displayed in the "resume" dialog.
         * @param size the size of the source file
         */
        void setSourceSize(KIO::filesize_t size);

        /**
         * Sets the modification time of the file
         *
         * Note that this is ignored if a direct copy (SlaveBase::copy) can be done,
         * in which case the mtime of the source is applied to the destination (if the protocol
         * supports the concept).
         */
        void setModificationTime( const QDateTime& mtime );

        /**
         * Returns the source URL.
         * @return the source URL
         */
        QUrl srcUrl() const;

        /**
         * Returns the destination URL.
         * @return the destination URL
         */
        QUrl destUrl() const;

        bool doSuspend();
        bool doResume();

    Q_SIGNALS:
        /**
         * Mimetype determined during a file copy.
         * This is never emitted during a move, and might not be emitted during
         * a file copy, depending on the slave. But when a get and a put are
         * being used (which is the common case), this signal forwards the
         * mimetype information from the get job.
         *
         * @param job the job that emitted this signal
         * @param type the mime type
         */
        void mimetype( KIO::Job *job, const QString &type );

    protected Q_SLOTS:
        /**
         * Called whenever a subjob finishes.
         * @param job the job that emitted this signal
         */
        virtual void slotResult( KJob *job );

    protected:
        FileCopyJob(FileCopyJobPrivate &dd);

    private:
        Q_PRIVATE_SLOT(d_func(), void slotStart())
        Q_PRIVATE_SLOT(d_func(), void slotData( KIO::Job *, const QByteArray &data))
        Q_PRIVATE_SLOT(d_func(), void slotDataReq( KIO::Job *, QByteArray &data))
        Q_PRIVATE_SLOT(d_func(), void slotMimetype( KIO::Job*, const QString& type ))
        Q_PRIVATE_SLOT(d_func(), void slotProcessedSize( KJob *job, qulonglong size ))
        Q_PRIVATE_SLOT(d_func(), void slotTotalSize( KJob *job, qulonglong size ))
        Q_PRIVATE_SLOT(d_func(), void slotPercent( KJob *job, unsigned long pct ))
        Q_PRIVATE_SLOT(d_func(), void slotCanResume( KIO::Job *job, KIO::filesize_t offset ))

        Q_DECLARE_PRIVATE(FileCopyJob)
    };

    class ListJobPrivate;
    /**
     * A ListJob is allows you to get the get the content of a directory.
     * Don't create the job directly, but use KIO::listRecursive() or
     * KIO::listDir() instead.
     * @see KIO::listRecursive()
     * @see KIO::listDir()
     */
    class KIOCORE_EXPORT ListJob : public SimpleJob {
    Q_OBJECT

    public:
        ~ListJob();

        /**
         * Returns the ListJob's redirection URL. This will be invalid if there
         * was no redirection.
         * @return the redirection url
         */
        const QUrl& redirectionUrl() const;

        /**
         * Do not apply any KIOSK restrictions to this job.
         */
        void setUnrestricted(bool unrestricted);

    Q_SIGNALS:
        /**
         * This signal emits the entry found by the job while listing.
         * The progress signals aren't specific to ListJob. It simply
         * uses SimpleJob's processedSize (number of entries listed) and
         * totalSize (total number of entries, if known),
         * as well as percent.
         * @param job the job that emitted this signal
         * @param list the list of UDSEntries
         */
        void entries( KIO::Job *job, const KIO::UDSEntryList& list); // TODO KDE5: use KIO::ListJob* argument to avoid casting

	/**
	 * This signal is emitted when a sub-directory could not be listed.
	 * The job keeps going, thus doesn't result in an overall error.
	 * @param job the job that emitted the signal
	 * @param subJob the job listing a sub-directory, which failed. Use
	 *		 url(), error() and errorText() on that job to find
	 *		 out more.
	 */
	void subError( KIO::ListJob *job, KIO::ListJob *subJob );

        /**
         * Signals a redirection.
         * Use to update the URL shown to the user.
         * The redirection itself is handled internally.
         * @param job the job that is redirected
         * @param url the new url
         */
        void redirection( KIO::Job *job, const QUrl &url );

        /**
         * Signals a permanent redirection.
         * The redirection itself is handled internally.
         * @param job the job that emitted this signal
         * @param fromUrl the original URL
         * @param toUrl the new URL
         */
        void permanentRedirection( KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl );

    protected Q_SLOTS:
        virtual void slotFinished( );
        virtual void slotMetaData( const KIO::MetaData &_metaData);
        virtual void slotResult( KJob *job );

    protected:
        ListJob(ListJobPrivate &dd);

    private:
        Q_PRIVATE_SLOT(d_func(), void slotListEntries( const KIO::UDSEntryList& list ))
        Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &url))
        Q_PRIVATE_SLOT(d_func(), void gotEntries( KIO::Job * subjob, const KIO::UDSEntryList& list ))
        Q_DECLARE_PRIVATE(ListJob)
    };

    class SpecialJobPrivate;
    /**
     * A class that sends a special command to an ioslave.
     * This allows you to send a binary blob to an ioslave and handle
     * its responses. The ioslave will receive the binary data as an
     * argument to the "special" function (inherited from SlaveBase::special()).
     *
     * Use this only on ioslaves that belong to your application. Sending
     * special commands to other ioslaves may cause unexpected behaviour.
     *
     * @see KIO::special
     */
    class KIOCORE_EXPORT SpecialJob : public TransferJob
    {
        Q_OBJECT
    public:
        /**
         * Creates a KIO::SpecialJob.
         *
         * @param url the URL to be passed to the ioslave
         * @param data the data to be sent to the SlaveBase::special() function.
         */
        explicit SpecialJob(const QUrl &url, const QByteArray &data = QByteArray());

        /**
         * Sets the QByteArray that is passed to SlaveBase::special() on
         * the ioslave.
         */
        void setArguments(const QByteArray &data);

        /**
         * Returns the QByteArray data that will be sent (or has been sent) to the
         * ioslave.
         */
        QByteArray arguments() const;

    public:
        ~SpecialJob();

    private:
        Q_DECLARE_PRIVATE(SpecialJob)
    };
}

// For source compatibility
#include <kio/mkdirjob.h>

#endif
