/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_STATJOB_H
#define KIO_STATJOB_H

#include "global.h"
#include "simplejob.h"
#include <kio/udsentry.h>

namespace KIO
{
class StatJobPrivate;
/**
 * @class KIO::StatJob statjob.h <KIO/StatJob>
 *
 * A KIO job that retrieves information about a file or directory.
 * @see KIO::stat()
 */
class KIOCORE_EXPORT StatJob : public SimpleJob
{
    Q_OBJECT

public:
    enum StatSide {
        SourceSide,
        DestinationSide,
    };

    ~StatJob() override;

    /**
     * A stat() can have two meanings. Either we want to read from this URL,
     * or to check if we can write to it. First case is "source", second is "dest".
     * It is necessary to know what the StatJob is for, to tune the KIO worker's behavior
     * (e.g. with FTP).
     * By default it is SourceSide.
     * @param side SourceSide or DestinationSide
     */
    void setSide(StatSide side);

    /**
     * Selects the level of @p details we want.
     * @since 5.69
     */
    void setDetails(KIO::StatDetails details);

    /**
     * @brief Result of the stat operation.
     * Call this in the slot connected to result,
     * and only after making sure no error happened.
     * @return the result of the stat
     */
    const UDSEntry &statResult() const;

    /**
     * @brief most local URL
     *
     * Since this method depends on UDSEntry::UDS_LOCAL_PATH having been previously set
     * by a KIO worker, ideally you should first check that the protocol Class of the URL
     * being stat'ed is ":local" before creating the StatJob at all. Typically only ":local"
     * KIO workers set UDS_LOCAL_PATH. See KProtocolInfo::protocolClass().
     *
     * Call this in a slot connected to the result signal, and only after making sure no error
     * happened.
     *
     * @return the most local URL for the URL we were stat'ing
     *
     * Sample usage:
     *
     * @code
     * auto *job = KIO::mostLocalUrl("desktop:/foo");
     * job->uiDelegate()->setWindow(this);
     * connect(job, &KJob::result, this, &MyClass::slotMostLocalUrlResult);
     * [...]
     * // and in slotMostLocalUrlResult(KJob *job)
     * if (job->error()) {
     *    [...] // doesn't exist
     * } else {
     *    const QUrl localUrl = job->mostLocalUrl();
     *    // localUrl = file:///$HOME/Desktop/foo
     *    [...]
     * }
     * @endcode
     *
     * \since 4.4
     */
    QUrl mostLocalUrl() const;

Q_SIGNALS:
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
     * @param job the job that is redirected
     * @param fromUrl the original URL
     * @param toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

protected Q_SLOTS:
    void slotFinished() override;

protected:
    KIOCORE_NO_EXPORT explicit StatJob(StatJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(StatJob)
    friend KIOCORE_EXPORT StatJob *mostLocalUrl(const QUrl &url, JobFlags flags);
};

/**
 * Find all details for one file or directory.
 *
 * @param url the URL of the file
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 */
KIOCORE_EXPORT StatJob *stat(const QUrl &url, JobFlags flags = DefaultFlags);
/**
 * Find all details for one file or directory.
 * This version of the call includes two additional booleans, @p sideIsSource and @p details.
 *
 * @param url the URL of the file
 * @param side is SourceSide when stating a source file (we will do a get on it if
 * the stat works) and DestinationSide when stating a destination file (target of a copy).
 * The reason for this parameter is that in some cases the KIO worker might not
 * be able to determine a file's existence (e.g. HTTP doesn't allow it, FTP
 * has issues with case-sensitivity on some systems).
 * When the worker can't reliably determine the existence of a file, it will:
 * @li be optimistic if SourceSide, i.e. it will assume the file exists,
 * and if it doesn't this will appear when actually trying to download it
 * @li be pessimistic if DestinationSide, i.e. it will assume the file
 * doesn't exist, to prevent showing "about to overwrite" errors to the user.
 * If you simply want to check for existence without downloading/uploading afterwards,
 * then you should use DestinationSide.
 *
 * @param details selects the level of details we want.
 * You should minimize the detail level for better performance.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 * @since 6.0
 */
KIOCORE_EXPORT StatJob *stat(const QUrl &url, KIO::StatJob::StatSide side, KIO::StatDetails details = KIO::StatDefaultDetails, JobFlags flags = DefaultFlags);

/**
 * Tries to map a local URL for the given URL, using a KIO job. This only makes sense for
 * protocols that have Class ":local" (such protocols most likely have KIO workers that set
 * UDSEntry::UDS_LOCAL_PATH); ideally you should check the URL protocol Class before creating
 * a StatJob. See KProtocolInfo::protocolClass().
 *
 * Starts a (stat) job for determining the "most local URL" for a given URL.
 * Retrieve the result with StatJob::mostLocalUrl in the result slot.
 *
 * @param url The URL we are stat'ing.
 *
 *
 * Sample usage:
 *
 * Here the KIO worker name is "foo", which for example could be:
 *  - "desktop", "fonts", "kdeconnect", these have class ":local"
 *  - "ftp", "sftp", "smb", these have class ":internet"
 *
 * @code
 *
 * QUrl url(QStringLiteral("foo://bar/");
 * if (url.isLocalFile()) { // If it's a local URL, there is no need to stat
 *    [...]
 * } else if (KProtocolInfo::protocolClass(url.scheme()) == QLatin1String(":local")) {
 *     // Not a local URL, but if the protocol class is ":local", we may be able to stat
 *     // and get a "most local URL"
 *     auto *job = KIO::mostLocalUrl(url);
 *     job->uiDelegate()->setWindow(this);
 *     connect(job, &KJob::result, this, &MyClass::slotMostLocalUrlResult);
 *     [...]
 *     // And in slotMostLocalUrlResult(KJob *job)
 *     if (job->error()) {
 *         [...] // Doesn't exist, ideally show an error message
 *     } else {
 *         const QUrl localUrl = job->mostLocalUrl();
 *         // localUrl = file:///local/path/to/bar/
 *         [...]
 *     }
 * }
 *
 * @endcode
 *
 */
KIOCORE_EXPORT StatJob *mostLocalUrl(const QUrl &url, JobFlags flags = DefaultFlags);

}

#endif
