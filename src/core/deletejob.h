// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright 2000       Stephan Kulow <coolo@kde.org>
    Copyright 2000-2006  David Faure <faure@kde.org>

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

#ifndef KIO_DELETEJOB_H
#define KIO_DELETEJOB_H

#include <QStringList>

#include "kiocore_export.h"
#include "global.h"

#include "job_base.h"

class QTimer;

namespace KIO
{

class DeleteJobPrivate;
/**
 * @class KIO::DeleteJob deletejob.h <KIO/DeleteJob>
 *
 * A more complex Job to delete files and directories.
 * Don't create the job directly, but use KIO::del() instead.
 *
 * @see KIO::del()
 */
class KIOCORE_EXPORT DeleteJob : public Job
{
    Q_OBJECT

public:
    ~DeleteJob() override;

    /**
     * Returns the list of URLs.
     * @return the list of URLs.
     */
    QList<QUrl> urls() const;

Q_SIGNALS:

    /**
    * Emitted when the total number of files is known.
     * @param job the job that emitted this signal
     * @param files the total number of files
     */
    void totalFiles(KJob *job, unsigned long files);
    /**
    * Emitted when the total number of directories is known.
     * @param job the job that emitted this signal
     * @param dirs the total number of directories
     */
    void totalDirs(KJob *job, unsigned long dirs);

    /**
    * Sends the number of processed files.
     * @param job the job that emitted this signal
     * @param files the number of processed files
     */
    void processedFiles(KIO::Job *job, unsigned long files);
    /**
    * Sends the number of processed directories.
     * @param job the job that emitted this signal
     * @param dirs the number of processed dirs
     */
    void processedDirs(KIO::Job *job, unsigned long dirs);

    /**
    * Sends the URL of the file that is currently being deleted.
     * @param job the job that emitted this signal
     * @param file the URL of the file or directory that is being
     *        deleted
     */
    void deleting(KIO::Job *job, const QUrl &file);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    DeleteJob(DeleteJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotStart())
    Q_PRIVATE_SLOT(d_func(), void slotEntries(KIO::Job *, const KIO::UDSEntryList &list))
    Q_PRIVATE_SLOT(d_func(), void slotReport())
    Q_DECLARE_PRIVATE(DeleteJob)
};

/**
 * Delete a file or directory.
 *
 * @param src file to delete
 * @param flags We support HideProgressInfo here
 * @return the job handling the operation
 */
KIOCORE_EXPORT DeleteJob *del(const QUrl &src, JobFlags flags = DefaultFlags);

/**
 * Deletes a list of files or directories.
 *
 * @param src the files to delete
 * @param flags We support HideProgressInfo here
 * @return the job handling the operation
 */
KIOCORE_EXPORT DeleteJob *del(const QList<QUrl> &src, JobFlags flags = DefaultFlags);
}

#endif
