/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_TRANSFERJOB_H
#define KIO_TRANSFERJOB_H

#include "simplejob.h"

namespace KIO
{

class TransferJobPrivate;
/**
 * @class KIO::TransferJob transferjob.h <KIO/TransferJob>
 *
 * The transfer job pumps data into and/or out of a Slave.
 * Data is sent to the slave on request of the slave ( dataReq).
 * If data coming from the slave can not be handled, the
 * reading of data from the slave should be suspended.
 */
class KIOCORE_EXPORT TransferJob : public SimpleJob
{
    Q_OBJECT

public:
    ~TransferJob() override;

    /**
     * Sets the modification time of the file to be created (by KIO::put)
     * Note that some kioslaves might ignore this.
     */
    void setModificationTime(const QDateTime &mtime);

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

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 3)
    /**
     * When enabled, the job reports the amount of data that has been sent,
     * instead of the amount of data that has been received.
     * @see slotProcessedSize
     * @see slotSpeed
     * @deprecated since 4.2.1, this is unnecessary (it is always false for
     *             KIO::get and true for KIO::put)
     */
    KIOCORE_DEPRECATED_VERSION(4, 3, "No longer needed")
    void setReportDataSent(bool enabled);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 3)
    /**
     *  Returns whether the job reports the amount of data that has been
     *  sent (true), or whether the job reports the amount of data that
     * has been received (false)
     * @deprecated since 4.2.1, this is unnecessary (it is always false for
     *             KIO::get and true for KIO::put)
     */
    KIOCORE_DEPRECATED_VERSION(4, 3, "No longer needed")
    bool reportDataSent() const;
#endif

    /**
     * Call this in the slot connected to result,
     * and only after making sure no error happened.
     * @return the MIME type of the URL
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
    void slotResult(KJob *job) override;

    /**
     * Reimplemented for internal reasons
     */
    bool doResume() override;

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
    void data(KIO::Job *job, const QByteArray &data);

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
    void dataReq(KIO::Job *job, QByteArray &data);

    /**
     * Signals a redirection.
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     * @param job the job that emitted this signal
     * @param url the new URL
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /**
     * Signals a permanent redirection.
     * The redirection itself is handled internally.
     * @param job the job that emitted this signal
     * @param fromUrl the original URL
     * @param toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 78)
    /**
     * MIME type determined.
     * @param job the job that emitted this signal
     * @param mimeType the MIME type
     * @deprecated Since 5.78, use mimeTypeFound(KIO::Job *, const QString &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 78, "Use KIO::TransferJob::mimeTypeFound(KIO::Job *, const QString &)")
    void mimetype(KIO::Job *job, const QString &mimeType);
#endif

    /**
     * MIME type determined.
     * @param job the job that emitted this signal
     * @param mimeType the MIME type
     * @since 5.78
     */
    void mimeTypeFound(KIO::Job *job, const QString &mimeType);

    /**
     * @internal
     * Emitted if the "put" job found an existing partial file
     * (in which case offset is the size of that file)
     * and emitted by the "get" job if it supports resuming to
     * the given offset - in this case @p offset is unused)
     */
    void canResume(KIO::Job *job, KIO::filesize_t offset);

protected Q_SLOTS:
    virtual void slotRedirection(const QUrl &url);
    void slotFinished() override;
    virtual void slotData(const QByteArray &data);
    virtual void slotDataReq();
    virtual void slotMimetype(const QString &mimetype);
    void slotMetaData(const KIO::MetaData &_metaData) override;

protected:
    TransferJob(TransferJobPrivate &dd);
private:
    Q_PRIVATE_SLOT(d_func(), void slotPostRedirection())
    Q_PRIVATE_SLOT(d_func(), void slotIODeviceClosed())
    Q_PRIVATE_SLOT(d_func(), void slotIODeviceClosedBeforeStart())
    Q_DECLARE_PRIVATE(TransferJob)

    // A FileCopyJob may control one or more TransferJobs
    friend class FileCopyJob;
    friend class FileCopyJobPrivate;
};

/**
 * Get (means: read).
 * This is the job to use in order to "download" a file into memory.
 * The slave emits the data through the data() signal.
 *
 * Special case: if you want to determine the MIME type of the file first,
 * and then read it with the appropriate component, you can still use
 * a KIO::get() directly. When that job emits the mimeType signal, (which is
 * guaranteed to happen before it emits any data), put the job on hold:
 *
 * @code
 *   job->putOnHold();
 *   KIO::Scheduler::publishSlaveOnHold();
 * @endcode
 *
 * and forget about the job. The next time someone does a KIO::get() on the
 * same URL (even in another process) this job will be resumed. This saves KIO
 * from doing two requests to the server.
 *
 * @param url the URL of the file
 * @param reload Reload to reload the file, NoReload if it can be taken from the cache
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT TransferJob *get(const QUrl &url, LoadType reload = NoReload, JobFlags flags = DefaultFlags);

/**
 * Put (means: write)
 *
 * @param url Where to write data.
 * @param permissions May be -1. In this case no special permission mode is set.
 * @param flags Can be HideProgressInfo, Overwrite and Resume here. WARNING:
 * Setting Resume means that the data will be appended to @p dest if @p dest exists.
 * @return the job handling the operation.
 * @see multi_get()
 */
KIOCORE_EXPORT TransferJob *put(const QUrl &url, int permissions,
                                JobFlags flags = DefaultFlags);

/**
 * HTTP POST (for form data).
 *
 * Example:
 * \code
 *    job = KIO::http_post( url, postData, KIO::HideProgressInfo );
 *    job->addMetaData("content-type", contentType );
 *    job->addMetaData("referrer", referrerURL);
 * \endcode
 *
 * @p postData is the data that you want to send and
 * @p contentType is the complete HTTP header line that
 * specifies the content's MIME type, for example
 * "Content-Type: text/xml".
 *
 * You MUST specify content-type!
 *
 * Often @p contentType is
 * "Content-Type: application/x-www-form-urlencoded" and
 * the @p postData is then an ASCII string (without null-termination!)
 * with characters like space, linefeed and percent escaped like %20,
 * %0A and %25.
 *
 * @param url Where to write the data.
 * @param postData Encoded data to post.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT TransferJob *http_post(const QUrl &url, const QByteArray &postData,
                                      JobFlags flags = DefaultFlags);

/**
 * HTTP POST.
 *
 * This function, unlike the one that accepts a QByteArray, accepts an IO device
 * from which to read the encoded data to be posted to the server in order to
 * to avoid holding the content of very large post requests, e.g. multimedia file
 * uploads, in memory.
 *
 * @param url Where to write the data.
 * @param device the device to read from
 * @param size Size of the encoded post data.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 *
 * @since 4.7
 */
KIOCORE_EXPORT TransferJob *http_post(const QUrl &url, QIODevice *device,
                                      qint64 size = -1, JobFlags flags = DefaultFlags);

/**
 * HTTP DELETE.
 *
 * Though this function servers the same purpose as KIO::file_delete, unlike
 * file_delete it accommodates HTTP specific actions such as redirections.
 *
 * @param url url resource to delete.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 *
 * @since 4.7.3
 */
KIOCORE_EXPORT TransferJob *http_delete(const QUrl &url, JobFlags flags = DefaultFlags);

}

#endif
