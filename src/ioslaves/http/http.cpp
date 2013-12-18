/*
   Copyright (C) 2000-2003 Waldo Bastian <bastian@kde.org>
   Copyright (C) 2000-2002 George Staikos <staikos@kde.org>
   Copyright (C) 2000-2002 Dawit Alemayehu <adawit@kde.org>
   Copyright (C) 2001,2002 Hamish Rodda <rodda@kde.org>
   Copyright (C) 2007      Nick Shaforostoff <shafff@ukr.net>
   Copyright (C) 2007      Daniel Nicoletti <mirttex@users.sourceforge.net>
   Copyright (C) 2008,2009 Andreas Hartmetz <ahartmetz@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License (LGPL) as published by the Free Software Foundation;
   either version 2 of the License, or (at your option) any later
   version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

// TODO delete / do not save very big files; "very big" to be defined

#define QT_NO_CAST_FROM_ASCII

#include "http.h"

#include <config-kioslave-http.h>

#include <unistd.h> // must be explicitly included for MacOSX

#include <QtXml/qdom.h>
#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QDateTime>
#include <QtCore/QBuffer>
#include <QtCore/QIODevice>
#include <QtCore/QTemporaryFile>
#include <QtCore/QStandardPaths>
#include <QDBusConnection>
#include <QtCore/QMimeDatabase>
#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QTcpSocket>
#include <QUrl>

#include <QDebug>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kservice.h>

#include <QDBusInterface>
#include <kremoteencoding.h>
#include <ktcpsocket.h>

#include <kio/ioslave_defaults.h>
#include <kio/http_slave_defaults.h>

#include <QDBusReply>
#include <QProcess>
#include <httpfilter.h>

#include <solid/networking.h>

#include <sys/stat.h>

#include "httpauthentication.h"

// HeaderTokenizer declarations
#include "parsinghelpers.h"
//string parsing helpers and HeaderTokenizer implementation
#include "parsinghelpers.cpp"


// KDE5 TODO (QT5) : use QString::htmlEscape or whatever https://qt.gitorious.org/qt/qtbase/merge_requests/56
// ends up with.
static QString htmlEscape(const QString &plain)
{
    QString rich;
    rich.reserve(int(plain.length() * 1.1));
        for (int i = 0; i < plain.length(); ++i) {
        if (plain.at(i) == QLatin1Char('<'))
            rich += QLatin1String("&lt;");
        else if (plain.at(i) == QLatin1Char('>'))
            rich += QLatin1String("&gt;");
        else if (plain.at(i) == QLatin1Char('&'))
            rich += QLatin1String("&amp;");
        else if (plain.at(i) == QLatin1Char('"'))
            rich += QLatin1String("&quot;");
        else
            rich += plain.at(i);
    }
    rich.squeeze();
    return rich;
}

static bool supportedProxyScheme(const QString& scheme)
{
    // scheme is supposed to be lowercase
    return (scheme.startsWith(QLatin1String("http"))
            || scheme == QLatin1String("socks"));
}

// see filenameFromUrl(): a sha1 hash is 160 bits
static const int s_hashedUrlBits = 160;   // this number should always be divisible by eight
static const int s_hashedUrlNibbles = s_hashedUrlBits / 4;
static const int s_hashedUrlBytes = s_hashedUrlBits / 8;
static const int s_MaxInMemPostBufSize = 256 * 1024;   // Write anyting over 256 KB to file...

using namespace KIO;

extern "C" Q_DECL_EXPORT int kdemain( int argc, char **argv )
{
    QCoreApplication app( argc, argv ); // needed for QSocketNotifier
    app.setApplicationName(QLatin1String("kio_http"));

    if (argc != 4)
    {
        fprintf(stderr, "Usage: kio_http protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    HTTPProtocol slave(argv[1], argv[2], argv[3]);
    slave.dispatchLoop();
    return 0;
}

/***********************************  Generic utility functions ********************/

static QString toQString(const QByteArray& value)
{
    return QString::fromLatin1(value.constData(), value.size());
}

static bool isCrossDomainRequest( const QString& fqdn, const QString& originURL )
{
  //TODO read the RFC
  if (originURL == QLatin1String("true")) // Backwards compatibility
     return true;

  QUrl url ( originURL );

  // Document Origin domain
  QString a = url.host();
  // Current request domain
  QString b = fqdn;

  if (a == b)
    return false;

  QStringList la = a.split(QLatin1Char('.'), QString::SkipEmptyParts);
  QStringList lb = b.split(QLatin1Char('.'), QString::SkipEmptyParts);

  if (qMin(la.count(), lb.count()) < 2) {
      return true;  // better safe than sorry...
  }

  while(la.count() > 2)
      la.pop_front();
  while(lb.count() > 2)
      lb.pop_front();

  return la != lb;
}

/*
  Eliminates any custom header that could potentially alter the request
*/
static QString sanitizeCustomHTTPHeader(const QString& _header)
{
  QString sanitizedHeaders;
  const QStringList headers = _header.split(QRegExp(QLatin1String("[\r\n]")));

  for(QStringList::ConstIterator it = headers.begin(); it != headers.end(); ++it)
  {
    // Do not allow Request line to be specified and ignore
    // the other HTTP headers.
    if (!(*it).contains(QLatin1Char(':')) ||
        (*it).startsWith(QLatin1String("host"), Qt::CaseInsensitive) ||
        (*it).startsWith(QLatin1String("proxy-authorization"), Qt::CaseInsensitive) ||
        (*it).startsWith(QLatin1String("via"), Qt::CaseInsensitive))
      continue;

    sanitizedHeaders += (*it);
    sanitizedHeaders += QLatin1String("\r\n");
  }
  sanitizedHeaders.chop(2);

  return sanitizedHeaders;
}

static bool isPotentialSpoofingAttack(const HTTPProtocol::HTTPRequest& request, const KConfigGroup* config)
{
    // qDebug() << request.url << "response code: " << request.responseCode << "previous response code:" << request.prevResponseCode;
    if (config->readEntry("no-spoof-check", false)) {
        return false;
    }

    if (request.url.userName().isEmpty()) {
        return false;
    }

    // We already have cached authentication.
    if (config->readEntry(QLatin1String("cached-www-auth"), false)) {
        return false;
    }

    const QString userName = config->readEntry(QLatin1String("LastSpoofedUserName"), QString());
    return ((userName.isEmpty() || userName != request.url.userName()) && request.responseCode != 401 && request.prevResponseCode != 401);
}

// for a given response code, conclude if the response is going to/likely to have a response body
static bool canHaveResponseBody(int responseCode, KIO::HTTP_METHOD method)
{
/* RFC 2616 says...
    1xx: false
    200: method HEAD: false, otherwise:true
    201: true
    202: true
    203: see 200
    204: false
    205: false
    206: true
    300: see 200
    301: see 200
    302: see 200
    303: see 200
    304: false
    305: probably like 300, RFC seems to expect disconnection afterwards...
    306: (reserved), for simplicity do it just like 200
    307: see 200
    4xx: see 200
    5xx :see 200
*/
    if (responseCode >= 100 && responseCode < 200) {
        return false;
    }
    switch (responseCode) {
    case 201:
    case 202:
    case 206:
        // RFC 2616 does not mention HEAD in the description of the above. if the assert turns out
        // to be a problem the response code should probably be treated just like 200 and friends.
        Q_ASSERT(method != HTTP_HEAD);
        return true;
    case 204:
    case 205:
    case 304:
        return false;
    default:
        break;
    }
    // safe (and for most remaining response codes exactly correct) default
    return method != HTTP_HEAD;
}

static bool isEncryptedHttpVariety(const QByteArray &p)
{
    return p == "https" || p == "webdavs";
}

static bool isValidProxy(const QUrl &u)
{
    return u.isValid() && !u.host().isEmpty();
}

static bool isHttpProxy(const QUrl &u)
{
    return isValidProxy(u) && u.scheme() == QLatin1String("http");
}

static QIODevice* createPostBufferDeviceFor (KIO::filesize_t size)
{
    QIODevice* device;
    if (size > static_cast<KIO::filesize_t>(s_MaxInMemPostBufSize))
      device = new QTemporaryFile;
    else
      device = new QBuffer;

    if (!device->open(QIODevice::ReadWrite))
      return 0;

    return device;
}

QByteArray HTTPProtocol::HTTPRequest::methodString() const
{
    if (!methodStringOverride.isEmpty())
        return (methodStringOverride + QLatin1Char(' ')).toLatin1();

    switch(method) {
    case HTTP_GET:
        return "GET ";
    case HTTP_PUT:
        return "PUT ";
    case HTTP_POST:
        return "POST ";
    case HTTP_HEAD:
        return "HEAD ";
    case HTTP_DELETE:
        return "DELETE ";
    case HTTP_OPTIONS:
        return "OPTIONS ";
    case DAV_PROPFIND:
        return "PROPFIND ";
    case DAV_PROPPATCH:
        return "PROPPATCH ";
    case DAV_MKCOL:
        return "MKCOL ";
    case DAV_COPY:
        return "COPY ";
    case DAV_MOVE:
        return "MOVE ";
    case DAV_LOCK:
        return "LOCK ";
    case DAV_UNLOCK:
        return "UNLOCK ";
    case DAV_SEARCH:
        return "SEARCH ";
    case DAV_SUBSCRIBE:
        return "SUBSCRIBE ";
    case DAV_UNSUBSCRIBE:
        return "UNSUBSCRIBE ";
    case DAV_POLL:
        return "POLL ";
    case DAV_NOTIFY:
        return "NOTIFY ";
    case DAV_REPORT:
        return "REPORT ";
    default:
        Q_ASSERT(false);
        return QByteArray();
    }
}

static QString formatHttpDate(const QDateTime &date)
{
    QString ret = date.toString(QStringLiteral("ddd, ")) + date.toString(Qt::RFC2822Date);
    ret.chop(6);    // remove " +0000"
    // RFCDate[Day] omits the second if zero, but HTTP requires it; see bug 240585.
    if (!date.time().second()) {
        ret.append(QLatin1String(":00"));
    }
    ret.append(QLatin1String(" GMT"));
    return ret;
}

static bool isAuthenticationRequired(int responseCode)
{
    return (responseCode == 401) || (responseCode == 407);
}

#define NO_SIZE ((KIO::filesize_t) -1)

#if HAVE_STRTOLL
#define STRTOLL strtoll
#else
#define STRTOLL strtol
#endif


/************************************** HTTPProtocol **********************************************/


HTTPProtocol::HTTPProtocol( const QByteArray &protocol, const QByteArray &pool,
                            const QByteArray &app )
    : TCPSlaveBase(protocol, pool, app, isEncryptedHttpVariety(protocol))
    , m_iSize(NO_SIZE)
    , m_iPostDataSize(NO_SIZE)
    , m_isBusy(false)
    , m_POSTbuf(0)
    , m_maxCacheAge(DEFAULT_MAX_CACHE_AGE)
    , m_maxCacheSize(DEFAULT_MAX_CACHE_SIZE)
    , m_protocol(protocol)
    , m_wwwAuth(0)
    , m_proxyAuth(0)
    , m_socketProxyAuth(0)
    , m_iError(0)
    , m_isLoadingErrorPage(false)
    , m_remoteRespTimeout(DEFAULT_RESPONSE_TIMEOUT)
    , m_iEOFRetryCount(0)
{
    reparseConfiguration();
    setBlocking(true);
    connect(socket(), SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
            this, SLOT(proxyAuthenticationForSocket(QNetworkProxy,QAuthenticator*)));
}

HTTPProtocol::~HTTPProtocol()
{
  httpClose(false);
}

void HTTPProtocol::reparseConfiguration()
{
    // qDebug();

    delete m_proxyAuth;
    delete m_wwwAuth;
    m_proxyAuth = 0;
    m_wwwAuth = 0;
    m_request.proxyUrl.clear(); //TODO revisit
    m_request.proxyUrls.clear();

    TCPSlaveBase::reparseConfiguration();
}

void HTTPProtocol::resetConnectionSettings()
{
  m_isEOF = false;
  m_iError = 0;
  m_isLoadingErrorPage = false;
}

quint16 HTTPProtocol::defaultPort() const
{
    return isEncryptedHttpVariety(m_protocol) ? DEFAULT_HTTPS_PORT : DEFAULT_HTTP_PORT;
}

void HTTPProtocol::resetResponseParsing()
{
  m_isRedirection = false;
  m_isChunked = false;
  m_iSize = NO_SIZE;
  clearUnreadBuffer();

  m_responseHeaders.clear();
  m_contentEncodings.clear();
  m_transferEncodings.clear();
  m_contentMD5.clear();
  m_mimeType.clear();

  setMetaData(QLatin1String("request-id"), m_request.id);
}

void HTTPProtocol::resetSessionSettings()
{
  // Follow HTTP/1.1 spec and enable keep-alive by default
  // unless the remote side tells us otherwise or we determine
  // the persistent link has been terminated by the remote end.
  m_request.isKeepAlive = true;
  m_request.keepAliveTimeout = 0;

  m_request.redirectUrl = QUrl();
  m_request.useCookieJar = config()->readEntry("Cookies", false);
  m_request.cacheTag.useCache = config()->readEntry("UseCache", true);
  m_request.preferErrorPage = config()->readEntry("errorPage", true);
  const bool noAuth = config()->readEntry("no-auth", false);
  m_request.doNotWWWAuthenticate = config()->readEntry("no-www-auth", noAuth);
  m_request.doNotProxyAuthenticate = config()->readEntry("no-proxy-auth", noAuth);
  m_strCacheDir = config()->readPathEntry("CacheDir", QString());
  m_maxCacheAge = config()->readEntry("MaxCacheAge", DEFAULT_MAX_CACHE_AGE);
  m_request.windowId = config()->readEntry("window-id");

  m_request.methodStringOverride = metaData(QLatin1String("CustomHTTPMethod"));

  // qDebug() << "Window Id =" << m_request.windowId;
  // qDebug() << "ssl_was_in_use =" << metaData(QLatin1String("ssl_was_in_use"));

  m_request.referrer.clear();
  // RFC 2616: do not send the referrer if the referrer page was served using SSL and
  //           the current page does not use SSL.
  if ( config()->readEntry("SendReferrer", true) &&
       (isEncryptedHttpVariety(m_protocol) || metaData(QLatin1String("ssl_was_in_use")) != QLatin1String("TRUE") ) )
  {
     QUrl refUrl(metaData(QLatin1String("referrer")));
     if (refUrl.isValid()) {
        // Sanitize
        QString protocol = refUrl.scheme();
        if (protocol.startsWith(QLatin1String("webdav"))) {
           protocol.replace(0, 6, QLatin1String("http"));
           refUrl.setScheme(protocol);
        }

        if (protocol.startsWith(QLatin1String("http"))) {
            m_request.referrer = toQString(refUrl.toEncoded(QUrl::RemoveUserInfo | QUrl::RemoveFragment));
        }
     }
  }

  if (config()->readEntry("SendLanguageSettings", true)) {
      m_request.charsets = config()->readEntry("Charsets", DEFAULT_PARTIAL_CHARSET_HEADER);
      if (!m_request.charsets.contains(QLatin1String("*;"), Qt::CaseInsensitive)) {
          m_request.charsets += QLatin1String(",*;q=0.5");
      }
      m_request.languages = config()->readEntry("Languages", DEFAULT_LANGUAGE_HEADER);
  } else {
      m_request.charsets.clear();
      m_request.languages.clear();
  }

  // Adjust the offset value based on the "resume" meta-data.
  QString resumeOffset = metaData(QLatin1String("resume"));
  if (!resumeOffset.isEmpty()) {
     m_request.offset = resumeOffset.toULongLong();
  } else {
     m_request.offset = 0;
  }
  // Same procedure for endoffset.
  QString resumeEndOffset = metaData(QLatin1String("resume_until"));
  if (!resumeEndOffset.isEmpty()) {
     m_request.endoffset = resumeEndOffset.toULongLong();
  } else {
     m_request.endoffset = 0;
  }

  m_request.disablePassDialog = config()->readEntry("DisablePassDlg", false);
  m_request.allowTransferCompression = config()->readEntry("AllowCompressedPage", true);
  m_request.id = metaData(QLatin1String("request-id"));

  // Store user agent for this host.
  if (config()->readEntry("SendUserAgent", true)) {
     m_request.userAgent = metaData(QLatin1String("UserAgent"));
  } else {
     m_request.userAgent.clear();
  }

  m_request.cacheTag.etag.clear();
  m_request.responseCode = 0;
  m_request.prevResponseCode = 0;

  delete m_wwwAuth;
  m_wwwAuth = 0;
  delete m_socketProxyAuth;
  m_socketProxyAuth = 0;

  // Obtain timeout values
  m_remoteRespTimeout = responseTimeout();

  // Bounce back the actual referrer sent
  setMetaData(QLatin1String("referrer"), m_request.referrer);

  // Reset the post data size
  m_iPostDataSize = NO_SIZE;

  // Reset the EOF retry counter
  m_iEOFRetryCount = 0;
}

void HTTPProtocol::setHost( const QString& host, quint16 port,
                            const QString& user, const QString& pass )
{
  // Reset the webdav-capable flags for this host
  if ( m_request.url.host() != host )
    m_davHostOk = m_davHostUnsupported = false;

  m_request.url.setHost(host);

  // is it an IPv6 address?
  if (host.indexOf(QLatin1Char(':')) == -1) {
      m_request.encoded_hostname = toQString(QUrl::toAce(host));
  } else  {
      int pos = host.indexOf(QLatin1Char('%'));
      if (pos == -1)
        m_request.encoded_hostname = QLatin1Char('[') + host + QLatin1Char(']');
      else
        // don't send the scope-id in IPv6 addresses to the server
        m_request.encoded_hostname = QLatin1Char('[') + host.left(pos) + QLatin1Char(']');
  }
  m_request.url.setPort((port > 0 && port != defaultPort()) ? port : -1);
  m_request.url.setUserName(user);
  m_request.url.setPassword(pass);

  // On new connection always clear previous proxy information...
  m_request.proxyUrl.clear();
  m_request.proxyUrls.clear();

  // qDebug() << "Hostname is now:" << m_request.url.host() << "(" << m_request.encoded_hostname << ")";
}

bool HTTPProtocol::maybeSetRequestUrl(const QUrl &u)
{
  // qDebug() << u;

  m_request.url = u;
  m_request.url.setPort(u.port(defaultPort()) != defaultPort() ? u.port() : -1);

  if (u.host().isEmpty()) {
     error( KIO::ERR_UNKNOWN_HOST, i18n("No host specified."));
     return false;
  }

  if (u.path().isEmpty()) {
     QUrl newUrl(u);
     newUrl.setPath(QLatin1String("/"));
     redirection(newUrl);
     finished();
     return false;
  }

  return true;
}

void HTTPProtocol::proceedUntilResponseContent( bool dataInternal /* = false */ )
{
  // qDebug();

  const bool status = (proceedUntilResponseHeader() && readBody(dataInternal));

  // If not an error condition or internal request, close
  // the connection based on the keep alive settings...
  if (!m_iError && !dataInternal) {
      httpClose(m_request.isKeepAlive);
  }

  // if data is required internally or we got error, don't finish,
  // it is processed before we finish()
  if (dataInternal || !status) {
      return;
  }

  if (!sendHttpError()) {
      finished();
  }
}

bool HTTPProtocol::proceedUntilResponseHeader()
{
  // qDebug();

  // Retry the request until it succeeds or an unrecoverable error occurs.
  // Recoverable errors are, for example:
  // - Proxy or server authentication required: Ask for credentials and try again,
  //   this time with an authorization header in the request.
  // - Server-initiated timeout on keep-alive connection: Reconnect and try again

  while (true) {
      if (!sendQuery()) {
          return false;
      }
      if (readResponseHeader()) {
          // Success, finish the request.
          break;
      }

      // If not loading error page and the response code requires us to resend the query,
      // then throw away any error message that might have been sent by the server.
      if (!m_isLoadingErrorPage && isAuthenticationRequired(m_request.responseCode)) {
          // This gets rid of any error page sent with 401 or 407 authentication required response...
          readBody(true);
      }

      // no success, close the cache file so the cache state is reset - that way most other code
      // doesn't have to deal with the cache being in various states.
      cacheFileClose();
      if (m_iError || m_isLoadingErrorPage) {
          // Unrecoverable error, abort everything.
          // Also, if we've just loaded an error page there is nothing more to do.
          // In that case we abort to avoid loops; some webservers manage to send 401 and
          // no authentication request. Or an auth request we don't understand.
          return false;
      }

      if (!m_request.isKeepAlive) {
          httpCloseConnection();
          m_request.isKeepAlive = true;
          m_request.keepAliveTimeout = 0;
      }
  }

  // Do not save authorization if the current response code is
  // 4xx (client error) or 5xx (server error).
  // qDebug() << "Previous Response:" << m_request.prevResponseCode;
  // qDebug() << "Current Response:" << m_request.responseCode;

  setMetaData(QLatin1String("responsecode"), QString::number(m_request.responseCode));
  setMetaData(QLatin1String("content-type"), m_mimeType);

  // At this point sendBody() should have delivered any POST data.
  clearPostDataBuffer();

  return true;
}

void HTTPProtocol::stat(const QUrl& url)
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
      return;
  resetSessionSettings();

  if ( m_protocol != "webdav" && m_protocol != "webdavs" )
  {
    QString statSide = metaData(QLatin1String("statSide"));
    if (statSide != QLatin1String("source"))
    {
      // When uploading we assume the file doesn't exit
      error(ERR_DOES_NOT_EXIST, url.toDisplayString());
      return;
    }

    // When downloading we assume it exists
    UDSEntry entry;
    entry.insert( KIO::UDSEntry::UDS_NAME, url.fileName() );
    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG ); // a file
    entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH ); // readable by everybody

    statEntry( entry );
    finished();
    return;
  }

  davStatList( url );
}

void HTTPProtocol::listDir( const QUrl& url )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  davStatList( url, false );
}

void HTTPProtocol::davSetRequest( const QByteArray& requestXML )
{
  // insert the document into the POST buffer, kill trailing zero byte
  cachePostData(requestXML);
}

