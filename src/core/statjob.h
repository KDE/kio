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
     *
     * Since this method depends on UDSEntry::UDS_LOCAL_PATH having been previously set
     * by a KIO slave, ideally you should first check that the protocol Class of the URL
     * being stat'ed is ":local" before creating the StatJob at all. Typically only ":local"
     * KIO slaves set UDS_LOCAL_PATH. See KProtocolInfo::protocolClass().
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
 * @deprecated since 5.69, use statDetails(const QUrl &, KIO::StatJob::StatSide, KIO::StatDetails, JobFlags)
 */
KIOCORE_DEPRECATED_VERSION(5, 69, "Use KIO::statDetails(const QUrl &, KIO::StatJob::StatSide, KIO::StatDetails, JobFlags)")
KIOCORE_EXPORT StatJob *stat(const QUrl &url, KIO::StatJob::StatSide side,
                             short int details, JobFlags flags = DefaultFlags);
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 69)
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
 * Tries to map a local URL for the given URL, using a KIO job. This only makes sense for
 * protocols that have Class ":local" (such protocols most likely have KIO Slaves that set
 * UDSEntry::UDS_LOCAL_PATH); ideally you should check the URL protocol Class before creating
 * a StatJob. See KProtocolInfo::protocolClass().
 *
 * Starts a (stat) job for determining the "most local URL" for a given URL.
 * Retrieve the result with StatJob::mostLocalUrl in the result slot.
 *
 * @param url The URL we are stat'ing.
 *
 * @since 4.4
 *
 * Sample usage:
 *
 * Here the KIO slave name is "foo", which for example could be:
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
