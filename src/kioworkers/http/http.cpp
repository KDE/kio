/*
    SPDX-FileCopyrightText: 2000-2003 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000-2002 George Staikos <staikos@kde.org>
    SPDX-FileCopyrightText: 2000-2002 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2001, 2002 Hamish Rodda <rodda@kde.org>
    SPDX-FileCopyrightText: 2007 Nick Shaforostoff <shafff@ukr.net>
    SPDX-FileCopyrightText: 2007-2018 Daniel Nicoletti <dantti12@gmail.com>
    SPDX-FileCopyrightText: 2008, 2009 Andreas Hartmetz <ahartmetz@gmail.com>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "http.h"
#include "debug.h"
#include "kioglobal_p.h"

#include <QAuthenticator>
#include <QBuffer>
#include <QCoreApplication>
#include <QDomDocument>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkProxy>
#include <QSslCipher>

#include <KLocalizedString>

#include <authinfo.h>
#include <ksslcertificatemanager.h>

#ifndef Q_OS_WIN
#include <sys/utsname.h>
#endif

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.http" FILE "http.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_http"));

    // start the worker
    HTTPProtocol worker(argv[1], argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}
}

class Cookies : public QNetworkCookieJar
{
    Q_OBJECT
public:
    Q_SIGNAL void cookiesAdded(const QString &cookieString);
    Q_SIGNAL void queryCookies(QString &cookieString);

    QList<QNetworkCookie> m_cookies;

    QList<QNetworkCookie> cookiesForUrl(const QUrl & /*url*/) const override
    {
        return m_cookies;
    }

    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl & /*url*/) override
    {
        QString cookieString;

        for (const QNetworkCookie &cookie : cookieList) {
            cookieString += QStringLiteral("Set-Cookie: ") + QString::fromUtf8(cookie.toRawForm()) + QLatin1Char('\n');
        }

        Q_EMIT cookiesAdded(cookieString);

        return true;
    }

    void setCookies(const QString &cookieString)
    {
        const QStringList cookiePieces = cookieString.mid(8).split(QLatin1Char(';'), Qt::SkipEmptyParts);

        for (const QString &cookiePiece : cookiePieces) {
            const QString name = cookiePiece.left(cookiePiece.indexOf(QLatin1Char('=')));
            const QString value = cookiePiece.mid(cookiePiece.indexOf(QLatin1Char('=')) + 1);

            QNetworkCookie cookie(name.toUtf8(), value.toUtf8());
            m_cookies << cookie;
        }
    }
};

namespace
{

bool isDav(const QString &protocol)
{
    return protocol.startsWith(QLatin1String("webdav")) || protocol.startsWith(QLatin1String("dav"));
}

QUrl protocolChangedToHttp(const QUrl &url)
{
    QUrl newUrl{url};
    QString protocol{newUrl.scheme()};
    // Keeps the 's' at the end, if present.
    protocol.replace(QLatin1String{"webdav"}, QLatin1String{"http"});
    protocol.replace(QLatin1String{"dav"}, QLatin1String{"http"});
    if (newUrl.scheme() != protocol)
        newUrl.setScheme(protocol);
    return newUrl;
}
};

HTTPProtocol::HTTPProtocol(const QByteArray &protocol, const QByteArray &pool, const QByteArray &app)
    : WorkerBase(protocol, pool, app)
{
}

HTTPProtocol::~HTTPProtocol()
{
}

QString readMimeType(QNetworkReply *reply)
{
    const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

    return contentType.left(contentType.indexOf(QLatin1Char(';')));
}

void HTTPProtocol::handleSslErrors(QNetworkReply *reply, const QList<QSslError> errors)
{
    if (!metaData(QStringLiteral("ssl_no_ui")).isEmpty() && metaData(QStringLiteral("ssl_no_ui")).compare(QLatin1String("false"), Qt::CaseInsensitive)) {
        return;
    }

    QList<QSslCertificate> certs = reply->sslConfiguration().peerCertificateChain();

    QStringList peerCertChain;
    for (const QSslCertificate &cert : std::as_const(certs)) {
        peerCertChain += QString::fromUtf8(cert.toPem());
    }

    auto sslErrors = errors;

    const QList<QSslError> fatalErrors = KSslCertificateManager::nonIgnorableErrors(sslErrors);
    if (!fatalErrors.isEmpty()) {
        qCWarning(KIOHTTP_LOG) << "SSL errors that cannot be ignored occurred" << fatalErrors;
        Q_EMIT errorOut(KIO::ERR_CANNOT_CONNECT);
        return;
    }

    KSslCertificateRule rule = KSslCertificateManager::self()->rule(certs.first(), m_hostName);

    // remove previously seen and acknowledged errors
    const QList<QSslError> remainingErrors = rule.filterErrors(sslErrors);
    if (remainingErrors.isEmpty()) {
        reply->ignoreSslErrors();
        return;
    }

    // try to fill in the blanks, i.e. missing certificates, and just assume that
    // those belong to the peer (==website or similar) certificate.
    for (int i = 0; i < sslErrors.count(); i++) {
        if (sslErrors[i].certificate().isNull()) {
            sslErrors[i] = QSslError(sslErrors[i].error(), certs[0]);
        }
    }

    QStringList certificateErrors;
    // encode the two-dimensional numeric error list using '\n' and '\t' as outer and inner separators
    for (const QSslCertificate &cert : std::as_const(certs)) {
        QString errorStr;
        for (const QSslError &error : std::as_const(sslErrors)) {
            if (error.certificate() == cert) {
                errorStr = QString::number(static_cast<int>(error.error())) + QLatin1Char('\t');
            }
        }
        if (errorStr.endsWith(QLatin1Char('\t'))) {
            errorStr.chop(1);
        }
        certificateErrors << errorStr;
    }

    const QSslCipher cipher = reply->sslConfiguration().sessionCipher();

    const QVariantMap sslData{
        {QStringLiteral("hostname"), m_hostName},
        {QStringLiteral("protocol"), cipher.protocolString()},
        {QStringLiteral("sslError"), errors.first().errorString()},
        {QStringLiteral("peerCertChain"), peerCertChain},
        {QStringLiteral("certificateErrors"), certificateErrors},
        {QStringLiteral("cipher"), cipher.name()},
        {QStringLiteral("bits"), cipher.supportedBits()},
        {QStringLiteral("usedBits"), cipher.usedBits()},
    };

    int result = sslError(sslData);

    if (result == 1) {
        QDateTime ruleExpiry = QDateTime::currentDateTime();

        const int result = messageBox(WarningTwoActionsCancel,
                                      i18n("Would you like to accept this "
                                           "certificate forever without "
                                           "being prompted?"),
                                      i18n("Server Authentication"),
                                      i18n("&Forever"),
                                      i18n("&Current Session only"));
        if (result == WorkerBase::PrimaryAction) {
            // accept forever ("for a very long time")
            ruleExpiry = ruleExpiry.addYears(1000);
        } else if (result == WorkerBase::SecondaryAction) {
            // accept "for a short time", half an hour.
            ruleExpiry = ruleExpiry.addSecs(30 * 60);
        } else {
            Q_EMIT errorOut(KIO::ERR_CANNOT_CONNECT);
            return;
        }

        rule.setExpiryDateTime(ruleExpiry);
        rule.setIgnoredErrors(sslErrors);
        KSslCertificateManager::self()->setRule(rule);

        reply->ignoreSslErrors();
    } else {
        Q_EMIT errorOut(KIO::ERR_CANNOT_CONNECT);
    }
}

HTTPProtocol::Response HTTPProtocol::makeDavRequest(const QUrl &url,
                                                    KIO::HTTP_METHOD method,
                                                    QByteArray &inputData,
                                                    DataMode dataMode,
                                                    const QMap<QByteArray, QByteArray> &extraHeaders)
{
    auto headers = extraHeaders;
    const QString locks = davProcessLocks();

    if (!headers.contains("Content-Type")) {
        headers.insert("Content-Type", "text/xml; charset=utf-8");
    }

    if (!locks.isEmpty()) {
        headers.insert("If", locks.toLatin1());
    }

    return makeRequest(url, method, inputData, dataMode, headers);
}

