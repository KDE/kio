/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2005 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/
#ifndef KIO_GLOBAL_H
#define KIO_GLOBAL_H

#include "kiocore_export.h"

#include <QString>
#include <QFile>  // for QFile::Permissions

#include <KJob>

#include "metadata.h" // for source compat
#include "jobtracker.h" // for source compat

class QUrl;

class QTime;

#if defined(Q_OS_WIN) && defined(Q_CC_MSVC)
// on windows ssize_t is not defined, only SSIZE_T exists
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

/**
 * @short A namespace for KIO globals
 *
 */
namespace KIO
{

/// 64-bit file offset
typedef qlonglong fileoffset_t;
/// 64-bit file size
typedef qulonglong filesize_t;

/**
 * Converts @p size from bytes to the string representation.
 *
 * @param  size  size in bytes
 * @return converted size as a string - e.g. 123.4 KiB , 12.0 MiB
 */
KIOCORE_EXPORT QString convertSize(KIO::filesize_t size);

/**
 * Converts a size to a string representation
 * Not unlike QString::number(...)
 *
 * @param size size in bytes
 * @return  converted size as a string - e.g. 123456789
 */
KIOCORE_EXPORT QString number(KIO::filesize_t size);

/**
 * Converts size from kibi-bytes (2^10) to the string representation.
 *
 * @param  kibSize  size in kibi-bytes (2^10)
 * @return converted size as a string - e.g. 123.4 KiB , 12.0 MiB
 */
KIOCORE_EXPORT QString convertSizeFromKiB(KIO::filesize_t kibSize);

/**
 * Calculates remaining time in seconds from total size, processed size and speed.
 *
 * @param  totalSize      total size in bytes
 * @param  processedSize  processed size in bytes
 * @param  speed          speed in bytes per second
 * @return calculated remaining time in seconds
 */
KIOCORE_EXPORT unsigned int calculateRemainingSeconds(KIO::filesize_t totalSize,
        KIO::filesize_t processedSize, KIO::filesize_t speed);

/**
 * Convert @p seconds to a string representing number of days, hours, minutes and seconds
 *
 * @param  seconds number of seconds to convert
 * @return string representation in a locale depending format
 */
KIOCORE_EXPORT QString convertSeconds(unsigned int seconds);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(3, 4)
/**
 * Calculates remaining time from total size, processed size and speed.
 *
 * @param  totalSize      total size in bytes
 * @param  processedSize  processed size in bytes
 * @param  speed          speed in bytes per second
 * @return calculated remaining time
 * @deprecated Since 3.4, use calculateRemainingSeconds() instead, as QTime is limited to 23:59:59
 */
KIOCORE_DEPRECATED_VERSION(3, 4, "Use KIO::calculateRemainingSeconds(KIO::filesize_t, KIO::filesize_t, KIO::filesize_t")
KIOCORE_EXPORT QTime calculateRemaining(KIO::filesize_t totalSize, KIO::filesize_t processedSize, KIO::filesize_t speed);
#endif

/**
 * Helper for showing information about a set of files and directories
 * @param items the number of items (= @p files + @p dirs + number of symlinks :)
 * @param files the number of files
 * @param dirs the number of dirs
 * @param size the sum of the size of the @p files
 * @param showSize whether to show the size in the result
 * @return the summary string
 */
KIOCORE_EXPORT QString itemsSummaryString(uint items, uint files, uint dirs, KIO::filesize_t size, bool showSize);

/**
 * Encodes (from the text displayed to the real filename)
 * This translates '/' into a "unicode fraction slash", QChar(0x2044).
 * Used by KIO::link, for instance.
 * @param str the file name to encode
 * @return the encoded file name
 */
KIOCORE_EXPORT QString encodeFileName(const QString &str);
/**
 * Decodes (from the filename to the text displayed)
 * This doesn't do anything anymore, it used to do the opposite of encodeFileName
 * when encodeFileName was using %2F for '/'.
 * @param str the file name to decode
 * @return the decoded file name
 */
KIOCORE_EXPORT QString decodeFileName(const QString &str);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 61)
/**
 * Given a directory path and a filename (which usually exists already),
 * this function returns a suggested name for a file that doesn't exist
 * in that directory. The existence is only checked for local urls though.
 * The suggested file name is of the form "foo 1", "foo 2" etc.
 * @since 5.0
 * @deprecated since 5.61, use KFileUtils::suggestName() from KCoreAddons
 */
KIOCORE_DEPRECATED_VERSION(5, 61, "Use KFileUtils::suggestName(const QUrl &, const QString &) from KCoreAddons")
KIOCORE_EXPORT QString suggestName(const QUrl &baseURL, const QString &oldName);
#endif

