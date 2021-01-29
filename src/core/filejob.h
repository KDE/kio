/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <kde@carewolf.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_FILEJOB_H
#define KIO_FILEJOB_H

#include "kiocore_export.h"
#include "simplejob.h"

namespace KIO
{

class FileJobPrivate;
/**
 * @class KIO::FileJob filejob.h <KIO/FileJob>
 *
 *  The file-job is an asynchronous version of normal file handling.
 *  It allows block-wise reading and writing, and allows seeking and truncation. Results are returned through signals.
 *
 *  Should always be created using KIO::open(QUrl)
 */

class KIOCORE_EXPORT FileJob : public SimpleJob
{
    Q_OBJECT

public:
    ~FileJob();

    /**
     * This function attempts to read up to \p size bytes from the URL passed to
     * KIO::open() and returns the bytes received via the data() signal.
     *
     * The read operation commences at the current file offset, and the file
     * offset is incremented by the number of bytes read, but this change in the
     * offset does not result in the position() signal being emitted.
     *
     * If the current file offset is at or past the end of file (i.e. EOD), no
     * bytes are read, and the data() signal returns an empty QByteArray.
     *
     * On error the data() signal is not emitted. To catch errors please connect
     * to the result() signal.
     *
     * @param size the requested amount of data to read
     *
     */
    void read(KIO::filesize_t size);

    /**
     * This function attempts to write all the bytes in \p data to the URL
     * passed to KIO::open() and returns the bytes written received via the
     * written() signal.
     *
     * The write operation commences at the current file offset, and the file
     * offset is incremented by the number of bytes read, but this change in the
     * offset does not result in the position() being emitted.
     *
     * On error the written() signal is not emitted. To catch errors please
     * connect to the result() signal.
     *
     * @param data the data to write
     */
    void write(const QByteArray &data);

    /**
     * Closes the file-slave
     *
     * The slave emits close() and result().
     */
    void close();

    /**
     * Seek
     *
     * The slave emits position() on successful seek to the specified \p offset.
     *
     * On error the position() signal is not emitted. To catch errors please
     * connect to the result() signal.
     *
     * @param offset the position from start to go to
     */
    void seek(KIO::filesize_t offset);

    /**
     * Truncate
     *
     * The slave emits truncated() on successful truncation to the specified \p length.
     *
     * On error the truncated() signal is not emitted. To catch errors please
     * connect to the result() signal.
     *
     * @param length the desired length to truncate to
     * @since 5.66
     */
    void truncate(KIO::filesize_t length);

    /**
     * Size
     *
     * @return the file size
     */
    KIO::filesize_t size();

Q_SIGNALS:
    /**
     * Data from the slave has arrived. Emitted after read().
     *
     * Unless a read() request was sent for 0 bytes, End of data (EOD) has been
     * reached if data.size() == 0
     *
     * @param job the job that emitted this signal
     * @param data data received from the slave.
     *
     */
    void data(KIO::Job *job, const QByteArray &data);

    /**
     * Signals the file is a redirection.
     * Follow this url manually to reach data
     * @param job the job that emitted this signal
     * @param url the new URL
     */
    void redirection(KIO::Job *job, const QUrl &url);

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
     * File is open, metadata has been determined and the
     * file-slave is ready to receive commands.
     * @param job the job that emitted this signal
     */
    void open(KIO::Job *job);

    /**
     * \p written bytes were written to the file. Emitted after write().
     * @param job the job that emitted this signal
     * @param written bytes written.
     */
    void written(KIO::Job *job, KIO::filesize_t written);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Signals that the file is closed and will accept no more commands.
     *
     * @param job the job that emitted this signal
     *
     * @deprecated since 5.79, use KIO::FileJob::fileClosed(KIO::Job *)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use KIO::FileJob::fileClosed(KIO::Job *)")
    void close(KIO::Job *job);
#endif

    /**
     * Signals that the file is closed and will accept no more commands.
     *
     * @param job the job that emitted this signal
     *
     * @since 5.79
     */
    void fileClosed(KIO::Job *job);

    /**
     * The file has reached this position. Emitted after seek().
     * @param job the job that emitted this signal
     * @param offset the new position
     */
    void position(KIO::Job *job, KIO::filesize_t offset);

    /**
     * The file has been truncated to this point. Emitted after truncate().
     * @param job the job that emitted this signal
     * @param length the new length of the file
     * @since 5.66
     */
    void truncated(KIO::Job *job, KIO::filesize_t length);

protected:
    FileJob(FileJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &))
    Q_PRIVATE_SLOT(d_func(), void slotData(const QByteArray &data))
    Q_PRIVATE_SLOT(d_func(), void slotMimetype(const QString &mimetype))
    Q_PRIVATE_SLOT(d_func(), void slotOpen())
    Q_PRIVATE_SLOT(d_func(), void slotWritten(KIO::filesize_t))
    Q_PRIVATE_SLOT(d_func(), void slotFinished())
    Q_PRIVATE_SLOT(d_func(), void slotPosition(KIO::filesize_t))
    Q_PRIVATE_SLOT(d_func(), void slotTruncated(KIO::filesize_t))
    Q_PRIVATE_SLOT(d_func(), void slotTotalSize(KIO::filesize_t))

    Q_DECLARE_PRIVATE(FileJob)
};

/**
 * Open ( random access I/O )
 *
 * The file-job emits open() when opened
 *
 * On error the open() signal is not emitted. To catch errors please
 * connect to the result() signal.
 *
 * @param url the URL of the file
 * @param mode the access privileges: see \ref OpenMode
 *
 * @return The file-handling job. It will never return 0. Errors are handled asynchronously
 * (emitted as signals).
 */
KIOCORE_EXPORT FileJob *open(const QUrl &url, QIODevice::OpenMode mode);

} // namespace

#endif
