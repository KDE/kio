/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastain <bastain@kde.org>
    SPDX-FileCopyrightText: 2000 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2008 Jarosław Staniek <staniek@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KPROTOCOLMANAGER_H
#define KPROTOCOLMANAGER_H

#include <QStringList>

#include "kio/global.h" // KIO::CacheControl
#include "kiocore_export.h"
#include "kprotocolinfo.h"

class KSharedConfig;
template<class T>
class QExplicitlySharedDataPointer;
typedef QExplicitlySharedDataPointer<KSharedConfig> KSharedConfigPtr;
namespace KIO
{
class SlaveConfigPrivate;
} // namespace KIO

/**
 * @class KProtocolManager kprotocolmanager.h <KProtocolManager>
 *
 * Provides information about I/O (Internet, etc.) settings chosen/set
 * by the end user.
 *
 * KProtocolManager has a heap of static functions that allows only read
 * access to KDE's IO related settings. These include proxy, cache, file
 * transfer resumption, timeout and user-agent related settings.
 *
 * The information provided by this class is generic enough to be applicable
 * to any application that makes use of KDE's IO sub-system.  Note that this
 * mean the proxy, timeout etc. settings are saved in a separate user-specific
 * config file and not in the config file of the application.
 *
 * Original author:
 * @author Torben Weis <weis@kde.org>
 *
 * Revised by:
 * @author Waldo Bastain <bastain@kde.org>
 * @author Dawit Alemayehu <adawit@kde.org>
 * @see KPAC
 */
class KIOCORE_EXPORT KProtocolManager
{
public:

    /*=========================== USER-AGENT SETTINGS ===========================*/

    /**
     * Returns the default user-agent string used for web browsing.
     *
     * @return the default user-agent string
     */
    static QString defaultUserAgent();

    /**
     * Returns the default user-agent value used for web browsing, for example
     * "Mozilla/5.0 (compatible; Konqueror/4.0; Linux; X11; i686; en_US) KHTML/4.0.1 (like Gecko)"
     *
     * @param keys can be any of the following:
     * @li 'o'    Show OS
     * @li 'v'    Show OS Version
     * @li 'p'    Show platform (only for X11)
     * @li 'm'    Show machine architecture
     * @li 'l'    Show language
     * @return the default user-agent value with the given @p keys
     */
    static QString defaultUserAgent(const QString &keys);

    /**
     * Returns the application's user-agent string.
     * Example string is "KMail/1.9.50 (Windows/6.0; KDE/3.97.1; i686; svn-762186; 2008-01-15)",
     * where "KMail" is the @p appName parameter, "1.9.50" is the @p appVersion parameter,
     * "Windows/6.0; KDE/3.97.1; i686" part is added automatically and "svn-762186; 2008-01-15"
     * is provided by @p extraInfo list.
     *
     * @param appName name of the application
     * @param appVersion name of the application
     * @param extraInfo a list of elements that will be appended to the string as extra information
     * @return the application's user-agent string
     *
     * @since 4.1
     */
    static QString userAgentForApplication(const QString &appName, const QString &appVersion,
                                           const QStringList &extraInfo = QStringList());

    /**
     * Returns the user-agent string configured for the
     * specified host.
     *
     * If hostname is not found or is empty (i.e. "" or
     * QString()) this function will return the default
     * user agent.
     *
     * @param hostname name of the host
     * @return specified user-agent string
     */
    static QString userAgentForHost(const QString &hostname);

    /**
     * Returns system name, version and machine type, for example "Windows", "5.1", "i686".
     * This information can be used for constructing custom user-agent strings.
     *
     * @param systemName system name
     * @param systemVersion system version
     * @param machine machine type

     * @return true if system name, version and machine type has been provided
     *
     * @since 4.1
     */
    static bool getSystemNameVersionAndMachine(
        QString &systemName, QString &systemVersion, QString &machine);

    /*=========================== TIMEOUT CONFIG ================================*/

    /**
     * Returns the preferred timeout value for reading from
     * remote connections in seconds.
     *
     * @return timeout value for remote connection in secs.
     */
    static int readTimeout();

    /**
     * Returns the preferred timeout value for remote connections
     * in seconds.
     *
     * @return timeout value for remote connection in secs.
     */
    static int connectTimeout();

    /**
     * Returns the preferred timeout value for proxy connections
     * in seconds.
     *
     * @return timeout value for proxy connection in secs.
     */
    static int proxyConnectTimeout();

    /**
     * Returns the preferred response timeout value for
     * remote connecting in seconds.
     *
     * @return timeout value for remote connection in seconds.
     */
    static int responseTimeout();

    /*=============================== PROXY CONFIG ==============================*/

