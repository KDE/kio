/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 **/

#ifndef KIO_FILEJOB_H
#define KIO_FILEJOB_H

#include <kio/kiocore_export.h>
#include <kio/jobclasses.h>

namespace KIO {

class FileJobPrivate;
/**
 *  The file-job is an asynchronious version of normal file handling.
 *  It allows block-wise reading and writing, and allows seeking. Results are returned through signals.
 *
 *  Should always be created using KIO::open(QUrl)
 */

class KIOCORE_EXPORT FileJob : public SimpleJob
{
Q_OBJECT

public:
    ~FileJob();

    /**
     * Read block
     *
     * The slave emits the data through data().
     * @param size the requested amount of data
     */
    void read( KIO::filesize_t size );

    /**
     * Write block
     *
     * @param data the data to write
     */
    void write( const QByteArray &data );

    /**
     * Close
     *
     * Closes the file-slave
     */
    void close();

    /**
     * Seek
     *
     * The slave emits position()
     * @param offset the position from start to go to
     */
    void seek( KIO::filesize_t offset );

    /**
     * Size
     *
     * @return the file size
     */
    KIO::filesize_t size();

Q_SIGNALS:
    /**
     * Data from the slave has arrived.
     * @param job the job that emitted this signal
     * @param data data received from the slave.
     */
    void data( KIO::Job *job, const QByteArray &data );

    /**
     * Signals the file is a redirection.
     * Follow this url manually to reach data
     * @param job the job that emitted this signal
     * @param url the new URL
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /**
     * Mimetype determined.
     * @param job the job that emitted this signal
     * @param type the mime type
     */
    void mimetype( KIO::Job *job, const QString &type );

    /**
     * File is open, metadata has been determined and the
     * file-slave is ready to receive commands.
     * @param job the job that emitted this signal
     */
    void open(KIO::Job *job);

    /**
     * Bytes written to the file.
     * @param job the job that emitted this signal
     * @param written bytes written.
     */
    void written( KIO::Job *job, KIO::filesize_t written);

    /**
     * File is closed and will accept no more commands
     * @param job the job that emitted this signal
     */
    void close(KIO::Job *job);

    /**
     * The file has reached this position. Emitted after seek.
     * @param job the job that emitted this signal
     * @param offset the new position
     */
    void position( KIO::Job *job, KIO::filesize_t offset);

protected:
    FileJob(FileJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &))
    Q_PRIVATE_SLOT(d_func(), void slotData( const QByteArray &data))
    Q_PRIVATE_SLOT(d_func(), void slotMimetype( const QString &mimetype ))
    Q_PRIVATE_SLOT(d_func(), void slotOpen( ))
    Q_PRIVATE_SLOT(d_func(), void slotWritten( KIO::filesize_t ))
    Q_PRIVATE_SLOT(d_func(), void slotFinished( ))
    Q_PRIVATE_SLOT(d_func(), void slotPosition( KIO::filesize_t ))
    Q_PRIVATE_SLOT(d_func(), void slotTotalSize( KIO::filesize_t ))

    Q_DECLARE_PRIVATE(FileJob)
};

/**
 * Open ( random access I/O )
 *
 * The file-job emits open() when opened
 * @param url the URL of the file
 * @param mode the access privileges: see \ref OpenMode
 *
 * @return The file-handling job. It will never return 0. Errors are handled asynchronously
 * (emitted as signals).
 */
KIOCORE_EXPORT FileJob *open(const QUrl &url, QIODevice::OpenMode mode);

} // namespace

#endif
