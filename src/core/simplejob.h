/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_SIMPLEJOB_H
#define KIO_SIMPLEJOB_H

#include "job_base.h"
#include <kio/global.h> // filesize_t

namespace KIO
{
class SimpleJobPrivate;
/*!
 * \class KIO::SimpleJob
 * \inheaderfile KIO/SimpleJob
 * \inmodule KIOCore
 *
 * \brief A simple job (one url and one command).
 *
 * This is the base class for all jobs that are scheduled.
 * Other jobs are high-level jobs (CopyJob, DeleteJob, FileCopyJob...)
 * that manage subjobs but aren't scheduled directly.
 */
class KIOCORE_EXPORT SimpleJob : public KIO::Job
{
    Q_OBJECT

public:
    ~SimpleJob() override;

protected:
    bool doSuspend() override;

    bool doResume() override;

    bool doKill() override;

public:
    /*!
     * Returns the SimpleJob's URL
     */
    const QUrl &url() const;

    /*!
     * Abort job.
     * Suspends worker to be reused by another job for the same request.
     */
    virtual void putOnHold();

    /*!
     * Discard suspended worker.
     */
    static void removeOnHold();

    /*!
     * Returns true when redirections are handled internally, the default.
     */
    bool isRedirectionHandlingEnabled() const;

    /*!
     * Set \a handle to false to prevent the internal handling of redirections.
     *
     * When this flag is set, redirection requests are simply forwarded to the
     * caller instead of being handled internally.
     *
     */
    void setRedirectionHandlingEnabled(bool handle);

public Q_SLOTS:
    /*!
     * \internal
     * Called on a worker's error.
     * Made public for the scheduler.
     */
    void slotError(int, const QString &);

protected Q_SLOTS:
    /*!
     * Called when the worker marks the job
     * as finished.
     */
    virtual void slotFinished();

    /*!
     * \internal
     * Called on a worker's warning.
     */
    virtual void slotWarning(const QString &);

    /*!
     * MetaData from the worker is received.
     *
     * \a _metaData the meta data
     * \sa metaData()
     */
    virtual void slotMetaData(const KIO::MetaData &_metaData);

protected:
    /*!
     * Creates a new simple job. You don't need to use this constructor,
     * unless you create a new job that inherits from SimpleJob.
     * \internal
     */
    KIOCORE_NO_EXPORT explicit SimpleJob(SimpleJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(SimpleJob)
};

/*!
 * \relates KIO::SimpleJob
 *
 * Removes a single directory.
 *
 * The directory is assumed to be empty.
 * The job will fail if the directory is not empty.
 * Use KIO::del() (DeleteJob) to delete non-empty directories.
 *
 * \a url The URL of the directory to remove.
 *
 * Returns a pointer to the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *rmdir(const QUrl &url);

/*!
 * \relates KIO::SimpleJob
 *
 * Changes permissions on a file or directory.
 * See the other chmod in chmodjob.h for changing many files
 * or directories.
 *
 * \a url The URL of file or directory.
 *
 * \a permissions The permissions to set.
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *chmod(const QUrl &url, int permissions);

/*!
 * \relates KIO::SimpleJob
 *
 * Changes ownership and group of a file or directory.
 *
 * \a url The URL of file or directory.
 *
 * \a owner the new owner
 *
 * \a group the new group
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *chown(const QUrl &url, const QString &owner, const QString &group);

/*!
 * \relates KIO::SimpleJob
 *
 * Changes the modification time on a file or directory.
 *
 * \a url The URL of file or directory.
 *
 * \a mtime The modification time to set.
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *setModificationTime(const QUrl &url, const QDateTime &mtime);

/*!
 * \relates KIO::SimpleJob
 *
 * Rename a file or directory.
 * Warning: this operation fails if a direct renaming is not
 * possible (like with files or dirs on separate partitions)
 * Use move or file_move in this case.
 *
 * \a src The original URL
 *
 * \a dest The final URL
 *
 * \a flags Can be Overwrite here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *rename(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::SimpleJob
 *
 * Create or move a symlink.
 * This is the lowlevel operation, similar to file_copy and file_move.
 * It doesn't do any check (other than those the worker does)
 * and it doesn't show rename and skip dialogs - use KIO::link for that.
 *
 * \a target The string that will become the "target" of the link (can be relative)
 *
 * \a dest The symlink to create.
 *
 * \a flags Can be Overwrite and HideProgressInfo
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *symlink(const QString &target, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::SimpleJob
 *
 * Execute any command that is specific to one worker (protocol).
 *
 * Examples are : HTTP POST, mount and unmount (kio_file)
 *
 * \a url The URL isn't passed to the worker, but is used to know
 *        which worker to send it to :-)
 *
 * \a data Packed data.  The meaning is completely dependent on the
 *        worker, but usually starts with an int for the command number.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *special(const QUrl &url, const QByteArray &data, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::SimpleJob
 *
 * Mount filesystem.
 *
 * Special job for kio_file.
 *
 * \a ro Mount read-only if \c true.
 *
 * \a fstype File system type (e.g. "ext2", can be empty).
 *
 * \a dev Device (e.g. /dev/sda0).
 *
 * \a point Mount point, can be \c null.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *mount(bool ro, const QByteArray &fstype, const QString &dev, const QString &point, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::SimpleJob
 *
 * Unmount filesystem.
 *
 * Special job for kio_file.
 *
 * \a point Point to unmount.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *unmount(const QString &point, JobFlags flags = DefaultFlags);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(6, 9)
/*!
 * \relates KIO::SimpleJob
 *
 * HTTP cache update
 *
 * Not implemented.
 *
 * \a url Url to update, protocol must be "http".
 *
 * \a no_cache If true, cache entry for \c url is deleted.
 *
 * \a expireDate Local machine time indicating when the entry is
 * supposed to expire.
 *
 * Returns the job handling the operation.
 * \deprecated[6.9]
 */
KIOCORE_DEPRECATED_VERSION(6, 9, "Not implemented")
KIOCORE_EXPORT SimpleJob *http_update_cache(const QUrl &url, bool no_cache, const QDateTime &expireDate);
#endif

/*!
 * \relates KIO::SimpleJob
 *
 * Delete a single file.
 *
 * \a src File to delete.
 *
 * \a flags Can be HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *file_delete(const QUrl &src, JobFlags flags = DefaultFlags);

}

#endif