    /**
     * Returns whether or not the user specified the
     * use of proxy server to make connections.
     * @return true to use a proxy
     */
    static bool useProxy();

    /**
     * Returns whether or not the proxy server
     * lookup should be reversed or not.
     * @return true to use a reversed proxy
     */
    static bool useReverseProxy();

    /**
     * Types of proxy configuration
     * @li NoProxy     - No proxy is used
     * @li ManualProxy - Proxies are manually configured
     * @li PACProxy    - A Proxy configuration URL has been given
     * @li WPADProxy   - A proxy should be automatically discovered
     * @li EnvVarProxy - Use the proxy values set through environment variables.
     */
    enum ProxyType {
        NoProxy,
        ManualProxy,
        PACProxy,
        WPADProxy,
        EnvVarProxy,
    };

    /**
     * Returns the type of proxy configuration that is used.
     * @return the proxy type
     */
    static ProxyType proxyType();

    /**
     * Proxy authorization modes.
     *
     * @li Prompt     - Ask for authorization as needed
     * @li Automatic  - Use auto login as defined in kionetrc files.
     */
    enum ProxyAuthMode {
        Prompt,
        Automatic,
    };

    /**
     * Returns the way proxy authorization should be handled.
     *
     * @return the proxy authorization mode
     * @see ProxyAuthMode
     */
    static ProxyAuthMode proxyAuthMode();

    /**
     * Returns the strings for hosts that should contacted
     * DIRECTLY, bypassing any proxy settings.
     * @return a list of (comma-separated) hostnames or partial host
     *         names
     */
    static QString noProxyFor();

    /**
     * Returns the proxy server address for a given
     * protocol.
     *
     * @param protocol the protocol whose proxy info is needed
     * @returns the proxy server address if one is available,
     *          or QString() if not available
     */
    static QString proxyFor(const QString &protocol);

    /**
     * Returns the Proxy server address for a given URL.
     *
     * If the selected proxy type is @ref PACProxy or @ref WPADProxy, then a
     * helper kded module, proxyscout, is used to determine the proxy information.
     * Otherwise, @ref proxyFor is used to find the proxy to use for the given url.
     *
     * If this function returns an empty string, then the request to a proxy server
     * must be denied. For a direct connection, without the use of a proxy, this
     * function will return "DIRECT".
     *
     * @param url the URL whose proxy info is needed
     * @returns the proxy server address if one is available, otherwise a QString().
     */
    static QString proxyForUrl(const QUrl &url);

    /**
     * Returns all the possible proxy server addresses for @p url.
     *
     * If the selected proxy type is @ref PACProxy or @ref WPADProxy, then a
     * helper kded module, proxyscout, is used to determine the proxy information.
     * Otherwise, @ref proxyFor is used to find the proxy to use for the given url.
     *
     * If this function returns empty list, then the request is to a proxy server
     * must be denied. For a direct connection, this function will return a single
     * entry of "DIRECT".
     *
     * @since 4.7
     *
     * @param url the URL whose proxy info is needed
     * @returns the proxy server address if one is available, otherwise an empty list .
     */
    static QStringList proxiesForUrl(const QUrl &url);

    /**
     * Marks this proxy as bad (down). It will not be used for the
     * next 30 minutes. (The script may supply an alternate proxy)
     * @param proxy the proxy to mark as bad (as URL)
     */
    static void badProxy(const QString &proxy);

    /**
     * Returns the URL of the script for automatic proxy configuration.
     * @return the proxy configuration script
     */
    static QString proxyConfigScript();

    /*========================== CACHE CONFIG ===================================*/

    /**
     * Returns true/false to indicate whether a cache
     * should be used
     *
     * @return true to use the cache, false otherwisea
     */
    static bool useCache();

    /**
     * Returns the maximum age in seconds cached files should be
     * kept before they are deleted as necessary.
     *
     * @return the maximum cache age in seconds
     */
    static int maxCacheAge();

    /**
     * Returns the maximum size that can be used for caching.
     *
     * By default this function returns the DEFAULT_MAX_CACHE_SIZE
     * value as defined in http_slave_defaults.h.  Not that the
     * value returned is in bytes, hence a value of 5120 would mean
     * 5 Kb.
     *
     * @return the maximum cache size in bytes
     */
    static int maxCacheSize(); // Maximum cache size in Kb.

    /**
     * The directory which contains the cache files.
     * @return the directory that contains the cache files
     */
    static QString cacheDir();

    /**
     * Returns the Cache control directive to be used.
     * @return the cache control value
     */
    static KIO::CacheControl cacheControl();

    /*============================ DOWNLOAD CONFIG ==============================*/

    /**
     * Returns true if partial downloads should be
     * automatically resumed.
     * @return true to resume partial downloads
     */
    static bool autoResume();