/**
 * Error codes that can be emitted by KIO.
 */
enum Error {
    ERR_CANNOT_OPEN_FOR_READING = KJob::UserDefinedError + 1,
    ERR_CANNOT_OPEN_FOR_WRITING = KJob::UserDefinedError + 2,
    ERR_CANNOT_LAUNCH_PROCESS = KJob::UserDefinedError + 3,
    ERR_INTERNAL = KJob::UserDefinedError + 4,
    ERR_MALFORMED_URL = KJob::UserDefinedError + 5,
    ERR_UNSUPPORTED_PROTOCOL = KJob::UserDefinedError + 6,
    ERR_NO_SOURCE_PROTOCOL = KJob::UserDefinedError + 7,
    ERR_UNSUPPORTED_ACTION = KJob::UserDefinedError + 8,
    ERR_IS_DIRECTORY = KJob::UserDefinedError + 9, ///< ... where a file was expected
    ERR_IS_FILE = KJob::UserDefinedError + 10, ///< ... where a directory was expected (e.g. listing)
    ERR_DOES_NOT_EXIST = KJob::UserDefinedError + 11,
    ERR_FILE_ALREADY_EXIST = KJob::UserDefinedError + 12,
    ERR_DIR_ALREADY_EXIST = KJob::UserDefinedError + 13,
    ERR_UNKNOWN_HOST = KJob::UserDefinedError + 14,
    ERR_ACCESS_DENIED = KJob::UserDefinedError + 15,
    ERR_WRITE_ACCESS_DENIED = KJob::UserDefinedError + 16,
    ERR_CANNOT_ENTER_DIRECTORY = KJob::UserDefinedError + 17,
    ERR_PROTOCOL_IS_NOT_A_FILESYSTEM = KJob::UserDefinedError + 18,
    ERR_CYCLIC_LINK = KJob::UserDefinedError + 19,
    ERR_USER_CANCELED = KJob::KilledJobError,
    ERR_CYCLIC_COPY = KJob::UserDefinedError + 21,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_CREATE_SOCKET = KJob::UserDefinedError + 22, ///< @deprecated Since 5.0, use ERR_CANNOT_CREATE_SOCKET
#endif
    ERR_CANNOT_CREATE_SOCKET = KJob::UserDefinedError + 22,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_CONNECT = KJob::UserDefinedError + 23, ///< @deprecated Since 5.0, use ERR_CANNOT_CONNECT
#endif
    ERR_CANNOT_CONNECT = KJob::UserDefinedError + 23,
    ERR_CONNECTION_BROKEN = KJob::UserDefinedError + 24,
    ERR_NOT_FILTER_PROTOCOL = KJob::UserDefinedError + 25,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_MOUNT = KJob::UserDefinedError + 26, ///< @deprecated Since 5.0, use ERR_CANNOT_MOUNT
#endif
    ERR_CANNOT_MOUNT = KJob::UserDefinedError + 26,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_UNMOUNT = KJob::UserDefinedError + 27, ///< @deprecated Since 5.0, use ERR_CANNOT_UNMOUNT
#endif
    ERR_CANNOT_UNMOUNT = KJob::UserDefinedError + 27,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_READ = KJob::UserDefinedError + 28, ///< @deprecated Since 5.0, use ERR_CANNOT_READ
#endif
    ERR_CANNOT_READ = KJob::UserDefinedError + 28,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_WRITE = KJob::UserDefinedError + 29, ///< @deprecated Since 5.0, use ERR_CANNOT_WRITE
#endif
    ERR_CANNOT_WRITE = KJob::UserDefinedError + 29,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_BIND = KJob::UserDefinedError + 30, ///< @deprecated Since 5.0, use ERR_CANNOT_BIND
#endif
    ERR_CANNOT_BIND = KJob::UserDefinedError + 30,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_LISTEN = KJob::UserDefinedError + 31, ///< @deprecated Since 5.0, use ERR_CANNOT_LISTEN
#endif
    ERR_CANNOT_LISTEN = KJob::UserDefinedError + 31,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_ACCEPT = KJob::UserDefinedError + 32, ///< @deprecated Since 5.0, use ERR_CANNOT_ACCEPT
#endif
    ERR_CANNOT_ACCEPT = KJob::UserDefinedError + 32,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_LOGIN = KJob::UserDefinedError + 33, ///< @deprecated Since 5.0, use ERR_CANNOT_LOGIN
#endif
    ERR_CANNOT_LOGIN = KJob::UserDefinedError + 33,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_STAT = KJob::UserDefinedError + 34, ///< @deprecated Since 5.0, use ERR_CANNOT_STAT
#endif
    ERR_CANNOT_STAT = KJob::UserDefinedError + 34,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_CLOSEDIR = KJob::UserDefinedError + 35, ///< @deprecated Since 5.0, use ERR_CANNOT_CLOSEDIR
#endif
    ERR_CANNOT_CLOSEDIR = KJob::UserDefinedError + 35,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_MKDIR = KJob::UserDefinedError + 37, ///< @deprecated Since 5.0, use ERR_CANNOT_MKDIR
#endif
    ERR_CANNOT_MKDIR = KJob::UserDefinedError + 37,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_RMDIR = KJob::UserDefinedError + 38, ///< @deprecated Since 5.0, use ERR_CANNOT_RMDIR
#endif
    ERR_CANNOT_RMDIR = KJob::UserDefinedError + 38,
    ERR_CANNOT_RESUME = KJob::UserDefinedError + 39,
    ERR_CANNOT_RENAME = KJob::UserDefinedError + 40,
    ERR_CANNOT_CHMOD = KJob::UserDefinedError + 41,
    ERR_CANNOT_DELETE = KJob::UserDefinedError + 42,
    // The text argument is the protocol that the dead slave supported.
    // This means for example: file, ftp, http, ...
    ERR_SLAVE_DIED = KJob::UserDefinedError + 43,
    ERR_OUT_OF_MEMORY = KJob::UserDefinedError + 44,
    ERR_UNKNOWN_PROXY_HOST = KJob::UserDefinedError + 45,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_AUTHENTICATE = KJob::UserDefinedError + 46, ///< @deprecated Since 5.0, use ERR_CANNOT_AUTHENTICATE
#endif
    ERR_CANNOT_AUTHENTICATE = KJob::UserDefinedError + 46,
    ERR_ABORTED = KJob::UserDefinedError + 47, ///< Action got aborted from application side
    ERR_INTERNAL_SERVER = KJob::UserDefinedError + 48,
    ERR_SERVER_TIMEOUT = KJob::UserDefinedError + 49,
    ERR_SERVICE_NOT_AVAILABLE = KJob::UserDefinedError + 50,
    ERR_UNKNOWN = KJob::UserDefinedError + 51,
    // (was a warning) ERR_CHECKSUM_MISMATCH = 52,
    ERR_UNKNOWN_INTERRUPT = KJob::UserDefinedError + 53,
    ERR_CANNOT_DELETE_ORIGINAL = KJob::UserDefinedError + 54,
    ERR_CANNOT_DELETE_PARTIAL = KJob::UserDefinedError + 55,
    ERR_CANNOT_RENAME_ORIGINAL = KJob::UserDefinedError + 56,
    ERR_CANNOT_RENAME_PARTIAL = KJob::UserDefinedError + 57,
    ERR_NEED_PASSWD = KJob::UserDefinedError + 58,
    ERR_CANNOT_SYMLINK = KJob::UserDefinedError + 59,
    ERR_NO_CONTENT = KJob::UserDefinedError + 60, ///< Action succeeded but no content will follow.
    ERR_DISK_FULL = KJob::UserDefinedError + 61,
    ERR_IDENTICAL_FILES = KJob::UserDefinedError + 62, ///< src==dest when moving/copying
    ERR_SLAVE_DEFINED = KJob::UserDefinedError + 63, ///< for slave specified errors that can be
    ///< rich text.  Email links will be handled
    ///< by the standard email app and all hrefs
    ///< will be handled by the standard browser.
    ///< <a href="exec:/khelpcenter ?" will be
    ///< forked.
    ERR_UPGRADE_REQUIRED = KJob::UserDefinedError + 64, ///< A transport upgrade is required to access this
    ///< object.  For instance, TLS is demanded by
    ///< the server in order to continue.
    ERR_POST_DENIED = KJob::UserDefinedError + 65, ///< Issued when trying to POST data to a certain Ports
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    ERR_COULD_NOT_SEEK = KJob::UserDefinedError + 66, ///< @deprecated Since 5.0, use ERR_CANNOT_SEEK
#endif
    // see job.cpp
    ERR_CANNOT_SEEK = KJob::UserDefinedError + 66,
    ERR_CANNOT_SETTIME = KJob::UserDefinedError + 67, ///< Emitted by setModificationTime
    ERR_CANNOT_CHOWN = KJob::UserDefinedError + 68,
    ERR_POST_NO_SIZE = KJob::UserDefinedError + 69,
    ERR_DROP_ON_ITSELF = KJob::UserDefinedError + 70, ///< from KIO::DropJob, @since 5.6
    ERR_CANNOT_MOVE_INTO_ITSELF = KJob::UserDefinedError + 71, ///< emitted by KIO::move, @since 5.18
    ERR_PASSWD_SERVER = KJob::UserDefinedError + 72, ///< returned by SlaveBase::openPasswordDialogV2, @since 5.24
    ERR_CANNOT_CREATE_SLAVE = KJob::UserDefinedError + 73, ///< used by Slave::createSlave, @since 5.30
    ERR_FILE_TOO_LARGE_FOR_FAT32 = KJob::UserDefinedError + 74, ///< @since 5.54
    ERR_OWNER_DIED = KJob::UserDefinedError + 75, ///< Value used between kuiserver and views when the job owner disappears unexpectedly. It should not be emitted by slaves. @since 5.54
    ERR_PRIVILEGE_NOT_REQUIRED = KJob::UserDefinedError + 76, ///< used by file ioslave, @since 5.60
    ERR_CANNOT_TRUNCATE = KJob::UserDefinedError + 77, // used by FileJob::truncate, @since 5.66
};

