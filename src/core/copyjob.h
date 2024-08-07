// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_COPYJOB_H
#define KIO_COPYJOB_H

#include <QObject>
#include <QStringList>
#include <QUrl>

#include "job_base.h"
#include "kiocore_export.h"

class QDateTime;

namespace KIO
{

class CopyJobPrivate;
/*!
 * \class KIO::CopyJob
 * \inheaderfile KIO/CopyJob
 * \inmodule KIOCore
 *
 * \brief CopyJob is used to move, copy or symlink files and directories.
 *
 * Don't create the job directly, but use KIO::copy(),
 * KIO::move(), KIO::link() and friends.
 *
 * \sa copy()
 * \sa copyAs()
 * \sa move()
 * \sa moveAs()
 * \sa link()
 * \sa linkAs()
 */
class KIOCORE_EXPORT CopyJob : public Job
{
    Q_OBJECT

public:
    /*!
     * Defines the mode of the operation
     *
     * \value Copy
     * \value Move
     * \value Link
     */
    enum CopyMode {
        Copy,
        Move,
        Link
    };

    ~CopyJob() override;

    /*!
     * Returns the mode of the operation (copy, move, or link),
     * depending on whether KIO::copy(), KIO::move() or KIO::link() was called.
     */
    CopyMode operationMode() const;

    /*!
     * Returns the list of source URLs.
     */
    QList<QUrl> srcUrls() const;

    /*!
     * Returns the destination URL.
     */
    QUrl destUrl() const;

    /*!
     * By default the permissions of the copied files will be those of the source files.
     *
     * But when copying "template" files to "new" files, people prefer the umask
     * to apply, rather than the template's permissions.
     * For that case, call setDefaultPermissions(true)
     */
    void setDefaultPermissions(bool b);

    /*!
     * Skip copying or moving any file when the destination already exists,
     * instead of the default behavior (interactive mode: showing a dialog to the user,
     * non-interactive mode: aborting with an error).
     * Initially added for a unit test.
     * \since 4.2
     */
    void setAutoSkip(bool autoSkip);

    /*!
     * Rename files automatically when the destination already exists,
     * instead of the default behavior (interactive mode: showing a dialog to the user,
     * non-interactive mode: aborting with an error).
     * Initially added for a unit test.
     * \since 4.7
     */
    void setAutoRename(bool autoRename);

    /*!
     * Reuse any directory that already exists, instead of the default behavior
     * (interactive mode: showing a dialog to the user,
     * non-interactive mode: aborting with an error).
     * \since 4.2
     */
    void setWriteIntoExistingDirectories(bool overwriteAllDirs);

    /*!
     * \reimp
     */
    bool doSuspend() override;

    /*!
     * \reimp
     */
    bool doResume() override;

Q_SIGNALS:
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
     * The job is copying a file or directory.
     *
     * Note: This signal is used for progress dialogs, it's not emitted for
     * every file or directory (this would be too slow), but every 200ms.
     *
     * \a job the job that emitted this signal
     *
     * \a src the URL of the file or directory that is currently
     *             being copied
     *
     * \a dest the destination of the current operation
     */
    void copying(KIO::Job *job, const QUrl &src, const QUrl &dest);
    /*!
     * The job is creating a symbolic link.
     *
     * Note: This signal is used for progress dialogs, it's not emitted for
     * every file or directory (this would be too slow), but every 200ms.
     *
     * \a job the job that emitted this signal
     *
     * \a target the URL of the file or directory that is currently
     *             being linked
     *
     * \a to the destination of the current operation
     */
    void linking(KIO::Job *job, const QString &target, const QUrl &to);
    /*!
     * The job is moving a file or directory.
     *
     * Note: This signal is used for progress dialogs, it's not emitted for
     * every file or directory (this would be too slow), but every 200ms.
     *
     * \a job the job that emitted this signal
     *
     * \a from the URL of the file or directory that is currently
     *             being moved
     *
     * \a to the destination of the current operation
     */
    void moving(KIO::Job *job, const QUrl &from, const QUrl &to);
    /*!
     * The job is creating the directory \a dir.
     *
     * This signal is emitted for every directory being created.
     *
     * \a job the job that emitted this signal
     *
     * \a dir the directory that is currently being created
     */
    void creatingDir(KIO::Job *job, const QUrl &dir);
    /*!
     * The user chose to rename \a from to \a to.
     *
     * \a job the job that emitted this signal
     *
     * \a from the original name
     *
     * \a to the new name
     */
    void renamed(KIO::Job *job, const QUrl &from, const QUrl &to);