HTTPProtocol::Response
HTTPProtocol::makeRequest(const QUrl &url, KIO::HTTP_METHOD method, QByteArray &inputData, DataMode dataMode, const QMap<QByteArray, QByteArray> &extraHeaders)
{
    /* HTTPProtocol::get(...) creates an empty inputData whether or not the calling function
     * sent data to the request. QNetworkRequest sends "Content-Length: 0" for all requests
     * when the device != nullptr, even if data is empty. Per RFC9110, "A user agent SHOULD NOT
     * send a Content-Length header field when the request message does not contain content and
     * the method semantics do not anticipate such data." Semantically, HTTP_GET, HTTP_HEAD,
     * and others shouldn't send the header when the data is empty. Workaround that behavior
     * here until and if Qt is modified. https://bugreports.qt.io/browse/QTBUG-138848 */
    const bool noBodyWhenEmpty = (method == KIO::HTTP_GET || method == KIO::HTTP_HEAD || method == KIO::HTTP_DELETE);
    QBuffer buffer(&inputData);
    QIODevice *bodyDevice = (noBodyWhenEmpty && inputData.isEmpty()) ? nullptr : &buffer;
    return makeRequest(url, method, bodyDevice, dataMode, extraHeaders);
}

static QString protocolForProxyType(QNetworkProxy::ProxyType type)
{
    switch (type) {
    case QNetworkProxy::DefaultProxy:
        break;
    case QNetworkProxy::Socks5Proxy:
        return QStringLiteral("socks");
    case QNetworkProxy::NoProxy:
        break;
    case QNetworkProxy::HttpProxy:
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        break;
    }

    return QStringLiteral("http");
}

