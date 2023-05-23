/*
    SPDX-FileCopyrightText: 2000, 2001 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2000, 2001 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000, 2001 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2001, 2002 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007 Daniel Nicoletti <mirttex@users.sourceforge.net>
    SPDX-FileCopyrightText: 2008, 2009 Andreas Hartmetz <ahartmetz@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTP_H
#define HTTP_H

#include <QDateTime>
#include <QList>
#include <QLocalSocket>
#include <QStringList>
#include <QUrl>

#include "httpmethod_p.h"
#include "kio/tcpworkerbase.h"

class QDomNodeList;
class QFile;
class QIODevice;

namespace KIO
{
class AuthInfo;
}

class HeaderTokenizer;
class KAbstractHttpAuthentication;

class HTTPProtocol : public QObject, public KIO::TCPWorkerBase
{
    Q_OBJECT
public:
    HTTPProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app);
    ~HTTPProtocol() override;

    /** HTTP version **/
    enum HTTP_REV { HTTP_None, HTTP_Unknown, HTTP_10, HTTP_11, SHOUTCAST };

    /** Authorization method used **/
    enum AUTH_SCHEME { AUTH_None, AUTH_Basic, AUTH_NTLM, AUTH_Digest, AUTH_Negotiate };

    /** DAV-specific request elements for the current connection **/
    struct DAVRequest {
        DAVRequest()
        {
            overwrite = false;
            depth = 0;
        }

        QString desturl;
        bool overwrite;
        int depth;
    };

    /** The request for the current connection **/
    struct HTTPRequest {
        HTTPRequest()
        {
            method = KIO::HTTP_UNKNOWN;
            offset = 0;
            endoffset = 0;
            allowTransferCompression = false;
            disablePassDialog = false;
            doNotWWWAuthenticate = false;
            doNotProxyAuthenticate = false;
            preferErrorPage = false;
            useCookieJar = false;
        }

        QByteArray methodString() const;

        QUrl url;
        QString encoded_hostname; // ### can be calculated on-the-fly
        // Persistent connections
        bool isKeepAlive;
        int keepAliveTimeout; // Timeout in seconds.

        KIO::HTTP_METHOD method;
        QString methodStringOverride; // Overrides method if non-empty.
        QByteArray sentMethodString; // Stores http method actually sent
        KIO::filesize_t offset;
        KIO::filesize_t endoffset;
        QString windowId; // Window Id this request is related to.
        // Header fields
        QString referrer;
        QString charsets;
        QString languages;
        QString userAgent;
        // Previous and current response codes
        unsigned int responseCode;
        unsigned int prevResponseCode;
        // Miscellaneous
        QString id;
        DAVRequest davData;
        QUrl redirectUrl;
        QUrl proxyUrl;
        QStringList proxyUrls;

        bool isPersistentProxyConnection;
        bool allowTransferCompression;
        bool disablePassDialog;
        bool doNotWWWAuthenticate;
        bool doNotProxyAuthenticate;
        // Indicates whether an error page or error message is preferred.
        bool preferErrorPage;

        // Use the cookie jar (or pass cookies to the application as metadata instead)
        bool useCookieJar;
        // Cookie flags
        enum { CookiesAuto, CookiesManual, CookiesNone } cookieMode;
    };

    /** State of the current connection to the server **/
    struct HTTPServerState {
        HTTPServerState()
        {
            isKeepAlive = false;
            isPersistentProxyConnection = false;
        }

        void initFrom(const HTTPRequest &request)
        {
            url = request.url;
            encoded_hostname = request.encoded_hostname;
            isKeepAlive = request.isKeepAlive;
            proxyUrl = request.proxyUrl;
            isPersistentProxyConnection = request.isPersistentProxyConnection;
        }

        void updateCredentials(const HTTPRequest &request)
        {
            if (url.host() == request.url.host() && url.port() == request.url.port()) {
                url.setUserName(request.url.userName());
                url.setPassword(request.url.password());
            }
            if (proxyUrl.host() == request.proxyUrl.host() && proxyUrl.port() == request.proxyUrl.port()) {
                proxyUrl.setUserName(request.proxyUrl.userName());
                proxyUrl.setPassword(request.proxyUrl.password());
            }
        }

        void clear()
        {
            url.clear();
            encoded_hostname.clear();
            proxyUrl.clear();
            isKeepAlive = false;
            isPersistentProxyConnection = false;
        }

        QUrl url;
        QString encoded_hostname;
        QUrl proxyUrl;
        bool isKeepAlive;
        bool isPersistentProxyConnection;
    };

    //---------------------- Re-implemented methods ----------------
    virtual void setHost(const QString &host, quint16 port, const QString &user, const QString &pass) override;

    void worker_status() override;

    KIO::WorkerResult get(const QUrl &url) override;
    KIO::WorkerResult put(const QUrl &url, int _mode, KIO::JobFlags flags) override;

    //----------------- Re-implemented methods for WebDAV -----------
    KIO::WorkerResult listDir(const QUrl &url) override;
    KIO::WorkerResult mkdir(const QUrl &url, int _permissions) override;

    KIO::WorkerResult rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags) override;
    KIO::WorkerResult copy(const QUrl &src, const QUrl &dest, int _permissions, KIO::JobFlags flags) override;
    KIO::WorkerResult del(const QUrl &url, bool _isfile) override;

    // ask the host whether it supports WebDAV & cache this info
    Q_REQUIRED_RESULT KIO::WorkerResult davHostOk();

    // send generic DAV request
    Q_REQUIRED_RESULT KIO::WorkerResult davGeneric(const QUrl &url, KIO::HTTP_METHOD method, qint64 size = -1);

    // Send requests to lock and unlock resources
    Q_REQUIRED_RESULT KIO::WorkerResult davLock(const QUrl &url, const QString &scope, const QString &type, const QString &owner);
    Q_REQUIRED_RESULT KIO::WorkerResult davUnlock(const QUrl &url);

    // Calls httpClose() and finished()
    Q_REQUIRED_RESULT KIO::WorkerResult davFinished();

    // Handle error conditions
    Q_REQUIRED_RESULT KIO::WorkerResult davError(int code = -1, const QString &url = QString());
    Q_REQUIRED_RESULT KIO::WorkerResult davError(QString &errorMsg, int code = -1, const QString &url = QString());
    //---------------------------- End WebDAV -----------------------

    /**
     * Special commands supported by this worker :
     * 1 - HTTP POST
     * 2 - Cache has been updated
     * 3 - SSL Certificate Cache has been updated
     * 4 - HTTP multi get
     * 5 - DAV LOCK     (see
     * 6 - DAV UNLOCK     README.webdav)
     */
    KIO::WorkerResult special(const QByteArray &data) override;

    KIO::WorkerResult mimetype(const QUrl &url) override;

    KIO::WorkerResult stat(const QUrl &url) override;

    void reparseConfiguration() override;

    /**
     * Forced close of connection
     */
    void closeConnection() override;

    Q_REQUIRED_RESULT KIO::WorkerResult post(const QUrl &url, qint64 size = -1);
    KIO::WorkerResult multiGet(const QByteArray &data) override;
    Q_REQUIRED_RESULT KIO::WorkerResult maybeSetRequestUrl(const QUrl &);

    /**
     * Generate and send error message based on response code.
     */
    Q_REQUIRED_RESULT KIO::WorkerResult sendHttpError();

    /**
     * Call WorkerBase::errorPage() and remember that we've called it
     */
    bool sendErrorPageNotification();

    /**
     * Check network status
     */
    bool isOffline();