/**
 * Specifies how to use the cache.
 * @see parseCacheControl()
 * @see getCacheControlString()
 */
enum CacheControl {
    CC_CacheOnly, ///< Fail request if not in cache
    CC_Cache,     ///< Use cached entry if available
    CC_Verify,    ///< Validate cached entry with remote site if expired
    CC_Refresh,   ///< Always validate cached entry with remote site
    CC_Reload,     ///< Always fetch from remote site.
};

/**
 * Specifies privilege file operation status.
 * @since 5.43
 */
enum  PrivilegeOperationStatus {
    OperationAllowed = 1,
    OperationCanceled,
    OperationNotAllowed,
};

/**
 * Describes the fields that a stat command will retrieve
 * @see UDSEntry
 * @see StatDetails
 * @since 5.69
 */
enum StatDetail {
    /// No field returned, useful to check if a file exists
    StatNoDetails = 0x0,
    /// Filename, access, type, size, linkdest
    StatBasic = 0x1,
    /// uid, gid
    StatUser = 0x2,
    /// atime, mtime, btime
    StatTime = 0x4,
    /// Resolve symlinks
    StatResolveSymlink = 0x8,
    /// acl Data
    StatAcl = 0x10,
    /// dev, inode
    StatInode = 0x20,
    /// recursive size @since 5.70
    StatRecursiveSize = 0x40,