HTTPProtocol::Response HTTPProtocol::makeRequest(const QUrl &url,
                                                 KIO::HTTP_METHOD method,
                                                 QIODevice *inputData,
                                                 HTTPProtocol::DataMode dataMode,
                                                 const QMap<QByteArray, QByteArray> &extraHeaders)
{
    QNetworkAccessManager nam;

    // Disable automatic redirect handling from Qt. We need to intercept redirects
    // to let KIO handle them
    nam.setRedirectPolicy(QNetworkRequest::ManualRedirectPolicy);

    auto cookies = new Cookies;

    if (metaData(QStringLiteral("cookies")) == QStringLiteral("manual")) {
        cookies->setCookies(metaData(QStringLiteral("setcookies")));

        connect(cookies, &Cookies::cookiesAdded, this, [this](const QString &cookiesString) {
            setMetaData(QStringLiteral("setcookies"), cookiesString);
        });
    }

    nam.setCookieJar(cookies);

    QUrl properUrl = protocolChangedToHttp(url);

    m_hostName = properUrl.host();

    connect(&nam, &QNetworkAccessManager::authenticationRequired, this, [this, url](QNetworkReply * /*reply*/, QAuthenticator *authenticator) {
        if (configValue(QStringLiteral("no-www-auth"), false)) {
            return;
        }

        KIO::AuthInfo authinfo;
        authinfo.url = url;
        authinfo.username = url.userName();
        authinfo.prompt = i18n(
            "You need to supply a username and a "
            "password to access this site.");
        authinfo.commentLabel = i18n("Site:");

        // try to get credentials from kpasswdserver's cache, then try asking the user.
        authinfo.verifyPath = false; // we have realm, no path based checking please!
        authinfo.realmValue = authenticator->realm();

        // Save the current authinfo url because it can be modified by the call to
        // checkCachedAuthentication. That way we can restore it if the call
        // modified it.
        const QUrl reqUrl = authinfo.url;

        if (checkCachedAuthentication(authinfo)) {
            authenticator->setUser(authinfo.username);
            authenticator->setPassword(authinfo.password);
        } else {
            // Reset url to the saved url...
            authinfo.url = reqUrl;
            authinfo.keepPassword = true;
            authinfo.comment = i18n("<b>%1</b> at <b>%2</b>", authinfo.realmValue.toHtmlEscaped(), authinfo.url.host());

            const int errorCode = openPasswordDialog(authinfo, QString());

            if (!errorCode) {
                authenticator->setUser(authinfo.username);
                authenticator->setPassword(authinfo.password);
                if (authinfo.keepPassword) {
                    cacheAuthentication(authinfo);
                }
            }
        }
    });

    connect(&nam, &QNetworkAccessManager::proxyAuthenticationRequired, this, [this](const QNetworkProxy &proxy, QAuthenticator *authenticator) {
        if (configValue(QStringLiteral("no-proxy-auth"), false)) {
            return;
        }

        QUrl proxyUrl;

        proxyUrl.setScheme(protocolForProxyType(proxy.type()));
        proxyUrl.setUserName(proxy.user());
        proxyUrl.setHost(proxy.hostName());
        proxyUrl.setPort(proxy.port());

        KIO::AuthInfo authinfo;
        authinfo.url = proxyUrl;
        authinfo.username = proxyUrl.userName();
        authinfo.prompt = i18n(
            "You need to supply a username and a password for "
            "the proxy server listed below before you are allowed "
            "to access any sites.");
        authinfo.keepPassword = true;
        authinfo.commentLabel = i18n("Proxy:");

        // try to get credentials from kpasswdserver's cache, then try asking the user.
        authinfo.verifyPath = false; // we have realm, no path based checking please!
        authinfo.realmValue = authenticator->realm();
        authinfo.comment = i18n("<b>%1</b> at <b>%2</b>", authinfo.realmValue.toHtmlEscaped(), proxyUrl.host());

        // Save the current authinfo url because it can be modified by the call to
        // checkCachedAuthentication. That way we can restore it if the call
        // modified it.
        const QUrl reqUrl = authinfo.url;

        if (checkCachedAuthentication(authinfo)) {
            authenticator->setUser(authinfo.username);
            authenticator->setPassword(authinfo.password);
        } else {
            // Reset url to the saved url...
            authinfo.url = reqUrl;
            authinfo.keepPassword = true;
            authinfo.comment = i18n("<b>%1</b> at <b>%2</b>", authinfo.realmValue.toHtmlEscaped(), authinfo.url.host());

            const int errorCode = openPasswordDialog(authinfo, QString());

            if (!errorCode) {
                authenticator->setUser(authinfo.username);
                authenticator->setPassword(authinfo.password);
                if (authinfo.keepPassword) {
                    cacheAuthentication(authinfo);
                }
            }
        }
    });

    QNetworkRequest request(properUrl);

    const QByteArray contentType = getContentType().toUtf8();

    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }

    const QString referrer = metaData(QStringLiteral("referrer"));
    if (!referrer.isEmpty()) {
        request.setRawHeader("Referer" /* sic! */, referrer.toUtf8());
    }

    QString userAgent = metaData(QStringLiteral("UserAgent"));
    if (userAgent.isEmpty()) {
        userAgent = defaultUserAgent();
    }
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent.toUtf8());

    const QString accept = metaData(QStringLiteral("accept"));
    if (!accept.isEmpty()) {
        request.setRawHeader("Accept", accept.toUtf8());
    }

    if (metaData(QStringLiteral("HttpVersion")) == QStringLiteral("http1")) {
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    }

    for (auto [key, value] : extraHeaders.asKeyValueRange()) {
        request.setRawHeader(key, value);
    }

    const QString customHeaders = metaData(QStringLiteral("customHTTPHeader"));
    if (!customHeaders.isEmpty()) {
        const QStringList headers = customHeaders.split(QLatin1String("\r\n"));

        for (const QString &header : headers) {
            const QStringList split = header.split(QLatin1String(": "));
            Q_ASSERT(split.size() == 2);

            request.setRawHeader(split[0].toUtf8(), split[1].toUtf8());
        }
    }

    if (inputData) {
        inputData->startTransaction(); // To be able to restart after redirects.
    }

    QNetworkReply *reply = nam.sendCustomRequest(request, methodToString(method), inputData);
    const auto replyDeleter = qScopeGuard([reply] {
        reply->deleteLater();
    });

    bool mimeTypeEmitted = false;

    QEventLoop loop;

    QObject::connect(reply, &QNetworkReply::sslErrors, &loop, [this, reply](const QList<QSslError> errors) {
        handleSslErrors(reply, errors);
    });

    qint64 lastTotalSize = -1;

    QObject::connect(reply, &QNetworkReply::downloadProgress, this, [this, &lastTotalSize](qint64 received, qint64 total) {
        if (total != lastTotalSize) {
            lastTotalSize = total;
            totalSize(total);
        }

        processedSize(received);
    });

    // From RFC 4918 5.2 Collection Resources:
    // > In general, clients SHOULD use the trailing slash form of collection names.
    // > If clients do not use the trailing slash form the client needs to be prepared to see a redirect response.
    // KIO doesn't handle trailing slashes well (especially in KDirLister), so handle it transparently.
    bool redirectToTrailingSlash = false;

    QObject::connect(reply, &QNetworkReply::metaDataChanged, [this, &mimeTypeEmitted, &redirectToTrailingSlash, reply, dataMode, url, method]() {
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (statusCode >= 300 && statusCode < 400) {
            const QString redir = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
            QUrl newUrl = url.resolved(QUrl(redir));

            // Restore the old protocol (http, webdav or dav) after the redirection, but with the correct encryption status
            bool upgradeToSecure = !url.scheme().endsWith(QLatin1Char('s')) && newUrl.scheme().endsWith(QLatin1Char('s'));
            bool downgradeToInsecure = url.scheme().endsWith(QLatin1Char('s')) && !newUrl.scheme().endsWith(QLatin1Char('s'));
            if (upgradeToSecure)
                newUrl.setScheme(url.scheme() + QLatin1Char('s'));
            else if (downgradeToInsecure)
                newUrl.setScheme(url.scheme().chopped(1));
            else
                newUrl.setScheme(url.scheme());

            // Handled after returning from the event loop.
            // 301 is necessary for Apache (see bugs 209508 and 187970).
            if (statusCode == 301 || statusCode == 307 || statusCode == 308) {
                if (url != newUrl && url == newUrl.adjusted(QUrl::StripTrailingSlash)) {
                    redirectToTrailingSlash = true;
                    return;
                }
            }

            if (statusCode == 301 || statusCode == 308) {
                setMetaData(QStringLiteral("permanent-redirect"), QStringLiteral("true"));
                redirection(newUrl);
            } else if (statusCode == 302) {
                if (method == KIO::HTTP_POST) {
                    setMetaData(QStringLiteral("redirect-to-get"), QStringLiteral("true"));
                }

                redirection(newUrl);
            } else if (statusCode == 303) {
                if (method != KIO::HTTP_HEAD) {
                    setMetaData(QStringLiteral("redirect-to-get"), QStringLiteral("true"));
                }

                redirection(newUrl);
            } else if (statusCode == 307) {
                redirection(newUrl);
            }
        } else if (statusCode == 206) {
            canResume();
        }

        if (!mimeTypeEmitted) {
            mimeType(readMimeType(reply));
            mimeTypeEmitted = true;
        }

        if (dataMode == Emit) {
            // Limit how much data we fetch at a time to avoid storing it all in RAM
            // do it in metaDataChanged to work around https://bugreports.qt.io/browse/QTBUG-15065
            reply->setReadBufferSize(2048);
        }
    });

    if (dataMode == Emit) {
        QObject::connect(reply, &QNetworkReply::readyRead, &nam, [this, reply] {
            while (reply->bytesAvailable() > 0) {
                QByteArray buf(2048, Qt::Uninitialized);
                qint64 readBytes = reply->read(buf.data(), 2048);
                if (readBytes == 0) {
                    // End of data => don't emit the final data() call yet, the reply metadata is not yet complete!
                    break;
                }
                buf.truncate(readBytes);
                data(buf);
            }
        });
    }

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(this, &HTTPProtocol::errorOut, &loop, [this, &loop](KIO::Error error) {
        lastError = error;
        loop.quit();
    });
    loop.exec();

    // If there was a foo -> foo/ redirect, follow it.
    if (redirectToTrailingSlash) {
        if (inputData) {
            inputData->rollbackTransaction();
        }
        QUrl newUrl = url;
        newUrl.setPath(newUrl.path() + QLatin1Char('/'));
        return makeRequest(newUrl, method, inputData, dataMode, extraHeaders);
    }
    if (inputData) {
        inputData->commitTransaction();
    }

    // make sure data is emitted at least once
    // NOTE: emitting an empty data set means "end of data" and must not happen
    // before we have set up our metadata properties etc. Only emit this at the
    // very end of the function if applicable.
    auto emitDataOnce = qScopeGuard([this] {
        data(QByteArray());
    });

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError && statusCode == 0) {
        // anything not listed here gets KIO::ERR_CANNOT_CONNECT
        static constexpr const std::pair<QNetworkReply::NetworkError, int> qnam2kio_errors[] = {
            {QNetworkReply::HostNotFoundError, KIO::ERR_UNKNOWN_HOST},
            {QNetworkReply::TimeoutError, KIO::ERR_SERVER_TIMEOUT},
            {QNetworkReply::OperationCanceledError, KIO::ERR_USER_CANCELED},
            {QNetworkReply::TemporaryNetworkFailureError, KIO::ERR_CONNECTION_BROKEN},
            {QNetworkReply::NetworkSessionFailedError, KIO::ERR_CONNECTION_BROKEN},
            {QNetworkReply::ProxyConnectionClosedError, KIO::ERR_CONNECTION_BROKEN},
            {QNetworkReply::ProxyNotFoundError, KIO::ERR_UNKNOWN_PROXY_HOST},
            {QNetworkReply::ProxyTimeoutError, KIO::ERR_SERVER_TIMEOUT},
            {QNetworkReply::ProtocolUnknownError, KIO::ERR_UNSUPPORTED_PROTOCOL},
        };

        const auto it = std::ranges::find_if(qnam2kio_errors, [reply](const auto &m) {
            return m.first == reply->error();
        });

        return {0, QByteArray(), it != std::end(qnam2kio_errors) ? (*it).second : KIO::ERR_CANNOT_CONNECT};
    }
    if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
        return {0, QByteArray(), KIO::ERR_ACCESS_DENIED};
    }

    if (configValue(QStringLiteral("PropagateHttpHeader"), false)) {
        QStringList headers;

        const auto headerPairs = reply->rawHeaderPairs();
        for (auto [key, value] : headerPairs) {
            headers << QString::fromLatin1(key + ": " + value);
        }

        setMetaData(QStringLiteral("HTTP-Headers"), headers.join(QLatin1Char('\n')));
    }

    QByteArray returnData;

    if (dataMode == Return) {
        returnData = reply->readAll();
    }

    setMetaData(QStringLiteral("responsecode"), QString::number(statusCode));
    setMetaData(QStringLiteral("content-type"), readMimeType(reply));

    return {statusCode, returnData};
}

KIO::WorkerResult HTTPProtocol::get(const QUrl &url)
{
    QByteArray inputData = getData();

    QString start = metaData(QStringLiteral("range-start"));

    if (start.isEmpty()) {
        // old name
        start = metaData(QStringLiteral("resume"));
    }

    QMap<QByteArray, QByteArray> headers;

    if (!start.isEmpty()) {
        headers.insert("Range", "bytes=" + start.toUtf8() + "-");
    }

    Response response = makeRequest(url, KIO::HTTP_GET, inputData, DataMode::Emit, headers);

    return sendHttpError(url, KIO::HTTP_GET, response);
}

