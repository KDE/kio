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
/**
 * @class KIO::SimpleJob simplejob.h <KIO/SimpleJob>
 *
 * A simple job (one url and one command).
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
    /**
     * Suspend this job
     * @see resume
     */
    bool doSuspend() override;

    /**
     * Resume this job
     * @see suspend
     */
    bool doResume() override;

    /**
     * Abort job.
     * This kills all subjobs and deletes the job.
     */
    bool doKill() override;

public:
    /**
     * Returns the SimpleJob's URL
     * @return the url
     */
    const QUrl &url() const;

    /**
     * Abort job.
     * Suspends slave to be reused by another job for the same request.
     */
    virtual void putOnHold();

    /**
     * Discard suspended slave.
     */
    static void removeOnHold();

    /**
     * Returns true when redirections are handled internally, the default.
     *
     * @since 4.4
     */
    bool isRedirectionHandlingEnabled() const;

    /**
     * Set @p handle to false to prevent the internal handling of redirections.
     *
     * When this flag is set, redirection requests are simply forwarded to the
     * caller instead of being handled internally.
     *
     * @since 4.4
     */
    void setRedirectionHandlingEnabled(bool handle);

public Q_SLOTS:
    /**
     * @internal
     * Called on a slave's error.
     * Made public for the scheduler.
     */
    void slotError(int, const QString &);

protected Q_SLOTS:
    /**
     * Called when the slave marks the job
     * as finished.
     */
    virtual void slotFinished();

    /**
     * @internal
     * Called on a slave's warning.
     */
    virtual void slotWarning(const QString &);

    /**
     * MetaData from the slave is received.
     * @param _metaData the meta data
     * @see metaData()
     */
    virtual void slotMetaData(const KIO::MetaData &_metaData);

protected:
    /**
     * Allow jobs that inherit SimpleJob and are aware
     * of redirections to store the SSL session used.
     * Retrieval is handled by SimpleJob::start
     * @param m_redirectionURL Reference to redirection URL,
     * used instead of m_url if not empty
     */
    void storeSSLSessionFromJob(const QUrl &m_redirectionURL);

    /**
     * Creates a new simple job. You don't need to use this constructor,
     * unless you create a new job that inherits from SimpleJob.
     */
    SimpleJob(SimpleJobPrivate &dd);
private:

    Q_DECLARE_PRIVATE(SimpleJob)
};

/**
 * Removes a single directory.
 *
 * The directory is assumed to be empty.
 * The job will fail if the directory is not empty.
 * Use KIO::del() (DeleteJob) to delete non-empty directories.
 *
 * @param url The URL of the directory to remove.
 * @return A pointer to the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *rmdir(const QUrl &url);

/**
 * Changes permissions on a file or directory.
 * See the other chmod in chmodjob.h for changing many files
 * or directories.
 *
 * @param url The URL of file or directory.
 * @param permissions The permissions to set.
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *chmod(const QUrl &url, int permissions);

/**
 * Changes ownership and group of a file or directory.
 *
 * @param url The URL of file or directory.
 * @param owner the new owner
 * @param group the new group
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *chown(const QUrl &url, const QString &owner, const QString &group);

/**
 * Changes the modification time on a file or directory.
 *
 * @param url The URL of file or directory.
 * @param mtime The modification time to set.
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *setModificationTime(const QUrl &url, const QDateTime &mtime);

/**
 * Rename a file or directory.
 * Warning: this operation fails if a direct renaming is not
 * possible (like with files or dirs on separate partitions)
 * Use move or file_move in this case.
 *
 * @param src The original URL
 * @param dest The final URL
 * @param flags Can be Overwrite here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *rename(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/**
 * Create or move a symlink.
 * This is the lowlevel operation, similar to file_copy and file_move.
 * It doesn't do any check (other than those the slave does)
 * and it doesn't show rename and skip dialogs - use KIO::link for that.
 * @param target The string that will become the "target" of the link (can be relative)
 * @param dest The symlink to create.
 * @param flags Can be Overwrite and HideProgressInfo
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *symlink(const QString &target, const QUrl &dest, JobFlags flags = DefaultFlags);

/**
 * Execute any command that is specific to one slave (protocol).
 *
 * Examples are : HTTP POST, mount and unmount (kio_file)
 *
 * @param url The URL isn't passed to the slave, but is used to know
 *        which slave to send it to :-)
 * @param data Packed data.  The meaning is completely dependent on the
 *        slave, but usually starts with an int for the command number.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *special(const QUrl &url, const QByteArray &data, JobFlags flags = DefaultFlags);

/**
 * Mount filesystem.
 *
 * Special job for @p kio_file.
 *
 * @param ro Mount read-only if @p true.
 * @param fstype File system type (e.g. "ext2", can be empty).
 * @param dev Device (e.g. /dev/sda0).
 * @param point Mount point, can be @p null.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *mount(bool ro, const QByteArray &fstype, const QString &dev, const QString &point, JobFlags flags = DefaultFlags);

/**
 * Unmount filesystem.
 *
 * Special job for @p kio_file.
 *
 * @param point Point to unmount.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *unmount(const QString &point, JobFlags flags = DefaultFlags);

/**
 * HTTP cache update
 *
 * @param url Url to update, protocol must be "http".
 * @param no_cache If true, cache entry for @p url is deleted.
 * @param expireDate Local machine time indicating when the entry is
 * supposed to expire.
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *http_update_cache(const QUrl &url, bool no_cache, const QDateTime &expireDate);

/**
 * Delete a single file.
 *
 * @param src File to delete.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT SimpleJob *file_delete(const QUrl &src, JobFlags flags = DefaultFlags);

}

#endif