void HTTPProtocol::davStatList( const QUrl& url, bool stat )
{
  UDSEntry entry;

  // check to make sure this host supports WebDAV
  if ( !davHostOk() )
    return;

  QMimeDatabase db;

  // Maybe it's a disguised SEARCH...
  QString query = metaData(QLatin1String("davSearchQuery"));
  if ( !query.isEmpty() )
  {
    QByteArray request = "<?xml version=\"1.0\"?>\r\n";
    request.append( "<D:searchrequest xmlns:D=\"DAV:\">\r\n" );
    request.append( query.toUtf8() );
    request.append( "</D:searchrequest>\r\n" );

    davSetRequest( request );
  } else {
    // We are only after certain features...
    QByteArray request;
    request = "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
    "<D:propfind xmlns:D=\"DAV:\">";

    // insert additional XML request from the davRequestResponse metadata
    if ( hasMetaData(QLatin1String("davRequestResponse")) )
      request += metaData(QLatin1String("davRequestResponse")).toUtf8();
    else {
      // No special request, ask for default properties
      request += "<D:prop>"
      "<D:creationdate/>"
      "<D:getcontentlength/>"
      "<D:displayname/>"
      "<D:source/>"
      "<D:getcontentlanguage/>"
      "<D:getcontenttype/>"
      "<D:getlastmodified/>"
      "<D:getetag/>"
      "<D:supportedlock/>"
      "<D:lockdiscovery/>"
      "<D:resourcetype/>"
      "</D:prop>";
    }
    request += "</D:propfind>";

    davSetRequest( request );
  }

  // WebDAV Stat or List...
  m_request.method = query.isEmpty() ? DAV_PROPFIND : DAV_SEARCH;
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;
  m_request.davData.depth = stat ? 0 : 1;
  if (!stat) {
    if (!m_request.url.path().endsWith(QLatin1Char('/')))
      m_request.url.setPath(m_request.url.path() + QLatin1Char('/'));
  }

  proceedUntilResponseContent( true );
  infoMessage(QLatin1String(""));

  // Has a redirection already been called? If so, we're done.
  if (m_isRedirection || m_iError) {
    if (m_isRedirection) {
        davFinished();
    }
    return;
  }

  QDomDocument multiResponse;
  multiResponse.setContent( m_webDavDataBuf, true );

  bool hasResponse = false;

  // qDebug() << endl << multiResponse.toString(2);

  for ( QDomNode n = multiResponse.documentElement().firstChild();
        !n.isNull(); n = n.nextSibling()) {
    QDomElement thisResponse = n.toElement();
    if (thisResponse.isNull())
      continue;

    hasResponse = true;

    QDomElement href = thisResponse.namedItem(QLatin1String("href")).toElement();
    if ( !href.isNull() ) {
      entry.clear();

      const QUrl thisURL(href.text()); // href.text() is a percent-encoded url.
      if ( thisURL.isValid() ) {
        QString name = thisURL.fileName();

        // base dir of a listDir(): name should be "."
        if (!stat && thisURL.adjusted(QUrl::StripTrailingSlash) == url.adjusted(QUrl::StripTrailingSlash)) // TODO: use QUrl::matches
          name = QLatin1Char('.');

        entry.insert( KIO::UDSEntry::UDS_NAME, name.isEmpty() ? href.text() : name );
      }

      QDomNodeList propstats = thisResponse.elementsByTagName(QLatin1String("propstat"));

      davParsePropstats( propstats, entry );

      // Since a lot of webdav servers seem not to send the content-type information
      // for the requested directory listings, we attempt to guess the mime-type from
      // the resource name so long as the resource is not a directory.
      if (entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE).isEmpty() &&
          entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE) != S_IFDIR) {
          QMimeType mime = db.mimeTypeForFile(thisURL.path(), QMimeDatabase::MatchExtension);
          if (mime.isValid() && !mime.isDefault()) {
              // qDebug() << "Setting" << mime.name() << "as guessed mime type for" << thisURL.path();
              entry.insert(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE, mime.name());
          }
      }

      if ( stat ) {
        // return an item
        statEntry( entry );
        davFinished();
        return;
      }

      listEntry(entry);
    } else   {
      // qDebug() << "Error: no URL contained in response to PROPFIND on" << url;
    }
  }

  if ( stat || !hasResponse ) {
    error(ERR_DOES_NOT_EXIST, url.toDisplayString());
    return;
  }

  davFinished();
}

void HTTPProtocol::davGeneric( const QUrl& url, KIO::HTTP_METHOD method, qint64 size )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  // check to make sure this host supports WebDAV
  if ( !davHostOk() )
    return;

  // WebDAV method
  m_request.method = method;
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;

  m_iPostDataSize = (size > -1 ? static_cast<KIO::filesize_t>(size) : NO_SIZE);
  proceedUntilResponseContent();
}

int HTTPProtocol::codeFromResponse( const QString& response )
{
  const int firstSpace = response.indexOf( QLatin1Char(' ') );
  const int secondSpace = response.indexOf( QLatin1Char(' '), firstSpace + 1 );
  return response.mid( firstSpace + 1, secondSpace - firstSpace - 1 ).toInt();
}

void HTTPProtocol::davParsePropstats( const QDomNodeList& propstats, UDSEntry& entry )
{
  QString mimeType;
  bool foundExecutable = false;
  bool isDirectory = false;
  uint lockCount = 0;
  uint supportedLockCount = 0;

  for ( int i = 0; i < propstats.count(); i++)
  {
    QDomElement propstat = propstats.item(i).toElement();

    QDomElement status = propstat.namedItem(QLatin1String("status")).toElement();
    if ( status.isNull() )
    {
      // error, no status code in this propstat
      // qDebug() << "Error, no status code in this propstat";
      return;
    }

    int code = codeFromResponse( status.text() );

    if ( code != 200 )
    {
      // qDebug() << "Got status code" << code << "(this may mean that some properties are unavailable)";
      continue;
    }

    QDomElement prop = propstat.namedItem( QLatin1String("prop") ).toElement();
    if ( prop.isNull() )
    {
      // qDebug() << "Error: no prop segment in this propstat.";
      return;
    }

    if ( hasMetaData( QLatin1String("davRequestResponse") ) )
    {
      QDomDocument doc;
      doc.appendChild(prop);
      entry.insert( KIO::UDSEntry::UDS_XML_PROPERTIES, doc.toString() );
    }

    for ( QDomNode n = prop.firstChild(); !n.isNull(); n = n.nextSibling() )
    {
      QDomElement property = n.toElement();
      if (property.isNull())
        continue;

      if ( property.namespaceURI() != QLatin1String("DAV:") )
      {
        // break out - we're only interested in properties from the DAV namespace
        continue;
      }

      if ( property.tagName() == QLatin1String("creationdate") )
      {
        // Resource creation date. Should be is ISO 8601 format.
        entry.insert(KIO::UDSEntry::UDS_CREATION_TIME,
                      parseDateTime(property.text(), property.attribute(QStringLiteral("dt"))).toTime_t());
      }
      else if ( property.tagName() == QLatin1String("getcontentlength") )
      {
        // Content length (file size)
        entry.insert( KIO::UDSEntry::UDS_SIZE, property.text().toULong() );
      }
      else if ( property.tagName() == QLatin1String("displayname") )
      {
        // Name suitable for presentation to the user
        setMetaData( QLatin1String("davDisplayName"), property.text() );
      }
      else if ( property.tagName() == QLatin1String("source") )
      {
        // Source template location
        QDomElement source = property.namedItem( QLatin1String("link") ).toElement()
                                      .namedItem( QLatin1String("dst") ).toElement();
        if ( !source.isNull() )
          setMetaData( QLatin1String("davSource"), source.text() );
      }
      else if ( property.tagName() == QLatin1String("getcontentlanguage") )
      {
        // equiv. to Content-Language header on a GET
        setMetaData( QLatin1String("davContentLanguage"), property.text() );
      }
      else if ( property.tagName() == QLatin1String("getcontenttype") )
      {
        // Content type (mime type)
        // This may require adjustments for other server-side webdav implementations
        // (tested with Apache + mod_dav 1.0.3)
        if ( property.text() == QLatin1String("httpd/unix-directory") )
        {
          isDirectory = true;
        }
        else
        {
          mimeType = property.text();
        }
      }
      else if ( property.tagName() == QLatin1String("executable") )
      {
        // File executable status
        if ( property.text() == QLatin1String("T") )
          foundExecutable = true;

      }
      else if ( property.tagName() == QLatin1String("getlastmodified") )
      {
        // Last modification date
        entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME,
                      parseDateTime(property.text(), property.attribute(QStringLiteral("dt"))).toTime_t());
      }
      else if ( property.tagName() == QLatin1String("getetag") )
      {
        // Entity tag
        setMetaData( QLatin1String("davEntityTag"), property.text() );
      }
      else if ( property.tagName() == QLatin1String("supportedlock") )
      {
        // Supported locking specifications
        for ( QDomNode n2 = property.firstChild(); !n2.isNull(); n2 = n2.nextSibling() )
        {
          QDomElement lockEntry = n2.toElement();
          if ( lockEntry.tagName() == QLatin1String("lockentry") )
          {
            QDomElement lockScope = lockEntry.namedItem( QLatin1String("lockscope") ).toElement();
            QDomElement lockType = lockEntry.namedItem( QLatin1String("locktype") ).toElement();
            if ( !lockScope.isNull() && !lockType.isNull() )
            {
              // Lock type was properly specified
              supportedLockCount++;
              const QString lockCountStr = QString::number(supportedLockCount);
              const QString scope = lockScope.firstChild().toElement().tagName();
              const QString type = lockType.firstChild().toElement().tagName();

              setMetaData( QLatin1String("davSupportedLockScope") + lockCountStr, scope );
              setMetaData( QLatin1String("davSupportedLockType") + lockCountStr, type );
            }
          }
        }
      }
      else if ( property.tagName() == QLatin1String("lockdiscovery") )
      {
        // Lists the available locks
        davParseActiveLocks( property.elementsByTagName( QLatin1String("activelock") ), lockCount );
      }
      else if ( property.tagName() == QLatin1String("resourcetype") )
      {
        // Resource type. "Specifies the nature of the resource."
        if ( !property.namedItem( QLatin1String("collection") ).toElement().isNull() )
        {
          // This is a collection (directory)
          isDirectory = true;
        }
      }
      else
      {
        // qDebug() << "Found unknown webdav property:" << property.tagName();
      }
    }
  }

  setMetaData( QLatin1String("davLockCount"), QString::number(lockCount) );
  setMetaData( QLatin1String("davSupportedLockCount"), QString::number(supportedLockCount) );

  entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, isDirectory ? S_IFDIR : S_IFREG );

  if ( foundExecutable || isDirectory )
  {
    // File was executable, or is a directory.
    entry.insert( KIO::UDSEntry::UDS_ACCESS, 0700 );
  }
  else
  {
    entry.insert( KIO::UDSEntry::UDS_ACCESS, 0600 );
  }

  if ( !isDirectory && !mimeType.isEmpty() )
  {
    entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, mimeType );
  }
}

void HTTPProtocol::davParseActiveLocks( const QDomNodeList& activeLocks,
                                        uint& lockCount )
{
  for ( int i = 0; i < activeLocks.count(); i++ )
  {
    const QDomElement activeLock = activeLocks.item(i).toElement();

    lockCount++;
    // required
    const QDomElement lockScope = activeLock.namedItem( QLatin1String("lockscope") ).toElement();
    const QDomElement lockType = activeLock.namedItem( QLatin1String("locktype") ).toElement();
    const QDomElement lockDepth = activeLock.namedItem( QLatin1String("depth") ).toElement();
    // optional
    const QDomElement lockOwner = activeLock.namedItem( QLatin1String("owner") ).toElement();
    const QDomElement lockTimeout = activeLock.namedItem( QLatin1String("timeout") ).toElement();
    const QDomElement lockToken = activeLock.namedItem( QLatin1String("locktoken") ).toElement();

    if ( !lockScope.isNull() && !lockType.isNull() && !lockDepth.isNull() )
    {
      // lock was properly specified
      lockCount++;
      const QString lockCountStr = QString::number(lockCount);
      const QString scope = lockScope.firstChild().toElement().tagName();
      const QString type = lockType.firstChild().toElement().tagName();
      const QString depth = lockDepth.text();

      setMetaData( QLatin1String("davLockScope") + lockCountStr, scope );
      setMetaData( QLatin1String("davLockType") + lockCountStr, type );
      setMetaData( QLatin1String("davLockDepth") + lockCountStr, depth );

      if ( !lockOwner.isNull() )
          setMetaData( QLatin1String("davLockOwner") + lockCountStr, lockOwner.text() );

      if ( !lockTimeout.isNull() )
          setMetaData( QLatin1String("davLockTimeout") + lockCountStr, lockTimeout.text() );

      if ( !lockToken.isNull() )
      {
        QDomElement tokenVal = lockScope.namedItem( QLatin1String("href") ).toElement();
        if ( !tokenVal.isNull() )
            setMetaData( QLatin1String("davLockToken") + lockCountStr, tokenVal.text() );
      }
    }
  }
}

QDateTime HTTPProtocol::parseDateTime(const QString &input, const QString &type)
{
  if ( type == QLatin1String("dateTime.tz") )
  {
    return QDateTime::fromString(input, Qt::ISODate);
  }
  else if ( type == QLatin1String("dateTime.rfc1123") )
  {
    return QDateTime::fromString(input, Qt::RFC2822Date);
  }

  // format not advertised... try to parse anyway
  QDateTime time = QDateTime::fromString(input, Qt::RFC2822Date);
  if (time.isValid())
    return time;

  return QDateTime::fromString(input, Qt::ISODate);
}

QString HTTPProtocol::davProcessLocks()
{
    if ( hasMetaData( QLatin1String("davLockCount") ) )
    {
        QString response = QLatin1String("If:");
        int numLocks = metaData( QLatin1String("davLockCount") ).toInt();
    bool bracketsOpen = false;
    for ( int i = 0; i < numLocks; i++ )
    {
      const QString countStr = QString::number(i);
      if ( hasMetaData( QLatin1String("davLockToken") + countStr ) )
      {
          if ( hasMetaData( QLatin1String("davLockURL") + countStr ) )
        {
          if ( bracketsOpen )
          {
            response += QLatin1Char(')');
            bracketsOpen = false;
          }
          response += QLatin1String(" <") + metaData( QLatin1String("davLockURL") + countStr ) + QLatin1Char('>');
        }

        if ( !bracketsOpen )
        {
          response += QLatin1String(" (");
          bracketsOpen = true;
        }
        else
        {
          response += QLatin1Char(' ');
        }

        if ( hasMetaData( QLatin1String("davLockNot") + countStr ) )
          response += QLatin1String("Not ");

        response += QLatin1Char('<') + metaData( QLatin1String("davLockToken") + countStr ) + QLatin1Char('>');
      }
    }

    if ( bracketsOpen )
      response += QLatin1Char(')');

    response += QLatin1String("\r\n");
    return response;
  }

  return QString();
}

bool HTTPProtocol::davHostOk()
{
  // FIXME needs to be reworked. Switched off for now.
  return true;

  // cached?
  if ( m_davHostOk )
  {
    // qDebug() << "true";
    return true;
  }
  else if ( m_davHostUnsupported )
  {
    // qDebug() << " false";
    davError( -2 );
    return false;
  }

  m_request.method = HTTP_OPTIONS;

  // query the server's capabilities generally, not for a specific URL
  m_request.url.setPath(QLatin1String("*"));
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;

  // clear davVersions variable, which holds the response to the DAV: header
  m_davCapabilities.clear();

  proceedUntilResponseHeader();

  if (m_davCapabilities.count())
  {
    for (int i = 0; i < m_davCapabilities.count(); i++)
    {
      bool ok;
      uint verNo = m_davCapabilities[i].toUInt(&ok);
      if (ok && verNo > 0 && verNo < 3)
      {
        m_davHostOk = true;
        // qDebug() << "Server supports DAV version" << verNo;
      }
    }

    if ( m_davHostOk )
      return true;
  }

  m_davHostUnsupported = true;
  davError( -2 );
  return false;
}

// This function is for closing proceedUntilResponseHeader(); requests
// Required because there may or may not be further info expected
void HTTPProtocol::davFinished()
{
  // TODO: Check with the DAV extension developers
  httpClose(m_request.isKeepAlive);
  finished();
}

void HTTPProtocol::mkdir( const QUrl& url, int )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = DAV_MKCOL;
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;

  proceedUntilResponseHeader();

  if ( m_request.responseCode == 201 )
    davFinished();
  else
    davError();
}

void HTTPProtocol::get( const QUrl& url )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = HTTP_GET;

  QString tmp(metaData(QLatin1String("cache")));
  if (!tmp.isEmpty())
    m_request.cacheTag.policy = parseCacheControl(tmp);
  else
    m_request.cacheTag.policy = DEFAULT_CACHE_CONTROL;

  proceedUntilResponseContent();
}

void HTTPProtocol::put( const QUrl &url, int, KIO::JobFlags flags )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;

  resetSessionSettings();

  // Webdav hosts are capable of observing overwrite == false
  if (m_protocol.startsWith("webdav")) { // krazy:exclude=strings
    if (!(flags & KIO::Overwrite)) {
      // check to make sure this host supports WebDAV
      if (!davHostOk())
        return;

      const QByteArray request ("<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
                                "<D:propfind xmlns:D=\"DAV:\"><D:prop>"
                                "<D:creationdate/>"
                                "<D:getcontentlength/>"
                                "<D:displayname/>"
                                "<D:resourcetype/>"
                                "</D:prop></D:propfind>");

      davSetRequest( request );

      // WebDAV Stat or List...
      m_request.method = DAV_PROPFIND;
      m_request.url.setQuery(QString());
      m_request.cacheTag.policy = CC_Reload;
      m_request.davData.depth = 0;

      proceedUntilResponseContent(true);

      if (!m_request.isKeepAlive) {
          httpCloseConnection(); // close connection if server requested it.
          m_request.isKeepAlive = true; // reset the keep alive flag.
      }

      if (m_request.responseCode == 207) {
        error(ERR_FILE_ALREADY_EXIST, QString());
        return;
      }

      // force re-authentication...
      delete m_wwwAuth;
      m_wwwAuth = 0;
    }
  }

  m_request.method = HTTP_PUT;
  m_request.cacheTag.policy = CC_Reload;

  proceedUntilResponseContent();
}

void HTTPProtocol::copy( const QUrl& src, const QUrl& dest, int, KIO::JobFlags flags )
{
  // qDebug() << src << "->" << dest;

  if (!maybeSetRequestUrl(dest) || !maybeSetRequestUrl(src))
    return;
  resetSessionSettings();

  // destination has to be "http(s)://..."
  QUrl newDest = dest;
  if (newDest.scheme() == QLatin1String("webdavs"))
    newDest.setScheme(QLatin1String("https"));
  else if (newDest.scheme() == QLatin1String("webdav"))
    newDest.setScheme(QLatin1String("http"));

  m_request.method = DAV_COPY;
  m_request.davData.desturl = newDest.toString();
  m_request.davData.overwrite = (flags & KIO::Overwrite);
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;

  proceedUntilResponseHeader();

  // The server returns a HTTP/1.1 201 Created or 204 No Content on successful completion
  if ( m_request.responseCode == 201 || m_request.responseCode == 204 )
    davFinished();
  else
    davError();
}

void HTTPProtocol::rename( const QUrl& src, const QUrl& dest, KIO::JobFlags flags )
{
  // qDebug() << src << "->" << dest;

  if (!maybeSetRequestUrl(dest) || !maybeSetRequestUrl(src))
    return;
  resetSessionSettings();

  // destination has to be "http://..."
  QUrl newDest = dest;
  if (newDest.scheme() == QLatin1String("webdavs"))
    newDest.setScheme(QLatin1String("https"));
  else if (newDest.scheme() == QLatin1String("webdav"))
    newDest.setScheme(QLatin1String("http"));

  m_request.method = DAV_MOVE;
  m_request.davData.desturl = newDest.toString();
  m_request.davData.overwrite = (flags & KIO::Overwrite);
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy = CC_Reload;

  proceedUntilResponseHeader();

  // Work around strict Apache-2 WebDAV implementation which refuses to cooperate
  // with webdav://host/directory, instead requiring webdav://host/directory/
  // (strangely enough it accepts Destination: without a trailing slash)
  // See BR# 209508 and BR#187970
  if ( m_request.responseCode == 301) {
    m_request.url = m_request.redirectUrl;
    m_request.method = DAV_MOVE;
    m_request.davData.desturl = newDest.toString();
    m_request.davData.overwrite = (flags & KIO::Overwrite);
    m_request.url.setQuery(QString());
    m_request.cacheTag.policy = CC_Reload;
    // force re-authentication...
    delete m_wwwAuth;
    m_wwwAuth = 0;
    proceedUntilResponseHeader();
  }

  if ( m_request.responseCode == 201 )
    davFinished();
  else
    davError();
}

void HTTPProtocol::del( const QUrl& url, bool )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;

  resetSessionSettings();

  m_request.method = HTTP_DELETE;
  m_request.cacheTag.policy = CC_Reload;

  if (m_protocol.startsWith("webdav")) { //krazy:exclude=strings due to QByteArray
    m_request.url.setQuery(QString());
    if (!proceedUntilResponseHeader()) {
      return;
    }

    // The server returns a HTTP/1.1 200 Ok or HTTP/1.1 204 No Content
    // on successful completion.
    if ( m_request.responseCode == 200 || m_request.responseCode == 204 || m_isRedirection)
      davFinished();
    else
      davError();

    return;
  }

  proceedUntilResponseContent();
}

void HTTPProtocol::post( const QUrl& url, qint64 size )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = HTTP_POST;
  m_request.cacheTag.policy= CC_Reload;

  m_iPostDataSize = (size > -1 ? static_cast<KIO::filesize_t>(size) : NO_SIZE);
  proceedUntilResponseContent();
}

void HTTPProtocol::davLock( const QUrl& url, const QString& scope,
                            const QString& type, const QString& owner )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = DAV_LOCK;
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy= CC_Reload;

  /* Create appropriate lock XML request. */
  QDomDocument lockReq;

  QDomElement lockInfo = lockReq.createElementNS( QLatin1String("DAV:"), QLatin1String("lockinfo") );
  lockReq.appendChild( lockInfo );

  QDomElement lockScope = lockReq.createElement( QLatin1String("lockscope") );
  lockInfo.appendChild( lockScope );

  lockScope.appendChild( lockReq.createElement( scope ) );

  QDomElement lockType = lockReq.createElement( QLatin1String("locktype") );
  lockInfo.appendChild( lockType );

  lockType.appendChild( lockReq.createElement( type ) );

  if ( !owner.isNull() ) {
    QDomElement ownerElement = lockReq.createElement( QLatin1String("owner") );
    lockReq.appendChild( ownerElement );

    QDomElement ownerHref = lockReq.createElement( QLatin1String("href") );
    ownerElement.appendChild( ownerHref );

    ownerHref.appendChild( lockReq.createTextNode( owner ) );
  }

  // insert the document into the POST buffer
  cachePostData(lockReq.toByteArray());

  proceedUntilResponseContent( true );

  if ( m_request.responseCode == 200 ) {
    // success
    QDomDocument multiResponse;
    multiResponse.setContent( m_webDavDataBuf, true );

    QDomElement prop = multiResponse.documentElement().namedItem( QLatin1String("prop") ).toElement();

    QDomElement lockdiscovery = prop.namedItem( QLatin1String("lockdiscovery") ).toElement();

    uint lockCount = 0;
    davParseActiveLocks( lockdiscovery.elementsByTagName( QLatin1String("activelock") ), lockCount );

    setMetaData( QLatin1String("davLockCount"), QString::number( lockCount ) );

    finished();

  } else
    davError();
}