    /**
     * Returns true if partial downloads should be marked
     * with a ".part" extension.
     * @return true if partial downloads should get an ".part" extension
     */
    static bool markPartial();

    /**
     * Returns the minimum file size for keeping aborted
     * downloads.
     *
     * Any data downloaded that does not meet this minimum
     * requirement will simply be discarded. The default size
     * is 5 KB.
     *
     * @return the minimum keep size for aborted downloads in bytes
     */
    static int minimumKeepSize();

    /*============================ NETWORK CONNECTIONS ==========================*/
    /**
     * Returns true if proxy connections should be persistent.
     * @return true if proxy connections should be persistent
     */
    static bool persistentProxyConnection();

    /**
     * Returns true if connections should be persistent
     * @return true if the connections should be persistent
     */
    static bool persistentConnections();

    /*===================== PROTOCOL CAPABILITIES ===============================*/

    /**
     * Returns whether the protocol can list files/objects.
     * If a protocol supports listing it can be browsed in e.g. file-dialogs
     * and konqueror.
     *
     * Whether a protocol supports listing is determined by the "listing="
     * field in the protocol description file.
     * If the protocol support listing it should list the fields it provides in
     * this field. If the protocol does not support listing this field should
     * remain empty (default.)
     *
     * @param url the url to check
     * @return true if the protocol support listing
     * @see listing()
     */
    static bool supportsListing(const QUrl &url);

    /**
     * Returns whether the protocol can retrieve data from URLs.
     *
     * This corresponds to the "reading=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if it is possible to read from the URL
     */
    static bool supportsReading(const QUrl &url);

    /**
     * Returns whether the protocol can store data to URLs.
     *
     * This corresponds to the "writing=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports writing
     */
    static bool supportsWriting(const QUrl &url);

    /**
     * Returns whether the protocol can create directories/folders.
     *
     * This corresponds to the "makedir=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can create directories
     */
    static bool supportsMakeDir(const QUrl &url);

    /**
     * Returns whether the protocol can delete files/objects.
     *
     * This corresponds to the "deleting=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports deleting
     */
    static bool supportsDeleting(const QUrl &url);

    /**
     * Returns whether the protocol can create links between files/objects.
     *
     * This corresponds to the "linking=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports linking
     */
    static bool supportsLinking(const QUrl &url);

    /**
     * Returns whether the protocol can move files/objects between different
     * locations.
     *
     * This corresponds to the "moving=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports moving
     */
    static bool supportsMoving(const QUrl &url);

    /**
     * Returns whether the protocol can be opened using KIO::open(const QUrl&).
     *
     * This corresponds to the "opening=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports opening
     */
    static bool supportsOpening(const QUrl &url);

    /**
     * Returns whether the protocol can be truncated with FileJob::truncate(KIO::filesize_t length).
     *
     * This corresponds to the "truncating=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol supports truncating
     * @since 5.66
     */
    static bool supportsTruncating(const QUrl &url);

    /**
     * Returns whether the protocol can copy files/objects directly from the
     * filesystem itself. If not, the application will read files from the
     * filesystem using the file-protocol and pass the data on to the destination
     * protocol.
     *
     * This corresponds to the "copyFromFile=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can copy files from the local file system
     */
    static bool canCopyFromFile(const QUrl &url);

    /**
     * Returns whether the protocol can copy files/objects directly to the
     * filesystem itself. If not, the application will receive the data from
     * the source protocol and store it in the filesystem using the
     * file-protocol.
     *
     * This corresponds to the "copyToFile=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can copy files to the local file system
     */
    static bool canCopyToFile(const QUrl &url);

    /**
     * Returns whether the protocol can rename (i.e. move fast) files/objects
     * directly from the filesystem itself. If not, the application will read
     * files from the filesystem using the file-protocol and pass the data on
     * to the destination protocol.
     *
     * This corresponds to the "renameFromFile=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can rename/move files from the local file system
     */
    static bool canRenameFromFile(const QUrl &url);

    /**
     * Returns whether the protocol can rename (i.e. move fast) files/objects
     * directly to the filesystem itself. If not, the application will receive
     * the data from the source protocol and store it in the filesystem using the
     * file-protocol.
     *
     * This corresponds to the "renameToFile=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can rename files to the local file system
     */
    static bool canRenameToFile(const QUrl &url);

    /**
     * Returns whether the protocol can recursively delete directories by itself.
     * If not (the usual case) then KIO will list the directory and delete files
     * and empty directories one by one.
     *
     * This corresponds to the "deleteRecursive=" field in the protocol description file.
     * Valid values for this field are "true" or "false" (default).
     *
     * @param url the url to check
     * @return true if the protocol can delete non-empty directories by itself.
     */
    static bool canDeleteRecursive(const QUrl &url);

