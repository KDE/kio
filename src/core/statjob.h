/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2013 David Faure <faure@kde.org>

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
        DestinationSide
    };

    ~StatJob() override;

    /**
     * A stat() can have two meanings. Either we want to read from this URL,
     * or to check if we can write to it. First case is "source", second is "dest".
     * It is necessary to know what the StatJob is for, to tune the kioslave's behavior
     * (e.g. with FTP).
     * By default it is SourceSide.
     * @param side SourceSide or DestinationSide
     */
    void setSide(StatSide side);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 0)
    /**
     * A stat() can have two meanings. Either we want to read from this URL,
     * or to check if we can write to it. First case is "source", second is "dest".
     * It is necessary to know what the StatJob is for, to tune the kioslave's behavior
     * (e.g. with FTP).
     * @param source true for "source" mode, false for "dest" mode
     * @deprecated Since 4.0, use setSide(StatSide side).
     */
    KIOCORE_DEPRECATED_VERSION(4, 0, "Use StatJob::setSide(StatSide)")
    void setSide(bool source);
#endif

    /**
     * Selects the level of @p details we want.
     * @since 5.69
     */
    void setDetails(KIO::StatDetails details);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 69)
    /**
     * @brief @see setDetails(KIO::StatDetails details)
     * Needed until setDetails(short int details) is removed
     */
    void setDetails(KIO::StatDetail detail);

    /**
     * Selects the level of @p details we want.
     * By default this is 2 (all details wanted, including modification time, size, etc.),
     * setDetails(1) is used when deleting: we don't need all the information if it takes
     * too much time, no need to follow symlinks etc.
     * setDetails(0) is used for very simple probing: we'll only get the answer
     * "it's a file or a directory, or it doesn't exist". This is used by KRun.
     * @param details 2 for all details, 1 for simple, 0 for very simple
     * @deprecated since 5.69, use setDetails(KIO::StatDetails)
     */
    KIOCORE_DEPRECATED_VERSION(5, 69, "Use setDetails(KIO::statDetails)")
    void setDetails(short int details);
#endif

    /**
     * @brief Result of the stat operation.
     * Call this in the slot connected to result,
     * and only after making sure no error happened.
     * @return the result of the stat
     */
    const UDSEntry &statResult() const;

    /**
     * @brief most local URL
     * Call this in the slot connected to result,
     * and only after making sure no error happened.
     * @return the most local URL for the URL we were stat'ing.
     *
     * Sample usage:
     *
     * @code
     * KIO::StatJob* job = KIO::mostLocalUrl("desktop:/foo");
     * job->uiDelegate()->setWindow(this);
     * connect(job, &KJob::result, this, &MyClass::slotMostLocalUrlResult);
     * [...]
     * // and in the slot slotMostLocalUrlResult(KJob *job)
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
    void slotMetaData(const KIO::MetaData &_metaData) override;
protected:
    StatJob(StatJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotStatEntry(const KIO::UDSEntry &entry))
    Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &url))
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
 * The reason for this parameter is that in some cases the kioslave might not
 * be able to determine a file's existence (e.g. HTTP doesn't allow it, FTP
 * has issues with case-sensitivity on some systems).
 * When the slave can't reliably determine the existence of a file, it will:
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
 * @since 5.69
 */
KIOCORE_EXPORT StatJob *statDetails(const QUrl &url, KIO::StatJob::StatSide side,
                             KIO::StatDetails details = KIO::StatDefaultDetails, JobFlags flags = DefaultFlags);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 69)