void HTTPProtocol::davUnlock( const QUrl& url )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = DAV_UNLOCK;
  m_request.url.setQuery(QString());
  m_request.cacheTag.policy= CC_Reload;

  proceedUntilResponseContent( true );

  if ( m_request.responseCode == 200 )
    finished();
  else
    davError();
}

QString HTTPProtocol::davError( int code /* = -1 */, const QString &_url )
{
  bool callError = false;
  if ( code == -1 ) {
    code = m_request.responseCode;
    callError = true;
  }
  if ( code == -2 ) {
    callError = true;
  }

  QString url = _url;
  if ( !url.isNull() )
    url = m_request.url.toDisplayString();

  QString action, errorString;
  int errorCode = ERR_SLAVE_DEFINED;

  // for 412 Precondition Failed
  QString ow = i18n( "Otherwise, the request would have succeeded." );

  switch ( m_request.method ) {
    case DAV_PROPFIND:
      action = i18nc( "request type", "retrieve property values" );
      break;
    case DAV_PROPPATCH:
      action = i18nc( "request type", "set property values" );
      break;
    case DAV_MKCOL:
      action = i18nc( "request type", "create the requested folder" );
      break;
    case DAV_COPY:
      action = i18nc( "request type", "copy the specified file or folder" );
      break;
    case DAV_MOVE:
      action = i18nc( "request type", "move the specified file or folder" );
      break;
    case DAV_SEARCH:
      action = i18nc( "request type", "search in the specified folder" );
      break;
    case DAV_LOCK:
      action = i18nc( "request type", "lock the specified file or folder" );
      break;
    case DAV_UNLOCK:
      action = i18nc( "request type", "unlock the specified file or folder" );
      break;
    case HTTP_DELETE:
      action = i18nc( "request type", "delete the specified file or folder" );
      break;
    case HTTP_OPTIONS:
      action = i18nc( "request type", "query the server's capabilities" );
      break;
    case HTTP_GET:
      action = i18nc( "request type", "retrieve the contents of the specified file or folder" );
      break;
    case DAV_REPORT:
      action = i18nc( "request type", "run a report in the specified folder" );
      break;
    case HTTP_PUT:
    case HTTP_POST:
    case HTTP_HEAD:
    default:
      // this should not happen, this function is for webdav errors only
      Q_ASSERT(0);
  }

  // default error message if the following code fails
  errorString = i18nc("%1: code, %2: request type", "An unexpected error (%1) occurred "
                      "while attempting to %2.", code, action);

  switch ( code )
  {
    case -2:
      // internal error: OPTIONS request did not specify DAV compliance
      // ERR_UNSUPPORTED_PROTOCOL
      errorString = i18n("The server does not support the WebDAV protocol.");
      break;
    case 207:
      // 207 Multi-status
    {
      // our error info is in the returned XML document.
      // retrieve the XML document

      // there was an error retrieving the XML document.
      // ironic, eh?
      if ( !readBody( true ) && m_iError )
        return QString();

      QStringList errors;
      QDomDocument multiResponse;

      multiResponse.setContent( m_webDavDataBuf, true );

      QDomElement multistatus = multiResponse.documentElement().namedItem( QLatin1String("multistatus") ).toElement();

      QDomNodeList responses = multistatus.elementsByTagName( QLatin1String("response") );

      for (int i = 0; i < responses.count(); i++)
      {
        int errCode;
        QString errUrl;

        QDomElement response = responses.item(i).toElement();
        QDomElement code = response.namedItem( QLatin1String("status") ).toElement();

        if ( !code.isNull() )
        {
          errCode = codeFromResponse( code.text() );
          QDomElement href = response.namedItem( QLatin1String("href") ).toElement();
          if ( !href.isNull() )
            errUrl = href.text();
          errors << davError( errCode, errUrl );
        }
      }

      //kError = ERR_SLAVE_DEFINED;
      errorString = i18nc( "%1: request type, %2: url",
                           "An error occurred while attempting to %1, %2. A "
                           "summary of the reasons is below.", action, url );

      errorString += QLatin1String("<ul>");

      Q_FOREACH(const QString& error, errors)
        errorString += QLatin1String("<li>") + error + QLatin1String("</li>");

      errorString += QLatin1String("</ul>");
    }
    case 403:
    case 500: // hack: Apache mod_dav returns this instead of 403 (!)
      // 403 Forbidden
      // ERR_ACCESS_DENIED
      errorString = i18nc( "%1: request type", "Access was denied while attempting to %1.",  action );
      break;
    case 405:
      // 405 Method Not Allowed
      if ( m_request.method == DAV_MKCOL ) {
        // ERR_DIR_ALREADY_EXIST
        errorString = url;
        errorCode = ERR_DIR_ALREADY_EXIST;
      }
      break;
    case 409:
      // 409 Conflict
      // ERR_ACCESS_DENIED
      errorString = i18n("A resource cannot be created at the destination "
                         "until one or more intermediate collections (folders) "
                         "have been created.");
      break;
    case 412:
      // 412 Precondition failed
      if ( m_request.method == DAV_COPY || m_request.method == DAV_MOVE ) {
        // ERR_ACCESS_DENIED
        errorString = i18n("The server was unable to maintain the liveness of "
                           "the properties listed in the propertybehavior XML "
                           "element or you attempted to overwrite a file while "
                           "requesting that files are not overwritten. %1",
                             ow );

      } else if ( m_request.method == DAV_LOCK ) {
        // ERR_ACCESS_DENIED
        errorString = i18n("The requested lock could not be granted. %1",  ow );
      }
      break;
    case 415:
      // 415 Unsupported Media Type
      // ERR_ACCESS_DENIED
      errorString = i18n("The server does not support the request type of the body.");
      break;
    case 423:
      // 423 Locked
      // ERR_ACCESS_DENIED
      errorString = i18nc( "%1: request type", "Unable to %1 because the resource is locked.",  action );
      break;
    case 425:
      // 424 Failed Dependency
      errorString = i18n("This action was prevented by another error.");
      break;
    case 502:
      // 502 Bad Gateway
      if ( m_request.method == DAV_COPY || m_request.method == DAV_MOVE ) {
        // ERR_WRITE_ACCESS_DENIED
        errorString = i18nc( "%1: request type", "Unable to %1 because the destination server refuses "
                           "to accept the file or folder.",  action );
      }
      break;
    case 507:
      // 507 Insufficient Storage
      // ERR_DISK_FULL
      errorString = i18n("The destination resource does not have sufficient space "
                         "to record the state of the resource after the execution "
                         "of this method.");
      break;
    default:
      break;
  }

  // if ( kError != ERR_SLAVE_DEFINED )
  //errorString += " (" + url + ')';

  if ( callError )
    error( errorCode, errorString );

  return errorString;
}

// HTTP generic error
static int httpGenericError(const HTTPProtocol::HTTPRequest& request, QString* errorString)
{
  Q_ASSERT(errorString);

  int errorCode = 0;
  errorString->clear();

  if (request.responseCode == 204) {
      errorCode = ERR_NO_CONTENT;
  }

  return errorCode;
}

// HTTP DELETE specific errors
static int httpDelError(const HTTPProtocol::HTTPRequest& request, QString* errorString)
{
  Q_ASSERT(errorString);

  int errorCode = 0;
  const int responseCode = request.responseCode;
  errorString->clear();

  switch (responseCode) {
  case 204:
      errorCode = ERR_NO_CONTENT;
      break;
  default:
      break;
  }

  if (!errorCode
      && (responseCode < 200 || responseCode > 400)
      && responseCode != 404) {
    errorCode = ERR_SLAVE_DEFINED;
    *errorString = i18n( "The resource cannot be deleted." );
  }

  return errorCode;
}

// HTTP PUT specific errors
static int httpPutError(const HTTPProtocol::HTTPRequest& request, QString* errorString)
{
  Q_ASSERT(errorString);

  int errorCode = 0;
  const int responseCode = request.responseCode;
  const QString action (i18nc("request type", "upload %1", request.url.toDisplayString()));

  switch (responseCode) {
    case 403:
    case 405:
    case 500: // hack: Apache mod_dav returns this instead of 403 (!)
      // 403 Forbidden
      // 405 Method Not Allowed
      // ERR_ACCESS_DENIED
      *errorString = i18nc( "%1: request type", "Access was denied while attempting to %1.",  action );
      errorCode = ERR_SLAVE_DEFINED;
      break;
    case 409:
      // 409 Conflict
      // ERR_ACCESS_DENIED
      *errorString = i18n("A resource cannot be created at the destination "
                  "until one or more intermediate collections (folders) "
                  "have been created.");
      errorCode = ERR_SLAVE_DEFINED;
      break;
    case 423:
      // 423 Locked
      // ERR_ACCESS_DENIED
      *errorString = i18nc( "%1: request type", "Unable to %1 because the resource is locked.",  action );
      errorCode = ERR_SLAVE_DEFINED;
      break;
    case 502:
      // 502 Bad Gateway
      // ERR_WRITE_ACCESS_DENIED;
      *errorString = i18nc( "%1: request type", "Unable to %1 because the destination server refuses "
                          "to accept the file or folder.",  action );
      errorCode = ERR_SLAVE_DEFINED;
      break;
    case 507:
      // 507 Insufficient Storage
      // ERR_DISK_FULL
      *errorString = i18n("The destination resource does not have sufficient space "
                        "to record the state of the resource after the execution "
                        "of this method.");
      errorCode = ERR_SLAVE_DEFINED;
      break;
    default:
      break;
  }

  if (!errorCode
      && (responseCode < 200 || responseCode > 400)
      && responseCode != 404) {
    errorCode = ERR_SLAVE_DEFINED;
    *errorString = i18nc("%1: response code, %2: request type",
                         "An unexpected error (%1) occurred while attempting to %2.",
                         responseCode, action);
  }

  return errorCode;
}

bool HTTPProtocol::sendHttpError()
{
  QString errorString;
  int errorCode = 0;

  switch (m_request.method) {
  case HTTP_GET:
  case HTTP_POST:
      errorCode = httpGenericError(m_request, &errorString);
      break;
  case HTTP_PUT:
      errorCode = httpPutError(m_request, &errorString);
      break;
  case HTTP_DELETE:
      errorCode = httpDelError(m_request, &errorString);
      break;
  default:
      break;
  }

  // Force any message previously shown by the client to be cleared.
  infoMessage(QLatin1String(""));

  if (errorCode) {
    error( errorCode, errorString );
    return true;
  }

  return false;
}

bool HTTPProtocol::sendErrorPageNotification()
{
    if (!m_request.preferErrorPage)
        return false;

    if (m_isLoadingErrorPage)
        qWarning() << "called twice during one request, something is probably wrong.";

    m_isLoadingErrorPage = true;
    SlaveBase::errorPage();
    return true;
}

bool HTTPProtocol::isOffline()
{
  // ### TEMPORARY WORKAROUND (While investigating why solid may
  // produce false positives)
  return false;

  Solid::Networking::Status status = Solid::Networking::status();

  // qDebug() << "networkstatus:" << status;

  // on error or unknown, we assume online
  return status == Solid::Networking::Unconnected;
}

void HTTPProtocol::multiGet(const QByteArray &data)
{
    QDataStream stream(data);
    quint32 n;
    stream >> n;

    // qDebug() << n;

    HTTPRequest saveRequest;
    if (m_isBusy)
        saveRequest = m_request;

    resetSessionSettings();

    for (unsigned i = 0; i < n; ++i) {
        QUrl url;
        stream >> url >> mIncomingMetaData;

        if (!maybeSetRequestUrl(url))
            continue;

        //### should maybe call resetSessionSettings() if the server/domain is
        //    different from the last request!

        // qDebug() << url;

        m_request.method = HTTP_GET;
        m_request.isKeepAlive = true;   //readResponseHeader clears it if necessary

        QString tmp = metaData(QLatin1String("cache"));
        if (!tmp.isEmpty())
            m_request.cacheTag.policy= parseCacheControl(tmp);
        else
            m_request.cacheTag.policy= DEFAULT_CACHE_CONTROL;

        m_requestQueue.append(m_request);
    }

    if (m_isBusy)
        m_request = saveRequest;
#if 0
    if (!m_isBusy) {
        m_isBusy = true;
        QMutableListIterator<HTTPRequest> it(m_requestQueue);
        while (it.hasNext()) {
            m_request = it.next();
            it.remove();
            proceedUntilResponseContent();
        }
        m_isBusy = false;
    }
#endif
    if (!m_isBusy) {
        m_isBusy = true;
        QMutableListIterator<HTTPRequest> it(m_requestQueue);
        // send the requests
        while (it.hasNext()) {
            m_request = it.next();
            sendQuery();
            // save the request state so we can pick it up again in the collection phase
            it.setValue(m_request);
            // qDebug() << "check one: isKeepAlive =" << m_request.isKeepAlive;
            if (m_request.cacheTag.ioMode != ReadFromCache) {
                m_server.initFrom(m_request);
            }
        }
        // collect the responses
        //### for the moment we use a hack: instead of saving and restoring request-id
        //    we just count up like ParallelGetJobs does.
        int requestId = 0;
        Q_FOREACH (const HTTPRequest &r, m_requestQueue) {
            m_request = r;
            // qDebug() << "check two: isKeepAlive =" << m_request.isKeepAlive;
            setMetaData(QLatin1String("request-id"), QString::number(requestId++));
            sendAndKeepMetaData();
            if (!(readResponseHeader() && readBody())) {
                return;
            }
            // the "next job" signal for ParallelGetJob is data of size zero which
            // readBody() sends without our intervention.
            // qDebug() << "check three: isKeepAlive =" << m_request.isKeepAlive;
            httpClose(m_request.isKeepAlive);  //actually keep-alive is mandatory for pipelining
        }

        finished();
        m_requestQueue.clear();
        m_isBusy = false;
    }
}

ssize_t HTTPProtocol::write (const void *_buf, size_t nbytes)
{
  size_t sent = 0;
  const char* buf = static_cast<const char*>(_buf);
  while (sent < nbytes)
  {
    int n = TCPSlaveBase::write(buf + sent, nbytes - sent);

    if (n < 0) {
      // some error occurred
      return -1;
    }

    sent += n;
  }

  return sent;
}

void HTTPProtocol::clearUnreadBuffer()
{
    m_unreadBuf.clear();
}

// Note: the implementation of unread/readBuffered assumes that unread will only
// be used when there is extra data we don't want to handle, and not to wait for more data.
void HTTPProtocol::unread(char *buf, size_t size)
{
    // implement LIFO (stack) semantics
    const int newSize = m_unreadBuf.size() + size;
    m_unreadBuf.resize(newSize);
    for (size_t i = 0; i < size; i++) {
        m_unreadBuf.data()[newSize - i - 1] = buf[i];
    }
    if (size) {
        //hey, we still have data, closed connection or not!
        m_isEOF = false;
    }
}

size_t HTTPProtocol::readBuffered(char *buf, size_t size, bool unlimited)
{
    size_t bytesRead = 0;
    if (!m_unreadBuf.isEmpty()) {
        const int bufSize = m_unreadBuf.size();
        bytesRead = qMin((int)size, bufSize);

        for (size_t i = 0; i < bytesRead; i++) {
            buf[i] = m_unreadBuf.constData()[bufSize - i - 1];
        }
        m_unreadBuf.truncate(bufSize - bytesRead);

        // If we have an unread buffer and the size of the content returned by the
        // server is unknown, e.g. chuncked transfer, return the bytes read here since
        // we may already have enough data to complete the response and don't want to
        // wait for more. See BR# 180631.
        if (unlimited)
            return bytesRead;
    }
    if (bytesRead < size) {
        int rawRead = TCPSlaveBase::read(buf + bytesRead, size - bytesRead);
        if (rawRead < 1) {
            m_isEOF = true;
            return bytesRead;
        }
        bytesRead += rawRead;
    }
    return bytesRead;
}

//### this method will detect an n*(\r\n) sequence if it crosses invocations.
//    it will look (n*2 - 1) bytes before start at most and never before buf, naturally.
//    supported number of newlines are one and two, in line with HTTP syntax.
// return true if numNewlines newlines were found.
bool HTTPProtocol::readDelimitedText(char *buf, int *idx, int end, int numNewlines)
{
    Q_ASSERT(numNewlines >=1 && numNewlines <= 2);
    char mybuf[64]; //somewhere close to the usual line length to avoid unread()ing too much
    int pos = *idx;
    while (pos < end && !m_isEOF) {
        int step = qMin((int)sizeof(mybuf), end - pos);
        if (m_isChunked) {
            //we might be reading the end of the very last chunk after which there is no data.
            //don't try to read any more bytes than there are because it causes stalls
            //(yes, it shouldn't stall but it does)
            step = 1;
        }
        size_t bufferFill = readBuffered(mybuf, step);

        for (size_t i = 0; i < bufferFill ; ++i, ++pos) {
            // we copy the data from mybuf to buf immediately and look for the newlines in buf.
            // that way we don't miss newlines split over several invocations of this method.
            buf[pos] = mybuf[i];

            // did we just copy one or two times the (usually) \r\n delimiter?
            // until we find even more broken webservers in the wild let's assume that they either
            // send \r\n (RFC compliant) or \n (broken) as delimiter...
            if (buf[pos] == '\n') {
                bool found = numNewlines == 1;
                if (!found) {   // looking for two newlines
                    // Detect \n\n and \n\r\n. The other cases (\r\n\n, \r\n\r\n) are covered by the first two.
                    found = ((pos >= 1 && buf[pos - 1] == '\n') ||
                             (pos >= 2 && buf[pos - 2] == '\n' && buf[pos - 1] == '\r'));
                }
                if (found) {
                    i++;    // unread bytes *after* CRLF
                    unread(&mybuf[i], bufferFill - i);
                    *idx = pos + 1;
                    return true;
                }
            }
        }
    }
    *idx = pos;
    return false;
}

static bool isCompatibleNextUrl(const QUrl &previous, const QUrl &now)
{
    if (previous.host() != now.host() || previous.port() != now.port()) {
        return false;
    }
    if (previous.userName().isEmpty() && previous.password().isEmpty()) {
        return true;
    }
    return previous.userName() == now.userName() && previous.password() == now.password();
}

bool HTTPProtocol::httpShouldCloseConnection()
{
    // qDebug();

    if (!isConnected()) {
        return false;
    }

    if (!m_request.proxyUrls.isEmpty() && !isAutoSsl()) {
        Q_FOREACH(const QString& url, m_request.proxyUrls) {
            if (url != QLatin1String("DIRECT")) {
                if (isCompatibleNextUrl(m_server.proxyUrl, QUrl(url))) {
                    return false;
                }
            }
        }
        return true;
    }

    return !isCompatibleNextUrl(m_server.url, m_request.url);
}

bool HTTPProtocol::httpOpenConnection()
{
    // qDebug();
    m_server.clear();

    // Only save proxy auth information after proxy authentication has
    // actually taken place, which will set up exactly this connection.
    disconnect(socket(), SIGNAL(connected()),
               this, SLOT(saveProxyAuthenticationForSocket()));

    clearUnreadBuffer();

    int connectError = 0;
    QString errorString;

    // Get proxy information...
    if (m_request.proxyUrls.isEmpty()) {
        m_request.proxyUrls = config()->readEntry("ProxyUrls", QStringList());
        // qDebug() << "Proxy URLs:" << m_request.proxyUrls;
    }

    if (m_request.proxyUrls.isEmpty()) {
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        connectError = connectToHost(m_request.url.host(), m_request.url.port(defaultPort()), &errorString);
    } else {
        QList<QUrl> badProxyUrls;
        Q_FOREACH(const QString& proxyUrl, m_request.proxyUrls) {
            if (proxyUrl == QLatin1String("DIRECT")) {
                QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
                connectError = connectToHost(m_request.url.host(), m_request.url.port(defaultPort()), &errorString);
                if (connectError == 0) {
                    //qDebug() << "Connected DIRECT: host=" << m_request.url.host() << "port=" << m_request.url.port(defaultPort());
                    break;
                } else {
                    continue;
                }
            }

            const QUrl url(proxyUrl);
            const QString proxyScheme(url.scheme());
            if (!supportedProxyScheme(proxyScheme)) {
                connectError = ERR_COULD_NOT_CONNECT;
                errorString = url.toDisplayString();
                badProxyUrls << url;
                continue;
            }

            QNetworkProxy::ProxyType proxyType = QNetworkProxy::NoProxy;
            if (proxyScheme == QLatin1String("socks")) {
                proxyType = QNetworkProxy::Socks5Proxy;
            } else if (isAutoSsl()) {
                proxyType = QNetworkProxy::HttpProxy;
            }

            // qDebug() << "Connecting to proxy: address=" << proxyUrl << "type=" << proxyType;

            if (proxyType == QNetworkProxy::NoProxy) {
                QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
                connectError = connectToHost(url.host(), url.port(), &errorString);
                if (connectError == 0) {
                    m_request.proxyUrl = url;
                    //qDebug() << "Connected to proxy: host=" << url.host() << "port=" << url.port();
                    break;
                } else {
                    if (connectError == ERR_UNKNOWN_HOST) {
                        connectError = ERR_UNKNOWN_PROXY_HOST;
                    }
                    //qDebug() << "Failed to connect to proxy:" << proxyUrl;
                    badProxyUrls << url;
                }
            } else {
                QNetworkProxy proxy(proxyType, url.host(), url.port(), url.userName(), url.password());
                QNetworkProxy::setApplicationProxy(proxy);
                connectError = connectToHost(m_request.url.host(), m_request.url.port(defaultPort()), &errorString);
                if (connectError == 0) {
                    // qDebug() << "Tunneling thru proxy: host=" << url.host() << "port=" << url.port();
                    break;
                } else {
                    if (connectError == ERR_UNKNOWN_HOST) {
                        connectError = ERR_UNKNOWN_PROXY_HOST;
                    }
                    // qDebug() << "Failed to connect to proxy:" << proxyUrl;
                    badProxyUrls << url;
                    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
                }
            }
        }

        if (!badProxyUrls.isEmpty()) {
            //TODO: Notify the client of BAD proxy addresses (needed for PAC setups).
        }
    }

    if (connectError != 0) {
        error(connectError, errorString);
        return false;
    }

    // Disable Nagle's algorithm, i.e turn on TCP_NODELAY.
    KTcpSocket *sock = qobject_cast<KTcpSocket*>(socket());
    if (sock) {
        // qDebug() << "TCP_NODELAY:" << sock->socketOption(QAbstractSocket::LowDelayOption);
        sock->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    }

    m_server.initFrom(m_request);
    connected();
    return true;
}