KIO::WorkerResult HTTPProtocol::put(const QUrl &url, int /*_mode*/, KIO::JobFlags flags)
{
    if (isDav(url.scheme())) {
        if (!(flags & KIO::Overwrite)) {
            // Checks if the destination exists and return an error if it does.
            if (davDestinationExists(url)) {
                return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, url.fileName());
            }
        }
    }

    QByteArray inputData = getData();
    Response response = makeRequest(url, KIO::HTTP_PUT, inputData, DataMode::Emit);

    return sendHttpError(url, KIO::HTTP_PUT, response);
}

KIO::WorkerResult HTTPProtocol::mimetype(const QUrl &url)
{
    QByteArray inputData = getData();
    Response response = makeRequest(url, KIO::HTTP_HEAD, inputData, DataMode::Discard);

    return sendHttpError(url, KIO::HTTP_HEAD, response);
}

KIO::WorkerResult HTTPProtocol::post(const QUrl &url, qint64 /*size*/)
{
    QByteArray inputData = getData();
    Response response = makeRequest(url, KIO::HTTP_POST, inputData, DataMode::Emit);

    return sendHttpError(url, KIO::HTTP_POST, response);
}

KIO::WorkerResult HTTPProtocol::special(const QByteArray &data)
{
    int tmp;
    QDataStream stream(data);

    stream >> tmp;
    switch (tmp) {
    case 1: { // HTTP POST
        QUrl url;
        qint64 size;
        stream >> url >> size;
        return post(url, size);
    }
    case 7: { // Generic WebDAV
        QUrl url;
        int method;
        qint64 size;
        stream >> url >> method >> size;
        return davGeneric(url, (KIO::HTTP_METHOD)method, size);
    }
    }
    return KIO::WorkerResult::pass();
}

QByteArray HTTPProtocol::getData()
{
    // TODO this is probably not great. Instead create a QIODevice that calls readData and pass that to QNAM?
    QByteArray dataBuffer;

    while (true) {
        dataReq();

        QByteArray buffer;
        const int bytesRead = readData(buffer);

        dataBuffer += buffer;

        // On done...
        if (bytesRead == 0) {
            // sendOk = (bytesSent == m_iPostDataSize);
            break;
        }
    }

    return dataBuffer;
}

QString HTTPProtocol::getContentType()
{
    QString contentType = metaData(QStringLiteral("content-type"));
    if (contentType.startsWith(QLatin1String("Content-Type: "), Qt::CaseInsensitive)) {
        contentType.remove(QLatin1String("Content-Type: "), Qt::CaseInsensitive);
    }
    return contentType;
}

KIO::WorkerResult HTTPProtocol::listDir(const QUrl &url)
{
    return davStatList(url, false);
}

KIO::WorkerResult HTTPProtocol::davStatList(const QUrl &url, bool stat)
{
    KIO::UDSEntry entry;

    QMimeDatabase db;

    KIO::HTTP_METHOD method;
    QByteArray inputData;

    // Maybe it's a disguised SEARCH...
    QString query = metaData(QStringLiteral("davSearchQuery"));
    if (!query.isEmpty()) {
        inputData =
            "<?xml version=\"1.0\"?>\r\n"
            "<D:searchrequest xmlns:D=\"DAV:\">\r\n"
            + query.toUtf8() + "</D:searchrequest>\r\n";

        method = KIO::DAV_SEARCH;
    } else {
        // We are only after certain features...
        inputData =
            "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
            "<D:propfind xmlns:D=\"DAV:\">"
            "<D:prop>"
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
            "<D:quota-available-bytes/>"
            "<D:quota-used-bytes/>"
            "</D:prop>"
            "</D:propfind>";
        method = KIO::DAV_PROPFIND;
    }

    const QMap<QByteArray, QByteArray> extraHeaders = {
        {"Depth", stat ? "0" : "1"},
    };

    Response response = makeDavRequest(url, method, inputData, DataMode::Return, extraHeaders);

    // If this is a redirection we don't have anything to do
    if (response.httpCode >= 300 && response.httpCode < 400)
        return KIO::WorkerResult::pass();

    QDomDocument multiResponse;
    multiResponse.setContent(response.data, QDomDocument::ParseOption::UseNamespaceProcessing);

    bool hasResponse = false;

    for (QDomNode n = multiResponse.documentElement().firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement thisResponse = n.toElement();
        if (thisResponse.isNull()) {
            continue;
        }

        hasResponse = true;

        QDomElement href = thisResponse.namedItem(QStringLiteral("href")).toElement();
        if (!href.isNull()) {
            entry.clear();

            const QUrl thisURL(href.text()); // href.text() is a percent-encoded url.
            if (thisURL.isValid()) {
                const QUrl adjustedThisURL = thisURL.adjusted(QUrl::StripTrailingSlash);
                const QUrl adjustedUrl = url.adjusted(QUrl::StripTrailingSlash);

                // base dir of a listDir(): name should be "."
                QString name;
                if (!stat && adjustedThisURL.path() == adjustedUrl.path()) {
                    name = QLatin1Char('.');
                } else {
                    name = adjustedThisURL.fileName();
                }

                entry.fastInsert(KIO::UDSEntry::UDS_NAME, name.isEmpty() ? href.text() : name);
            }

            QDomNodeList propstats = thisResponse.elementsByTagName(QStringLiteral("propstat"));

            davParsePropstats(propstats, entry);

            // Since a lot of webdav servers seem not to send the content-type information
            // for the requested directory listings, we attempt to guess the MIME type from
            // the resource name so long as the resource is not a directory.
            if (entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE).isEmpty() && entry.numberValue(KIO::UDSEntry::UDS_FILE_TYPE) != S_IFDIR) {
                QMimeType mime = db.mimeTypeForFile(thisURL.path(), QMimeDatabase::MatchExtension);
                if (mime.isValid() && !mime.isDefault()) {
                    // qCDebug(KIO_HTTP) << "Setting" << mime.name() << "as guessed MIME type for" << thisURL.path();
                    entry.fastInsert(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE, mime.name());
                }
            }

            if (stat) {
                // return an item
                statEntry(entry);
                return KIO::WorkerResult::pass();
            }
            listEntry(entry);
        } else {
            // qCDebug(KIO_HTTP) << "Error: no URL contained in response to PROPFIND on" << url;
        }
    }

    if (stat || !hasResponse) {
        return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }

    return KIO::WorkerResult::pass();
}

