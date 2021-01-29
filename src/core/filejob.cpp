/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <kde@carewolf.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "filejob.h"

#include "slavebase.h"
#include "scheduler.h"
#include "slave.h"


#include "job_p.h"

class KIO::FileJobPrivate: public KIO::SimpleJobPrivate
{
public:
    FileJobPrivate(const QUrl &url, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, CMD_OPEN, packedArgs), m_open(false), m_size(0)
    {}

    bool m_open;
    QString m_mimetype;
    KIO::filesize_t m_size;

    void slotRedirection(const QUrl &url);
    void slotData(const QByteArray &data);
    void slotMimetype(const QString &mimetype);
    void slotOpen();
    void slotWritten(KIO::filesize_t);
    void slotFinished();
    void slotPosition(KIO::filesize_t);
    void slotTruncated(KIO::filesize_t);
    void slotTotalSize(KIO::filesize_t);

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) override;

    Q_DECLARE_PUBLIC(FileJob)

    static inline FileJob *newJob(const QUrl &url, const QByteArray &packedArgs)
    {
        FileJob *job = new FileJob(*new FileJobPrivate(url, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

using namespace KIO;

FileJob::FileJob(FileJobPrivate &dd)
    : SimpleJob(dd)
{
}

FileJob::~FileJob()
{
}

void FileJob::read(KIO::filesize_t size)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    KIO_ARGS << size;
    d->m_slave->send(CMD_READ, packedArgs);
}

void FileJob::write(const QByteArray &_data)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    d->m_slave->send(CMD_WRITE, _data);
}

void FileJob::seek(KIO::filesize_t offset)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    KIO_ARGS << KIO::filesize_t(offset);
    d->m_slave->send(CMD_SEEK, packedArgs);
}

void FileJob::truncate(KIO::filesize_t length)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    KIO_ARGS << KIO::filesize_t(length);
    d->m_slave->send(CMD_TRUNCATE, packedArgs);
}

void FileJob::close()
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    d->m_slave->send(CMD_CLOSE);
    // ###  close?
}

KIO::filesize_t FileJob::size()
{
    Q_D(FileJob);
    if (!d->m_open) {
        return 0;
    }

    return d->m_size;
}

// Slave sends data
void FileJobPrivate::slotData(const QByteArray &_data)
{
    Q_Q(FileJob);
    Q_EMIT q_func()->data(q, _data);
}

void FileJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(FileJob);
    //qDebug() << url;
    Q_EMIT q->redirection(q, url);
}

void FileJobPrivate::slotMimetype(const QString &type)
{
    Q_Q(FileJob);
    m_mimetype = type;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    Q_EMIT q->mimetype(q, m_mimetype);
#endif
    Q_EMIT q->mimeTypeFound(q, m_mimetype);
}

void FileJobPrivate::slotPosition(KIO::filesize_t pos)
{
    Q_Q(FileJob);
    Q_EMIT q->position(q, pos);
}

void FileJobPrivate::slotTruncated(KIO::filesize_t length)
{
    Q_Q(FileJob);
    Q_EMIT q->truncated(q, length);
}

void FileJobPrivate::slotTotalSize(KIO::filesize_t t_size)
{
    m_size = t_size;
    Q_Q(FileJob);
    q->setTotalAmount(KJob::Bytes, m_size);
}

void FileJobPrivate::slotOpen()
{
    Q_Q(FileJob);
    m_open = true;
    Q_EMIT q->open(q);
}

void FileJobPrivate::slotWritten(KIO::filesize_t t_written)
{
    Q_Q(FileJob);
    Q_EMIT q->written(q, t_written);
}

void FileJobPrivate::slotFinished()
{
    Q_Q(FileJob);
    //qDebug() << this << m_url;
    m_open = false;

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    Q_EMIT q->close(q);
#endif
    Q_EMIT q->fileClosed(q);

    // Return slave to the scheduler
    slaveDone();
    // Scheduler::doJob(this);
    q->emitResult();
}

void FileJobPrivate::start(Slave *slave)
{
    Q_Q(FileJob);
    q->connect(slave, &KIO::SlaveInterface::data, q, [this](const QByteArray &ba) { slotData(ba); });

    q->connect(slave, &KIO::SlaveInterface::redirection, q, [this](const QUrl &url) { slotRedirection(url); });

    q->connect(slave, &KIO::SlaveInterface::mimeType, q, [this](const QString &mimeType) { slotMimetype(mimeType); });

    q->connect(slave, &KIO::SlaveInterface::open, q, [this]() { slotOpen(); });

    q->connect(slave, &KIO::SlaveInterface::finished, q, [this]() { slotFinished(); });

    q->connect(slave, &KIO::SlaveInterface::position, q, [this](KIO::filesize_t pos) { slotPosition(pos); });

    q->connect(slave, &KIO::SlaveInterface::truncated, q, [this](KIO::filesize_t length) { slotTruncated(length); });

    q->connect(slave, &KIO::SlaveInterface::written, q, [this](KIO::filesize_t dataWritten) { slotWritten(dataWritten); });

    q->connect(slave, &KIO::SlaveInterface::totalSize, q, [this](KIO::filesize_t size) { slotTotalSize(size); });

    SimpleJobPrivate::start(slave);
}

FileJob *KIO::open(const QUrl &url, QIODevice::OpenMode mode)
{
    // Send decoded path and encoded query
    KIO_ARGS << url << mode;
    return FileJobPrivate::newJob(url, packedArgs);
}

#include "moc_filejob.cpp"