bool HTTPProtocol::satisfyRequestFromCache(bool *cacheHasPage)
{
    // qDebug();

    if (m_request.cacheTag.useCache) {
        const bool offline = isOffline();

        if (offline && m_request.cacheTag.policy != KIO::CC_Reload) {
            m_request.cacheTag.policy= KIO::CC_CacheOnly;
        }

        const bool isCacheOnly = m_request.cacheTag.policy == KIO::CC_CacheOnly;
        const CacheTag::CachePlan plan = m_request.cacheTag.plan(m_maxCacheAge);

        bool openForReading = false;
        if (plan == CacheTag::UseCached || plan == CacheTag::ValidateCached) {
            openForReading = cacheFileOpenRead();

            if (!openForReading && (isCacheOnly || offline)) {
                // cache-only or offline -> we give a definite answer and it is "no"
                *cacheHasPage = false;
                if (isCacheOnly) {
                    error(ERR_DOES_NOT_EXIST, m_request.url.toDisplayString());
                } else if (offline) {
                    error(ERR_COULD_NOT_CONNECT, m_request.url.toDisplayString());
                }
                return true;
            }
        }

        if (openForReading) {
            m_request.cacheTag.ioMode = ReadFromCache;
            *cacheHasPage = true;
            // return false if validation is required, so a network request will be sent
            return m_request.cacheTag.plan(m_maxCacheAge) == CacheTag::UseCached;
        }
    }
    *cacheHasPage = false;
    return false;
}

QString HTTPProtocol::formatRequestUri() const
{
    // Only specify protocol, host and port when they are not already clear, i.e. when
    // we handle HTTP proxying ourself and the proxy server needs to know them.
    // Sending protocol/host/port in other cases confuses some servers, and it's not their fault.
    if (isHttpProxy(m_request.proxyUrl) && !isAutoSsl()) {
        QUrl u;

        QString protocol = m_request.url.scheme();
        if (protocol.startsWith(QLatin1String("webdav"))) {
            protocol.replace(0, qstrlen("webdav"), QLatin1String("http"));
        }
        u.setScheme(protocol);

        u.setHost(m_request.url.host());
        // if the URL contained the default port it should have been stripped earlier
        Q_ASSERT(m_request.url.port() != defaultPort());
        u.setPort(m_request.url.port());
        u.setPath(m_request.url.path(QUrl::FullyEncoded));
        u.setQuery(m_request.url.query(QUrl::FullyEncoded));
        return u.toString();
    } else {
        QString result = m_request.url.path(QUrl::FullyEncoded);
        if (m_request.url.hasQuery())
            result += QLatin1Char('?') + m_request.url.query(QUrl::FullyEncoded);
        return result;
    }
}

/**
 * This function is responsible for opening up the connection to the remote
 * HTTP server and sending the header.  If this requires special
 * authentication or other such fun stuff, then it will handle it.  This
 * function will NOT receive anything from the server, however.  This is in
 * contrast to previous incarnations of 'httpOpen' as this method used to be
 * called.
 *
 * The basic process now is this:
 *
 * 1) Open up the socket and port
 * 2) Format our request/header
 * 3) Send the header to the remote server
 * 4) Call sendBody() if the HTTP method requires sending body data
 */
bool HTTPProtocol::sendQuery()
{
  // qDebug();

  // Cannot have an https request without autoSsl!  This can
  // only happen if  the current installation does not support SSL...
  if (isEncryptedHttpVariety(m_protocol) && !isAutoSsl()) {
    error(ERR_UNSUPPORTED_PROTOCOL, toQString(m_protocol));
    return false;
  }

  // Check the reusability of the current connection.
  if (httpShouldCloseConnection()) {
    httpCloseConnection();
  }

  // Create a new connection to the remote machine if we do
  // not already have one...
  // NB: the !m_socketProxyAuth condition is a workaround for a proxied Qt socket sometimes
  // looking disconnected after receiving the initial 407 response.
  // I guess the Qt socket fails to hide the effect of  proxy-connection: close after receiving
  // the 407 header.
  if ((!isConnected() && !m_socketProxyAuth))
  {
    if (!httpOpenConnection())
    {
       // qDebug() << "Couldn't connect, oopsie!";
       return false;
    }
  }

  m_request.cacheTag.ioMode = NoCache;
  QString header;
  bool hasBodyData = false;
  bool hasDavData = false;

  {
    header = toQString(m_request.methodString());
    QString davHeader;

    // Fill in some values depending on the HTTP method to guide further processing
    switch (m_request.method)
    {
    case HTTP_GET: {
        bool cacheHasPage = false;
        if (satisfyRequestFromCache(&cacheHasPage)) {
            // qDebug() << "cacheHasPage =" << cacheHasPage;
            return cacheHasPage;
        }
        if (!cacheHasPage) {
            // start a new cache file later if appropriate
            m_request.cacheTag.ioMode = WriteToCache;
        }
        break;
    }
    case HTTP_HEAD:
        break;
    case HTTP_PUT:
    case HTTP_POST:
        hasBodyData = true;
        break;
    case HTTP_DELETE:
    case HTTP_OPTIONS:
        break;
    case DAV_PROPFIND:
        hasDavData = true;
        davHeader = QLatin1String("Depth: ");
        if ( hasMetaData( QLatin1String("davDepth") ) )
        {
          // qDebug() << "Reading DAV depth from metadata:" << metaData( QLatin1String("davDepth") );
          davHeader += metaData( QLatin1String("davDepth") );
        }
        else
        {
          if ( m_request.davData.depth == 2 )
              davHeader += QLatin1String("infinity");
          else
              davHeader += QString::number( m_request.davData.depth );
        }
        davHeader += QLatin1String("\r\n");
        break;
    case DAV_PROPPATCH:
        hasDavData = true;
        break;
    case DAV_MKCOL:
        break;
    case DAV_COPY:
    case DAV_MOVE:
        davHeader = QLatin1String("Destination: ") + m_request.davData.desturl;
        // infinity depth means copy recursively
        // (optional for copy -> but is the desired action)
        davHeader += QLatin1String("\r\nDepth: infinity\r\nOverwrite: ");
        davHeader += QLatin1Char(m_request.davData.overwrite ? 'T' : 'F');
        davHeader += QLatin1String("\r\n");
        break;
    case DAV_LOCK:
        davHeader = QLatin1String("Timeout: ");
        {
          uint timeout = 0;
          if ( hasMetaData( QLatin1String("davTimeout") ) )
            timeout = metaData( QLatin1String("davTimeout") ).toUInt();
          if ( timeout == 0 )
            davHeader += QLatin1String("Infinite");
          else
            davHeader += QLatin1String("Seconds-") + QString::number(timeout);
        }
        davHeader += QLatin1String("\r\n");
        hasDavData = true;
        break;
    case DAV_UNLOCK:
        davHeader = QLatin1String("Lock-token: ") + metaData(QLatin1String("davLockToken")) + QLatin1String("\r\n");
        break;
    case DAV_SEARCH:
    case DAV_REPORT:
        hasDavData = true;
        /* fall through */
    case DAV_SUBSCRIBE:
    case DAV_UNSUBSCRIBE:
    case DAV_POLL:
        break;
    default:
        error (ERR_UNSUPPORTED_ACTION, QString());
        return false;
    }
    // DAV_POLL; DAV_NOTIFY

    header += formatRequestUri() + QLatin1String(" HTTP/1.1\r\n"); /* start header */

    /* support for virtual hosts and required by HTTP 1.1 */
    header += QLatin1String("Host: ") + m_request.encoded_hostname;
    if (m_request.url.port(defaultPort()) != defaultPort()) {
        header += QLatin1Char(':') + QString::number(m_request.url.port());
    }
    header += QLatin1String("\r\n");

    // Support old HTTP/1.0 style keep-alive header for compatibility
    // purposes as well as performance improvements while giving end
    // users the ability to disable this feature for proxy servers that
    // don't support it, e.g. junkbuster proxy server.
    if (isHttpProxy(m_request.proxyUrl) && !isAutoSsl()) {
        header += QLatin1String("Proxy-Connection: ");
    } else {
        header += QLatin1String("Connection: ");
    }
    if (m_request.isKeepAlive) {
        header += QLatin1String("keep-alive\r\n");
    } else {
        header += QLatin1String("close\r\n");
    }

    if (!m_request.userAgent.isEmpty())
    {
        header += QLatin1String("User-Agent: ");
        header += m_request.userAgent;
        header += QLatin1String("\r\n");
    }

    if (!m_request.referrer.isEmpty())
    {
        header += QLatin1String("Referer: "); //Don't try to correct spelling!
        header += m_request.referrer;
        header += QLatin1String("\r\n");
    }

    if ( m_request.endoffset > m_request.offset )
    {
        header += QLatin1String("Range: bytes=");
        header += KIO::number(m_request.offset);
        header += QLatin1Char('-');
        header += KIO::number(m_request.endoffset);
        header += QLatin1String("\r\n");
        // qDebug() << "kio_http : Range =" << KIO::number(m_request.offset) << "-"  << KIO::number(m_request.endoffset);
    }
    else if ( m_request.offset > 0 && m_request.endoffset == 0 )
    {
        header += QLatin1String("Range: bytes=");
        header += KIO::number(m_request.offset);
        header += QLatin1String("-\r\n");
        // qDebug() << "kio_http: Range =" << KIO::number(m_request.offset);
    }

    if ( !m_request.cacheTag.useCache || m_request.cacheTag.policy==CC_Reload )
    {
      /* No caching for reload */
      header += QLatin1String("Pragma: no-cache\r\n"); /* for HTTP/1.0 caches */
      header += QLatin1String("Cache-control: no-cache\r\n"); /* for HTTP >=1.1 caches */
    }
    else if (m_request.cacheTag.plan(m_maxCacheAge) == CacheTag::ValidateCached)
    {
      // qDebug() << "needs validation, performing conditional get.";
      /* conditional get */
      if (!m_request.cacheTag.etag.isEmpty())
        header += QLatin1String("If-None-Match: ") + m_request.cacheTag.etag + QLatin1String("\r\n");

      if (m_request.cacheTag.lastModifiedDate.isValid()) {
        const QString httpDate = formatHttpDate(m_request.cacheTag.lastModifiedDate);
        header += QStringLiteral("If-Modified-Since: ") + httpDate + QStringLiteral("\r\n");
        setMetaData(QStringLiteral("modified"), httpDate);
      }
    }

    header += QLatin1String("Accept: ");
    const QString acceptHeader = metaData(QLatin1String("accept"));
    if (!acceptHeader.isEmpty())
      header += acceptHeader;
    else
      header += QLatin1String(DEFAULT_ACCEPT_HEADER);
    header += QLatin1String("\r\n");

    if (m_request.allowTransferCompression)
      header += QLatin1String("Accept-Encoding: gzip, deflate, x-gzip, x-deflate\r\n");

    if (!m_request.charsets.isEmpty())
      header += QLatin1String("Accept-Charset: ") + m_request.charsets + QLatin1String("\r\n");

    if (!m_request.languages.isEmpty())
      header += QLatin1String("Accept-Language: ") + m_request.languages + QLatin1String("\r\n");

    QString cookieStr;
    const QString cookieMode = metaData(QLatin1String("cookies")).toLower();

    if (cookieMode == QLatin1String("none"))
    {
      m_request.cookieMode = HTTPRequest::CookiesNone;
    }
    else if (cookieMode == QLatin1String("manual"))
    {
      m_request.cookieMode = HTTPRequest::CookiesManual;
      cookieStr = metaData(QLatin1String("setcookies"));
    }
    else
    {
      m_request.cookieMode = HTTPRequest::CookiesAuto;
      if (m_request.useCookieJar)
        cookieStr = findCookies(m_request.url.toString());
    }

    if (!cookieStr.isEmpty())
      header += cookieStr + QLatin1String("\r\n");

    const QString customHeader = metaData( QLatin1String("customHTTPHeader") );
    if (!customHeader.isEmpty())
    {
      header += sanitizeCustomHTTPHeader(customHeader);
      header += QLatin1String("\r\n");
    }

    const QString contentType = metaData(QLatin1String("content-type"));
    if (!contentType.isEmpty())
    {
      if (!contentType.startsWith(QLatin1String("content-type"), Qt::CaseInsensitive))
        header += QLatin1String("Content-Type: ");
      header += contentType;
      header += QLatin1String("\r\n");
    }

    // DoNotTrack feature...
    if (config()->readEntry("DoNotTrack", false))
      header += QLatin1String("DNT: 1\r\n");

    // Remember that at least one failed (with 401 or 407) request/response
    // roundtrip is necessary for the server to tell us that it requires
    // authentication. However, we proactively add authentication headers if when
    // we have cached credentials to avoid the extra roundtrip where possible.
    header += authenticationHeader();

    if ( m_protocol == "webdav" || m_protocol == "webdavs" )
    {
      header += davProcessLocks();

      // add extra webdav headers, if supplied
      davHeader += metaData(QLatin1String("davHeader"));

      // Set content type of webdav data
      if (hasDavData)
        davHeader += QLatin1String("Content-Type: text/xml; charset=utf-8\r\n");

      // add extra header elements for WebDAV
      header += davHeader;
    }
  }

  // qDebug() << "============ Sending Header:";
  //Q_FOREACH (const QString &s, header.split(QLatin1String("\r\n"), QString::SkipEmptyParts)) {
    // qDebug() << s;
  //}

  // End the header iff there is no payload data. If we do have payload data
  // sendBody() will add another field to the header, Content-Length.
  if (!hasBodyData && !hasDavData)
    header += QLatin1String("\r\n");


  // Now that we have our formatted header, let's send it!

  // Clear out per-connection settings...
  resetConnectionSettings();

  // Send the data to the remote machine...
  const QByteArray headerBytes = header.toLatin1();
  ssize_t written = write(headerBytes.constData(), headerBytes.length());
  bool sendOk = (written == (ssize_t) headerBytes.length());
  if (!sendOk)
  {
    // qDebug() << "Connection broken! (" << m_request.url.host() << ")"
    //             << "  -- intended to write" << headerBytes.length()
    //             << "bytes but wrote" << (int)written << ".";

    // The server might have closed the connection due to a timeout, or maybe
    // some transport problem arose while the connection was idle.
    if (m_request.isKeepAlive)
    {
       httpCloseConnection();
       return true; // Try again
    }

    // qDebug() << "sendOk == false. Connection broken !"
    //             << "  -- intended to write" << headerBytes.length()
    //             << "bytes but wrote" << (int)written << ".";
    error( ERR_CONNECTION_BROKEN, m_request.url.host() );
    return false;
  }
  else {
    // qDebug() << "sent it!";
  }

  bool res = true;
  if (hasBodyData || hasDavData)
    res = sendBody();

  infoMessage(i18n("%1 contacted. Waiting for reply...", m_request.url.host()));

  return res;
}

void HTTPProtocol::forwardHttpResponseHeader(bool forwardImmediately)
{
  // Send the response header if it was requested...
  if (!config()->readEntry("PropagateHttpHeader", false))
      return;

  setMetaData(QLatin1String("HTTP-Headers"), m_responseHeaders.join(QString(QLatin1Char('\n'))));

  if (forwardImmediately)
      sendMetaData();
}

bool HTTPProtocol::parseHeaderFromCache()
{
    // qDebug();
    if (!cacheFileReadTextHeader2()) {
        return false;
    }

    Q_FOREACH (const QString &str, m_responseHeaders) {
        const QString header = str.trimmed();
        if (header.startsWith(QLatin1String("content-type:"), Qt::CaseInsensitive)) {
            int pos = header.indexOf(QLatin1String("charset="), Qt::CaseInsensitive);
            if (pos != -1) {
                const QString charset = header.mid(pos + 8).toLower();
                m_request.cacheTag.charset = charset;
                setMetaData(QLatin1String("charset"), charset);
            }
        } else if (header.startsWith(QLatin1String("content-language:"), Qt::CaseInsensitive)) {
            const QString language = header.mid(17).trimmed().toLower();
            setMetaData(QLatin1String("content-language"), language);
        } else if (header.startsWith(QLatin1String("content-disposition:"), Qt::CaseInsensitive)) {
            parseContentDisposition(header.mid(20).toLower());
        }
    }

    if (m_request.cacheTag.lastModifiedDate.isValid()) {
        setMetaData(QStringLiteral("modified"), formatHttpDate(m_request.cacheTag.lastModifiedDate));
    }

    // this header comes from the cache, so the response must have been cacheable :)
    setCacheabilityMetadata(true);
    // qDebug() << "Emitting mimeType" << m_mimeType;
    forwardHttpResponseHeader(false);
    mimeType(m_mimeType);
    // IMPORTANT: Do not remove the call below or the http response headers will
    // not be available to the application if this slave is put on hold.
    forwardHttpResponseHeader();
    return true;
}

void HTTPProtocol::fixupResponseMimetype()
{
    if (m_mimeType.isEmpty())
        return;

    // qDebug() << "before fixup" << m_mimeType;
    // Convert some common mimetypes to standard mimetypes
    if (m_mimeType == QLatin1String("application/x-targz"))
        m_mimeType = QLatin1String("application/x-compressed-tar");
    else if (m_mimeType == QLatin1String("image/x-png"))
        m_mimeType = QLatin1String("image/png");
    else if (m_mimeType == QLatin1String("audio/x-mp3") || m_mimeType == QLatin1String("audio/x-mpeg") || m_mimeType == QLatin1String("audio/mp3"))
        m_mimeType = QLatin1String("audio/mpeg");
    else if (m_mimeType == QLatin1String("audio/microsoft-wave"))
        m_mimeType = QLatin1String("audio/x-wav");
    else if (m_mimeType == QLatin1String("image/x-ms-bmp"))
        m_mimeType = QLatin1String("image/bmp");

    // Crypto ones....
    else if (m_mimeType == QLatin1String("application/pkix-cert") ||
             m_mimeType == QLatin1String("application/binary-certificate")) {
        m_mimeType = QLatin1String("application/x-x509-ca-cert");
    }

    // Prefer application/x-compressed-tar or x-gzpostscript over application/x-gzip.
    else if (m_mimeType == QLatin1String("application/x-gzip")) {
        if ((m_request.url.path().endsWith(QLatin1String(".tar.gz"))) ||
            (m_request.url.path().endsWith(QLatin1String(".tar"))))
            m_mimeType = QLatin1String("application/x-compressed-tar");
        if ((m_request.url.path().endsWith(QLatin1String(".ps.gz"))))
            m_mimeType = QLatin1String("application/x-gzpostscript");
    }

    // Prefer application/x-xz-compressed-tar over application/x-xz for LMZA compressed
    // tar files. Arch Linux AUR servers notoriously send the wrong mimetype for this.
    else if(m_mimeType == QLatin1String("application/x-xz")) {
        if (m_request.url.path().endsWith(QLatin1String(".tar.xz")) ||
            m_request.url.path().endsWith(QLatin1String(".txz"))) {
            m_mimeType = QLatin1String("application/x-xz-compressed-tar");
        }
    }

    // Some webservers say "text/plain" when they mean "application/x-bzip"
    else if ((m_mimeType == QLatin1String("text/plain")) || (m_mimeType == QLatin1String("application/octet-stream"))) {
        const QString ext = QFileInfo(m_request.url.path()).suffix().toUpper();
        if (ext == QLatin1String("BZ2"))
            m_mimeType = QLatin1String("application/x-bzip");
        else if (ext == QLatin1String("PEM"))
            m_mimeType = QLatin1String("application/x-x509-ca-cert");
        else if (ext == QLatin1String("SWF"))
            m_mimeType = QLatin1String("application/x-shockwave-flash");
        else if (ext == QLatin1String("PLS"))
            m_mimeType = QLatin1String("audio/x-scpls");
        else if (ext == QLatin1String("WMV"))
            m_mimeType = QLatin1String("video/x-ms-wmv");
        else if (ext == QLatin1String("WEBM"))
            m_mimeType = QLatin1String("video/webm");
        else if (ext == QLatin1String("DEB"))
            m_mimeType = QLatin1String("application/x-deb");
    }
    // qDebug() << "after fixup" << m_mimeType;
}


void HTTPProtocol::fixupResponseContentEncoding()
{
    // WABA: Correct for tgz files with a gzip-encoding.
    // They really shouldn't put gzip in the Content-Encoding field!
    // Web-servers really shouldn't do this: They let Content-Size refer
    // to the size of the tgz file, not to the size of the tar file,
    // while the Content-Type refers to "tar" instead of "tgz".
    if (!m_contentEncodings.isEmpty() && m_contentEncodings.last() == QLatin1String("gzip")) {
        if (m_mimeType == QLatin1String("application/x-tar")) {
            m_contentEncodings.removeLast();
            m_mimeType = QLatin1String("application/x-compressed-tar");
        } else if (m_mimeType == QLatin1String("application/postscript")) {
            // LEONB: Adding another exception for psgz files.
            // Could we use the mimelnk files instead of hardcoding all this?
            m_contentEncodings.removeLast();
            m_mimeType = QLatin1String("application/x-gzpostscript");
        } else if ((m_request.allowTransferCompression &&
                   m_mimeType == QLatin1String("text/html"))
                   ||
                   (m_request.allowTransferCompression &&
                   m_mimeType != QLatin1String("application/x-compressed-tar") &&
                   m_mimeType != QLatin1String("application/x-tgz") && // deprecated name
                   m_mimeType != QLatin1String("application/x-targz") && // deprecated name
                   m_mimeType != QLatin1String("application/x-gzip"))) {
            // Unzip!
        } else {
            m_contentEncodings.removeLast();
            m_mimeType = QLatin1String("application/x-gzip");
        }
    }

    // We can't handle "bzip2" encoding (yet). So if we get something with
    // bzip2 encoding, we change the mimetype to "application/x-bzip".
    // Note for future changes: some web-servers send both "bzip2" as
    //   encoding and "application/x-bzip[2]" as mimetype. That is wrong.
    //   currently that doesn't bother us, because we remove the encoding
    //   and set the mimetype to x-bzip anyway.
    if (!m_contentEncodings.isEmpty() && m_contentEncodings.last() == QLatin1String("bzip2")) {
        m_contentEncodings.removeLast();
        m_mimeType = QLatin1String("application/x-bzip");
    }
}

//Return true if the term was found, false otherwise. Advance *pos.
//If (*pos + strlen(term) >= end) just advance *pos to end and return false.
//This means that users should always search for the shortest terms first.
static bool consume(const char input[], int *pos, int end, const char *term)
{
    // note: gcc/g++ is quite good at optimizing away redundant strlen()s
    int idx = *pos;
    if (idx + (int)strlen(term) >= end) {
        *pos = end;
        return false;
    }
    if (strncasecmp(&input[idx], term, strlen(term)) == 0) {
        *pos = idx + strlen(term);
        return true;
    }
    return false;
}

/**
 * This function will read in the return header from the server.  It will
 * not read in the body of the return message.  It will also not transmit
 * the header to our client as the client doesn't need to know the gory
 * details of HTTP headers.
 */
