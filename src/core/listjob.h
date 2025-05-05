/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_LISTJOB_H
#define KIO_LISTJOB_H

#include "simplejob.h"
#include <kio/udsentry.h>

namespace KIO
{
class ListJobPrivate;
/*!
 * \class KIO::ListJob
 * \inmodule KIOCore
 * \inheaderfile KIO/ListJob
 *
 * \brief A ListJob is allows you to get the get the content of a directory.
 *
 * Don't create the job directly, but use KIO::listRecursive() or
 * KIO::listDir() instead.
 *
 * \sa KIO::listRecursive()
 * \sa KIO::listDir()
 */
class KIOCORE_EXPORT ListJob : public SimpleJob
{
    Q_OBJECT

public:
    ~ListJob() override;

    /*!
     * \value IncludeHidden Include hidden files in the listing.
     */
    enum class ListFlag {
        IncludeHidden = 1 << 0,
    };
    Q_DECLARE_FLAGS(ListFlags, ListFlag)

    /*!
     * Returns the ListJob's redirection URL. This will be invalid if there
     * was no redirection.
     */
    const QUrl &redirectionUrl() const;

    /*!
     * Do not apply any KIOSK restrictions to this job.
     */
    void setUnrestricted(bool unrestricted);

Q_SIGNALS:
    /*!
     * This signal emits the entry found by the job while listing.
     * The progress signals aren't specific to ListJob. It simply
     * uses SimpleJob's processedSize (number of entries listed) and
     * totalSize (total number of entries, if known),
     * as well as percent.
     *
     * \a job the job that emitted this signal
     *
     * \a list the list of UDSEntries
     */
    void entries(KIO::Job *job, const KIO::UDSEntryList &list); // TODO KDE5: use KIO::ListJob* argument to avoid casting

    /*!
     * This signal is emitted when a sub-directory could not be listed.
     * The job keeps going, thus doesn't result in an overall error.
     *
     * \a job the job that emitted the signal
     *
     * \a subJob the job listing a sub-directory, which failed. Use
     *       url(), error() and errorText() on that job to find
     *       out more.
     */
    void subError(KIO::ListJob *job, KIO::ListJob *subJob);

    /*!
     * Signals a redirection.
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     *
     * \a job the job that is redirected
     *
     * \a url the new url
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /*!
     * Signals a permanent redirection.
     * The redirection itself is handled internally.
     *
     * \a job the job that emitted this signal
     *
     * \a fromUrl the original URL
     *
     * \a toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

protected Q_SLOTS:
    void slotFinished() override;
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit ListJob(ListJobPrivate &dd);

    Q_DECLARE_PRIVATE(ListJob)
    friend class ListJobPrivate;
};

/*!
 * \relates KIO::ListJob
 *
 * List the contents of \a url, which is assumed to be a directory.
 *
 * "." and ".." are returned, filter them out if you don't want them.
 *
 * \a url the url of the directory
 *
 * \a flags Can be HideProgressInfo here
 *
 * \a listFlags Can be used to control whether hidden files are included
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT ListJob *listDir(const QUrl &url, JobFlags flags = DefaultFlags, ListJob::ListFlags listFlags = ListJob::ListFlag::IncludeHidden);

/*!
 * \relates KIO::ListJob
 *
 * The same as the previous method, but recurses subdirectories.
 * Directory links are not followed.
 *
 * "." and ".." are returned but only for the toplevel directory.
 * Filter them out if you don't want them.
 *
 * \a url the url of the directory
 *
 * \a flags Can be HideProgressInfo here
 *
 * \a listFlags Can be used to control whether hidden files are included
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT ListJob *listRecursive(const QUrl &url, JobFlags flags = DefaultFlags, ListJob::ListFlags listFlags = ListJob::ListFlag::IncludeHidden);

Q_DECLARE_OPERATORS_FOR_FLAGS(KIO::ListJob::ListFlags)
}

#endif
