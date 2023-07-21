/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2014 Mathias Tillman <master.homer@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "filesystemfreespacejob.h"
#include "job.h"
#include "job_p.h"
#include <worker_p.h>

using namespace KIO;

class KIO::FileSystemFreeSpaceJobPrivate : public SimpleJobPrivate
{
public:
    FileSystemFreeSpaceJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
    {
    }

    /**
     * @internal
     * Called by the scheduler when a @p worker gets to
     * work on this job.
     * @param worker the worker that starts working on this job
     */
    void start(Worker *worker) override;

    Q_DECLARE_PUBLIC(FileSystemFreeSpaceJob)

    static inline FileSystemFreeSpaceJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs)
    {
        FileSystemFreeSpaceJob *job = new FileSystemFreeSpaceJob(*new FileSystemFreeSpaceJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
    KIO::filesize_t size = -1;
    KIO::filesize_t availableSize = -1;
};

FileSystemFreeSpaceJob::FileSystemFreeSpaceJob(FileSystemFreeSpaceJobPrivate &dd)
    : SimpleJob(dd)
{
}

FileSystemFreeSpaceJob::~FileSystemFreeSpaceJob()
{
}
KIO::filesize_t FileSystemFreeSpaceJob::size() const
{
    Q_D(const FileSystemFreeSpaceJob);
    return d->size;
}
KIO::filesize_t FileSystemFreeSpaceJob::availableSize() const
{
    Q_D(const FileSystemFreeSpaceJob);
    return d->availableSize;
}

void FileSystemFreeSpaceJobPrivate::start(Worker *worker)
{
    SimpleJobPrivate::start(worker);
}

void FileSystemFreeSpaceJob::slotFinished()
{
    Q_D(FileSystemFreeSpaceJob);
    const QString totalStr = queryMetaData(QStringLiteral("total"));
    const QString availableStr = queryMetaData(QStringLiteral("available"));

    if (availableStr.isEmpty()) { // CopyJob only cares for available. "total" is optional
        setError(KIO::ERR_UNSUPPORTED_ACTION);
    }
    d->size = totalStr.toULongLong();
    d->availableSize = availableStr.toULongLong();

    // Return worker to the scheduler
    SimpleJob::slotFinished();
}

KIO::FileSystemFreeSpaceJob *KIO::fileSystemFreeSpace(const QUrl &url)
{
    KIO_ARGS << url;
    return FileSystemFreeSpaceJobPrivate::newJob(url, CMD_FILESYSTEMFREESPACE, packedArgs);
}

#include "moc_filesystemfreespacejob.cpp"