bool HTTPProtocol::readResponseHeader()
{
    resetResponseParsing();
    if (m_request.cacheTag.ioMode == ReadFromCache &&
        m_request.cacheTag.plan(m_maxCacheAge) == CacheTag::UseCached) {
        // parseHeaderFromCache replaces this method in case of cached content
        return parseHeaderFromCache();
    }

try_again:
    // qDebug();

    bool upgradeRequired = false;   // Server demands that we upgrade to something
                                    // This is also true if we ask to upgrade and
                                    // the server accepts, since we are now
                                    // committed to doing so
    bool noHeadersFound = false;

    m_request.cacheTag.charset.clear();
    m_responseHeaders.clear();

    static const int maxHeaderSize = 128 * 1024;

    char buffer[maxHeaderSize];
    bool cont = false;
    bool bCanResume = false;

    if (!isConnected()) {
        // qDebug() << "No connection.";
        return false; // Reestablish connection and try again
    }

#if 0
    // NOTE: This is unnecessary since TCPSlaveBase::read does the same exact
    // thing. Plus, if we are unable to read from the socket we need to resend
    // the request as done below, not error out! Do not assume remote server
    // will honor persistent connections!!
    if (!waitForResponse(m_remoteRespTimeout)) {
        // qDebug() << "Got socket error:" << socket()->errorString();
        // No response error
        error(ERR_SERVER_TIMEOUT , m_request.url.host());
        return false;
    }
#endif

    int bufPos = 0;
    bool foundDelimiter = readDelimitedText(buffer, &bufPos, maxHeaderSize, 1);
    if (!foundDelimiter && bufPos < maxHeaderSize) {
        // qDebug() << "EOF while waiting for header start.";
        if (m_request.isKeepAlive && m_iEOFRetryCount < 2) {
            m_iEOFRetryCount++;
            httpCloseConnection(); // Try to reestablish connection.
            return false; // Reestablish connection and try again.
        }

        if (m_request.method == HTTP_HEAD) {
            // HACK
            // Some web-servers fail to respond properly to a HEAD request.
            // We compensate for their failure to properly implement the HTTP standard
            // by assuming that they will be sending html.
            // qDebug() << "HEAD -> returned mimetype:" << DEFAULT_MIME_TYPE;
            mimeType(QLatin1String(DEFAULT_MIME_TYPE));
            return true;
        }

        // qDebug() << "Connection broken !";
        error( ERR_CONNECTION_BROKEN, m_request.url.host() );
        return false;
    }
    if (!foundDelimiter) {
        //### buffer too small for first line of header(!)
        Q_ASSERT(0);
    }

    // qDebug() << "============ Received Status Response:";
    // qDebug() << QByteArray(buffer, bufPos).trimmed();

    HTTP_REV httpRev = HTTP_None;
    int idx = 0;

    if (idx != bufPos && buffer[idx] == '<') {
        // qDebug() << "No valid HTTP header found! Document starts with XML/HTML tag";
        // document starts with a tag, assume HTML instead of text/plain
        m_mimeType = QLatin1String("text/html");
        m_request.responseCode = 200; // Fake it
        httpRev = HTTP_Unknown;
        m_request.isKeepAlive = false;
        noHeadersFound = true;
        // put string back
        unread(buffer, bufPos);
        goto endParsing;
    }

    // "HTTP/1.1" or similar
    if (consume(buffer, &idx, bufPos, "ICY ")) {
        httpRev = SHOUTCAST;
        m_request.isKeepAlive = false;
    } else if (consume(buffer, &idx, bufPos, "HTTP/")) {
        if (consume(buffer, &idx, bufPos, "1.0")) {
            httpRev = HTTP_10;
            m_request.isKeepAlive = false;
        } else if (consume(buffer, &idx, bufPos, "1.1")) {
            httpRev = HTTP_11;
        }
    }

    if (httpRev == HTTP_None && bufPos != 0) {
        // Remote server does not seem to speak HTTP at all
        // Put the crap back into the buffer and hope for the best
        // qDebug() << "DO NOT WANT." << bufPos;
        unread(buffer, bufPos);
        if (m_request.responseCode) {
            m_request.prevResponseCode = m_request.responseCode;
        }
        m_request.responseCode = 200; // Fake it
        httpRev = HTTP_Unknown;
        m_request.isKeepAlive = false;
        noHeadersFound = true;
        goto endParsing;
    }

    // response code //### maybe wrong if we need several iterations for this response...
    //### also, do multiple iterations (cf. try_again) to parse one header work w/ pipelining?
    if (m_request.responseCode) {
        m_request.prevResponseCode = m_request.responseCode;
    }
    skipSpace(buffer, &idx, bufPos);
    //TODO saner handling of invalid response code strings
    if (idx != bufPos) {
        m_request.responseCode = atoi(&buffer[idx]);
    } else {
        m_request.responseCode = 200;
    }
    // move idx to start of (yet to be fetched) next line, skipping the "OK"
    idx = bufPos;
    // (don't bother parsing the "OK", what do we do if it isn't there anyway?)

    // immediately act on most response codes...

    // Protect users against bogus username intended to fool them into visiting
    // sites they had no intention of visiting.
    if (isPotentialSpoofingAttack(m_request, config())) {
        // qDebug() << "**** POTENTIAL ADDRESS SPOOFING:" << m_request.url;
        const int result =
            messageBox(WarningYesNo,
                       i18nc("@info Security check on url being accessed",
                             "<p>You are about to log in to the site \"%1\" "
                             "with the username \"%2\", but the website "
                             "does not require authentication. "
                             "This may be an attempt to trick you.</p>"
                             "<p>Is \"%1\" the site you want to visit?</p>",
                             m_request.url.host(), m_request.url.userName()),
                       i18nc("@title:window", "Confirm Website Access"));
        if (result == SlaveBase::No) {
            error(ERR_USER_CANCELED, m_request.url.toDisplayString());
            return false;
        }
        setMetaData(QLatin1String("{internal~currenthost}LastSpoofedUserName"), m_request.url.userName());
    }

    if (m_request.responseCode != 200 && m_request.responseCode != 304) {
        m_request.cacheTag.ioMode = NoCache;

        if (m_request.responseCode >= 500 && m_request.responseCode <= 599) {
            // Server side errors
            if (m_request.method == HTTP_HEAD) {
                ; // Ignore error
            } else {
                if (!sendErrorPageNotification()) {
                    error(ERR_INTERNAL_SERVER, m_request.url.toDisplayString());
                    return false;
                }
            }
        } else if (m_request.responseCode == 416) {
            // Range not supported
            m_request.offset = 0;
            return false; // Try again.
        } else if (m_request.responseCode == 426) {
            // Upgrade Required
            upgradeRequired = true;
        } else if (m_request.responseCode >= 400 && m_request.responseCode <= 499 && !isAuthenticationRequired(m_request.responseCode)) {
            // Any other client errors
            // Tell that we will only get an error page here.
            if (!sendErrorPageNotification()) {
                if (m_request.responseCode == 403)
                    error(ERR_ACCESS_DENIED, m_request.url.toDisplayString());
                else
                    error(ERR_DOES_NOT_EXIST, m_request.url.toDisplayString());
                return false;
            }
        } else if (m_request.responseCode >= 301 && m_request.responseCode<= 303) {
            // 301 Moved permanently
            if (m_request.responseCode == 301) {
                setMetaData(QLatin1String("permanent-redirect"), QLatin1String("true"));
            }
            // 302 Found (temporary location)
            // 303 See Other
            // NOTE: This is wrong according to RFC 2616 (section 10.3.[2-4,8]).
            // However, because almost all client implementations treat a 301/302
            // response as a 303 response in violation of the spec, many servers
            // have simply adapted to this way of doing things! Thus, we are
            // forced to do the same thing. Otherwise, we loose compatibility and
            // might not be able to correctly retrieve sites that redirect.
            if (m_request.method != HTTP_HEAD) {
                m_request.method = HTTP_GET; // Force a GET
            }
        } else if (m_request.responseCode == 204) {
            // No content

            // error(ERR_NO_CONTENT, i18n("Data have been successfully sent."));
            // Short circuit and do nothing!

            // The original handling here was wrong, this is not an error: eg. in the
            // example of a 204 No Content response to a PUT completing.
            // m_iError = true;
            // return false;
        } else if (m_request.responseCode == 206) {
            if (m_request.offset) {
                bCanResume = true;
            }
        } else if (m_request.responseCode == 102) {
            // Processing (for WebDAV)
            /***
             * This status code is given when the server expects the
             * command to take significant time to complete. So, inform
             * the user.
             */
            infoMessage( i18n( "Server processing request, please wait..." ) );
            cont = true;
        } else if (m_request.responseCode == 100) {
            // We got 'Continue' - ignore it
            cont = true;
        }
    } // (m_request.responseCode != 200 && m_request.responseCode != 304)

endParsing:
    bool authRequiresAnotherRoundtrip = false;

    // Skip the whole header parsing if we got no HTTP headers at all
    if (!noHeadersFound) {
        // Auth handling
        const bool wasAuthError = isAuthenticationRequired(m_request.prevResponseCode);
        const bool isAuthError = isAuthenticationRequired(m_request.responseCode);
        const bool sameAuthError = (m_request.responseCode == m_request.prevResponseCode);
        // qDebug() << "wasAuthError=" << wasAuthError << "isAuthError=" << isAuthError
        //            << "sameAuthError=" << sameAuthError;
        // Not the same authorization error as before and no generic error?
        // -> save the successful credentials.
        if (wasAuthError && (m_request.responseCode < 400 || (isAuthError && !sameAuthError))) {
            saveAuthenticationData();
        }

        // done with the first line; now tokenize the other lines

        // TODO review use of STRTOLL vs. QByteArray::toInt()

        foundDelimiter = readDelimitedText(buffer, &bufPos, maxHeaderSize, 2);
        // qDebug() << " -- full response:" << endl << QByteArray(buffer, bufPos).trimmed();
        // Use this to see newlines:
        //qDebug() << " -- full response:" << endl << QByteArray(buffer, bufPos).replace("\r", "\\r").replace("\n", "\\n\n");
        Q_ASSERT(foundDelimiter);

        //NOTE because tokenizer will overwrite newlines in case of line continuations in the header
        //     unread(buffer, bufSize) will not generally work anymore. we don't need it either.
        //     either we have a http response line -> try to parse the header, fail if it doesn't work
        //     or we have garbage -> fail.
        HeaderTokenizer tokenizer(buffer);
        tokenizer.tokenize(idx, sizeof(buffer));

        // Note that not receiving "accept-ranges" means that all bets are off
        // wrt the server supporting ranges.
        TokenIterator tIt = tokenizer.iterator("accept-ranges");
        if (tIt.hasNext() && tIt.next().toLower().startsWith("none")) { // krazy:exclude=strings
            bCanResume = false;
        }

        tIt = tokenizer.iterator("keep-alive");
        while (tIt.hasNext()) {
            QByteArray ka = tIt.next().trimmed().toLower();
            if (ka.startsWith("timeout=")) { // krazy:exclude=strings
                int ka_timeout = ka.mid(qstrlen("timeout=")).trimmed().toInt();
                if (ka_timeout > 0)
                    m_request.keepAliveTimeout = ka_timeout;
                if (httpRev == HTTP_10) {
                    m_request.isKeepAlive = true;
                }

                break; // we want to fetch ka timeout only
            }
        }

        // get the size of our data
        tIt = tokenizer.iterator("content-length");
        if (tIt.hasNext()) {
            m_iSize = STRTOLL(tIt.next().constData(), 0, 10);
        }

        tIt = tokenizer.iterator("content-location");
        if (tIt.hasNext()) {
            setMetaData(QLatin1String("content-location"), toQString(tIt.next().trimmed()));
        }

        // which type of data do we have?
        QString mediaValue;
        QString mediaAttribute;
        tIt = tokenizer.iterator("content-type");
        if (tIt.hasNext()) {
            QList<QByteArray> l = tIt.next().split(';');
            if (!l.isEmpty()) {
                // Assign the mime-type.
                m_mimeType = toQString(l.first().trimmed().toLower());
                if (m_mimeType.startsWith(QLatin1Char('"'))) {
                    m_mimeType.remove(0, 1);
                }
                if (m_mimeType.endsWith(QLatin1Char('"'))) {
                    m_mimeType.chop(1);
                }
                // qDebug() << "Content-type:" << m_mimeType;
                l.removeFirst();
            }

            // If we still have text, then it means we have a mime-type with a
            // parameter (eg: charset=iso-8851) ; so let's get that...
            Q_FOREACH (const QByteArray &statement, l) {
                const int index = statement.indexOf('=');
                if (index <= 0) {
                    mediaAttribute = toQString(statement.mid(0, index));
                } else {
                    mediaAttribute = toQString(statement.mid(0, index));
                    mediaValue = toQString(statement.mid(index+1));
                }
                mediaAttribute = mediaAttribute.trimmed();
                mediaValue     = mediaValue.trimmed();

                bool quoted = false;
                if (mediaValue.startsWith(QLatin1Char('"'))) {
                    quoted = true;
                    mediaValue.remove(0, 1);
                }

                if (mediaValue.endsWith(QLatin1Char('"'))) {
                    mediaValue.chop(1);
                }

                // qDebug() << "Encoding-type:" << mediaAttribute << "=" << mediaValue;

                if (mediaAttribute == QLatin1String("charset")) {
                    mediaValue = mediaValue.toLower();
                    m_request.cacheTag.charset = mediaValue;
                    setMetaData(QLatin1String("charset"), mediaValue);
                } else {
                    setMetaData(QLatin1String("media-") + mediaAttribute, mediaValue);
                    if (quoted) {
                        setMetaData(QLatin1String("media-") + mediaAttribute + QLatin1String("-kio-quoted"),
                                    QLatin1String("true"));
                    }
                }
            }
        }

        // content?
        tIt = tokenizer.iterator("content-encoding");
        while (tIt.hasNext()) {
            // This is so wrong !!  No wonder kio_http is stripping the
            // gzip encoding from downloaded files.  This solves multiple
            // bug reports and caitoo's problem with downloads when such a
            // header is encountered...

            // A quote from RFC 2616:
            // " When present, its (Content-Encoding) value indicates what additional
            // content have been applied to the entity body, and thus what decoding
            // mechanism must be applied to obtain the media-type referenced by the
            // Content-Type header field.  Content-Encoding is primarily used to allow
            // a document to be compressed without loosing the identity of its underlying
            // media type.  Simply put if it is specified, this is the actual mime-type
            // we should use when we pull the resource !!!
            addEncoding(toQString(tIt.next()), m_contentEncodings);
        }
        // Refer to RFC 2616 sec 15.5/19.5.1 and RFC 2183
        tIt = tokenizer.iterator("content-disposition");
        if (tIt.hasNext()) {
            parseContentDisposition(toQString(tIt.next()));
        }
        tIt = tokenizer.iterator("content-language");
        if (tIt.hasNext()) {
            QString language = toQString(tIt.next().trimmed());
            if (!language.isEmpty()) {
                setMetaData(QLatin1String("content-language"), language);
            }
        }

        tIt = tokenizer.iterator("proxy-connection");
        if (tIt.hasNext() && isHttpProxy(m_request.proxyUrl) && !isAutoSsl()) {
            QByteArray pc = tIt.next().toLower();
            if (pc.startsWith("close")) { // krazy:exclude=strings
                m_request.isKeepAlive = false;
            } else if (pc.startsWith("keep-alive")) { // krazy:exclude=strings
                m_request.isKeepAlive = true;
            }
        }

        tIt = tokenizer.iterator("link");
        if (tIt.hasNext()) {
            // We only support Link: <url>; rel="type"   so far
            QStringList link = toQString(tIt.next()).split(QLatin1Char(';'), QString::SkipEmptyParts);
            if (link.count() == 2) {
                QString rel = link[1].trimmed();
                if (rel.startsWith(QLatin1String("rel=\""))) {
                    rel = rel.mid(5, rel.length() - 6);
                    if (rel.toLower() == QLatin1String("pageservices")) {
                        //### the remove() part looks fishy!
                        QString url = link[0].remove(QRegExp(QLatin1String("[<>]"))).trimmed();
                        setMetaData(QLatin1String("PageServices"), url);
                    }
                }
            }
        }

        tIt = tokenizer.iterator("p3p");
        if (tIt.hasNext()) {
            // P3P privacy policy information
            QStringList policyrefs, compact;
            while (tIt.hasNext()) {
                QStringList policy = toQString(tIt.next().simplified())
                                     .split(QLatin1Char('='), QString::SkipEmptyParts);
                if (policy.count() == 2) {
                    if (policy[0].toLower() == QLatin1String("policyref")) {
                        policyrefs << policy[1].remove(QRegExp(QLatin1String("[\")\']"))).trimmed();
                    } else if (policy[0].toLower() == QLatin1String("cp")) {
                        // We convert to cp\ncp\ncp\n[...]\ncp to be consistent with
                        // other metadata sent in strings.  This could be a bit more
                        // efficient but I'm going for correctness right now.
                        const QString s = policy[1].remove(QRegExp(QLatin1String("[\")\']")));
                        const QStringList cps = s.split(QLatin1Char(' '), QString::SkipEmptyParts);
                        compact << cps;
                    }
                }
            }
            if (!policyrefs.isEmpty()) {
                setMetaData(QLatin1String("PrivacyPolicy"), policyrefs.join(QLatin1String("\n")));
            }
            if (!compact.isEmpty()) {
                setMetaData(QLatin1String("PrivacyCompactPolicy"), compact.join(QLatin1String("\n")));
            }
        }

        // continue only if we know that we're at least HTTP/1.0
        if (httpRev == HTTP_11 || httpRev == HTTP_10) {
            // let them tell us if we should stay alive or not
            tIt = tokenizer.iterator("connection");
            while (tIt.hasNext()) {
                QByteArray connection = tIt.next().toLower();
                if (!(isHttpProxy(m_request.proxyUrl) && !isAutoSsl())) {
                    if (connection.startsWith("close")) { // krazy:exclude=strings
                        m_request.isKeepAlive = false;
                    } else if (connection.startsWith("keep-alive")) { // krazy:exclude=strings
                        m_request.isKeepAlive = true;
                    }
                }
                if (connection.startsWith("upgrade")) { // krazy:exclude=strings
                    if (m_request.responseCode == 101) {
                        // Ok, an upgrade was accepted, now we must do it
                        upgradeRequired = true;
                    } else if (upgradeRequired) {  // 426
                        // Nothing to do since we did it above already
                    }
                }
            }
            // what kind of encoding do we have?  transfer?
            tIt = tokenizer.iterator("transfer-encoding");
            while (tIt.hasNext()) {
                // If multiple encodings have been applied to an entity, the
                // transfer-codings MUST be listed in the order in which they
                // were applied.
                addEncoding(toQString(tIt.next().trimmed()), m_transferEncodings);
            }

            // md5 signature
            tIt = tokenizer.iterator("content-md5");
            if (tIt.hasNext()) {
                m_contentMD5 = toQString(tIt.next().trimmed());
            }

            // *** Responses to the HTTP OPTIONS method follow
            // WebDAV capabilities
            tIt = tokenizer.iterator("dav");
            while (tIt.hasNext()) {
                m_davCapabilities << toQString(tIt.next());
            }
            // *** Responses to the HTTP OPTIONS method finished
        }


        // Now process the HTTP/1.1 upgrade
        QStringList upgradeOffers;
        tIt = tokenizer.iterator("upgrade");
        if (tIt.hasNext()) {
            // Now we have to check to see what is offered for the upgrade
            QString offered = toQString(tIt.next());
            upgradeOffers = offered.split(QRegExp(QLatin1String("[ \n,\r\t]")), QString::SkipEmptyParts);
        }
        Q_FOREACH (const QString &opt, upgradeOffers) {
            if (opt == QLatin1String("TLS/1.0")) {
                if (!startSsl() && upgradeRequired) {
                    error(ERR_UPGRADE_REQUIRED, opt);
                    return false;
                }
            } else if (opt == QLatin1String("HTTP/1.1")) {
                httpRev = HTTP_11;
            } else if (upgradeRequired) {
                // we are told to do an upgrade we don't understand
                error(ERR_UPGRADE_REQUIRED, opt);
                return false;
            }
        }

        // Harvest cookies (mmm, cookie fields!)
        QByteArray cookieStr; // In case we get a cookie.
        tIt = tokenizer.iterator("set-cookie");
        while (tIt.hasNext()) {
            cookieStr += "Set-Cookie: ";
            cookieStr += tIt.next();
            cookieStr += '\n';
        }
        if (!cookieStr.isEmpty()) {
            if ((m_request.cookieMode == HTTPRequest::CookiesAuto) && m_request.useCookieJar) {
                // Give cookies to the cookiejar.
                const QString domain = config()->readEntry("cross-domain");
                if (!domain.isEmpty() && isCrossDomainRequest(m_request.url.host(), domain)) {
                    cookieStr = "Cross-Domain\n" + cookieStr;
                }
                addCookies(m_request.url.toString(), cookieStr);
            } else if (m_request.cookieMode == HTTPRequest::CookiesManual) {
                // Pass cookie to application
                setMetaData(QLatin1String("setcookies"), QString::fromUtf8(cookieStr)); // ## is encoding ok?
            }
        }

        // We need to reread the header if we got a '100 Continue' or '102 Processing'
        // This may be a non keepalive connection so we handle this kind of loop internally
        if ( cont )
        {
            // qDebug() << "cont; returning to mark try_again";
            goto try_again;
        }

        if (!m_isChunked && (m_iSize == NO_SIZE) && m_request.isKeepAlive &&
            canHaveResponseBody(m_request.responseCode, m_request.method)) {
            // qDebug() << "Ignoring keep-alive: otherwise unable to determine response body length.";
            m_request.isKeepAlive = false;
        }

        // TODO cache the proxy auth data (not doing this means a small performance regression for now)

        // we may need to send (Proxy or WWW) authorization data
        if ((!m_request.doNotWWWAuthenticate && m_request.responseCode == 401) ||
            (!m_request.doNotProxyAuthenticate && m_request.responseCode == 407)) {
            authRequiresAnotherRoundtrip = handleAuthenticationHeader(&tokenizer);
            if (m_iError) {
                // If error is set, then handleAuthenticationHeader failed.
                return false;
            }
        } else {
            authRequiresAnotherRoundtrip = false;
        }

        QString locationStr;
        // In fact we should do redirection only if we have a redirection response code (300 range)
        tIt = tokenizer.iterator("location");
        if (tIt.hasNext() && m_request.responseCode > 299 && m_request.responseCode < 400) {
            locationStr = QString::fromUtf8(tIt.next().trimmed());
        }
        // We need to do a redirect
        if (!locationStr.isEmpty())
        {
            QUrl u = m_request.url.resolved(QUrl(locationStr));
            if(!u.isValid())
            {
                error(ERR_MALFORMED_URL, u.toDisplayString());
                return false;
            }

            // preserve #ref: (bug 124654)
            // if we were at http://host/resource1#ref, we sent a GET for "/resource1"
            // if we got redirected to http://host/resource2, then we have to re-add
            // the fragment:
            if (m_request.url.hasFragment() && !u.hasFragment() &&
                (m_request.url.host() == u.host()) &&
                (m_request.url.scheme() == u.scheme()))
                u.setFragment(m_request.url.fragment());

            m_isRedirection = true;

            if (!m_request.id.isEmpty())
            {
                sendMetaData();
            }

            // If we're redirected to a http:// url, remember that we're doing webdav...
            if (m_protocol == "webdav" || m_protocol == "webdavs"){
                if(u.scheme() == QLatin1String("http")){
                    u.setScheme(QLatin1String("webdav"));
                }else if(u.scheme() == QLatin1String("https")){
                    u.setScheme(QLatin1String("webdavs"));
                }

                m_request.redirectUrl = u;
            }

            // qDebug() << "Re-directing from" << m_request.url << "to" << u;

            redirection(u);

            // It would be hard to cache the redirection response correctly. The possible benefit
            // is small (if at all, assuming fast disk and slow network), so don't do it.
            cacheFileClose();
            setCacheabilityMetadata(false);
        }

        // Inform the job that we can indeed resume...
        if (bCanResume && m_request.offset) {
            //TODO turn off caching???
            canResume();
        } else {
            m_request.offset = 0;
        }

        // Correct a few common wrong content encodings
        fixupResponseContentEncoding();

        // Correct some common incorrect pseudo-mimetypes
        fixupResponseMimetype();

        // parse everything related to expire and other dates, and cache directives; also switch
        // between cache reading and writing depending on cache validation result.
        cacheParseResponseHeader(tokenizer);
    }

    if (m_request.cacheTag.ioMode == ReadFromCache) {
        if (m_request.cacheTag.policy == CC_Verify &&
            m_request.cacheTag.plan(m_maxCacheAge) != CacheTag::UseCached) {
            // qDebug() << "Reading resource from cache even though the cache plan is not "
            //                "UseCached; the server is probably sending wrong expiry information.";
        }
        // parseHeaderFromCache replaces this method in case of cached content
        return parseHeaderFromCache();
    }

    if (config()->readEntry("PropagateHttpHeader", false) ||
        m_request.cacheTag.ioMode == WriteToCache) {
        // store header lines if they will be used; note that the tokenizer removing
        // line continuation special cases is probably more good than bad.
        int nextLinePos = 0;
        int prevLinePos = 0;
        bool haveMore = true;
        while (haveMore) {
            haveMore = nextLine(buffer, &nextLinePos, bufPos);
            int prevLineEnd = nextLinePos;
            while (buffer[prevLineEnd - 1] == '\r' || buffer[prevLineEnd - 1] == '\n') {
                prevLineEnd--;
            }

            m_responseHeaders.append(QString::fromLatin1(&buffer[prevLinePos],
                                                         prevLineEnd - prevLinePos));
            prevLinePos = nextLinePos;
        }

        // IMPORTANT: Do not remove this line because forwardHttpResponseHeader
        // is called below. This line is here to ensure the response headers are
        // available to the client before it receives mimetype information.
        // The support for putting ioslaves on hold in the KIO-QNAM integration
        // will break if this line is removed.
        setMetaData(QLatin1String("HTTP-Headers"), m_responseHeaders.join(QString(QLatin1Char('\n'))));
    }

    // Let the app know about the mime-type iff this is not a redirection and
    // the mime-type string is not empty.
    if (!m_isRedirection && m_request.responseCode != 204 &&
        (!m_mimeType.isEmpty() || m_request.method == HTTP_HEAD) &&
        (m_isLoadingErrorPage || !authRequiresAnotherRoundtrip)) {
        // qDebug() << "Emitting mimetype " << m_mimeType;
        mimeType( m_mimeType );
    }

    // IMPORTANT: Do not move the function call below before doing any
    // redirection. Otherwise it might mess up some sites, see BR# 150904.
    forwardHttpResponseHeader();

    if (m_request.method == HTTP_HEAD)
        return true;

    return !authRequiresAnotherRoundtrip; // return true if no more credentials need to be sent
}