protected Q_SLOTS:
    void slotData(const QByteArray &);
    Q_REQUIRED_RESULT KIO::WorkerResult slotFilterError(const QString &text);
    Q_REQUIRED_RESULT KIO::WorkerResult error(int errid, const QString &text);
    Q_REQUIRED_RESULT KIO::WorkerResult proxyAuthenticationForSocket(const QNetworkProxy &, QAuthenticator *);
    void saveProxyAuthenticationForSocket();

protected:
    int readChunked(); ///< Read a chunk
    int readLimited(); ///< Read maximum m_iSize bytes.
    int readUnlimited(); ///< Read as much as possible.

    /**
     * A thin wrapper around TCPWorkerBase::write() that will retry writing as
     * long as no error occurs.
     */
    ssize_t write(const void *buf, size_t nbytes);
    using WorkerBase::write;

    /**
     * Add an encoding on to the appropriate stack this
     * is necessary because transfer encodings and
     * content encodings must be handled separately.
     */
    void addEncoding(const QString &, QStringList &);

    quint16 defaultPort() const;

    // The methods between here and sendQuery() are helpers for sendQuery().

    /**
     * Return true if the request is already "done", false otherwise.
     *
     * @p cacheHasPage will be set to true if the page was found, false otherwise.
     */
    QString formatRequestUri() const;
    /**
     * create HTTP authentications response(s), if any
     */
    QString authenticationHeader();
    Q_REQUIRED_RESULT KIO::WorkerResult sendQuery();

    /**
     * Close transfer
     */
    void httpClose(bool keepAlive);
    /**
     * Open connection
     */
    Q_REQUIRED_RESULT KIO::WorkerResult httpOpenConnection();
    /**
     * Close connection
     */
    void httpCloseConnection();
    /**
     * Check whether to keep or close the connection.
     */
    bool httpShouldCloseConnection();

    void forwardHttpResponseHeader(bool forwardImmediately = true);

    /**
     * fix common MIME type errors by webservers.
     *
     * Helper for readResponseHeader().
     */
    void fixupResponseMimetype();
    /**
     * fix common content-encoding errors by webservers.
     *
     * Helper for readResponseHeader().
     */
    void fixupResponseContentEncoding();

    Q_REQUIRED_RESULT KIO::WorkerResult readResponseHeader();
    void parseContentDisposition(const QString &disposition);

    Q_REQUIRED_RESULT KIO::WorkerResult sendBody();
    Q_REQUIRED_RESULT KIO::WorkerResult sendCachedBody();

    // where dataInternal == true, the content is to be made available
    // to an internal function.
    Q_REQUIRED_RESULT KIO::WorkerResult readBody(bool dataInternal = false);

    /**
     * Performs a WebDAV stat or list
     */
    void davSetRequest(const QByteArray &requestXML);
    Q_REQUIRED_RESULT KIO::WorkerResult davStatList(const QUrl &url, bool stat = true);
    void davParsePropstats(const QDomNodeList &propstats, KIO::UDSEntry &entry);
    void davParseActiveLocks(const QDomNodeList &activeLocks, uint &lockCount);

    /**
     * Parses a date & time string
     */
    QDateTime parseDateTime(const QString &input, const QString &type);

    /**
     * Returns the error code from a "HTTP/1.1 code Code Name" string
     */
    int codeFromResponse(const QString &response);

    /**
     * Extracts locks from metadata
     * Returns the appropriate If: header
     */
    QString davProcessLocks();

    /**
     * Send a cookie to the cookiejar
     */
    void addCookies(const QString &url, const QByteArray &cookieHeader);

    /**
     * Look for cookies in the cookiejar
     */
    QString findCookies(const QString &url);

    /**
     * Do everything proceedUntilResponseHeader does, and also get the response body.
     * This is being used as a replacement for proceedUntilResponseHeader() in
     * situations where we actually expect the response to have a body / payload data.
     *
     * where dataInternal == true, the content is to be made available
     * to an internal function.
     */
    Q_REQUIRED_RESULT KIO::WorkerResult proceedUntilResponseContent(bool dataInternal = false);

    /**
     * Ensure we are connected, send our query, and get the response header.
     */
    Q_REQUIRED_RESULT KIO::WorkerResult proceedUntilResponseHeader();

    /**
     * Resets any per session settings.
     */
    void resetSessionSettings();

    /**
     * Resets variables related to parsing a response.
     */
    void resetResponseParsing();

    /**
     * Resets any per connection settings. These are different from
     * per-session settings in that they must be invalidated every time
     * a request is made, e.g. a retry to re-send the header to the
     * server, as compared to only when a new request arrives.
     */
    void resetConnectionSettings();

    /**
     * Caches the POST data in a temporary buffer.
     *
     * Depending on size of content, the temporary buffer might be
     * created either in memory or on disk as (a temporary file).
     */
    void cachePostData(const QByteArray &);

    /**
     * Clears the POST data buffer.
     *
     * Note that calling this function results in the POST data buffer
     * getting completely deleted.
     */
    void clearPostDataBuffer();

    /**
     * Returns true on successful retrieval of all content data.
     */
    Q_REQUIRED_RESULT KIO::WorkerResult retrieveAllData();

    /**
     * Saves HTTP authentication data.
     */
    void saveAuthenticationData();

    /**
     * Handles HTTP authentication.
     */
    bool handleAuthenticationHeader(const HeaderTokenizer *tokenizer);

    /**
     * Handles file -> webdav put requests.
     */
    Q_REQUIRED_RESULT KIO::WorkerResult copyPut(const QUrl &src, const QUrl &dest, KIO::JobFlags flags);

    /**
     * Stats a remote DAV file and returns true if it already exists.
     */
    bool davDestinationExists();

    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) override;

