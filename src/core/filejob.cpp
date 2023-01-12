/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006 Allan Sandfeld Jensen <kde@carewolf.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "filejob.h"

#include "job_p.h"
#include "worker_p.h"

class KIO::FileJobPrivate : public KIO::SimpleJobPrivate
{
public:
    FileJobPrivate(const QUrl &url, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, CMD_OPEN, packedArgs)
        , m_open(false)
        , m_size(0)
    {
    }

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
     * Called by the scheduler when a @p worker gets to
     * work on this job.
     * @param worker the worker that starts working on this job
     */
    void start(Worker *worker) override;

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
    d->m_worker->send(CMD_READ, packedArgs);
}

void FileJob::write(const QByteArray &_data)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    d->m_worker->send(CMD_WRITE, _data);
}

void FileJob::seek(KIO::filesize_t offset)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    KIO_ARGS << KIO::filesize_t(offset);
    d->m_worker->send(CMD_SEEK, packedArgs);
}

void FileJob::truncate(KIO::filesize_t length)
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    KIO_ARGS << KIO::filesize_t(length);
    d->m_worker->send(CMD_TRUNCATE, packedArgs);
}

void FileJob::close()
{
    Q_D(FileJob);
    if (!d->m_open) {
        return;
    }

    d->m_worker->send(CMD_CLOSE);
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

// Worker sends data
void FileJobPrivate::slotData(const QByteArray &_data)
{
    Q_Q(FileJob);
    Q_EMIT q_func()->data(q, _data);
}

void FileJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(FileJob);
    // qDebug() << url;
    Q_EMIT q->redirection(q, url);
}

void FileJobPrivate::slotMimetype(const QString &type)
{
    Q_Q(FileJob);
    m_mimetype = type;
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
    // qDebug() << this << m_url;
    m_open = false;

    Q_EMIT q->fileClosed(q);

    // Return worker to the scheduler
    workerDone();
    // Scheduler::doJob(this);
    q->emitResult();
}

void FileJobPrivate::start(Worker *worker)
{
    Q_Q(FileJob);
    q->connect(worker, &KIO::WorkerInterface::data, q, [this](const QByteArray &ba) {
        slotData(ba);
    });

    q->connect(worker, &KIO::WorkerInterface::redirection, q, [this](const QUrl &url) {
        slotRedirection(url);
    });

    q->connect(worker, &KIO::WorkerInterface::mimeType, q, [this](const QString &mimeType) {
        slotMimetype(mimeType);
    });

    q->connect(worker, &KIO::WorkerInterface::open, q, [this]() {
        slotOpen();
    });

    q->connect(worker, &KIO::WorkerInterface::finished, q, [this]() {
        slotFinished();
    });

    q->connect(worker, &KIO::WorkerInterface::position, q, [this](KIO::filesize_t pos) {
        slotPosition(pos);
    });

    q->connect(worker, &KIO::WorkerInterface::truncated, q, [this](KIO::filesize_t length) {
        slotTruncated(length);
    });

    q->connect(worker, &KIO::WorkerInterface::written, q, [this](KIO::filesize_t dataWritten) {
        slotWritten(dataWritten);
    });

    q->connect(worker, &KIO::WorkerInterface::totalSize, q, [this](KIO::filesize_t size) {
        slotTotalSize(size);
    });

    SimpleJobPrivate::start(worker);
}

FileJob *KIO::open(const QUrl &url, QIODevice::OpenMode mode)
{
    // Send decoded path and encoded query
    KIO_ARGS << url << mode;
    return FileJobPrivate::newJob(url, packedArgs);
}

#include "moc_filejob.cpp"
