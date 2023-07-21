/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2014 Mathias Tillman <master.homer@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILESYSTEMFREESPACEJOB_H
#define FILESYSTEMFREESPACEJOB_H

#include "kiocore_export.h"
#include "simplejob.h"

namespace KIO
{
class FileSystemFreeSpaceJobPrivate;
/**
 * @class KIO::FileSystemFreeSpaceJob filesystemfreespacejob.h <KIO/FileSystemFreeSpaceJob>
 *
 * A KIO job that retrieves the total and available size of a filesystem.
 * @since 5.3
 */
class KIOCORE_EXPORT FileSystemFreeSpaceJob : public SimpleJob
{
    Q_OBJECT

public:
    ~FileSystemFreeSpaceJob() override;

    /**
     * Get total amount of space
     * @since 6.0
     */
    KIO::filesize_t size() const;

    /**
     * Get available amount of space
     * @since 6.0
     */
    KIO::filesize_t availableSize() const;

public:
    KIOCORE_NO_EXPORT explicit FileSystemFreeSpaceJob(FileSystemFreeSpaceJobPrivate &dd);

private:
    void slotFinished() override;
    Q_DECLARE_PRIVATE(FileSystemFreeSpaceJob)
};

/**
 * Get a filesystem's total and available space.
 *
 * @param url Url to the filesystem.
 * @return the job handling the operation.
 */
KIOCORE_EXPORT FileSystemFreeSpaceJob *fileSystemFreeSpace(const QUrl &url);

}

#endif /* FILESYSTEMFREESPACEJOB_H */
