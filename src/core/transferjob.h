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
/*!
 * \class KIO::TransferJob
 * \inheaderfile KIO/TransferJob
 * \inmodule KIOCore
 *
 * \brief The transfer job pumps data into and/or out of a KIO worker.
 *
 * Data is sent to the worker on request of the worker ( dataReq).
 * If data coming from the worker can not be handled, the
 * reading of data from the worker should be suspended.
 */
class KIOCORE_EXPORT TransferJob : public SimpleJob
{
    Q_OBJECT

public:
    ~TransferJob() override;

    /*!
     * Sets the modification time of the file to be created (by KIO::put)
     * Note that some KIO workers might ignore this.
     */
    void setModificationTime(const QDateTime &mtime);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(6, 3)
    /*!
     * Checks whether we got an error page. This currently only happens
     * with HTTP urls. Call this from your slot connected to result().
     *
     * Returns \c true if we got an (HTML) error page from the server
     * instead of what we asked for.
     *
     * Not implemented.
     *
     * \deprecated[6.3]
     */
    KIOCORE_DEPRECATED_VERSION(6, 3, "Not implemented")
    bool isErrorPage() const;
#endif

    /*!
     * Enable the async data mode.
     * When async data is enabled, data should be provided to the job by
     * calling sendAsyncData() instead of returning data in the
     * dataReq() signal.
     */
    void setAsyncDataEnabled(bool enabled);

    /*!
     * Provide data to the job when async data is enabled.
     * Should be called exactly once after receiving a dataReq signal
     * Sending an empty block indicates end of data.
     */
    void sendAsyncData(const QByteArray &data);

    /*!
     * Call this in the slot connected to result,
     * and only after making sure no error happened.
     *
     * Returns the MIME type of the URL
     */
    QString mimetype() const;

    /*!
     * After the job has finished, it will return the final url in case a redirection
     * has happened.
     *
     * Returns the final url that can be empty in case no redirection has happened.
     *
     * \since 5.0
     */
    QUrl redirectUrl() const;

    /*!
     * Set the total size of data that we are going to send
     * in a put job. Helps getting proper progress information.
     *
     * \since 4.2.1
     */
    void setTotalSize(KIO::filesize_t bytes);

protected:
    bool doResume() override;

Q_SIGNALS:
    /*!
     * Data from the worker has arrived.
     *
     * \a job the job that emitted this signal
     *
     * \a data data received from the worker.
     *
     * End of data (EOD) has been reached if data.size() == 0, however, you
     * should not be certain of data.size() == 0 ever happening (e.g. in case
     * of an error), so you should rely on result() instead.
     */
    void data(KIO::Job *job, const QByteArray &data);

    /*!
     * Request for data.
     *
     * Please note, that you shouldn't put too large chunks
     * of data in it as this requires copies within the frame
     * work, so you should rather split the data you want
     * to pass here in reasonable chunks (about 1MB maximum)
     *
     * \a job the job that emitted this signal
     *
     * \a data buffer to fill with data to send to the
     * worker. An empty buffer indicates end of data. (EOD)
     */
    void dataReq(KIO::Job *job, QByteArray &data);

    /*!
     * Signals a redirection.
     *
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     *
     * \a job the job that emitted this signal
     *
     * \a url the new URL
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /*!
     * Signals a permanent redirection.
     *
     * The redirection itself is handled internally.
     *
     * \a job the job that emitted this signal
     *
     * \a fromUrl the original URL
     *
     * \a toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

    /*!
     * MIME type determined.
     *
     * \a job the job that emitted this signal
     *
     * \a mimeType the MIME type
     *
     * \since 5.78
     */
    void mimeTypeFound(KIO::Job *job, const QString &mimeType);

    /*!
     * \internal
     *
     * Emitted if the "put" job found an existing partial file
     * (in which case offset is the size of that file)
     * and emitted by the "get" job if it supports resuming to
     * the given offset - in this case \a offset is unused)
     */
    void canResume(KIO::Job *job, KIO::filesize_t offset);

protected Q_SLOTS:
    /*!
     *
     */
    virtual void slotRedirection(const QUrl &url);

    void slotFinished() override;

    /*!
     *
     */
    virtual void slotData(const QByteArray &data);

    /*!
     *
     */
    virtual void slotDataReq();

    /*!
     *
     */
    virtual void slotMimetype(const QString &mimetype);

protected:
    KIOCORE_NO_EXPORT explicit TransferJob(TransferJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(TransferJob)

    // A FileCopyJob may control one or more TransferJobs
    friend class FileCopyJob;
    friend class FileCopyJobPrivate;
};

/*!
 * \relates KIO::TransferJob
 *
 * Get (means: read).
 * This is the job to use in order to "download" a file into memory.
 * The worker emits the data through the data() signal.
 *
 * Special case: if you want to determine the MIME type of the file first,
 * and then read it with the appropriate component, you can still use
 * a KIO::get() directly. When that job emits the mimeType signal, (which is
 * guaranteed to happen before it emits any data), put the job on hold:
 *
 * \code
 *   job->putOnHold();
 * \endcode
 *
 * and forget about the job. The next time someone does a KIO::get() on the
 * same URL (even in another process) this job will be resumed. This saves KIO
 * from doing two requests to the server.
 *
 * \a url the URL of the file
 *
 * \a reload Reload to reload the file, NoReload if it can be taken from the cache
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT TransferJob *get(const QUrl &url, LoadType reload = NoReload, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::TransferJob
 *
 * Put (means: write)
 *
 * \a url Where to write data.
 *
 * \a permissions May be -1. In this case no special permission mode is set.
 *
 * \a flags Can be HideProgressInfo, Overwrite and Resume here.
 *
 * \warning Setting Resume means that the data will be appended to \a dest if \a dest exists.
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT TransferJob *put(const QUrl &url, int permissions, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::TransferJob
 *
 * HTTP POST (for form data).
 *
 * Example:
 * \code
 *    job = KIO::http_post( url, postData, KIO::HideProgressInfo );
 *    job->addMetaData("content-type", contentType );
 * \endcode
 *
 * \a postData is the data that you want to send and
 *
 * \c contentType is the complete HTTP header line that
 * specifies the content's MIME type, for example
 * "Content-Type: text/xml".
 *
 * You MUST specify content-type!
 *
 * Often \c contentType is
 * "Content-Type: application/x-www-form-urlencoded" and
 * the \a postData is then an ASCII string (without null-termination!)
 * with characters like space, linefeed and percent escaped like %20,
 * %0A and %25.
 *
 * \a url Where to write the data.
 *
 * \a postData Encoded data to post.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT TransferJob *http_post(const QUrl &url, const QByteArray &postData, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::TransferJob
 *
 * HTTP POST.
 *
 * This function, unlike the one that accepts a QByteArray, accepts an IO device
 * from which to read the encoded data to be posted to the server in order to
 * to avoid holding the content of very large post requests, e.g. multimedia file
 * uploads, in memory.
 *
 * \a url Where to write the data.
 *
 * \a device the device to read from
 *
 * \a size Size of the encoded post data.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 *
 */
KIOCORE_EXPORT TransferJob *http_post(const QUrl &url, QIODevice *device, qint64 size = -1, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::TransferJob
 *
 * HTTP DELETE.
 *
 * Though this function servers the same purpose as KIO::file_delete, unlike
 * file_delete it accommodates HTTP specific actions such as redirections.
 *
 * \a url url resource to delete.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 *
 * \since 4.7.3
 */
KIOCORE_EXPORT TransferJob *http_delete(const QUrl &url, JobFlags flags = DefaultFlags);

}

#endif