void HTTPProtocol::davParsePropstats(const QDomNodeList &propstats, KIO::UDSEntry &entry)
{
    QString mimeType;
    bool foundExecutable = false;
    bool isDirectory = false;
    uint lockCount = 0;
    uint supportedLockCount = 0;
    qlonglong quotaUsed = -1;
    qlonglong quotaAvailable = -1;

    for (int i = 0; i < propstats.count(); i++) {
        QDomElement propstat = propstats.item(i).toElement();

        QDomElement status = propstat.namedItem(QStringLiteral("status")).toElement();
        if (status.isNull()) {
            // error, no status code in this propstat
            // qCDebug(KIO_HTTP) << "Error, no status code in this propstat";
            return;
        }

        int code = codeFromResponse(status.text());

        if (code != 200) {
            // qCDebug(KIO_HTTP) << "Got status code" << code << "(this may mean that some properties are unavailable)";
            continue;
        }

        QDomElement prop = propstat.namedItem(QStringLiteral("prop")).toElement();
        if (prop.isNull()) {
            // qCDebug(KIO_HTTP) << "Error: no prop segment in this propstat.";
            return;
        }

        // TODO unnecessary?
        if (hasMetaData(QStringLiteral("davRequestResponse"))) {
            QDomDocument doc;
            doc.appendChild(prop);
            entry.replace(KIO::UDSEntry::UDS_XML_PROPERTIES, doc.toString());
        }

        for (QDomNode n = prop.firstChild(); !n.isNull(); n = n.nextSibling()) {
            QDomElement property = n.toElement();
            if (property.isNull()) {
                continue;
            }

            if (property.namespaceURI() != QLatin1String("DAV:")) {
                // break out - we're only interested in properties from the DAV namespace
                continue;
            }

            if (property.tagName() == QLatin1String("creationdate")) {
                // Resource creation date. Should be is ISO 8601 format.
                auto datetime = parseDateTime(property.text(), property.attribute(QStringLiteral("dt")));
                if (datetime.isValid()) {
                    entry.replace(KIO::UDSEntry::UDS_CREATION_TIME, datetime.toSecsSinceEpoch());
                } else {
                    qWarning() << "Failed to parse creationdate" << property.text() << property.attribute(QStringLiteral("dt"));
                }
            } else if (property.tagName() == QLatin1String("getcontentlength")) {
                // Content length (file size)
                entry.replace(KIO::UDSEntry::UDS_SIZE, property.text().toULong());
            } else if (property.tagName() == QLatin1String("displayname")) {
                // Name suitable for presentation to the user
                setMetaData(QStringLiteral("davDisplayName"), property.text());
            } else if (property.tagName() == QLatin1String("source")) {
                // Source template location
                QDomElement source = property.namedItem(QStringLiteral("link")).toElement().namedItem(QStringLiteral("dst")).toElement();
                if (!source.isNull()) {
                    setMetaData(QStringLiteral("davSource"), source.text());
                }
            } else if (property.tagName() == QLatin1String("getcontentlanguage")) {
                // equiv. to Content-Language header on a GET
                setMetaData(QStringLiteral("davContentLanguage"), property.text());
            } else if (property.tagName() == QLatin1String("getcontenttype")) {
                // Content type (MIME type)
                // This may require adjustments for other server-side webdav implementations
                // (tested with Apache + mod_dav 1.0.3)
                if (property.text() == QLatin1String("httpd/unix-directory")) {
                    isDirectory = true;
                } else if (property.text() != QLatin1String("application/octet-stream")) {
                    // The server could be lazy and always return application/octet-stream;
                    // we will guess the MIME type later in that case.
                    mimeType = property.text();
                }
            } else if (property.tagName() == QLatin1String("executable")) {
                // File executable status
                if (property.text() == QLatin1Char('T')) {
                    foundExecutable = true;
                }

            } else if (property.tagName() == QLatin1String("getlastmodified")) {
                // Last modification date
                auto datetime = parseDateTime(property.text(), property.attribute(QStringLiteral("dt")));
                if (datetime.isValid()) {
                    entry.replace(KIO::UDSEntry::UDS_MODIFICATION_TIME, datetime.toSecsSinceEpoch());
                } else {
                    qWarning() << "Failed to parse getlastmodified" << property.text() << property.attribute(QStringLiteral("dt"));
                }
            } else if (property.tagName() == QLatin1String("getetag")) {
                // Entity tag
                setMetaData(QStringLiteral("davEntityTag"), property.text());
            } else if (property.tagName() == QLatin1String("supportedlock")) {
                // Supported locking specifications
                for (QDomNode n2 = property.firstChild(); !n2.isNull(); n2 = n2.nextSibling()) {
                    QDomElement lockEntry = n2.toElement();
                    if (lockEntry.tagName() == QLatin1String("lockentry")) {
                        QDomElement lockScope = lockEntry.namedItem(QStringLiteral("lockscope")).toElement();
                        QDomElement lockType = lockEntry.namedItem(QStringLiteral("locktype")).toElement();
                        if (!lockScope.isNull() && !lockType.isNull()) {
                            // Lock type was properly specified
                            supportedLockCount++;
                            const QString lockCountStr = QString::number(supportedLockCount);
                            const QString scope = lockScope.firstChild().toElement().tagName();
                            const QString type = lockType.firstChild().toElement().tagName();

                            setMetaData(QLatin1String("davSupportedLockScope") + lockCountStr, scope);
                            setMetaData(QLatin1String("davSupportedLockType") + lockCountStr, type);
                        }
                    }
                }
            } else if (property.tagName() == QLatin1String("lockdiscovery")) {
                // Lists the available locks
                davParseActiveLocks(property.elementsByTagName(QStringLiteral("activelock")), lockCount);
            } else if (property.tagName() == QLatin1String("resourcetype")) {
                // Resource type. "Specifies the nature of the resource."
                if (!property.namedItem(QStringLiteral("collection")).toElement().isNull()) {
                    // This is a collection (directory)
                    isDirectory = true;
                }
            } else if (property.tagName() == QLatin1String("quota-used-bytes")) {
                // Quota-used-bytes. "Contains the amount of storage already in use."
                bool ok;
                qlonglong used = property.text().toLongLong(&ok);
                if (ok) {
                    quotaUsed = used;
                }
            } else if (property.tagName() == QLatin1String("quota-available-bytes")) {
                // Quota-available-bytes. "Indicates the maximum amount of additional storage available."
                bool ok;
                qlonglong available = property.text().toLongLong(&ok);
                if (ok) {
                    quotaAvailable = available;
                }
            } else {
                // qCDebug(KIO_HTTP) << "Found unknown webdav property:" << property.tagName();
            }
        }
    }

    setMetaData(QStringLiteral("davLockCount"), QString::number(lockCount));
    setMetaData(QStringLiteral("davSupportedLockCount"), QString::number(supportedLockCount));

    entry.replace(KIO::UDSEntry::UDS_FILE_TYPE, isDirectory ? S_IFDIR : S_IFREG);

    if (foundExecutable || isDirectory) {
        // File was executable, or is a directory.
        entry.replace(KIO::UDSEntry::UDS_ACCESS, 0700);
    } else {
        entry.replace(KIO::UDSEntry::UDS_ACCESS, 0600);
    }

    if (!isDirectory && !mimeType.isEmpty()) {
        entry.replace(KIO::UDSEntry::UDS_MIME_TYPE, mimeType);
    }

    if (quotaUsed >= 0 && quotaAvailable >= 0) {
        // Only used and available storage properties exist, the total storage size has to be calculated.
        setMetaData(QStringLiteral("total"), QString::number(quotaUsed + quotaAvailable));
        setMetaData(QStringLiteral("available"), QString::number(quotaAvailable));
    }
}

void HTTPProtocol::davParseActiveLocks(const QDomNodeList &activeLocks, uint &lockCount)
{
    for (int i = 0; i < activeLocks.count(); i++) {
        const QDomElement activeLock = activeLocks.item(i).toElement();

        lockCount++;
        // required
        const QDomElement lockScope = activeLock.namedItem(QStringLiteral("lockscope")).toElement();
        const QDomElement lockType = activeLock.namedItem(QStringLiteral("locktype")).toElement();
        const QDomElement lockDepth = activeLock.namedItem(QStringLiteral("depth")).toElement();
        // optional
        const QDomElement lockOwner = activeLock.namedItem(QStringLiteral("owner")).toElement();
        const QDomElement lockTimeout = activeLock.namedItem(QStringLiteral("timeout")).toElement();
        const QDomElement lockToken = activeLock.namedItem(QStringLiteral("locktoken")).toElement();

        if (!lockScope.isNull() && !lockType.isNull() && !lockDepth.isNull()) {
            // lock was properly specified
            lockCount++;
            const QString lockCountStr = QString::number(lockCount);
            const QString scope = lockScope.firstChild().toElement().tagName();
            const QString type = lockType.firstChild().toElement().tagName();
            const QString depth = lockDepth.text();

            setMetaData(QLatin1String("davLockScope") + lockCountStr, scope);
            setMetaData(QLatin1String("davLockType") + lockCountStr, type);
            setMetaData(QLatin1String("davLockDepth") + lockCountStr, depth);

            if (!lockOwner.isNull()) {
                setMetaData(QLatin1String("davLockOwner") + lockCountStr, lockOwner.text());
            }

            if (!lockTimeout.isNull()) {
                setMetaData(QLatin1String("davLockTimeout") + lockCountStr, lockTimeout.text());
            }

            if (!lockToken.isNull()) {
                QDomElement tokenVal = lockScope.namedItem(QStringLiteral("href")).toElement();
                if (!tokenVal.isNull()) {
                    setMetaData(QLatin1String("davLockToken") + lockCountStr, tokenVal.text());
                }
            }
        }
    }
}