    /**
     * This setting defines the strategy to use for generating a filename, when
     * copying a file or directory to another directory. By default the destination
     * filename is made out of the filename in the source URL. However if the
     * ioslave displays names that are different from the filename of the URL
     * (e.g. kio_fonts shows Arial for arial.ttf, or kio_trash shows foo.txt and
     * uses some internal URL), using Name means that the display name (UDS_NAME)
     * will be used to as the filename in the destination directory.
     *
     * This corresponds to the "fileNameUsedForCopying=" field in the protocol description file.
     * Valid values for this field are "Name" or "FromURL" (default).
     *
     * @param url the url to check
     * @return how to generate the filename in the destination directory when copying/moving
     */
    static KProtocolInfo::FileNameUsedForCopying fileNameUsedForCopying(const QUrl &url);

    /**
     * Returns default MIME type for this URL based on the protocol.
     *
     * This corresponds to the "defaultMimetype=" field in the protocol description file.
     *
     * @param url the url to check
     * @return the default MIME type of the protocol, or an empty string if unknown
     */
    static QString defaultMimetype(const QUrl &url);

    /**
     * Returns whether the protocol should be treated as a filesystem
     * or as a stream when reading from it.
     *
     * This corresponds to the "input=" field in the protocol description file.
     * Valid values for this field are "filesystem", "stream" or "none" (default).
     *
     * @param url the url to check
     * @return the input type of the given @p url
     */
    static KProtocolInfo::Type inputType(const QUrl &url);

    /**
     * Returns whether the protocol should be treated as a filesystem
     * or as a stream when writing to it.
     *
     * This corresponds to the "output=" field in the protocol description file.
     * Valid values for this field are "filesystem", "stream" or "none" (default).
     *
     * @param url the url to check
     * @return the output type of the given @p url
     */
    static KProtocolInfo::Type outputType(const QUrl &url);

    /**
     * Returns the list of fields this protocol returns when listing
     * The current possibilities are
     * Name, Type, Size, Date, AccessDate, Access, Owner, Group, Link, URL, MimeType
     * as well as Extra1, Extra2 etc. for extra fields (see extraFields).
     *
     * This corresponds to the "listing=" field in the protocol description file.
     * The supported fields should be separated with ',' in the protocol description file.
     *
     * @param url the url to check
     * @return a list of field names
     */
    static QStringList listing(const QUrl &url);

    /**
     * Returns whether the protocol can act as a source protocol.
     *
     * A source protocol retrieves data from or stores data to the
     * location specified by a URL.
     * A source protocol is the opposite of a filter protocol.
     *
     * The "source=" field in the protocol description file determines
     * whether a protocol is a source protocol or a filter protocol.
     * @param url the url to check
     * @return true if the protocol is a source of data (e.g. http), false if the
     *         protocol is a filter (e.g. gzip)
     */
    static bool isSourceProtocol(const QUrl &url);

    /**
     * Returns which protocol handles this MIME type, if it's an archive MIME type.
     * For instance zip:/ handles application/x-zip.
     *
     * This is defined in the protocol description file using an entry like
     * "archiveMimetype=application/x-zip"
     *
     * @param mimeType the MIME type to check
     * @return the protocol that can handle this archive MIME type, for instance "zip".
     * @since 4.1
     */
    static QString protocolForArchiveMimetype(const QString &mimeType);

    /*=============================== OTHERS ====================================*/

    /**
     * Force a reload of the general config file of
     * io-slaves ( kioslaverc).
     */
    static void reparseConfiguration();

    /**
     * Return the protocol to use in order to handle the given @p url
     * It's usually the same, except that FTP, when handled by a proxy,
     * needs an HTTP ioslave.
     *
     * When a proxy is to be used, proxy contains the URL for the proxy.
     * @param url the url to check
     * @param proxy the URL of the proxy to use
     * @return the slave protocol (e.g. 'http'), can be null if unknown
     */
    static QString slaveProtocol(const QUrl &url, QString &proxy);

    /**
     * Overloaded function that returns a list of all available proxy servers.
     *
     * @since 4.7
     */
    static QString slaveProtocol(const QUrl &url, QStringList &proxy);

    /**
     * Return Accept-Languages header built up according to user's desktop
     * language settings.
     * @return Accept-Languages header string
     */
    static QString acceptLanguagesHeader();

    /**
     * Returns the charset to use for the specified @ref url.
     *
     * @since 4.10
     */
    static QString charsetFor(const QUrl &url);

private:
    friend class KIO::SlaveConfigPrivate;

    /**
     * @internal
     * (Shared with SlaveConfig)
     */
    KIOCORE_NO_EXPORT static QMap<QString, QString> entryMap(const QString &group);
};
#endif