void HTTPProtocol::parseContentDisposition(const QString &disposition)
{
    const QMap<QString, QString> parameters = contentDispositionParser(disposition);

    QMap<QString, QString>::const_iterator i = parameters.constBegin();
    while (i != parameters.constEnd()) {
        setMetaData(QLatin1String("content-disposition-") + i.key(), i.value());
        // qDebug() << "Content-Disposition:" << i.key() << "=" << i.value();
        ++i;
    }
}

void HTTPProtocol::addEncoding(const QString &_encoding, QStringList &encs)
{
  QString encoding = _encoding.trimmed().toLower();
  // Identity is the same as no encoding
  if (encoding == QLatin1String("identity")) {
    return;
  } else if (encoding == QLatin1String("8bit")) {
    // Strange encoding returned by http://linac.ikp.physik.tu-darmstadt.de
    return;
  } else if (encoding == QLatin1String("chunked")) {
    m_isChunked = true;
    // Anyone know of a better way to handle unknown sizes possibly/ideally with unsigned ints?
    //if ( m_cmd != CMD_COPY )
      m_iSize = NO_SIZE;
  } else if ((encoding == QLatin1String("x-gzip")) || (encoding == QLatin1String("gzip"))) {
    encs.append(QLatin1String("gzip"));
  } else if ((encoding == QLatin1String("x-bzip2")) || (encoding == QLatin1String("bzip2"))) {
    encs.append(QLatin1String("bzip2")); // Not yet supported!
  } else if ((encoding == QLatin1String("x-deflate")) || (encoding == QLatin1String("deflate"))) {
    encs.append(QLatin1String("deflate"));
  } else {
    // qDebug() << "Unknown encoding encountered.  " << "Please write code. Encoding =" << encoding;
  }
}

void HTTPProtocol::cacheParseResponseHeader(const HeaderTokenizer &tokenizer)
{
    if (!m_request.cacheTag.useCache)
        return;

    // might have to add more response codes
    if (m_request.responseCode != 200 && m_request.responseCode != 304) {
        return;
    }

    const QDateTime currentDate = QDateTime::currentDateTime();
    bool mayCache = m_request.cacheTag.ioMode != NoCache;

    TokenIterator tIt = tokenizer.iterator("last-modified");
    if (tIt.hasNext()) {
        m_request.cacheTag.lastModifiedDate =
              QDateTime::fromString(toQString(tIt.next()), Qt::RFC2822Date);

        //### might be good to canonicalize the date by using QDateTime::toString()
        if (m_request.cacheTag.lastModifiedDate.isValid()) {
            setMetaData(QLatin1String("modified"), toQString(tIt.current()));
        }
    }

    // determine from available information when the response was served by the origin server
    {
        QDateTime dateHeader;
        tIt = tokenizer.iterator("date");
        if (tIt.hasNext()) {
            dateHeader = QDateTime::fromString(toQString(tIt.next()), Qt::RFC2822Date);
            // -1 on error
        }

        qint64 ageHeader = 0;
        tIt = tokenizer.iterator("age");
        if (tIt.hasNext()) {
            ageHeader = tIt.next().toLongLong();
            // 0 on error
        }

        if (dateHeader.isValid()) {
            m_request.cacheTag.servedDate = dateHeader;
        } else if (ageHeader) {
            m_request.cacheTag.servedDate = currentDate.addSecs(-ageHeader);
        } else {
            m_request.cacheTag.servedDate = currentDate;
        }
    }

    bool hasCacheDirective = false;
    // determine when the response "expires", i.e. becomes stale and needs revalidation
    {
        // (we also parse other cache directives here)
        qint64 maxAgeHeader = 0;
        tIt = tokenizer.iterator("cache-control");
        while (tIt.hasNext()) {
            QByteArray cacheStr = tIt.next().toLower();
            if (cacheStr.startsWith("no-cache") || cacheStr.startsWith("no-store")) { // krazy:exclude=strings
                // Don't put in cache
                mayCache = false;
                hasCacheDirective = true;
            } else if (cacheStr.startsWith("max-age=")) { // krazy:exclude=strings
                QByteArray ba = cacheStr.mid(qstrlen("max-age=")).trimmed();
                bool ok = false;
                maxAgeHeader = ba.toLongLong(&ok);
                if (ok) {
                    hasCacheDirective = true;
                }
            }
        }

        QDateTime expiresHeader;
        tIt = tokenizer.iterator("expires");
        if (tIt.hasNext()) {
            expiresHeader = QDateTime::fromString(toQString(tIt.next()), Qt::RFC2822Date);
            // qDebug() << "parsed expire date from 'expires' header:" << tIt.current();
        }

        if (maxAgeHeader) {
            m_request.cacheTag.expireDate = m_request.cacheTag.servedDate.addSecs(maxAgeHeader);
        } else if (expiresHeader.isValid()) {
            m_request.cacheTag.expireDate = expiresHeader;
        } else {
            // heuristic expiration date
            if (m_request.cacheTag.lastModifiedDate.isValid()) {
                // expAge is following the RFC 2616 suggestion for heuristic expiration
                qint64 expAge =
                        (m_request.cacheTag.lastModifiedDate.secsTo(m_request.cacheTag.servedDate)) / 10;
                // not in the RFC: make sure not to have a huge heuristic cache lifetime
                expAge = qMin(expAge, qint64(3600 * 24));
                m_request.cacheTag.expireDate = m_request.cacheTag.servedDate.addSecs(expAge);
            } else {
                m_request.cacheTag.expireDate = m_request.cacheTag.servedDate.addSecs(DEFAULT_CACHE_EXPIRE);
            }
        }
        // make sure that no future clock monkey business causes the cache entry to un-expire
        if (m_request.cacheTag.expireDate < currentDate) {
            m_request.cacheTag.expireDate.setMSecsSinceEpoch(0);  // January 1, 1970 :)
        }
    }

    tIt = tokenizer.iterator("etag");
    if (tIt.hasNext()) {
        QString prevEtag = m_request.cacheTag.etag;
        m_request.cacheTag.etag = toQString(tIt.next());
        if (m_request.cacheTag.etag != prevEtag && m_request.responseCode == 304) {
            // qDebug() << "304 Not Modified but new entity tag - I don't think this is legal HTTP.";
        }
    }

    // whoops.. we received a warning
    tIt = tokenizer.iterator("warning");
    if (tIt.hasNext()) {
        //Don't use warning() here, no need to bother the user.
        //Those warnings are mostly about caches.
        infoMessage(toQString(tIt.next()));
    }

    // Cache management (HTTP 1.0)
    tIt = tokenizer.iterator("pragma");
    while (tIt.hasNext()) {
        if (tIt.next().toLower().startsWith("no-cache")) { // krazy:exclude=strings
            mayCache = false;
            hasCacheDirective = true;
        }
    }

    // The deprecated Refresh Response
    tIt = tokenizer.iterator("refresh");
    if (tIt.hasNext()) {
        mayCache = false;
        setMetaData(QLatin1String("http-refresh"), toQString(tIt.next().trimmed()));
    }

    // We don't cache certain text objects
    if (m_mimeType.startsWith(QLatin1String("text/")) && (m_mimeType != QLatin1String("text/css")) &&
        (m_mimeType != QLatin1String("text/x-javascript")) && !hasCacheDirective) {
        // Do not cache secure pages or pages
        // originating from password protected sites
        // unless the webserver explicitly allows it.
        if (isUsingSsl() || m_wwwAuth) {
            mayCache = false;
        }
    }

    // note that we've updated cacheTag, so the plan() is with current data
    if (m_request.cacheTag.plan(m_maxCacheAge) == CacheTag::ValidateCached) {
        // qDebug() << "Cache needs validation";
        if (m_request.responseCode == 304) {
            // qDebug() << "...was revalidated by response code but not by updated expire times. "
            //                "We're going to set the expire date to 60 seconds in the future...";
            m_request.cacheTag.expireDate = currentDate.addSecs(60);
            if (m_request.cacheTag.policy == CC_Verify &&
                m_request.cacheTag.plan(m_maxCacheAge) != CacheTag::UseCached) {
                // "apparently" because we /could/ have made an error ourselves, but the errors I
                // witnessed were all the server's fault.
                // qDebug() << "this proxy or server apparently sends bogus expiry information.";
            }
        }
    }

    // validation handling
    if (mayCache && m_request.responseCode == 200 && !m_mimeType.isEmpty()) {
        // qDebug() << "Cache, adding" << m_request.url;
        // ioMode can still be ReadFromCache here if we're performing a conditional get
        // aka validation
        m_request.cacheTag.ioMode = WriteToCache;
        if (!cacheFileOpenWrite()) {
            // qDebug() << "Error creating cache entry for" << m_request.url << "!\n";
        }
        m_maxCacheSize = config()->readEntry("MaxCacheSize", DEFAULT_MAX_CACHE_SIZE);
    } else if (m_request.responseCode == 304 && m_request.cacheTag.file) {
        if (!mayCache) {
            // qDebug() << "This webserver is confused about the cacheability of the data it sends.";
        }
        // the cache file should still be open for reading, see satisfyRequestFromCache().
        Q_ASSERT(m_request.cacheTag.file->openMode() == QIODevice::ReadOnly);
        Q_ASSERT(m_request.cacheTag.ioMode == ReadFromCache);
    } else {
        cacheFileClose();
    }

    setCacheabilityMetadata(mayCache);
}

void HTTPProtocol::setCacheabilityMetadata(bool cachingAllowed)
{
    if (!cachingAllowed) {
        setMetaData(QLatin1String("no-cache"), QLatin1String("true"));
        setMetaData(QLatin1String("expire-date"), QLatin1String("1")); // Expired
    } else {
        QString tmp;
        tmp.setNum(m_request.cacheTag.expireDate.toTime_t());
        setMetaData(QLatin1String("expire-date"), tmp);
        // slightly changed semantics from old creationDate, probably more correct now
        tmp.setNum(m_request.cacheTag.servedDate.toTime_t());
        setMetaData(QLatin1String("cache-creation-date"), tmp);
    }
}

bool HTTPProtocol::sendCachedBody()
{
  infoMessage(i18n("Sending data to %1" ,  m_request.url.host()));

  QByteArray cLength ("Content-Length: ");
  cLength += QByteArray::number(m_POSTbuf->size());
  cLength += "\r\n\r\n";

  // qDebug() << "sending cached data (size=" << m_POSTbuf->size() << ")";

  // Send the content length...
  bool sendOk = (write(cLength.data(), cLength.size()) == (ssize_t) cLength.size());
  if (!sendOk) {
    // qDebug() << "Connection broken when sending " << "content length: (" << m_request.url.host() << ")";
    error( ERR_CONNECTION_BROKEN, m_request.url.host() );
    return false;
  }

  // Make sure the read head is at the beginning...
  m_POSTbuf->reset();

  // Send the data...
  while (!m_POSTbuf->atEnd()) {
      const QByteArray buffer = m_POSTbuf->read(s_MaxInMemPostBufSize);
      sendOk = (write(buffer.data(), buffer.size()) == (ssize_t) buffer.size());
      if (!sendOk)  {
        // qDebug() << "Connection broken when sending message body: (" << m_request.url.host() << ")";
        error( ERR_CONNECTION_BROKEN, m_request.url.host() );
        return false;
      }
  }

  return true;
}

bool HTTPProtocol::sendBody()
{
  // If we have cached data, the it is either a repost or a DAV request so send
  // the cached data...
  if (m_POSTbuf)
    return sendCachedBody();

  if (m_iPostDataSize == NO_SIZE) {
    // Try the old approach of retireving content data from the job
    // before giving up.
    if (retrieveAllData())
      return sendCachedBody();

    error(ERR_POST_NO_SIZE, m_request.url.host());
    return false;
  }

  // qDebug() << "sending data (size=" << m_iPostDataSize << ")";

  infoMessage(i18n("Sending data to %1",  m_request.url.host()));

  QByteArray cLength ("Content-Length: ");
  cLength += QByteArray::number(m_iPostDataSize);
  cLength += "\r\n\r\n";

  // qDebug() << cLength.trimmed();

  // Send the content length...
  bool sendOk = (write(cLength.data(), cLength.size()) == (ssize_t) cLength.size());
  if (!sendOk) {
    // The server might have closed the connection due to a timeout, or maybe
    // some transport problem arose while the connection was idle.
    if (m_request.isKeepAlive)
    {
      httpCloseConnection();
      return true; // Try again
    }

    // qDebug() << "Connection broken while sending POST content size to" << m_request.url.host();
    error( ERR_CONNECTION_BROKEN, m_request.url.host() );
    return false;
  }

  // Send the amount
  totalSize(m_iPostDataSize);

  // If content-length is 0, then do nothing but simply return true.
  if (m_iPostDataSize == 0)
    return true;

  sendOk = true;
  KIO::filesize_t bytesSent = 0;

  while (true) {
    dataReq();

    QByteArray buffer;
    const int bytesRead = readData(buffer);

    // On done...
    if (bytesRead == 0) {
      sendOk = (bytesSent == m_iPostDataSize);
      break;
    }

    // On error return false...
    if (bytesRead < 0) {
      error(ERR_ABORTED, m_request.url.host());
      sendOk = false;
      break;
    }

    // Cache the POST data in case of a repost request.
    cachePostData(buffer);

    // This will only happen if transmitting the data fails, so we will simply
    // cache the content locally for the potential re-transmit...
    if (!sendOk)
      continue;

    if (write(buffer.data(), bytesRead) == static_cast<ssize_t>(bytesRead)) {
      bytesSent += bytesRead;
      processedSize(bytesSent);  // Send update status...
      continue;
    }

    // qDebug() << "Connection broken while sending POST content to" << m_request.url.host();
    error(ERR_CONNECTION_BROKEN, m_request.url.host());
    sendOk = false;
  }

  return sendOk;
}

void HTTPProtocol::httpClose( bool keepAlive )
{
  // qDebug() << "keepAlive =" << keepAlive;

  cacheFileClose();

  // Only allow persistent connections for GET requests.
  // NOTE: we might even want to narrow this down to non-form
  // based submit requests which will require a meta-data from
  // khtml.
  if (keepAlive) {
    if (!m_request.keepAliveTimeout)
       m_request.keepAliveTimeout = DEFAULT_KEEP_ALIVE_TIMEOUT;
    else if (m_request.keepAliveTimeout > 2*DEFAULT_KEEP_ALIVE_TIMEOUT)
       m_request.keepAliveTimeout = 2*DEFAULT_KEEP_ALIVE_TIMEOUT;

    // qDebug() << "keep alive (" << m_request.keepAliveTimeout << ")";
    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );
    stream << int(99); // special: Close connection
    setTimeoutSpecialCommand(m_request.keepAliveTimeout, data);

    return;
  }

  httpCloseConnection();
}

void HTTPProtocol::closeConnection()
{
  // qDebug();
  httpCloseConnection();
}

void HTTPProtocol::httpCloseConnection()
{
  // qDebug();
  m_server.clear();
  disconnectFromHost();
  clearUnreadBuffer();
  setTimeoutSpecialCommand(-1); // Cancel any connection timeout
}

void HTTPProtocol::slave_status()
{
  // qDebug();

  if ( !isConnected() )
     httpCloseConnection();

  slaveStatus( m_server.url.host(), isConnected() );
}

void HTTPProtocol::mimetype( const QUrl& url )
{
  // qDebug() << url;

  if (!maybeSetRequestUrl(url))
    return;
  resetSessionSettings();

  m_request.method = HTTP_HEAD;
  m_request.cacheTag.policy= CC_Cache;

  if (proceedUntilResponseHeader()) {
    httpClose(m_request.isKeepAlive);
    finished();
  }

  // qDebug() << m_mimeType;
}

void HTTPProtocol::special( const QByteArray &data )
{
  // qDebug();

  int tmp;
  QDataStream stream(data);

  stream >> tmp;
  switch (tmp) {
    case 1: // HTTP POST
    {
      QUrl url;
      qint64 size;
      stream >> url >> size;
      post( url, size );
      break;
    }
    case 2: // cache_update
    {
        QUrl url;
        bool no_cache;
        qint64 expireDate;
        stream >> url >> no_cache >> expireDate;
        if (no_cache) {
            QString filename = cacheFilePathFromUrl(url);
            // there is a tiny risk of deleting the wrong file due to hash collisions here.
            // this is an unimportant performance issue.
            // FIXME on Windows we may be unable to delete the file if open
            QFile::remove(filename);
            finished();
            break;
        }
        // let's be paranoid and inefficient here...
        HTTPRequest savedRequest = m_request;

        m_request.url = url;
        if (cacheFileOpenRead()) {
            m_request.cacheTag.expireDate.setTime_t(expireDate);
            cacheFileClose(); // this sends an update command to the cache cleaner process
        }

        m_request = savedRequest;
        finished();
        break;
    }
    case 5: // WebDAV lock
    {
      QUrl url;
      QString scope, type, owner;
      stream >> url >> scope >> type >> owner;
      davLock( url, scope, type, owner );
      break;
    }
    case 6: // WebDAV unlock
    {
      QUrl url;
      stream >> url;
      davUnlock( url );
      break;
    }
    case 7: // Generic WebDAV
    {
      QUrl url;
      int method;
      qint64 size;
      stream >> url >> method >> size;
      davGeneric( url, (KIO::HTTP_METHOD) method, size );
      break;
    }
    case 99: // Close Connection
    {
      httpCloseConnection();
      break;
    }
    default:
      // Some command we don't understand.
      // Just ignore it, it may come from some future version of KDE.
      break;
  }
}

/**
 * Read a chunk from the data stream.
 */
int HTTPProtocol::readChunked()
{
  if ((m_iBytesLeft == 0) || (m_iBytesLeft == NO_SIZE))
  {
     // discard CRLF from previous chunk, if any, and read size of next chunk

     int bufPos = 0;
     m_receiveBuf.resize(4096);

     bool foundCrLf = readDelimitedText(m_receiveBuf.data(), &bufPos, m_receiveBuf.size(), 1);

     if (foundCrLf && bufPos == 2) {
         // The previous read gave us the CRLF from the previous chunk. As bufPos includes
         // the trailing CRLF it has to be > 2 to possibly include the next chunksize.
         bufPos = 0;
         foundCrLf = readDelimitedText(m_receiveBuf.data(), &bufPos, m_receiveBuf.size(), 1);
     }
     if (!foundCrLf) {
         // qDebug() << "Failed to read chunk header.";
         return -1;
     }
     Q_ASSERT(bufPos > 2);

     long long nextChunkSize = STRTOLL(m_receiveBuf.data(), 0, 16);
     if (nextChunkSize < 0)
     {
        // qDebug() << "Negative chunk size";
        return -1;
     }
     m_iBytesLeft = nextChunkSize;

     // qDebug() << "Chunk size =" << m_iBytesLeft << "bytes";

     if (m_iBytesLeft == 0)
     {
       // Last chunk; read and discard chunk trailer.
       // The last trailer line ends with CRLF and is followed by another CRLF
       // so we have CRLFCRLF like at the end of a standard HTTP header.
       // Do not miss a CRLFCRLF spread over two of our 4K blocks: keep three previous bytes.
       //NOTE the CRLF after the chunksize also counts if there is no trailer. Copy it over.
       char trash[4096];
       trash[0] = m_receiveBuf.constData()[bufPos - 2];
       trash[1] = m_receiveBuf.constData()[bufPos - 1];
       int trashBufPos = 2;
       bool done = false;
       while (!done && !m_isEOF) {
           if (trashBufPos > 3) {
               // shift everything but the last three bytes out of the buffer
               for (int i = 0; i < 3; i++) {
                   trash[i] = trash[trashBufPos - 3 + i];
               }
               trashBufPos = 3;
           }
           done = readDelimitedText(trash, &trashBufPos, 4096, 2);
       }
       if (m_isEOF && !done) {
           // qDebug() << "Failed to read chunk trailer.";
           return -1;
       }

       return 0;
     }
  }

  int bytesReceived = readLimited();
  if (!m_iBytesLeft) {
     m_iBytesLeft = NO_SIZE; // Don't stop, continue with next chunk
  }
  return bytesReceived;
}

