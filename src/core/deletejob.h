// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_DELETEJOB_H
#define KIO_DELETEJOB_H

#include <QStringList>

#include "global.h"
#include "kiocore_export.h"

#include "job_base.h"

class QTimer;

namespace KIO
{
class DeleteJobPrivate;
/*!
 * \class KIO::DeleteJob
 * \inheaderfile KIO/DeleteJob
 * \inmodule KIOCore
 *
 * \brief A more complex Job to delete files and directories.
 *
 * Don't create the job directly, but use KIO::del() instead.
 *
 * \sa KIO::del()
 */
class KIOCORE_EXPORT DeleteJob : public Job
{
    Q_OBJECT

public:
    ~DeleteJob() override;

    /*!
     * Returns the list of URLs.
     */
    QList<QUrl> urls() const;

Q_SIGNALS:

    /*!
     * Emitted when the total number of files is known.
     *
     * \a job the job that emitted this signal
     *
     * \a files the total number of files
     */
    void totalFiles(KJob *job, unsigned long files);
    /*!
     * Emitted when the total number of directories is known.
     *
     * \a job the job that emitted this signal
     *
     * \a dirs the total number of directories
     */
    void totalDirs(KJob *job, unsigned long dirs);

    /*!
     * Sends the number of processed files.
     *
     * \a job the job that emitted this signal
     *
     * \a files the number of processed files
     */
    void processedFiles(KIO::Job *job, unsigned long files);
    /*!
     * Sends the number of processed directories.
     *
     * \a job the job that emitted this signal
     *
     * \a dirs the number of processed dirs
     */
    void processedDirs(KIO::Job *job, unsigned long dirs);

    /*!
     * Sends the URL of the file that is currently being deleted.
     *
     * \a job the job that emitted this signal
     *
     * \a file the URL of the file or directory that is being
     *        deleted
     */
    void deleting(KIO::Job *job, const QUrl &file);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit DeleteJob(DeleteJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(DeleteJob)
};

/*!
 * \relates KIO::DeleteJob
 *
 * Delete a file or directory.
 *
 * \a src file to delete
 *
 * \a flags We support HideProgressInfo here
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT DeleteJob *del(const QUrl &src, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::DeleteJob
 *
 * Deletes a list of files or directories.
 *
 * \a src the files to delete
 *
 * \a flags We support HideProgressInfo here
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT DeleteJob *del(const QList<QUrl> &src, JobFlags flags = DefaultFlags);
}

#endif
