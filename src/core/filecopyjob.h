/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_FILECOPYJOB_H
#define KIO_FILECOPYJOB_H

#include "job_base.h"
#include <kio/global.h> // filesize_t

namespace KIO
{
class FileCopyJobPrivate;
/*!
 * \class KIO::FileCopyJob
 * \inheaderfile KIO/FileCopyJob
 * \inmodule KIOCore
 *
 * The FileCopyJob copies data from one place to another.
 *
 * \sa KIO::file_copy()
 * \sa KIO::file_move()
 */
class KIOCORE_EXPORT FileCopyJob : public Job
{
    Q_OBJECT

public:
    ~FileCopyJob() override;
    /*!
     * If you know the size of the source file, call this method
     * to inform this job. It will be displayed in the "resume" dialog.
     *
     * \a size the size of the source file
     */
    void setSourceSize(KIO::filesize_t size);

    /*!
     * Sets the modification time of the file
     *
     * Note that this is ignored if a direct copy (WorkerBase::copy) can be done,
     * in which case the mtime of the source is applied to the destination (if the protocol
     * supports the concept).
     */
    void setModificationTime(const QDateTime &mtime);

    /*!
     * Returns the source URL
     */
    QUrl srcUrl() const;

    /*!
     * Returns the destination URL
     */
    QUrl destUrl() const;

    bool doSuspend() override;
    bool doResume() override;
    bool doKill() override;

Q_SIGNALS:
    /*!
     * MIME type determined during a file copy.
     * This is never emitted during a move, and might not be emitted during
     * a file copy, depending on the worker. But when a get and a put are
     * being used (which is the common case), this signal forwards the
     * MIME type information from the get job.
     *
     * \a job the job that emitted this signal
     *
     * \a mimeType the MIME type
     *
     * \since 5.78
     */
    void mimeTypeFound(KIO::Job *job, const QString &mimeType);

protected Q_SLOTS:
    /*!
     * Called whenever a subjob finishes.
     *
     * \a job the job that emitted this signal
     */
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit FileCopyJob(FileCopyJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(FileCopyJob)
};

/*!
 * \relates KIO::FileCopyJob
 *
 * Copy a single file.
 *
 * Uses either WorkerBase::copy() if the worker supports that
 * or get() and put() otherwise.
 *
 * \a src Where to get the file
 *
 * \a dest Where to put the file
 *
 * \a permissions the file mode permissions to set on \a dest; if this is -1
 * (the default) no special permissions will be set on \a dest, i.e. it'll have
 * the default system permissions for newly created files, and the owner and group
 * permissions are not preserved.
 *
 * \a flags Can be JobFlag::HideProgressInfo, Overwrite and Resume here
 * WARNING: Setting JobFlag::Resume means that the data will be appended to
 * \a dest if \a dest exists
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT FileCopyJob *file_copy(const QUrl &src, const QUrl &dest, int permissions = -1, JobFlags flags = DefaultFlags);

/*!
 * \relates KIO::FileCopyJob
 *
 * Overload for catching code mistakes. Do NOT call this method (it is not implemented),
 * insert a value for permissions (-1 by default) before the JobFlags.
 */
FileCopyJob *file_copy(const QUrl &src, const QUrl &dest, JobFlags flags) Q_DECL_EQ_DELETE; // not implemented - on purpose.

/*!
 * \relates KIO::FileCopyJob
 *
 * Move a single file.
 *
 * Use either WorkerBase::rename() if the worker supports that,
 * or copy() and del() otherwise, or eventually get() & put() & del()
 *
 * \a src Where to get the file
 *
 * \a dest Where to put the file
 *
 * \a permissions the file mode permissions to set on \a dest; if this is -1
 * (the default), no special permissions are set on \a dest, i.e. it'll have
 * the default system permissions for newly created files, and the owner and group
 * permissions are not preserved.
 *
 * \a flags Can be HideProgressInfo, Overwrite and Resume here
 * WARNING: Setting JobFlag::Resume means that the data will be appended to
 * \a dest if \a dest exists
 *
 * Returns the job handling the operation
 */
KIOCORE_EXPORT FileCopyJob *file_move(const QUrl &src, const QUrl &dest, int permissions = -1, JobFlags flags = DefaultFlags);

/*
 * Overload for catching code mistakes. Do NOT call this method (it is not implemented),
 * insert a value for permissions (-1 by default) before the JobFlags.
 */
FileCopyJob *file_move(const QUrl &src, const QUrl &dest, JobFlags flags) Q_DECL_EQ_DELETE; // not implemented - on purpose.

}

#endif