QDateTime HTTPProtocol::parseDateTime(const QString &input, const QString &type)
{
    if (type == QLatin1String("dateTime.tz")) {
        return QDateTime::fromString(input, Qt::ISODate);
    }

    // Qt decided to no longer support "GMT" for some reason: QTBUG-114681
    QString inputUtc = input;
    inputUtc.replace(QLatin1String("GMT"), QLatin1String("+0000"));

    if (type == QLatin1String("dateTime.rfc1123")) {
        return QDateTime::fromString(inputUtc, Qt::RFC2822Date);
    }

    // format not advertised... try to parse anyway
    QDateTime time = QDateTime::fromString(inputUtc, Qt::RFC2822Date);
    if (time.isValid()) {
        return time;
    }

    return QDateTime::fromString(input, Qt::ISODate);
}

int HTTPProtocol::codeFromResponse(const QString &response)
{
    const int firstSpace = response.indexOf(QLatin1Char(' '));
    const int secondSpace = response.indexOf(QLatin1Char(' '), firstSpace + 1);

    return QStringView(response).mid(firstSpace + 1, secondSpace - firstSpace - 1).toInt();
}

KIO::WorkerResult HTTPProtocol::stat(const QUrl &url)
{
    if (!isDav(url.scheme())) {
        QString statSide = metaData(QStringLiteral("statSide"));
        if (statSide != QLatin1String("source")) {
            // When uploading we assume the file does not exist.
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }

        // When downloading we assume it exists
        KIO::UDSEntry entry;
        entry.reserve(3);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, url.fileName());
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG); // a file
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH); // readable by everybody

        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    return davStatList(url, true);
}

KIO::WorkerResult HTTPProtocol::mkdir(const QUrl &url, int)
{
    QByteArray inputData;
    Response response = makeDavRequest(url, KIO::DAV_MKCOL, inputData, DataMode::Discard);

    if (response.httpCode != 201) {
        return davError(KIO::DAV_MKCOL, url, response);
    }
    return KIO::WorkerResult::pass();
}

KIO::WorkerResult HTTPProtocol::rename(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    QMap<QByteArray, QByteArray> extraHeaders = {
        {"Destination", dest.toString(QUrl::FullyEncoded).toUtf8()},
        {"Overwrite", (flags & KIO::Overwrite) ? "T" : "F"},
        {"Depth", "infinity"},
    };

    QByteArray inputData;
    Response response = makeDavRequest(src, KIO::DAV_MOVE, inputData, DataMode::Discard, extraHeaders);

    // The server returns a HTTP/1.1 201 Created or 204 No Content on successful completion
    if (response.httpCode == 201 || response.httpCode == 204) {
        return KIO::WorkerResult::pass();
    }
    return davError(KIO::DAV_MOVE, src, response);
}

KIO::WorkerResult HTTPProtocol::copy(const QUrl &src, const QUrl &dest, int, KIO::JobFlags flags)
{
    const bool isSourceLocal = src.isLocalFile();
    const bool isDestinationLocal = dest.isLocalFile();

    if (isSourceLocal && !isDestinationLocal) {
        return copyPut(src, dest, flags);
    }

    if (!(flags & KIO::Overwrite)) {
        // Checks if the destination exists and return an error if it does.
        if (davDestinationExists(dest)) {
            return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest.fileName());
        }
    }

    QMap<QByteArray, QByteArray> extraHeaders = {
        {"Destination", dest.toString(QUrl::FullyEncoded).toUtf8()},
        {"Overwrite", (flags & KIO::Overwrite) ? "T" : "F"},
        {"Depth", "infinity"},
    };

    QByteArray inputData;
    Response response = makeDavRequest(src, KIO::DAV_COPY, inputData, DataMode::Discard, extraHeaders);

    // The server returns a HTTP/1.1 201 Created or 204 No Content on successful completion
    if (response.httpCode == 201 || response.httpCode == 204) {
        return KIO::WorkerResult::pass();
    }

    return davError(KIO::DAV_COPY, src, response);
}

KIO::WorkerResult HTTPProtocol::del(const QUrl &url, bool)
{
    if (isDav(url.scheme())) {
        Response response = makeRequest(url, KIO::HTTP_DELETE, {}, DataMode::Discard);

        // The server returns a HTTP/1.1 200 Ok or HTTP/1.1 204 No Content
        // on successful completion.
        if (response.httpCode == 200 || response.httpCode == 204 /*|| m_isRedirection*/) {
            return KIO::WorkerResult::pass();
        }
        return davError(KIO::HTTP_DELETE, url, response);
    }

    Response response = makeRequest(url, KIO::HTTP_DELETE, {}, DataMode::Discard);

    return sendHttpError(url, KIO::HTTP_DELETE, response);
}

KIO::WorkerResult HTTPProtocol::copyPut(const QUrl &src, const QUrl &dest, KIO::JobFlags flags)
{
    if (!(flags & KIO::Overwrite)) {
        // Checks if the destination exists and return an error if it does.
        if (davDestinationExists(dest)) {
            return KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, dest.fileName());
        }
    }

    auto sourceFile = new QFile(src.toLocalFile());
    if (!sourceFile->open(QFile::ReadOnly)) {
        return KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, src.fileName());
    }

    Response response = makeRequest(dest, KIO::HTTP_PUT, sourceFile, {});

    return sendHttpError(dest, KIO::HTTP_PUT, response);
}

bool HTTPProtocol::davDestinationExists(const QUrl &url)
{
    QByteArray request(
        "<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
        "<D:propfind xmlns:D=\"DAV:\"><D:prop>"
        "<D:creationdate/>"
        "<D:getcontentlength/>"
        "<D:displayname/>"
        "<D:resourcetype/>"
        "</D:prop></D:propfind>");

    const QMap<QByteArray, QByteArray> extraHeaders = {
        {"Depth", "0"},
    };

    Response response = makeDavRequest(url, KIO::DAV_PROPFIND, request, DataMode::Discard, extraHeaders);

    if (response.httpCode >= 200 && response.httpCode < 300) {
        // 2XX means the file exists. This includes 207 (multi-status response).
        // qCDebug(KIO_HTTP) << "davDestinationExists: file exists. code:" << m_request.responseCode;
        return true;
    } else {
        // qCDebug(KIO_HTTP) << "davDestinationExists: file does not exist. code:" << m_request.responseCode;
    }

    return false;
}

KIO::WorkerResult HTTPProtocol::davGeneric(const QUrl &url, KIO::HTTP_METHOD method, qint64 size)
{
    // TODO what about size?
    Q_UNUSED(size)
    QMap<QByteArray, QByteArray> extraHeaders;

    if (method == KIO::DAV_PROPFIND || method == KIO::DAV_REPORT) {
        int depth = 0;

        if (hasMetaData(QStringLiteral("davDepth"))) {
            depth = metaData(QStringLiteral("davDepth")).toInt();
        } else {
            // TODO is warning here appropriate?
            qWarning() << "Performing DAV PROPFIND or REPORT without specifying davDepth";
        }

        extraHeaders.insert("Depth", QByteArray::number(depth));
    }

    QByteArray inputData = getData();
    Response response = makeDavRequest(url, method, inputData, DataMode::Emit, extraHeaders);

    // TODO old code seems to use http error, not dav error
    return sendHttpError(url, method, response);
}

KIO::WorkerResult HTTPProtocol::fileSystemFreeSpace(const QUrl &url)
{
    return davStatList(url, true);
}