protected:
    /* This stores information about the credentials already tried
     * during the authentication stage (in case the auth method uses
     * a username and password). Initially the job-provided credentials
     * are used (if any). In case of failure the credential cache is
     * queried and if this fails the user is asked to provide credentials
     * interactively (unless forbidden by metadata) */
    enum TriedCredentials {
        NoCredentials = 0,
        JobCredentials,
        CachedCredentials,
        UserInputCredentials,
    };

    HTTPServerState m_server;
    HTTPRequest m_request;
    QList<HTTPRequest> m_requestQueue;

    // Processing related
    KIO::filesize_t m_iSize; ///< Expected size of message
    KIO::filesize_t m_iPostDataSize;
    KIO::filesize_t m_iBytesLeft; ///< # of bytes left to receive in this message.
    KIO::filesize_t m_iContentLeft; ///< # of content bytes left
    QByteArray m_receiveBuf; ///< Receive buffer
    bool m_dataInternal; ///< Data is for internal consumption
    bool m_isChunked; ///< Chunked transfer encoding

    bool m_isBusy; ///< Busy handling request queue.
    bool m_isEOF;
    bool m_isEOD;

    //--- Settings related to a single response only
    bool m_isRedirection; ///< Indicates current request is a redirection
    QStringList m_responseHeaders; ///< All headers

    // Language/Encoding related
    QStringList m_transferEncodings;
    QStringList m_contentEncodings;
    QString m_contentMD5;
    QString m_mimeType; // TODO QByteArray?

    //--- WebDAV
    // Data structure to hold data which will be passed to an internal func.
    QByteArray m_webDavDataBuf;
    QStringList m_davCapabilities;

    bool m_davHostOk;
    bool m_davHostUnsupported;
    //----------

    // Mimetype determination
    bool m_cpMimeBuffer;
    QByteArray m_mimeTypeBuffer;

    // Holds the POST data so it won't get lost on if we
    // happened to get a 401/407 response when submitting
    // a form.
    QIODevice *m_POSTbuf;

    // Operation mode
    QByteArray m_protocol;

    KAbstractHttpAuthentication *m_wwwAuth;
    QList<QByteArray> m_blacklistedWwwAuthMethods;
    TriedCredentials m_triedWwwCredentials;
    KAbstractHttpAuthentication *m_proxyAuth;
    QList<QByteArray> m_blacklistedProxyAuthMethods;
    TriedCredentials m_triedProxyCredentials;
    // For proxy auth when it's handled by the Qt/KDE socket classes
    QAuthenticator *m_socketProxyAuth;

    // The current KIO error on this request / response pair - zero / KJob::NoError if no error
    int m_kioError;
    QString m_kioErrorString;
    // Whether we are loading an error page (body of a reply with error response code)
    bool m_isLoadingErrorPage;

    // Values that determine the remote connection timeouts.
    int m_remoteRespTimeout;

    // EOF Retry count
    quint8 m_iEOFRetryCount;

    QByteArray m_unreadBuf;
    void clearUnreadBuffer();
    void unread(char *buf, size_t size);
    size_t readBuffered(char *buf, size_t size, bool unlimited = true);
    bool readDelimitedText(char *buf, int *idx, int end, int numNewlines);
};
#endif
