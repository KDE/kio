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
#include <slave.h>

using namespace KIO;

class KIO::FileSystemFreeSpaceJobPrivate: public SimpleJobPrivate
{
public:
    FileSystemFreeSpaceJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
    { }

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) override;

    Q_DECLARE_PUBLIC(FileSystemFreeSpaceJob)

    static inline FileSystemFreeSpaceJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs)
    {
        FileSystemFreeSpaceJob *job = new FileSystemFreeSpaceJob(*new FileSystemFreeSpaceJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

FileSystemFreeSpaceJob::FileSystemFreeSpaceJob(FileSystemFreeSpaceJobPrivate &dd)
    : SimpleJob(dd)
{
}

FileSystemFreeSpaceJob::~FileSystemFreeSpaceJob()
{
}

void FileSystemFreeSpaceJobPrivate::start(Slave *slave)
{
    SimpleJobPrivate::start(slave);
}

void FileSystemFreeSpaceJob::slotFinished()
{
    const QString totalStr = queryMetaData(QStringLiteral("total"));
    const QString availableStr = queryMetaData(QStringLiteral("available"));

    if (availableStr.isEmpty()) { // CopyJob only cares for available. "total" is optional
        setError(KIO::ERR_UNSUPPORTED_ACTION);
    }
    const KIO::filesize_t total = totalStr.toULongLong();
    const KIO::filesize_t available = availableStr.toULongLong();
    Q_EMIT result(this, total, available);

    // Return slave to the scheduler
    SimpleJob::slotFinished();
}

KIO::FileSystemFreeSpaceJob *KIO::fileSystemFreeSpace(const QUrl &url)
{
    KIO_ARGS << url;
    return FileSystemFreeSpaceJobPrivate::newJob(url, CMD_FILESYSTEMFREESPACE, packedArgs);
}

#include "moc_filesystemfreespacejob.cpp"