KIO::WorkerResult HTTPProtocol::davError(KIO::HTTP_METHOD method, const QUrl &url, const Response &response)
{
    if (response.kioCode == KIO::ERR_ACCESS_DENIED) {
        return KIO::WorkerResult::fail(response.kioCode, url.toDisplayString());
    }

    QString discard;
    return davError(discard, method, response.httpCode, url, response.data);
}

QString HTTPProtocol::davProcessLocks()
{
    if (hasMetaData(QStringLiteral("davLockCount"))) {
        QString response;
        int numLocks = metaData(QStringLiteral("davLockCount")).toInt();
        bool bracketsOpen = false;
        for (int i = 0; i < numLocks; i++) {
            const QString countStr = QString::number(i);
            if (hasMetaData(QLatin1String("davLockToken") + countStr)) {
                if (hasMetaData(QLatin1String("davLockURL") + countStr)) {
                    if (bracketsOpen) {
                        response += QLatin1Char(')');
                        bracketsOpen = false;
                    }
                    response += QLatin1String(" <") + metaData(QLatin1String("davLockURL") + countStr) + QLatin1Char('>');
                }

                if (!bracketsOpen) {
                    response += QLatin1String(" (");
                    bracketsOpen = true;
                } else {
                    response += QLatin1Char(' ');
                }

                if (hasMetaData(QLatin1String("davLockNot") + countStr)) {
                    response += QLatin1String("Not ");
                }

                response += QLatin1Char('<') + metaData(QLatin1String("davLockToken") + countStr) + QLatin1Char('>');
            }
        }

        if (bracketsOpen) {
            response += QLatin1Char(')');
        }

        return response;
    }

    return QString();
}

KIO::WorkerResult HTTPProtocol::davError(QString &errorMsg, KIO::HTTP_METHOD method, int code, const QUrl &url, const QByteArray &responseData)
{
    QString action;
    QString errorString;
    int errorCode = KIO::ERR_WORKER_DEFINED;

    // for 412 Precondition Failed
    QString ow = i18n("Otherwise, the request would have succeeded.");

    if (method == KIO::DAV_PROPFIND) {
        action = i18nc("request type", "retrieve property values");
    } else if (method == KIO::DAV_PROPPATCH) {
        action = i18nc("request type", "set property values");
    } else if (method == KIO::DAV_MKCOL) {
        action = i18nc("request type", "create the requested folder");
    } else if (method == KIO::DAV_COPY) {
        action = i18nc("request type", "copy the specified file or folder");
    } else if (method == KIO::DAV_MOVE) {
        action = i18nc("request type", "move the specified file or folder");
    } else if (method == KIO::DAV_SEARCH) {
        action = i18nc("request type", "search in the specified folder");
    } else if (method == KIO::DAV_LOCK) {
        action = i18nc("request type", "lock the specified file or folder");
    } else if (method == KIO::DAV_UNLOCK) {
        action = i18nc("request type", "unlock the specified file or folder");
    } else if (method == KIO::HTTP_DELETE) {
        action = i18nc("request type", "delete the specified file or folder");
    } else if (method == KIO::HTTP_OPTIONS) {
        action = i18nc("request type", "query the server's capabilities");
    } else if (method == KIO::HTTP_GET) {
        action = i18nc("request type", "retrieve the contents of the specified file or folder");
    } else if (method == KIO::DAV_REPORT) {
        action = i18nc("request type", "run a report in the specified folder");
    } else {
        // this should not happen, this function is for webdav errors only
        Q_ASSERT(0);
    }

    // default error message if the following code fails
    errorString = i18nc("%1: code, %2: request type",
                        "An unexpected error (%1) occurred "
                        "while attempting to %2.",
                        code,
                        action);

    switch (code) {
    case 207:
        // 207 Multi-status
        {
            // our error info is in the returned XML document.
            // retrieve the XML document

            QStringList errors;
            QDomDocument multiResponse;
            multiResponse.setContent(responseData, QDomDocument::ParseOption::UseNamespaceProcessing);

            QDomElement multistatus = multiResponse.documentElement().namedItem(QStringLiteral("multistatus")).toElement();

            QDomNodeList responses = multistatus.elementsByTagName(QStringLiteral("response"));

            for (int i = 0; i < responses.count(); i++) {
                int errCode;
                QUrl errUrl;

                QDomElement response = responses.item(i).toElement();
                QDomElement code = response.namedItem(QStringLiteral("status")).toElement();

                if (!code.isNull()) {
                    errCode = codeFromResponse(code.text());
                    QDomElement href = response.namedItem(QStringLiteral("href")).toElement();
                    if (!href.isNull()) {
                        errUrl = QUrl(href.text());
                    }
                    QString error;
                    (void)davError(error, method, errCode, errUrl, {});
                    errors << error;
                }
            }

            errorString = i18nc("%1: request type, %2: url",
                                "An error occurred while attempting to %1, %2. A "
                                "summary of the reasons is below.",
                                action,
                                url.toString());

            errorString += QLatin1String("<ul>");

            for (const QString &error : std::as_const(errors)) {
                errorString += QLatin1String("<li>") + error + QLatin1String("</li>");
            }

            errorString += QLatin1String("</ul>");
        }
        break;
    case 403:
    case 500: // hack: Apache mod_dav returns this instead of 403 (!)
        // 403 Forbidden
        // ERR_ACCESS_DENIED
        errorString = i18nc("%1: request type", "Access was denied while attempting to %1.", action);
        break;
    case 405:
        // 405 Method Not Allowed
        if (method == KIO::DAV_MKCOL) {
            // ERR_DIR_ALREADY_EXIST
            errorString = url.toString();
            errorCode = KIO::ERR_DIR_ALREADY_EXIST;
        }
        break;
    case 409:
        // 409 Conflict
        // ERR_ACCESS_DENIED
        errorString = i18n(
            "A resource cannot be created at the destination "
            "until one or more intermediate collections (folders) "
            "have been created.");
        break;
    case 412:
        // 412 Precondition failed
        if (method == KIO::DAV_COPY || method == KIO::DAV_MOVE) {
            // ERR_ACCESS_DENIED
            errorString = i18n(
                "The server was unable to maintain the liveness of the\n"
                "properties listed in the propertybehavior XML element\n"
                "or you attempted to overwrite a file while requesting\n"
                "that files are not overwritten.\n %1",
                ow);

        } else if (method == KIO::DAV_LOCK) {
            // ERR_ACCESS_DENIED
            errorString = i18n("The requested lock could not be granted. %1", ow);
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
        errorString = i18nc("%1: request type", "Unable to %1 because the resource is locked.", action);
        break;
    case 425:
        // 424 Failed Dependency
        errorString = i18n("This action was prevented by another error.");
        break;
    case 502:
        // 502 Bad Gateway
        if (method == KIO::DAV_COPY || method == KIO::DAV_MOVE) {
            // ERR_WRITE_ACCESS_DENIED
            errorString = i18nc("%1: request type",
                                "Unable to %1 because the destination server refuses "
                                "to accept the file or folder.",
                                action);
        }
        break;
    case 507:
        // 507 Insufficient Storage
        // ERR_DISK_FULL
        errorString = i18n(
            "The destination resource does not have sufficient space "
            "to record the state of the resource after the execution "
            "of this method.");
        break;
    default:
        break;
    }

    errorMsg = errorString;
    return KIO::WorkerResult::fail(errorCode, errorString);
}

// HTTP generic error
static int httpGenericError(int responseCode, QString *errorString)
{
    Q_ASSERT(errorString);

    int errorCode = 0;
    errorString->clear();

    if (responseCode == 204) {
        errorCode = KIO::ERR_NO_CONTENT;
    }

    if (responseCode >= 400 && responseCode <= 499) {
        errorCode = KIO::ERR_DOES_NOT_EXIST;
    }

    if (responseCode >= 500 && responseCode <= 599) {
        errorCode = KIO::ERR_INTERNAL_SERVER;
    }

    return errorCode;
}

// HTTP DELETE specific errors
static int httpDelError(int responseCode, QString *errorString)
{
    Q_ASSERT(errorString);

    int errorCode = 0;
    errorString->clear();

    switch (responseCode) {
    case 204:
        errorCode = KIO::ERR_NO_CONTENT;
        break;
    default:
        break;
    }

    if (!errorCode && (responseCode < 200 || responseCode > 400) && responseCode != 404) {
        errorCode = KIO::ERR_WORKER_DEFINED;
        *errorString = i18n("The resource cannot be deleted.");
    }

    if (responseCode >= 400 && responseCode <= 499) {
        errorCode = KIO::ERR_DOES_NOT_EXIST;
    }

    if (responseCode >= 500 && responseCode <= 599) {
        errorCode = KIO::ERR_INTERNAL_SERVER;
    }

    return errorCode;
}

// HTTP PUT specific errors
static int httpPutError(const QUrl &url, int responseCode, QString *errorString)
{
    Q_ASSERT(errorString);

    int errorCode = 0;
    const QString action(i18nc("request type", "upload %1", url.toDisplayString()));

    switch (responseCode) {
    case 403:
    case 405:
    case 500: // hack: Apache mod_dav returns this instead of 403 (!)
        // 403 Forbidden
        // 405 Method Not Allowed
        // ERR_ACCESS_DENIED
        *errorString = i18nc("%1: request type", "Access was denied while attempting to %1.", action);
        errorCode = KIO::ERR_WORKER_DEFINED;
        break;
    case 409:
        // 409 Conflict
        // ERR_ACCESS_DENIED
        *errorString = i18n(
            "A resource cannot be created at the destination "
            "until one or more intermediate collections (folders) "
            "have been created.");
        errorCode = KIO::ERR_WORKER_DEFINED;
        break;
    case 423:
        // 423 Locked
        // ERR_ACCESS_DENIED
        *errorString = i18nc("%1: request type", "Unable to %1 because the resource is locked.", action);
        errorCode = KIO::ERR_WORKER_DEFINED;
        break;
    case 502:
        // 502 Bad Gateway
        // ERR_WRITE_ACCESS_DENIED;
        *errorString = i18nc("%1: request type",
                             "Unable to %1 because the destination server refuses "
                             "to accept the file or folder.",
                             action);
        errorCode = KIO::ERR_WORKER_DEFINED;
        break;
    case 507:
        // 507 Insufficient Storage
        // ERR_DISK_FULL
        *errorString = i18n(
            "The destination resource does not have sufficient space "
            "to record the state of the resource after the execution "
            "of this method.");
        errorCode = KIO::ERR_WORKER_DEFINED;
        break;
    default:
        break;
    }

    if (!errorCode && (responseCode < 200 || responseCode > 400) && responseCode != 404) {
        errorCode = KIO::ERR_WORKER_DEFINED;
        *errorString = i18nc("%1: response code, %2: request type", "An unexpected error (%1) occurred while attempting to %2.", responseCode, action);
    }

    if (responseCode >= 400 && responseCode <= 499) {
        errorCode = KIO::ERR_DOES_NOT_EXIST;
    }

    if (responseCode >= 500 && responseCode <= 599) {
        errorCode = KIO::ERR_INTERNAL_SERVER;
    }

    return errorCode;
}

KIO::WorkerResult HTTPProtocol::sendHttpError(const QUrl &url, KIO::HTTP_METHOD method, const HTTPProtocol::Response &response)
{
    if (response.kioCode != KJob::NoError) {
        return KIO::WorkerResult::fail(response.kioCode, url.toDisplayString());
    }

    QString errorString;
    int errorCode = KJob::NoError;
    int responseCode = response.httpCode;

    if (method == KIO::HTTP_PUT) {
        errorCode = httpPutError(url, responseCode, &errorString);
    } else if (method == KIO::HTTP_DELETE) {
        errorCode = httpDelError(responseCode, &errorString);
    } else {
        errorCode = httpGenericError(responseCode, &errorString);
    }

    if (errorCode) {
        if (errorCode == KIO::ERR_DOES_NOT_EXIST) {
            errorString = url.toDisplayString();
        }

        return KIO::WorkerResult::fail(errorCode, errorString);
    }

    return KIO::WorkerResult::pass();
}

QByteArray HTTPProtocol::methodToString(KIO::HTTP_METHOD method)
{
    switch (method) {
    case KIO::HTTP_GET:
        return "GET";
    case KIO::HTTP_PUT:
        return "PUT";
    case KIO::HTTP_POST:
        return "POST";
    case KIO::HTTP_HEAD:
        return "HEAD";
    case KIO::HTTP_DELETE:
        return "DELETE";
    case KIO::HTTP_OPTIONS:
        return "OPTIONS";
    case KIO::DAV_PROPFIND:
        return "PROPFIND";
    case KIO::DAV_PROPPATCH:
        return "PROPPATCH";
    case KIO::DAV_MKCOL:
        return "MKCOL";
    case KIO::DAV_COPY:
        return "COPY";
    case KIO::DAV_MOVE:
        return "MOVE";
    case KIO::DAV_LOCK:
        return "LOCK";
    case KIO::DAV_UNLOCK:
        return "UNLOCK";
    case KIO::DAV_SEARCH:
        return "SEARCH";
    case KIO::DAV_SUBSCRIBE:
        return "SUBSCRIBE";
    case KIO::DAV_UNSUBSCRIBE:
        return "UNSUBSCRIBE";
    case KIO::DAV_POLL:
        return "POLL";
    case KIO::DAV_NOTIFY:
        return "NOTIFY";
    case KIO::DAV_REPORT:
        return "REPORT";
    default:
        Q_ASSERT(false);
        return QByteArray();
    }
}

// This is not the OS, but the windowing system, e.g. X11 on Unix/Linux.
static QString platform()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_DARWIN)
    return QStringLiteral("X11");
