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
/**
 * @class KIO::ListJob listjob.h <KIO/ListJob>
 *
 * A ListJob is allows you to get the get the content of a directory.
 * Don't create the job directly, but use KIO::listRecursive() or
 * KIO::listDir() instead.
 * @see KIO::listRecursive()
 * @see KIO::listDir()
 */
class KIOCORE_EXPORT ListJob : public SimpleJob
{
    Q_OBJECT

public:
    ~ListJob() override;

    /**
     * Returns the ListJob's redirection URL. This will be invalid if there
     * was no redirection.
     * @return the redirection url
     */
    const QUrl &redirectionUrl() const;

    /**
     * Do not apply any KIOSK restrictions to this job.
     */
    void setUnrestricted(bool unrestricted);

Q_SIGNALS:
    /**
     * This signal emits the entry found by the job while listing.
     * The progress signals aren't specific to ListJob. It simply
     * uses SimpleJob's processedSize (number of entries listed) and
     * totalSize (total number of entries, if known),
     * as well as percent.
     * @param job the job that emitted this signal
     * @param list the list of UDSEntries
     */
    void entries(KIO::Job *job, const KIO::UDSEntryList &list);  // TODO KDE5: use KIO::ListJob* argument to avoid casting

    /**
     * This signal is emitted when a sub-directory could not be listed.
     * The job keeps going, thus doesn't result in an overall error.
     * @param job the job that emitted the signal
     * @param subJob the job listing a sub-directory, which failed. Use
     *       url(), error() and errorText() on that job to find
     *       out more.
     */
    void subError(KIO::ListJob *job, KIO::ListJob *subJob);

    /**
     * Signals a redirection.
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     * @param job the job that is redirected
     * @param url the new url
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /**
     * Signals a permanent redirection.
     * The redirection itself is handled internally.
     * @param job the job that emitted this signal
     * @param fromUrl the original URL
     * @param toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

protected Q_SLOTS:
    void slotFinished() override;
    void slotMetaData(const KIO::MetaData &_metaData) override;
    void slotResult(KJob *job) override;

protected:
    ListJob(ListJobPrivate &dd);
    Q_DECLARE_PRIVATE(ListJob)
    friend class ListJobPrivate;
};

/**
 * List the contents of @p url, which is assumed to be a directory.
 *
 * "." and ".." are returned, filter them out if you don't want them.
 *
 *
 * @param url the url of the directory
 * @param flags Can be HideProgressInfo here
 * @param includeHidden true for all files, false to cull out UNIX hidden
 *                      files/dirs (whose names start with dot)
 * @return the job handling the operation.
 */
KIOCORE_EXPORT ListJob *listDir(const QUrl &url, JobFlags flags = DefaultFlags,
                                bool includeHidden = true);

/**
 * The same as the previous method, but recurses subdirectories.
 * Directory links are not followed.
 *
 * "." and ".." are returned but only for the toplevel directory.
 * Filter them out if you don't want them.
 *
 * @param url the url of the directory
 * @param flags Can be HideProgressInfo here
 * @param includeHidden true for all files, false to cull out UNIX hidden
 *                      files/dirs (whose names start with dot)
 * @return the job handling the operation.
 */
KIOCORE_EXPORT ListJob *listRecursive(const QUrl &url, JobFlags flags = DefaultFlags,
                                      bool includeHidden = true);

}

#endif
