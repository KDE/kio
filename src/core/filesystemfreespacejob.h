/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>
                  2014 Mathias Tillman <master.homer@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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

Q_SIGNALS:
    /**
     * Signals the result
     * @param job the job that is redirected
     * @param size total amount of space
     * @param available amount of free space
     */
    void result(KIO::Job *job, KIO::filesize_t size, KIO::filesize_t available);

protected Q_SLOTS:
    void slotFinished() override;

public:
    FileSystemFreeSpaceJob(FileSystemFreeSpaceJobPrivate &dd);

private:
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