int HTTPProtocol::readLimited()
{
  if (!m_iBytesLeft)
    return 0;

  m_receiveBuf.resize(4096);

  int bytesToReceive;
  if (m_iBytesLeft > KIO::filesize_t(m_receiveBuf.size()))
     bytesToReceive = m_receiveBuf.size();
  else
     bytesToReceive = m_iBytesLeft;

  const int bytesReceived = readBuffered(m_receiveBuf.data(), bytesToReceive, false);

  if (bytesReceived <= 0)
     return -1; // Error: connection lost

  m_iBytesLeft -= bytesReceived;
  return bytesReceived;
}

int HTTPProtocol::readUnlimited()
{
  if (m_request.isKeepAlive)
  {
     // qDebug() << "Unbounded datastream on a Keep-alive connection!";
     m_request.isKeepAlive = false;
  }

  m_receiveBuf.resize(4096);

  int result = readBuffered(m_receiveBuf.data(), m_receiveBuf.size());
  if (result > 0)
     return result;

  m_isEOF = true;
  m_iBytesLeft = 0;
  return 0;
}

void HTTPProtocol::slotData(const QByteArray &_d)
{
   if (!_d.size())
   {
      m_isEOD = true;
      return;
   }

   if (m_iContentLeft != NO_SIZE)
   {
      if (m_iContentLeft >= KIO::filesize_t(_d.size()))
         m_iContentLeft -= _d.size();
      else
         m_iContentLeft = NO_SIZE;
   }

   QByteArray d = _d;
   if ( !m_dataInternal )
   {
      // If a broken server does not send the mime-type,
      // we try to id it from the content before dealing
      // with the content itself.
      if ( m_mimeType.isEmpty() && !m_isRedirection &&
           !( m_request.responseCode >= 300 && m_request.responseCode <=399) )
      {
        // qDebug() << "Determining mime-type from content...";
        int old_size = m_mimeTypeBuffer.size();
        m_mimeTypeBuffer.resize( old_size + d.size() );
        memcpy( m_mimeTypeBuffer.data() + old_size, d.data(), d.size() );
        if ( (m_iBytesLeft != NO_SIZE) && (m_iBytesLeft > 0)
             && (m_mimeTypeBuffer.size() < 1024) )
        {
          m_cpMimeBuffer = true;
          return;   // Do not send up the data since we do not yet know its mimetype!
        }

        // qDebug() << "Mimetype buffer size:" << m_mimeTypeBuffer.size();

        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForFileNameAndData(m_request.url.path(), m_mimeTypeBuffer);
        if (mime.isValid() && !mime.isDefault())
        {
          m_mimeType = mime.name();
          // qDebug() << "Mimetype from content:" << m_mimeType;
        }

        if ( m_mimeType.isEmpty() )
        {
          m_mimeType = QLatin1String( DEFAULT_MIME_TYPE );
          // qDebug() << "Using default mimetype:" <<  m_mimeType;
        }

        //### we could also open the cache file here

        if ( m_cpMimeBuffer )
        {
          d.resize(0);
          d.resize(m_mimeTypeBuffer.size());
          memcpy(d.data(), m_mimeTypeBuffer.data(), d.size());
        }
        mimeType(m_mimeType);
        m_mimeTypeBuffer.resize(0);
      }

      //qDebug() << "Sending data of size" << d.size();
      data( d );
      if (m_request.cacheTag.ioMode == WriteToCache) {
        cacheFileWritePayload(d);
      }
   }
   else
   {
      uint old_size = m_webDavDataBuf.size();
      m_webDavDataBuf.resize (old_size + d.size());
      memcpy (m_webDavDataBuf.data() + old_size, d.data(), d.size());
   }
}

/**
 * This function is our "receive" function.  It is responsible for
 * downloading the message (not the header) from the HTTP server.  It
 * is called either as a response to a client's KIOJob::dataEnd()
 * (meaning that the client is done sending data) or by 'sendQuery()'
 * (if we are in the process of a PUT/POST request). It can also be
 * called by a webDAV function, to receive stat/list/property/etc.
 * data; in this case the data is stored in m_webDavDataBuf.
 */
bool HTTPProtocol::readBody( bool dataInternal /* = false */ )
{
  // special case for reading cached body since we also do it in this function. oh well.
  if (!canHaveResponseBody(m_request.responseCode, m_request.method) &&
      !(m_request.cacheTag.ioMode == ReadFromCache && m_request.responseCode == 304 &&
        m_request.method != HTTP_HEAD)) {
      return true;
  }

  m_isEOD = false;
  // Note that when dataInternal is true, we are going to:
  // 1) save the body data to a member variable, m_webDavDataBuf
  // 2) _not_ advertise the data, speed, size, etc., through the
  //    corresponding functions.
  // This is used for returning data to WebDAV.
  m_dataInternal = dataInternal;
  if (dataInternal) {
    m_webDavDataBuf.clear();
  }

  // Check if we need to decode the data.
  // If we are in copy mode, then use only transfer decoding.
  bool useMD5 = !m_contentMD5.isEmpty();

  // Deal with the size of the file.
  KIO::filesize_t sz = m_request.offset;
  if ( sz )
    m_iSize += sz;

  if (!m_isRedirection) {
      // Update the application with total size except when
      // it is compressed, or when the data is to be handled
      // internally (webDAV).  If compressed we have to wait
      // until we uncompress to find out the actual data size
      if ( !dataInternal ) {
          if ((m_iSize > 0) && (m_iSize != NO_SIZE)) {
              totalSize(m_iSize);
              infoMessage(i18n("Retrieving %1 from %2...", KIO::convertSize(m_iSize),
                          m_request.url.host()));
          } else {
              totalSize(0);
          }
      }

      if (m_request.cacheTag.ioMode == ReadFromCache) {
          // qDebug() << "reading data from cache...";

          m_iContentLeft = NO_SIZE;

          QByteArray d;
          while (true) {
              d = cacheFileReadPayload(MAX_IPC_SIZE);
              if (d.isEmpty()) {
                  break;
              }
              slotData(d);
              sz += d.size();
              if (!dataInternal) {
                  processedSize(sz);
              }
          }

          m_receiveBuf.resize(0);

          if (!dataInternal) {
              data(QByteArray());
          }

          return true;
      }
  }

  if (m_iSize != NO_SIZE)
    m_iBytesLeft = m_iSize - sz;
  else
    m_iBytesLeft = NO_SIZE;

  m_iContentLeft = m_iBytesLeft;

  if (m_isChunked)
    m_iBytesLeft = NO_SIZE;

  // qDebug() << KIO::number(m_iBytesLeft) << "bytes left.";

  // Main incoming loop...  Gather everything while we can...
  m_cpMimeBuffer = false;
  m_mimeTypeBuffer.resize(0);

  HTTPFilterChain chain;

  // redirection ignores the body
  if (!m_isRedirection) {
      QObject::connect(&chain, SIGNAL(output(QByteArray)),
                       this, SLOT(slotData(QByteArray)));
  }
  QObject::connect(&chain, SIGNAL(error(QString)),
          this, SLOT(slotFilterError(QString)));

   // decode all of the transfer encodings
  while (!m_transferEncodings.isEmpty())
  {
    QString enc = m_transferEncodings.takeLast();
    if ( enc == QLatin1String("gzip") )
      chain.addFilter(new HTTPFilterGZip);
    else if ( enc == QLatin1String("deflate") )
      chain.addFilter(new HTTPFilterDeflate);
  }

  // From HTTP 1.1 Draft 6:
  // The MD5 digest is computed based on the content of the entity-body,
  // including any content-coding that has been applied, but not including
  // any transfer-encoding applied to the message-body. If the message is
  // received with a transfer-encoding, that encoding MUST be removed
  // prior to checking the Content-MD5 value against the received entity.
  HTTPFilterMD5 *md5Filter = 0;
  if ( useMD5 )
  {
     md5Filter = new HTTPFilterMD5;
     chain.addFilter(md5Filter);
  }

  // now decode all of the content encodings
  // -- Why ?? We are not
  // -- a proxy server, be a client side implementation!!  The applications
  // -- are capable of determinig how to extract the encoded implementation.
  // WB: That's a misunderstanding. We are free to remove the encoding.
  // WB: Some braindead www-servers however, give .tgz files an encoding
  // WB: of "gzip" (or even "x-gzip") and a content-type of "applications/tar"
  // WB: They shouldn't do that. We can work around that though...
  while (!m_contentEncodings.isEmpty())
  {
    QString enc = m_contentEncodings.takeLast();
    if ( enc == QLatin1String("gzip") )
      chain.addFilter(new HTTPFilterGZip);
    else if ( enc == QLatin1String("deflate") )
      chain.addFilter(new HTTPFilterDeflate);
  }

  while (!m_isEOF)
  {
    int bytesReceived;

    if (m_isChunked)
       bytesReceived = readChunked();
    else if (m_iSize != NO_SIZE)
       bytesReceived = readLimited();
    else
       bytesReceived = readUnlimited();

    // make sure that this wasn't an error, first
    // qDebug() << "bytesReceived:"
    //              << (int) bytesReceived << " m_iSize:" << (int) m_iSize << " Chunked:"
    //              << m_isChunked << " BytesLeft:"<< (int) m_iBytesLeft;
    if (bytesReceived == -1)
    {
      if (m_iContentLeft == 0)
      {
         // gzip'ed data sometimes reports a too long content-length.
         // (The length of the unzipped data)
         m_iBytesLeft = 0;
         break;
      }
      // Oh well... log an error and bug out
      // qDebug() << "bytesReceived==-1 sz=" << (int)sz << " Connection broken !";
      error(ERR_CONNECTION_BROKEN, m_request.url.host());
      return false;
    }

    // I guess that nbytes == 0 isn't an error.. but we certainly
    // won't work with it!
    if (bytesReceived > 0)
    {
      // Important: truncate the buffer to the actual size received!
      // Otherwise garbage will be passed to the app
      m_receiveBuf.truncate( bytesReceived );

      chain.slotInput(m_receiveBuf);

      if (m_iError)
         return false;

      sz += bytesReceived;
      if (!dataInternal)
        processedSize( sz );
    }
    m_receiveBuf.resize(0); // res

    if (m_iBytesLeft && m_isEOD && !m_isChunked)
    {
      // gzip'ed data sometimes reports a too long content-length.
      // (The length of the unzipped data)
      m_iBytesLeft = 0;
    }

    if (m_iBytesLeft == 0)
    {
      // qDebug() << "EOD received! Left ="<< KIO::number(m_iBytesLeft);
      break;
    }
  }
  chain.slotInput(QByteArray()); // Flush chain.

  if ( useMD5 )
  {
    QString calculatedMD5 = md5Filter->md5();

    if ( m_contentMD5 != calculatedMD5 )
      qWarning() << "MD5 checksum MISMATCH! Expected:"
                     << calculatedMD5 << ", Got:" << m_contentMD5;
  }

  // Close cache entry
  if (m_iBytesLeft == 0) {
      cacheFileClose(); // no-op if not necessary
  }

  if (!dataInternal && sz <= 1)
  {
    if (m_request.responseCode >= 500 && m_request.responseCode <= 599) {
      error(ERR_INTERNAL_SERVER, m_request.url.host());
      return false;
    } else if (m_request.responseCode >= 400 && m_request.responseCode <= 499 &&
               !isAuthenticationRequired(m_request.responseCode)) {
      error(ERR_DOES_NOT_EXIST, m_request.url.host());
      return false;
    }
  }

  if (!dataInternal && !m_isRedirection)
    data( QByteArray() );

  return true;
}

void HTTPProtocol::slotFilterError(const QString &text)
{
    error(KIO::ERR_SLAVE_DEFINED, text);
}

void HTTPProtocol::error( int _err, const QString &_text )
{
  // Close the connection only on connection errors. Otherwise, honor the
  // keep alive flag.
  if (_err == ERR_CONNECTION_BROKEN || _err == ERR_COULD_NOT_CONNECT)
      httpClose(false);
  else
      httpClose(m_request.isKeepAlive);

  if (!m_request.id.isEmpty())
  {
    forwardHttpResponseHeader();
    sendMetaData();
  }

  // It's over, we don't need it anymore
  clearPostDataBuffer();

  SlaveBase::error( _err, _text );
  m_iError = _err;
}


void HTTPProtocol::addCookies( const QString &url, const QByteArray &cookieHeader )
{
   qlonglong windowId = m_request.windowId.toLongLong();
   QDBusInterface kcookiejar( QLatin1String("org.kde.kded5"), QLatin1String("/modules/kcookiejar"), QLatin1String("org.kde.KCookieServer") );
   (void)kcookiejar.call( QDBus::NoBlock, QLatin1String("addCookies"), url,
                           cookieHeader, windowId );
}

QString HTTPProtocol::findCookies( const QString &url)
{
  qlonglong windowId = m_request.windowId.toLongLong();
  QDBusInterface kcookiejar( QLatin1String("org.kde.kded5"), QLatin1String("/modules/kcookiejar"), QLatin1String("org.kde.KCookieServer") );
  QDBusReply<QString> reply = kcookiejar.call( QLatin1String("findCookies"), url, windowId );

  if ( !reply.isValid() )
  {
     qWarning() << "Can't communicate with kded_kcookiejar!";
     return QString();
  }
  return reply;
}

/******************************* CACHING CODE ****************************/

HTTPProtocol::CacheTag::CachePlan HTTPProtocol::CacheTag::plan(int maxCacheAge) const
{
    //notable omission: we're not checking cache file presence or integrity
    switch (policy) {
    case KIO::CC_Refresh:
        // Conditional GET requires the presence of either an ETag or
        // last modified date.
        if (lastModifiedDate.isValid() || !etag.isEmpty()) {
            return ValidateCached;
        }
        break;
    case KIO::CC_Reload:
        return IgnoreCached;
    case KIO::CC_CacheOnly:
    case KIO::CC_Cache:
        return UseCached;
    default:
        break;
    }

    Q_ASSERT((policy == CC_Verify || policy == CC_Refresh));
    QDateTime currentDate = QDateTime::currentDateTime();
    if ((servedDate.isValid() && (currentDate > servedDate.addSecs(maxCacheAge))) ||
        (expireDate.isValid() && (currentDate > expireDate))) {
        return ValidateCached;
    }
    return UseCached;
}

// !START SYNC!
// The following code should be kept in sync
// with the code in http_cache_cleaner.cpp

// we use QDataStream; this is just an illustration
struct BinaryCacheFileHeader
{
    quint8 version[2];
    quint8 compression; // for now fixed to 0
    quint8 reserved;    // for now; also alignment
    qint32 useCount;
    qint64 servedDate;
    qint64 lastModifiedDate;
    qint64 expireDate;
    qint32 bytesCached;
    // packed size should be 36 bytes; we explicitly set it here to make sure that no compiler
    // padding ruins it. We write the fields to disk without any padding.
    static const int size = 36;
};

enum CacheCleanerCommandCode {
    InvalidCommand = 0,
    CreateFileNotificationCommand,
    UpdateFileCommand
};

// illustration for cache cleaner update "commands"
struct CacheCleanerCommand
{
    BinaryCacheFileHeader header;
    quint32 commandCode;
    // filename in ASCII, binary isn't worth the coding and decoding
    quint8 filename[s_hashedUrlNibbles];
};

QByteArray HTTPProtocol::CacheTag::serialize() const
{
    QByteArray ret;
    QDataStream stream(&ret, QIODevice::WriteOnly);
    stream << quint8('A');
    stream << quint8('\n');
    stream << quint8(0);
    stream << quint8(0);

    stream << fileUseCount;

    stream << servedDate;
    stream << lastModifiedDate;
    stream << expireDate;

    stream << bytesCached;
    Q_ASSERT(ret.size() == BinaryCacheFileHeader::size);
    return ret;
}


static bool compareByte(QDataStream *stream, quint8 value)
{
    quint8 byte;
    *stream >> byte;
    return byte == value;
}

// If starting a new file cacheFileWriteVariableSizeHeader() must have been called *before*
// calling this! This is to fill in the headerEnd field.
// If the file is not new headerEnd has already been read from the file and in fact the variable
// size header *may* not be rewritten because a size change would mess up the file layout.
bool HTTPProtocol::CacheTag::deserialize(const QByteArray &d)
{
    if (d.size() != BinaryCacheFileHeader::size) {
        return false;
    }
    QDataStream stream(d);
    stream.setVersion(QDataStream::Qt_4_5);

    bool ok = true;
    ok = ok && compareByte(&stream, 'A');
    ok = ok && compareByte(&stream, '\n');
    ok = ok && compareByte(&stream, 0);
    ok = ok && compareByte(&stream, 0);
    if (!ok) {
        return false;
    }

    stream >> fileUseCount;

    stream >> servedDate;
    stream >> lastModifiedDate;
    stream >> expireDate;

    stream >> bytesCached;

    return true;
}

/* Text part of the header, directly following the binary first part:
URL\n
etag\n
mimetype\n
header line\n
header line\n
...
\n
*/

static QUrl storableUrl(const QUrl &url)
{
    QUrl ret(url);
    ret.setPassword(QString());
    ret.setFragment(QString());
    return ret;
}

static void writeLine(QIODevice *dev, const QByteArray &line)
{
    static const char linefeed = '\n';
    dev->write(line);
    dev->write(&linefeed, 1);
}

void HTTPProtocol::cacheFileWriteTextHeader()
{
    QFile *&file = m_request.cacheTag.file;
    Q_ASSERT(file);
    Q_ASSERT(file->openMode() & QIODevice::WriteOnly);

    file->seek(BinaryCacheFileHeader::size);
    writeLine(file, storableUrl(m_request.url).toEncoded());
    writeLine(file, m_request.cacheTag.etag.toLatin1());
    writeLine(file, m_mimeType.toLatin1());
    writeLine(file, m_responseHeaders.join(QString(QLatin1Char('\n'))).toLatin1());
    // join("\n") adds no \n to the end, but writeLine() does.
    // Add another newline to mark the end of text.
    writeLine(file, QByteArray());
}

static bool readLineChecked(QIODevice *dev, QByteArray *line)
{
    *line = dev->readLine(MAX_IPC_SIZE);
    // if nothing read or the line didn't fit into 8192 bytes(!)
    if (line->isEmpty() || !line->endsWith('\n')) {
        return false;
    }
    // we don't actually want the newline!
    line->chop(1);
    return true;
}

bool HTTPProtocol::cacheFileReadTextHeader1(const QUrl &desiredUrl)
{
    QFile *&file = m_request.cacheTag.file;
    Q_ASSERT(file);
    Q_ASSERT(file->openMode() == QIODevice::ReadOnly);

    QByteArray readBuf;
    bool ok = readLineChecked(file, &readBuf);
    if (storableUrl(desiredUrl).toEncoded() != readBuf) {
        // qDebug() << "You have witnessed a very improbable hash collision!";
        return false;
    }

    ok = ok && readLineChecked(file, &readBuf);
    m_request.cacheTag.etag = toQString(readBuf);

    return ok;
}

bool HTTPProtocol::cacheFileReadTextHeader2()
{
    QFile *&file = m_request.cacheTag.file;
    Q_ASSERT(file);
    Q_ASSERT(file->openMode() == QIODevice::ReadOnly);

    bool ok = true;
    QByteArray readBuf;
#ifndef NDEBUG
    // we assume that the URL and etag have already been read
    qint64 oldPos = file->pos();
    file->seek(BinaryCacheFileHeader::size);
    ok = ok && readLineChecked(file, &readBuf);
    ok = ok && readLineChecked(file, &readBuf);
    Q_ASSERT(file->pos() == oldPos);
#endif
    ok = ok && readLineChecked(file, &readBuf);
    m_mimeType = toQString(readBuf);

    m_responseHeaders.clear();
    // read as long as no error and no empty line found
    while (true) {
        ok = ok && readLineChecked(file, &readBuf);
        if (ok && !readBuf.isEmpty()) {
            m_responseHeaders.append(toQString(readBuf));
        } else {
            break;
        }
    }
    return ok; // it may still be false ;)
}

static QString filenameFromUrl(const QUrl &url)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(storableUrl(url).toEncoded());
    return toQString(hash.result().toHex());
}

QString HTTPProtocol::cacheFilePathFromUrl(const QUrl &url) const
{
    QString filePath = m_strCacheDir;
    if (!filePath.endsWith(QLatin1Char('/'))) {
        filePath.append(QLatin1Char('/'));
    }
    filePath.append(filenameFromUrl(url));
    return filePath;
}

bool HTTPProtocol::cacheFileOpenRead()
{
    // qDebug();
    QString filename = cacheFilePathFromUrl(m_request.url);

    QFile *&file = m_request.cacheTag.file;
    if (file) {
        // qDebug() << "File unexpectedly open; old file is" << file->fileName() << "new name is" << filename;
        Q_ASSERT(file->fileName() == filename);
    }
    Q_ASSERT(!file);
    file = new QFile(filename);
    if (file->open(QIODevice::ReadOnly)) {
        QByteArray header = file->read(BinaryCacheFileHeader::size);
        if (!m_request.cacheTag.deserialize(header)) {
            // qDebug() << "Cache file header is invalid.";

            file->close();
        }
    }

    if (file->isOpen() && !cacheFileReadTextHeader1(m_request.url)) {
        file->close();
    }

    if (!file->isOpen()) {
        cacheFileClose();
        return false;
    }
    return true;
}


bool HTTPProtocol::cacheFileOpenWrite()
{
    // qDebug();
    QString filename = cacheFilePathFromUrl(m_request.url);

    // if we open a cache file for writing while we have a file open for reading we must have
    // found out that the old cached content is obsolete, so delete the file.
    QFile *&file = m_request.cacheTag.file;
    if (file) {
        // ensure that the file is in a known state - either open for reading or null
        Q_ASSERT(!qobject_cast<QTemporaryFile *>(file));
        Q_ASSERT((file->openMode() & QIODevice::WriteOnly) == 0);
        Q_ASSERT(file->fileName() == filename);
        // qDebug() << "deleting expired cache entry and recreating.";
        file->remove();
        delete file;
        file = 0;
    }

    // note that QTemporaryFile will automatically append random chars to filename
    file = new QTemporaryFile(filename);
    file->open(QIODevice::WriteOnly);

    // if we have started a new file we have not initialized some variables from disk data.
    m_request.cacheTag.fileUseCount = 0;  // the file has not been *read* yet
    m_request.cacheTag.bytesCached = 0;

    if ((file->openMode() & QIODevice::WriteOnly) == 0) {
        // qDebug() << "Could not open file for writing:" << file->fileName()
        //             << "due to error" << file->error();
        cacheFileClose();
        return false;
    }
    return true;
}