#elif defined(Q_OS_MAC)
    return QStringLiteral("Macintosh");
#elif defined(Q_OS_WIN)
    return QStringLiteral("Windows");
#else
    return QStringLiteral("Unknown");
#endif
}

QString HTTPProtocol::defaultUserAgent()
{
    if (!m_defaultUserAgent.isEmpty()) {
        return m_defaultUserAgent;
    }

    QString systemName;
    QString machine;
    QString supp;
    const bool sysInfoFound = getSystemNameVersionAndMachine(systemName, machine);

    supp += platform();

    if (sysInfoFound) {
        supp += QLatin1String("; ") + systemName;
        supp += QLatin1Char(' ') + machine;
    }

    QString appName = QCoreApplication::applicationName();
    if (appName.isEmpty() || appName.startsWith(QLatin1String("kcmshell"), Qt::CaseInsensitive)) {
        appName = QStringLiteral("KDE");
    }
    QString appVersion = QCoreApplication::applicationVersion();
    if (appVersion.isEmpty()) {
        appVersion += QLatin1String(KIO_VERSION_STRING);
    }

    m_defaultUserAgent = QLatin1String("Mozilla/5.0 (%1) ").arg(supp)
        + QLatin1String("KIO/%1.%2 ").arg(QString::number(KIO_VERSION_MAJOR), QString::number(KIO_VERSION_MINOR))
        + QLatin1String("%1/%2").arg(appName, appVersion);

    return m_defaultUserAgent;
}

bool HTTPProtocol::getSystemNameVersionAndMachine(QString &systemName, QString &machine)
{
#if defined(Q_OS_WIN)
    systemName = QStringLiteral("Windows");
#else
    struct utsname unameBuf;
    if (0 != uname(&unameBuf)) {
        return false;
    }
    systemName = QString::fromUtf8(unameBuf.sysname);
    machine = QString::fromUtf8(unameBuf.machine);
#endif
    return true;
}

#include "http.moc"
#include "moc_http.cpp"