/**
 * Find all details for one file or directory.
 * This version of the call includes two additional booleans, @p sideIsSource and @p details.
 *
 * @param url the URL of the file
 * @param side is SourceSide when stating a source file (we will do a get on it if
 * the stat works) and DestinationSide when stating a destination file (target of a copy).
 * The reason for this parameter is that in some cases the kioslave might not
 * be able to determine a file's existence (e.g. HTTP doesn't allow it, FTP
 * has issues with case-sensitivity on some systems).
 * When the slave can't reliably determine the existence of a file, it will:
 * @li be optimistic if SourceSide, i.e. it will assume the file exists,
 * and if it doesn't this will appear when actually trying to download it
 * @li be pessimistic if DestinationSide, i.e. it will assume the file
 * doesn't exist, to prevent showing "about to overwrite" errors to the user.
 * If you simply want to check for existence without downloading/uploading afterwards,
 * then you should use DestinationSide.
 *
 * @param details selects the level of details we want.
 * By default this is 2 (all details wanted, including modification time, size, etc.),
 * setDetails(1) is used when deleting: we don't need all the information if it takes
 * too much time, no need to follow symlinks etc.
 * setDetails(0) is used for very simple probing: we'll only get the answer
 * "it's a file or a directory or a symlink, or it doesn't exist". This is used by KRun and DeleteJob.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 * @deprecated since 5.69, use statDetails(const QUrl &, KIO::StatSide, KIO::StatDetails, JobFlags)
 */
KIOCORE_DEPRECATED_VERSION(5, 69, "Use KIO::statDetails(const QUrl &, KIO::StatSide, KIO::StatDetails details, JobFlags)")
KIOCORE_EXPORT StatJob *stat(const QUrl &url, KIO::StatJob::StatSide side,
                             short int details, JobFlags flags = DefaultFlags);

/**
 * Converts the legacy stat details int to a StatDetail Flag
 * @param details @see setDetails()
 * @since 5.69
 * @deprecated since 5.69, use directly KIO::StatDetails
 */
KIOCORE_DEPRECATED_VERSION(5, 69, "Use directly KIO::StatDetails")
KIOCORE_EXPORT KIO::StatDetails detailsToStatDetails(int details);

#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 0)
/**
 * Find all details for one file or directory.
 * This version of the call includes two additional booleans, @p sideIsSource and @p details.
 *
 * @param url the URL of the file
 * @param sideIsSource is true when stating a source file (we will do a get on it if
 * the stat works) and false when stating a destination file (target of a copy).
 * The reason for this parameter is that in some cases the kioslave might not
 * be able to determine a file's existence (e.g. HTTP doesn't allow it, FTP
 * has issues with case-sensitivity on some systems).
 * When the slave can't reliably determine the existence of a file, it will:
 * @li be optimistic if sideIsSource=true, i.e. it will assume the file exists,
 * and if it doesn't this will appear when actually trying to download it
 * @li be pessimistic if sideIsSource=false, i.e. it will assume the file
 * doesn't exist, to prevent showing "about to overwrite" errors to the user.
 * If you simply want to check for existence without downloading/uploading afterwards,
 * then you should use sideIsSource=false.
 *
 * @param details selects the level of details we want.
 * By default this is 2 (all details wanted, including modification time, size, etc.),
 * setDetails(1) is used when deleting: we don't need all the information if it takes
 * too much time, no need to follow symlinks etc.
 * setDetails(0) is used for very simple probing: we'll only get the answer
 * "it's a file or a directory, or it doesn't exist". This is used by KRun.
 * @param flags Can be HideProgressInfo here
 * @return the job handling the operation.
 * @deprecated Since 4.0, use stat(const QUrl &, KIO::StatJob::StatSide, short int, JobFlags)
 */
KIOCORE_DEPRECATED_VERSION(4, 0, "Use KIO::stat(const QUrl &, KIO::StatJob::StatSide, short int, JobFlags)")
KIOCORE_EXPORT StatJob *stat(const QUrl &url, bool sideIsSource,
                             short int details, JobFlags flags = DefaultFlags);
#endif

/**
 * Tries to map a local URL for the given URL, using a KIO job.
 *
 * Starts a (stat) job for determining the "most local URL" for a given URL.
 * Retrieve the result with StatJob::mostLocalUrl in the result slot.
 * @param url The URL we are testing.
 * \since 4.4
 */
KIOCORE_EXPORT StatJob *mostLocalUrl(const QUrl &url, JobFlags flags = DefaultFlags);

}

#endif
