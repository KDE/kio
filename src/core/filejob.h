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
/*!
 * \class KIO::FileJob
 * \inmodule KIOCore
 * \inheaderfile KIO/FileJob
 *
 * The file-job is an asynchronous version of normal file handling.
 * It allows block-wise reading and writing, and allows seeking and truncation. Results are returned through signals.
 *
 *  Should always be created using KIO::open(const QUrl&, QIODevice::OpenMode).
 */

class KIOCORE_EXPORT FileJob : public SimpleJob
{
    Q_OBJECT

public:
    ~FileJob() override;

    /*!
     * This function attempts to read up to \a size bytes from the URL passed to
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
     */
    void read(KIO::filesize_t size);

    /*!
     * This function attempts to write all the bytes in \a data to the URL
     * passed to KIO::open() and returns the bytes written received via the
     * written() signal.
     *
     * The write operation commences at the current file offset, and the file
     * offset is incremented by the number of bytes read, but this change in the
     * offset does not result in the position() being emitted.
     *
     * On error the written() signal is not emitted. To catch errors please
     * connect to the result() signal.
     */
    void write(const QByteArray &data);

    /*!
     * Closes the file KIO worker.
     *
     * The worker emits close() and result().
     */
    void close();

    /*!
     * Seek
     *
     * The worker emits position() on successful seek to the specified \a offset.
     *
     * On error the position() signal is not emitted. To catch errors please
     * connect to the result() signal.
     */
    void seek(KIO::filesize_t offset);

    /*!
     * Truncate
     *
     * The worker emits truncated() on successful truncation to the specified \a length.
     *
     * On error the truncated() signal is not emitted. To catch errors please
     * connect to the result() signal.
     *
     * \since 5.66
     */
    void truncate(KIO::filesize_t length);

    /*!
     * Returns the file size
     */
    KIO::filesize_t size();

Q_SIGNALS:
    /*!
     * Data from the worker has arrived. Emitted after read().
     *
     * Unless a read() request was sent for 0 bytes, End of data (EOD) has been
     * reached if data.size() == 0
     *
     * \a job the job that emitted this signal
     *
     * \a data data received from the worker.
     *
     */
    void data(KIO::Job *job, const QByteArray &data);

    /*!
     * Signals the file is a redirection.
     * Follow this url manually to reach data
     *
     * \a job the job that emitted this signal
     *
     * \a url the new URL
     */
    void redirection(KIO::Job *job, const QUrl &url);

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
     * File is open, metadata has been determined and the
     * file KIO worker is ready to receive commands.
     *
     * \a job the job that emitted this signal
     */
    void open(KIO::Job *job);

    /*!
     * \a written bytes were written to the file. Emitted after write().
     *
     * \a job the job that emitted this signal
     *
     * \a written bytes written.
     */
    void written(KIO::Job *job, KIO::filesize_t written);

    /*!
     * Signals that the file is closed and will accept no more commands.
     *
     * \a job the job that emitted this signal
     *
     * \since 5.79
     */
    void fileClosed(KIO::Job *job);

    /*!
     * The file has reached this position. Emitted after seek().
     *
     * \a job the job that emitted this signal
     *
     * \a offset the new position
     */
    void position(KIO::Job *job, KIO::filesize_t offset);

    /*!
     * The file has been truncated to this point. Emitted after truncate().
     *
     * \a job the job that emitted this signal
     *
     * \a length the new length of the file
     *
     * \since 5.66
     */
    void truncated(KIO::Job *job, KIO::filesize_t length);

protected:
    KIOCORE_NO_EXPORT explicit FileJob(FileJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(FileJob)
};

/*!
 * \relates KIO::FileJob
 *
 * Open ( random access I/O )
 *
 * The file-job emits open() when opened
 *
 * On error the open() signal is not emitted. To catch errors please
 * connect to the result() signal.
 *
 * \a url the URL of the file
 *
 * \a mode the access privileges: see OpenMode
 *
 * Returns the file-handling job. It will never return 0. Errors are handled asynchronously
 * (emitted as signals).
 */
KIOCORE_EXPORT FileJob *open(const QUrl &url, QIODevice::OpenMode mode);

} // namespace

#endif