static QByteArray makeCacheCleanerCommand(const HTTPProtocol::CacheTag &cacheTag,
                                          CacheCleanerCommandCode cmd)
{
    QByteArray ret = cacheTag.serialize();
    QDataStream stream(&ret, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_4_5);

    stream.skipRawData(BinaryCacheFileHeader::size);
    // append the command code
    stream << quint32(cmd);
    // append the filename
    QString fileName = cacheTag.file->fileName();
    int basenameStart = fileName.lastIndexOf(QLatin1Char('/')) + 1;
    QByteArray baseName = fileName.mid(basenameStart, s_hashedUrlNibbles).toLatin1();
    stream.writeRawData(baseName.constData(), baseName.size());

    Q_ASSERT(ret.size() == BinaryCacheFileHeader::size + sizeof(quint32) + s_hashedUrlNibbles);
    return ret;
}

//### not yet 100% sure when and when not to call this
void HTTPProtocol::cacheFileClose()
{
    // qDebug();

    QFile *&file = m_request.cacheTag.file;
    if (!file) {
        return;
    }

    m_request.cacheTag.ioMode = NoCache;

    QByteArray ccCommand;
    QTemporaryFile *tempFile = qobject_cast<QTemporaryFile *>(file);

    if (file->openMode() & QIODevice::WriteOnly) {
        Q_ASSERT(tempFile);

        if (m_request.cacheTag.bytesCached && !m_iError) {
            QByteArray header = m_request.cacheTag.serialize();
            tempFile->seek(0);
            tempFile->write(header);

            ccCommand = makeCacheCleanerCommand(m_request.cacheTag, CreateFileNotificationCommand);

            QString oldName = tempFile->fileName();
            QString newName = oldName;
            int basenameStart = newName.lastIndexOf(QLatin1Char('/')) + 1;
            // remove the randomized name part added by QTemporaryFile
            newName.chop(newName.length() - basenameStart - s_hashedUrlNibbles);
            // qDebug() << "Renaming temporary file" << oldName << "to" << newName;

            // on windows open files can't be renamed
            tempFile->setAutoRemove(false);
            delete tempFile;
            file = 0;

            if (!QFile::rename(oldName, newName)) {
                // ### currently this hides a minor bug when force-reloading a resource. We
                //     should not even open a new file for writing in that case.
                // qDebug() << "Renaming temporary file failed, deleting it instead.";
                QFile::remove(oldName);
                ccCommand.clear();  // we have nothing of value to tell the cache cleaner
            }
        } else {
            // oh, we've never written payload data to the cache file.
            // the temporary file is closed and removed and no proper cache entry is created.
        }
    } else if (file->openMode() == QIODevice::ReadOnly) {
        Q_ASSERT(!tempFile);
        ccCommand = makeCacheCleanerCommand(m_request.cacheTag, UpdateFileCommand);
    }
    delete file;
    file = 0;

    if (!ccCommand.isEmpty()) {
        sendCacheCleanerCommand(ccCommand);
    }
}

void HTTPProtocol::sendCacheCleanerCommand(const QByteArray &command)
{
    // qDebug();
    Q_ASSERT(command.size() == BinaryCacheFileHeader::size + s_hashedUrlNibbles + sizeof(quint32));
    int attempts = 0;
    while (m_cacheCleanerConnection.state() != QLocalSocket::ConnectedState && attempts < 6) {
        if (attempts == 2) {
            QString exe = QFile::decodeName(CMAKE_INSTALL_PREFIX "/" LIBEXEC_INSTALL_DIR "/kio_http_cache_cleaner");
            if (QFile::exists(exe)) {
                QProcess::startDetached(exe);
            } else {
                qWarning() << exe << "not found!";
            }
        }
        QString socketFileName = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QLatin1Char('/') + QLatin1String("kio_http_cache_cleaner");
        m_cacheCleanerConnection.connectToServer(socketFileName, QIODevice::WriteOnly);
        m_cacheCleanerConnection.waitForConnected(1500);
        attempts++;
    }

    if (m_cacheCleanerConnection.state() == QLocalSocket::ConnectedState) {
        m_cacheCleanerConnection.write(command);
        m_cacheCleanerConnection.flush();
    } else {
        // updating the stats is not vital, so we just give up.
        // qDebug() << "Could not connect to cache cleaner, not updating stats of this cache file.";
    }
}

QByteArray HTTPProtocol::cacheFileReadPayload(int maxLength)
{
    Q_ASSERT(m_request.cacheTag.file);
    Q_ASSERT(m_request.cacheTag.ioMode == ReadFromCache);
    Q_ASSERT(m_request.cacheTag.file->openMode() == QIODevice::ReadOnly);
    QByteArray ret = m_request.cacheTag.file->read(maxLength);
    if (ret.isEmpty()) {
        cacheFileClose();
    }
    return ret;
}


void HTTPProtocol::cacheFileWritePayload(const QByteArray &d)
{
    if (!m_request.cacheTag.file) {
        return;
    }

    // If the file being downloaded is so big that it exceeds the max cache size,
    // do not cache it! See BR# 244215. NOTE: this can be improved upon in the
    // future...
    if (m_iSize >= KIO::filesize_t(m_maxCacheSize * 1024)) {
        // qDebug() << "Caching disabled because content size is too big.";
        cacheFileClose();
        return;
    }

    Q_ASSERT(m_request.cacheTag.ioMode == WriteToCache);
    Q_ASSERT(m_request.cacheTag.file->openMode() & QIODevice::WriteOnly);

    if (d.isEmpty()) {
        cacheFileClose();
    }

    //TODO: abort if file grows too big!

    // write the variable length text header as soon as we start writing to the file
    if (!m_request.cacheTag.bytesCached) {
        cacheFileWriteTextHeader();
    }
    m_request.cacheTag.bytesCached += d.size();
    m_request.cacheTag.file->write(d);
}

void HTTPProtocol::cachePostData(const QByteArray& data)
{
    if (!m_POSTbuf) {
        m_POSTbuf = createPostBufferDeviceFor(qMax(m_iPostDataSize, static_cast<KIO::filesize_t>(data.size())));
        if (!m_POSTbuf)
            return;
    }

    m_POSTbuf->write (data.constData(), data.size());
}

void HTTPProtocol::clearPostDataBuffer()
{
    if (!m_POSTbuf)
        return;

    delete m_POSTbuf;
    m_POSTbuf = 0;
}

bool HTTPProtocol::retrieveAllData()
{
    if (!m_POSTbuf) {
        m_POSTbuf = createPostBufferDeviceFor(s_MaxInMemPostBufSize + 1);
    }

    if (!m_POSTbuf) {
        error (ERR_OUT_OF_MEMORY, m_request.url.host());
        return false;
    }

    while (true) {
        dataReq();
        QByteArray buffer;
        const int bytesRead = readData(buffer);

        if (bytesRead < 0) {
            error(ERR_ABORTED, m_request.url.host());
            return false;
        }

        if (bytesRead == 0) {
            break;
        }

        m_POSTbuf->write(buffer.constData(), buffer.size());
    }

    return true;
}

// The above code should be kept in sync
// with the code in http_cache_cleaner.cpp
// !END SYNC!

//**************************  AUTHENTICATION CODE ********************/

QString HTTPProtocol::authenticationHeader()
{
    QByteArray ret;

    // If the internal meta-data "cached-www-auth" is set, then check for cached
    // authentication data and preemtively send the authentication header if a
    // matching one is found.
    if (!m_wwwAuth && config()->readEntry("cached-www-auth", false)) {
        KIO::AuthInfo authinfo;
        authinfo.url = m_request.url;
        authinfo.realmValue = config()->readEntry("www-auth-realm", QString());
        // If no relam metadata, then make sure path matching is turned on.
        authinfo.verifyPath = (authinfo.realmValue.isEmpty());

        const bool useCachedAuth = (m_request.responseCode == 401 || !config()->readEntry("no-preemptive-auth-reuse", false));

        if (useCachedAuth && checkCachedAuthentication(authinfo)) {
            const QByteArray cachedChallenge = config()->readEntry("www-auth-challenge", QByteArray());
            if (!cachedChallenge.isEmpty()) {
                m_wwwAuth = KAbstractHttpAuthentication::newAuth(cachedChallenge, config());
                if (m_wwwAuth) {
                    // qDebug() << "creating www authentcation header from cached info";
                    m_wwwAuth->setChallenge(cachedChallenge, m_request.url, m_request.methodString());
                    m_wwwAuth->generateResponse(authinfo.username, authinfo.password);
                }
            }
        }
    }

    // If the internal meta-data "cached-proxy-auth" is set, then check for cached
    // authentication data and preemtively send the authentication header if a
    // matching one is found.
    if (!m_proxyAuth && config()->readEntry("cached-proxy-auth", false)) {
        KIO::AuthInfo authinfo;
        authinfo.url = m_request.proxyUrl;
        authinfo.realmValue = config()->readEntry("proxy-auth-realm", QString());
        // If no relam metadata, then make sure path matching is turned on.
        authinfo.verifyPath = (authinfo.realmValue.isEmpty());

        if (checkCachedAuthentication(authinfo)) {
            const QByteArray cachedChallenge = config()->readEntry("proxy-auth-challenge", QByteArray());
            if (!cachedChallenge.isEmpty()) {
                m_proxyAuth = KAbstractHttpAuthentication::newAuth(cachedChallenge, config());
                if (m_proxyAuth) {
                    // qDebug() << "creating proxy authentcation header from cached info";
                    m_proxyAuth->setChallenge(cachedChallenge, m_request.proxyUrl, m_request.methodString());
                    m_proxyAuth->generateResponse(authinfo.username, authinfo.password);
                }
            }
        }
    }

    // the authentication classes don't know if they are for proxy or webserver authentication...
    if (m_wwwAuth && !m_wwwAuth->isError()) {
        ret += "Authorization: ";
        ret += m_wwwAuth->headerFragment();
    }

    if (m_proxyAuth && !m_proxyAuth->isError()) {
        ret += "Proxy-Authorization: ";
        ret += m_proxyAuth->headerFragment();
    }

    return toQString(ret); // ## encoding ok?
}

static QString protocolForProxyType(QNetworkProxy::ProxyType type)
{
    switch (type) {
      case QNetworkProxy::DefaultProxy:
          break;
      case QNetworkProxy::Socks5Proxy:
          return QLatin1String("socks");
      case QNetworkProxy::NoProxy:
          break;
      case QNetworkProxy::HttpProxy:
      case QNetworkProxy::HttpCachingProxy:
      case QNetworkProxy::FtpCachingProxy:
      default:
          break;
    }

    return QLatin1String("http");
}

void HTTPProtocol::proxyAuthenticationForSocket(const QNetworkProxy &proxy, QAuthenticator *authenticator)
{
    // qDebug() << "realm:" << authenticator->realm() << "user:" << authenticator->user();

    // Set the proxy URL...
    m_request.proxyUrl.setScheme(protocolForProxyType(proxy.type()));
    m_request.proxyUrl.setUserName(proxy.user());
    m_request.proxyUrl.setHost(proxy.hostName());
    m_request.proxyUrl.setPort(proxy.port());

    AuthInfo info;
    info.url = m_request.proxyUrl;
    info.realmValue = authenticator->realm();
    info.username = authenticator->user();
    info.verifyPath = info.realmValue.isEmpty();

    const bool haveCachedCredentials = checkCachedAuthentication(info);
    const bool retryAuth = (m_socketProxyAuth != 0);

    // if m_socketProxyAuth is a valid pointer then authentication has been attempted before,
    // and it was not successful. see below and saveProxyAuthenticationForSocket().
    if (!haveCachedCredentials || retryAuth) {
        // Save authentication info if the connection succeeds. We need to disconnect
        // this after saving the auth data (or an error) so we won't save garbage afterwards!
        connect(socket(), SIGNAL(connected()),
                this, SLOT(saveProxyAuthenticationForSocket()));
        //### fillPromptInfo(&info);
        info.prompt = i18n("You need to supply a username and a password for "
                           "the proxy server listed below before you are allowed "
                           "to access any sites.");
        info.keepPassword = true;
        info.commentLabel = i18n("Proxy:");
        info.comment = i18n("<b>%1</b> at <b>%2</b>", htmlEscape(info.realmValue), m_request.proxyUrl.host());

        const QString errMsg ((retryAuth ? i18n("Proxy Authentication Failed.") : QString()));

        if (!openPasswordDialog(info, errMsg)) {
            // qDebug() << "looks like the user canceled proxy authentication.";
            error(ERR_USER_CANCELED, m_request.proxyUrl.host());
            delete m_proxyAuth;
            m_proxyAuth = 0;
            return;
        }
    }
    authenticator->setUser(info.username);
    authenticator->setPassword(info.password);
    authenticator->setOption(QLatin1String("keepalive"), info.keepPassword);

    if (m_socketProxyAuth) {
        *m_socketProxyAuth = *authenticator;
    } else {
        m_socketProxyAuth = new QAuthenticator(*authenticator);
    }

    if (!m_request.proxyUrl.userName().isEmpty()) {
        m_request.proxyUrl.setUserName(info.username);
    }
}

void HTTPProtocol::saveProxyAuthenticationForSocket()
{
    // qDebug() << "Saving authenticator";
    disconnect(socket(), SIGNAL(connected()),
               this, SLOT(saveProxyAuthenticationForSocket()));
    Q_ASSERT(m_socketProxyAuth);
    if (m_socketProxyAuth) {
        // qDebug() << "realm:" << m_socketProxyAuth->realm() << "user:" << m_socketProxyAuth->user();
        KIO::AuthInfo a;
        a.verifyPath = true;
        a.url = m_request.proxyUrl;
        a.realmValue = m_socketProxyAuth->realm();
        a.username = m_socketProxyAuth->user();
        a.password = m_socketProxyAuth->password();
        a.keepPassword = m_socketProxyAuth->option(QLatin1String("keepalive")).toBool();
        cacheAuthentication(a);
    }
    delete m_socketProxyAuth;
    m_socketProxyAuth = 0;
}

void HTTPProtocol::saveAuthenticationData()
{
    KIO::AuthInfo authinfo;
    bool alreadyCached = false;
    KAbstractHttpAuthentication *auth = 0;
    switch (m_request.prevResponseCode) {
    case 401:
        auth = m_wwwAuth;
        alreadyCached = config()->readEntry("cached-www-auth", false);
        break;
    case 407:
        auth = m_proxyAuth;
        alreadyCached = config()->readEntry("cached-proxy-auth", false);
        break;
    default:
        Q_ASSERT(false); // should never happen!
    }

    // Prevent recaching of the same credentials over and over again.
    if (auth && (!auth->realm().isEmpty() || !alreadyCached)) {
        auth->fillKioAuthInfo(&authinfo);
        if (auth == m_wwwAuth) {
            setMetaData(QLatin1String("{internal~currenthost}cached-www-auth"), QLatin1String("true"));
            if (!authinfo.realmValue.isEmpty())
                setMetaData(QLatin1String("{internal~currenthost}www-auth-realm"), authinfo.realmValue);
            if (!authinfo.digestInfo.isEmpty())
                setMetaData(QLatin1String("{internal~currenthost}www-auth-challenge"), authinfo.digestInfo);
        } else {
            setMetaData(QLatin1String("{internal~allhosts}cached-proxy-auth"), QLatin1String("true"));
            if (!authinfo.realmValue.isEmpty())
                setMetaData(QLatin1String("{internal~allhosts}proxy-auth-realm"), authinfo.realmValue);
            if (!authinfo.digestInfo.isEmpty())
                setMetaData(QLatin1String("{internal~allhosts}proxy-auth-challenge"), authinfo.digestInfo);
        }

        // qDebug() << "Cache authentication info ?" << authinfo.keepPassword;

        if (authinfo.keepPassword) {
            cacheAuthentication(authinfo);
            // qDebug() << "Cached authentication for" << m_request.url;
        }
    }
    // Update our server connection state which includes www and proxy username and password.
    m_server.updateCredentials(m_request);
}

bool HTTPProtocol::handleAuthenticationHeader(const HeaderTokenizer* tokenizer)
{
    KIO::AuthInfo authinfo;
    QList<QByteArray> authTokens;
    KAbstractHttpAuthentication **auth;

    if (m_request.responseCode == 401) {
        auth = &m_wwwAuth;
        authTokens = tokenizer->iterator("www-authenticate").all();
        authinfo.url = m_request.url;
        authinfo.username = m_server.url.userName();
        authinfo.prompt = i18n("You need to supply a username and a "
                               "password to access this site.");
        authinfo.commentLabel = i18n("Site:");
    } else {
        // make sure that the 407 header hasn't escaped a lower layer when it shouldn't.
        // this may break proxy chains which were never tested anyway, and AFAIK they are
        // rare to nonexistent in the wild.
        Q_ASSERT(QNetworkProxy::applicationProxy().type() == QNetworkProxy::NoProxy);
        auth = &m_proxyAuth;
        authTokens = tokenizer->iterator("proxy-authenticate").all();
        authinfo.url = m_request.proxyUrl;
        authinfo.username = m_request.proxyUrl.userName();
        authinfo.prompt = i18n("You need to supply a username and a password for "
                               "the proxy server listed below before you are allowed "
                               "to access any sites." );
        authinfo.commentLabel = i18n("Proxy:");
    }

    bool authRequiresAnotherRoundtrip = false;

    // Workaround brain dead server responses that violate the spec and
    // incorrectly return a 401/407 without the required WWW/Proxy-Authenticate
    // header fields. See bug 215736...
    if (!authTokens.isEmpty()) {
        QString errorMsg;
        authRequiresAnotherRoundtrip = true;

        if (m_request.responseCode == m_request.prevResponseCode && *auth) {
            // Authentication attempt failed. Retry...
            if ((*auth)->wasFinalStage()) {
                errorMsg = (m_request.responseCode == 401 ?
                            i18n("Authentication Failed.") :
                            i18n("Proxy Authentication Failed."));
                delete *auth;
                *auth = 0;
            } else {     // Create authentication header
                //  WORKAROUND: The following piece of code prevents brain dead IIS
                // servers that send back multiple "WWW-Authenticate" headers from
                // screwing up our authentication logic during the challenge
                // phase (Type 2) of NTLM authenticaiton.
                QMutableListIterator<QByteArray> it (authTokens);
                const QByteArray authScheme ((*auth)->scheme().trimmed());
                while (it.hasNext()) {
                    if (qstrnicmp(authScheme.constData(), it.next().constData(), authScheme.length()) != 0) {
                        it.remove();
                    }
                }
            }
        }

try_next_auth_scheme:
        QByteArray bestOffer = KAbstractHttpAuthentication::bestOffer(authTokens);
        if (*auth) {
            const QByteArray authScheme ((*auth)->scheme().trimmed());
            if (qstrnicmp(authScheme.constData(), bestOffer.constData(), authScheme.length()) != 0) {
                // huh, the strongest authentication scheme offered has changed.
                delete *auth;
                *auth = 0;
            }
        }

        if (!(*auth)) {
            *auth = KAbstractHttpAuthentication::newAuth(bestOffer, config());
        }

        if (*auth) {
            // qDebug() << "Trying authentication scheme:" << (*auth)->scheme();

            // remove trailing space from the method string, or digest auth will fail
            (*auth)->setChallenge(bestOffer, authinfo.url, m_request.methodString());

            QString username, password;
            bool generateAuthHeader = true;
            if ((*auth)->needCredentials()) {
                // use credentials supplied by the application if available
                if (!m_request.url.userName().isEmpty() && !m_request.url.password().isEmpty()) {
                    username = m_request.url.userName();
                    password = m_request.url.password();
                    // don't try this password any more
                    m_request.url.setPassword(QString());
                } else {
                    // try to get credentials from kpasswdserver's cache, then try asking the user.
                    authinfo.verifyPath = false; // we have realm, no path based checking please!
                    authinfo.realmValue = (*auth)->realm();
                    if (authinfo.realmValue.isEmpty() && !(*auth)->supportsPathMatching())
                        authinfo.realmValue = QLatin1String((*auth)->scheme());

                    // Save the current authinfo url because it can be modified by the call to
                    // checkCachedAuthentication. That way we can restore it if the call
                    // modified it.
                    const QUrl reqUrl = authinfo.url;
                    if (!errorMsg.isEmpty() || !checkCachedAuthentication(authinfo)) {
                        // Reset url to the saved url...
                        authinfo.url = reqUrl;
                        authinfo.keepPassword = true;
                        authinfo.comment = i18n("<b>%1</b> at <b>%2</b>",
                                                htmlEscape(authinfo.realmValue), authinfo.url.host());

                        if (!openPasswordDialog(authinfo, errorMsg)) {
                            generateAuthHeader = false;
                            authRequiresAnotherRoundtrip = false;
                            if (!sendErrorPageNotification()) {
                                error(ERR_ACCESS_DENIED, reqUrl.host());
                            }
                            // qDebug() << "looks like the user canceled the authentication dialog";
                            delete *auth;
                            *auth = 0;
                        }
                    }
                    username = authinfo.username;
                    password = authinfo.password;
                }
            }

            if (generateAuthHeader) {
                (*auth)->generateResponse(username, password);
                (*auth)->setCachePasswordEnabled(authinfo.keepPassword);

                // qDebug() << "isError=" << (*auth)->isError()
                //             << "needCredentials=" << (*auth)->needCredentials()
                //             << "forceKeepAlive=" << (*auth)->forceKeepAlive()
                //             << "forceDisconnect=" << (*auth)->forceDisconnect();

                if ((*auth)->isError()) {
                    authTokens.removeOne(bestOffer);
                    if (!authTokens.isEmpty()) {
                        goto try_next_auth_scheme;
                    } else {
                        error(ERR_UNSUPPORTED_ACTION, i18n("Authorization failed."));
                        authRequiresAnotherRoundtrip = false;
                    }
                    //### return false; ?
                } else if ((*auth)->forceKeepAlive()) {
                    //### think this through for proxied / not proxied
                    m_request.isKeepAlive = true;
                } else if ((*auth)->forceDisconnect()) {
                    //### think this through for proxied / not proxied
                    m_request.isKeepAlive = false;
                    httpCloseConnection();
                }
            }
        } else {
            authRequiresAnotherRoundtrip = false;
            if (!sendErrorPageNotification()) {
                error(ERR_UNSUPPORTED_ACTION, i18n("Unknown Authorization method."));
            }
        }
    }

    return authRequiresAnotherRoundtrip;
}