    /*!
     * The job emits this signal when copying or moving a file or directory successfully finished.
     * This signal is mainly for the Undo feature.
     * If you simply want to know when a copy job is done, use result().
     *
     * \a job the job that emitted this signal
     *
     * \a from the source URL
     *
     * \a to the destination URL
     *
     * \a mtime the modification time of the source file, hopefully set on the destination file
     * too (when the KIO worker supports it).
     *
     * \a directory indicates whether a file or directory was successfully copied/moved.
     *                  true for a directory, false for file
     *
     * \a renamed indicates that the destination URL was created using a
     * rename operation (i.e. fast directory moving). true if is has been renamed
     */
    void copyingDone(KIO::Job *job, const QUrl &from, const QUrl &to, const QDateTime &mtime, bool directory, bool renamed);
    /*!
     * The job is copying or moving a symbolic link, that points to target.
     * The new link is created in \a to. The existing one is/was in \a from.
     * This signal is mainly for the Undo feature.
     *
     * \a job the job that emitted this signal
     *
     * \a from the source URL
     *
     * \a target the target
     *
     * \a to the destination URL
     */
    void copyingLinkDone(KIO::Job *job, const QUrl &from, const QString &target, const QUrl &to);
protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit CopyJob(CopyJobPrivate &dd);
    void emitResult();

private:
    Q_DECLARE_PRIVATE(CopyJob)
};

/*!
 * \relates KIO::CopyJob
 *
 * Copy a file or directory \a src into the destination \a dest,
 * which can be a file (including the final filename) or a directory
 * (into which \a src will be copied).
 *
 * This emulates the cp command completely.
 *
 * \a src the file or directory to copy
 * \a dest the destination
 * \a flags copy() supports HideProgressInfo and Overwrite.
 * Note: Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files". However if "dest" exists, then src is copied
 * into a subdir of dest, just like "cp" does. Use copyAs if you don't want that.
 *
 * Returns the job handling the operation
 *
 * \sa copyAs()
 */
KIOCORE_EXPORT CopyJob *copy(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Copy a file or directory \a src into the destination \a dest,
 * which is the destination name in any case, even for a directory.
 *
 * As opposed to copy(), this doesn't emulate cp, but is the only
 * way to copy a directory, giving it a new name and getting an error
 * box if a directory already exists with the same name (or writing the
 * contents of \a src into \a dest, when using Overwrite).
 *
 * \a src the file or directory to copy
 * \a dest the destination
 * \a flags copyAs() supports HideProgressInfo and Overwrite.
 *
 * \note Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files".
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *copyAs(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Copy a list of file/dirs \a src into a destination directory \a dest.
 *
 * \a src the list of files and/or directories
 *
 * \a dest the destination
 *
 * \a flags copy() supports HideProgressInfo and Overwrite.
 * Note: Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files". However if "dest" exists, then src is copied
 * into a subdir of dest, just like "cp" does.
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *copy(const QList<QUrl> &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Moves a file or directory \a src to the given destination \a dest.
 *
 * \a src the file or directory to copy
 *
 * \a dest the destination
 *
 * \a flags move() supports HideProgressInfo and Overwrite.
 * Note: Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files". However if "dest" exists, then src is copied
 * into a subdir of dest, just like "cp" does.
 *
 * Returns the job handling the operation
 * \sa copy()
 * \sa moveAs()
 */
KIOCORE_EXPORT CopyJob *move(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);
/*!
 * \relates KIO::CopyJob
 *
 * Moves a file or directory \a src to the given destination \a dest. Unlike move()
 * this operation will not move \a src into \a dest when \a dest exists: it will
 * either fail, or move the contents of \a src into it if Overwrite is set.
 *
 * \a src the file or directory to copy
 *
 * \a dest the destination
 *
 * \a flags moveAs() supports HideProgressInfo and Overwrite.
 * Note: Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files".
 *
 * Returns the job handling the operation
 * \sa copyAs()
 */
KIOCORE_EXPORT CopyJob *moveAs(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);
/*!
 * \relates KIO::CopyJob
 *
 * Moves a list of files or directories \a src to the given destination \a dest.
 *
 * \a src the list of files or directories to copy
 *
 * \a dest the destination
 *
 * \a flags move() supports HideProgressInfo and Overwrite.
 * Note: Overwrite has the meaning of both "write into existing directories" and
 * "overwrite existing files". However if "dest" exists, then src is copied
 * into a subdir of dest, just like "cp" does.
 *
 * Returns the job handling the operation
 * \sa copy()
 */
KIOCORE_EXPORT CopyJob *move(const QList<QUrl> &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Create a link.
 * If the protocols and hosts are the same, a Unix symlink will be created.
 * Otherwise, a .desktop file of Type Link and pointing to the src URL will be created.
 *
 * \a src The existing file or directory, 'target' of the link.
 *
 * \a destDir Destination directory where the link will be created.
 *
 * \a flags link() supports HideProgressInfo only
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *link(const QUrl &src, const QUrl &destDir, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Create several links
 * If the protocols and hosts are the same, a Unix symlink will be created.
 * Otherwise, a .desktop file of Type Link and pointing to the src URL will be created.
 *
 * \a src The existing files or directories, 'targets' of the link.
 *
 * \a destDir Destination directory where the links will be created.
 *
 * \a flags link() supports HideProgressInfo only
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *link(const QList<QUrl> &src, const QUrl &destDir, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Create a link. Unlike link() this operation will fail when \a dest is an existing
 * directory rather than the final name for the link.
 * If the protocols and hosts are the same, a Unix symlink will be created.
 * Otherwise, a .desktop file of Type Link and pointing to the src URL will be created.
 *
 * \a src The existing file or directory, 'target' of the link.
 *
 * \a dest Destination (i.e. the final symlink)
 *
 * \a flags linkAs() supports HideProgressInfo only
 *
 * Returns the job handling the operation
 * \sa link()
 * \sa copyAs()
 */
KIOCORE_EXPORT CopyJob *linkAs(const QUrl &src, const QUrl &dest, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Trash a file or directory.
 * This is currently only supported for local files and directories.
 * Use QUrl::fromLocalFile to create a URL from a local file path.
 *
 * \a src file to delete
 *
 * \a flags trash() supports HideProgressInfo only
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *trash(const QUrl &src, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::CopyJob
 *
 * Trash a list of files or directories.
 * This is currently only supported for local files and directories.
 *
 * \a src the files to delete
 *
 * \a flags trash() supports HideProgressInfo only
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT CopyJob *trash(const QList<QUrl> &src, JobFlags flags = DefaultFlags);

}

#endif