    /// Default value includes fields provided by other entries
    StatDefaultDetails = StatBasic | StatUser | StatTime | StatAcl | StatResolveSymlink,
};
/**
 * Stores a combination of #StatDetail values.
 */
Q_DECLARE_FLAGS(StatDetails, StatDetail)

Q_DECLARE_OPERATORS_FOR_FLAGS(KIO::StatDetails)

/**
 * Parses the string representation of the cache control option.
 *
 * @param cacheControl the string representation
 * @return the cache control value
 * @see getCacheControlString()
 */
KIOCORE_EXPORT KIO::CacheControl parseCacheControl(const QString &cacheControl);

/**
 * Returns a string representation of the given cache control method.
 *
 * @param cacheControl the cache control method
 * @return the string representation
 * @see parseCacheControl()
 */
KIOCORE_EXPORT QString getCacheControlString(KIO::CacheControl cacheControl);

/**
 * Return the "favicon" (see http://www.favicon.com) for the given @p url,
 * if available. Does NOT attempt to download the favicon, it only returns
 * one that is already available.
 *
 * If unavailable, returns QString().
 * Use KIO::FavIconRequestJob instead of this method if you can wait
 * for the favicon to be downloaded.
 *
 * @param url the URL of the favicon
 * @return the path to the icon (to be passed to QIcon()), or QString()
 *
 * @since 5.0
 */
KIOCORE_EXPORT QString favIconForUrl(const QUrl &url);

/**
 * Converts KIO file permissions from mode_t to QFile::Permissions format.
 *
 * This is a convenience function for converting KIO permissions parameter from
 * mode_t to QFile::Permissions.
 *
 * @param permissions KIO file permissions.
 *
 * @return -1 if @p permissions is -1, otherwise its OR'ed QFile::Permission equivalent.
 * @since 4.12
 */
KIOCORE_EXPORT QFile::Permissions convertPermissions(int permissions);

/**
 * Return the icon name for a URL.
 * Most of the time this returns the MIME type icon,
 * but also has fallback to favicon and protocol-specific icon.
 *
 * Pass this to QIcon::fromTheme().
 *
 * @since 5.0
 */
KIOCORE_EXPORT QString iconNameForUrl(const QUrl &url);

/**
 * This function is useful to implement the "Up" button in a file manager for example.
 *
 * @return a URL that is a level higher
 *
 * @since 5.0
 */
KIOCORE_EXPORT QUrl upUrl(const QUrl &url);

}
#endif
