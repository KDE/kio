/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2009-2012 Dawit Alemayehu <adawit @ kde.org>
    SPDX-FileCopyrightText: 2008-2009 Urs Wolfer <uwolfer @ kde.org>
    SPDX-FileCopyrightText: 2007 Trolltech ASA

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "accessmanager.h"

#include "accessmanagerreply_p.h"
#include "job.h"
#include <KJobWidgets>
#include "scheduler.h"
#include "kio_widgets_debug.h"
#include <KConfigGroup>
#include <KSharedConfig>
#include <kprotocolinfo.h>
#include <KLocalizedString>

#include <QUrl>
#include <QNetworkCookie>
#include <QPointer>
#include <QWidget>
#include <QDBusInterface>
#include <QDBusReply>
#include <QNetworkReply>
#include <QSslCipher>
#include <QSslCertificate>
#include <QSslConfiguration>

#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

static const QNetworkRequest::Attribute gSynchronousNetworkRequestAttribute = QNetworkRequest::SynchronousRequestAttribute;

static qint64 sizeFromRequest(const QNetworkRequest &req)
{
    const QVariant size = req.header(QNetworkRequest::ContentLengthHeader);
    if (!size.isValid()) {
        return -1;
    }
    bool ok = false;
    const qlonglong value = size.toLongLong(&ok);
    return (ok ? value : -1);
}

namespace KIO
{

class Q_DECL_HIDDEN AccessManager::AccessManagerPrivate
{
public:
    AccessManagerPrivate()
        : externalContentAllowed(true),
          emitReadyReadOnMetaDataChange(false),
          window(nullptr)
    {}

    void setMetaDataForRequest(QNetworkRequest request, KIO::MetaData &metaData);

    bool externalContentAllowed;
    bool emitReadyReadOnMetaDataChange;
    KIO::MetaData requestMetaData;
    KIO::MetaData sessionMetaData;
    QPointer<QWidget> window;
};

namespace Integration
{

class Q_DECL_HIDDEN CookieJar::CookieJarPrivate
{
public:
    CookieJarPrivate()
        : windowId((WId) - 1),
          isEnabled(true),
          isStorageDisabled(false)
    {}

    WId windowId;
    bool isEnabled;
    bool isStorageDisabled;
};

}

}

using namespace KIO;

AccessManager::AccessManager(QObject *parent)
    : QNetworkAccessManager(parent), d(new AccessManager::AccessManagerPrivate())
{
    // KDE Cookiejar (KCookieJar) integration...
    setCookieJar(new KIO::Integration::CookieJar);
}

AccessManager::~AccessManager()
{
    delete d;
}

void AccessManager::setExternalContentAllowed(bool allowed)
{
    d->externalContentAllowed = allowed;
}

bool AccessManager::isExternalContentAllowed() const
{
    return d->externalContentAllowed;
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
void AccessManager::setCookieJarWindowId(WId id)
{
    QWidget *window = QWidget::find(id);
    if (!window) {
        return;
    }

    KIO::Integration::CookieJar *jar = qobject_cast<KIO::Integration::CookieJar *> (cookieJar());
    if (jar) {
        jar->setWindowId(id);
    }

    d->window = window->isWindow() ? window : window->window();
}
#endif

void AccessManager::setWindow(QWidget *widget)
{
    if (!widget) {
        return;
    }

    d->window = widget->isWindow() ? widget : widget->window();

    if (!d->window) {
        return;
    }

    KIO::Integration::CookieJar *jar = qobject_cast<KIO::Integration::CookieJar *> (cookieJar());
    if (jar) {
        jar->setWindowId(d->window->winId());
    }
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 0)
WId AccessManager::cookieJarWindowid() const
{
    KIO::Integration::CookieJar *jar = qobject_cast<KIO::Integration::CookieJar *> (cookieJar());
    if (jar) {
        return jar->windowId();
    }

    return 0;
}
#endif

QWidget *AccessManager::window() const
{
    return d->window;
}

KIO::MetaData &AccessManager::requestMetaData()
{
    return d->requestMetaData;
}

KIO::MetaData &AccessManager::sessionMetaData()
{
    return d->sessionMetaData;
}

void AccessManager::putReplyOnHold(QNetworkReply *reply)
{
    KDEPrivate::AccessManagerReply *r = qobject_cast<KDEPrivate::AccessManagerReply *>(reply);
    if (!r) {
        return;
    }

    r->putOnHold();
}

void AccessManager::setEmitReadyReadOnMetaDataChange(bool enable)
{
    d->emitReadyReadOnMetaDataChange = enable;
}

QNetworkReply *AccessManager::createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData)
{
    const QUrl reqUrl(req.url());

    if (!d->externalContentAllowed &&
            !KDEPrivate::AccessManagerReply::isLocalRequest(reqUrl) &&
            reqUrl.scheme() != QL1S("data")) {
        //qDebug() << "Blocked: " << reqUrl;
        return new KDEPrivate::AccessManagerReply(op, req, QNetworkReply::ContentAccessDenied, i18n("Blocked request."), this);
    }

    // Check if the internal ignore content disposition header is set.
    const bool ignoreContentDisposition = req.hasRawHeader("x-kdewebkit-ignore-disposition");

    // Retrieve the KIO meta data...
    KIO::MetaData metaData;
    d->setMetaDataForRequest(req, metaData);

    KIO::SimpleJob *kioJob = nullptr;

    switch (op) {
    case HeadOperation: {
        //qDebug() << "HeadOperation:" << reqUrl;
        kioJob = KIO::mimetype(reqUrl, KIO::HideProgressInfo);
        break;
    }
    case GetOperation: {
        //qDebug() << "GetOperation:" << reqUrl;
        if (!reqUrl.path().isEmpty() || reqUrl.host().isEmpty()) {
            kioJob = KIO::storedGet(reqUrl, KIO::NoReload, KIO::HideProgressInfo);
        } else {
            kioJob = KIO::stat(reqUrl, KIO::HideProgressInfo);
        }

        // WORKAROUND: Avoid the brain damaged stuff QtWebKit does when a POST
        // operation is redirected! See BR# 268694.
        metaData.remove(QStringLiteral("content-type")); // Remove the content-type from a GET/HEAD request!
        break;
    }
    case PutOperation: {
        //qDebug() << "PutOperation:" << reqUrl;
        if (outgoingData) {
            Q_ASSERT(outgoingData->isReadable());
            StoredTransferJob* storedJob = KIO::storedPut(outgoingData, reqUrl, -1, KIO::HideProgressInfo);
            storedJob->setAsyncDataEnabled(outgoingData->isSequential());

            QVariant len = req.header(QNetworkRequest::ContentLengthHeader);
            if (len.isValid()) {
                storedJob->setTotalSize(len.toInt());
            }

            kioJob = storedJob;
        } else {
            kioJob = KIO::put(reqUrl, -1, KIO::HideProgressInfo);
        }
        break;
    }
    case PostOperation: {
        kioJob = KIO::storedHttpPost(outgoingData, reqUrl, sizeFromRequest(req), KIO::HideProgressInfo);
        if (!metaData.contains(QLatin1String("content-type")))  {
            const QVariant header = req.header(QNetworkRequest::ContentTypeHeader);
            if (header.isValid()) {
                metaData.insert(QStringLiteral("content-type"),
                                (QStringLiteral("Content-Type: ") + header.toString()));
            } else {
                metaData.insert(QStringLiteral("content-type"),
                                QStringLiteral("Content-Type: application/x-www-form-urlencoded"));
            }
        }
        break;
    }
    case DeleteOperation: {
        //qDebug() << "DeleteOperation:" << reqUrl;
        kioJob = KIO::http_delete(reqUrl, KIO::HideProgressInfo);
        break;
    }
    case CustomOperation: {
        const QByteArray &method = req.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
        //qDebug() << "CustomOperation:" << reqUrl << "method:" << method << "outgoing data:" << outgoingData;

        if (method.isEmpty()) {
            return new KDEPrivate::AccessManagerReply(op, req, QNetworkReply::ProtocolUnknownError, i18n("Unknown HTTP verb."), this);
        }

        const qint64 size = sizeFromRequest(req);
        if (size > 0) {
            kioJob = KIO::http_post(reqUrl, outgoingData, size, KIO::HideProgressInfo);
        } else {
            kioJob = KIO::get(reqUrl, KIO::NoReload, KIO::HideProgressInfo);
        }

        metaData.insert(QStringLiteral("CustomHTTPMethod"), QString::fromUtf8(method));
        break;
    }
    default: {
        qCWarning(KIO_WIDGETS) << "Unsupported KIO operation requested! Deferring to QNetworkAccessManager...";
        return QNetworkAccessManager::createRequest(op, req, outgoingData);
    }
    }

    // Set the job priority
    switch (req.priority()) {
    case QNetworkRequest::HighPriority:
        KIO::Scheduler::setJobPriority(kioJob, -5);
        break;
    case QNetworkRequest::LowPriority:
        KIO::Scheduler::setJobPriority(kioJob, 5);
        break;
    default:
        break;
    }

    KDEPrivate::AccessManagerReply *reply;

    /*
      NOTE: Here we attempt to handle synchronous XHR requests. Unfortunately,
      due to the fact that QNAM is both synchronous and multi-thread while KIO
      is completely the opposite (asynchronous and not thread safe), the code
      below might cause crashes like the one reported in bug# 287778 (nested
      event loops are inherently dangerous).

      Unfortunately, all attempts to address the crash has so far failed due to
      the many regressions they caused, e.g. bug# 231932 and 297954. Hence, until
      a solution is found, we have to live with the side effects of creating
      nested event loops.
    */
    if (req.attribute(gSynchronousNetworkRequestAttribute).toBool()) {
        KJobWidgets::setWindow(kioJob, d->window);
        kioJob->setRedirectionHandlingEnabled(true);
        if (kioJob->exec()) {
            QByteArray data;
            if (StoredTransferJob *storedJob = qobject_cast< KIO::StoredTransferJob * >(kioJob)) {
                data = storedJob->data();
            }
            reply = new KDEPrivate::AccessManagerReply(op, req, data, kioJob->url(), kioJob->metaData(), this);
            //qDebug() << "Synchronous XHR:" << reply << reqUrl;
        } else {
            qCWarning(KIO_WIDGETS) << "Failed to create a synchronous XHR for" << reqUrl;
            qCWarning(KIO_WIDGETS) << "REASON:" << kioJob->errorString();
            reply = new KDEPrivate::AccessManagerReply(op, req, QNetworkReply::UnknownNetworkError, kioJob->errorText(), this);
        }
    } else {
        // Set the window on the KIO ui delegate
        if (d->window) {
            KJobWidgets::setWindow(kioJob, d->window);
        }

        // Disable internal automatic redirection handling
        kioJob->setRedirectionHandlingEnabled(false);

        // Set the job priority
        switch (req.priority()) {
        case QNetworkRequest::HighPriority:
            KIO::Scheduler::setJobPriority(kioJob, -5);
            break;
        case QNetworkRequest::LowPriority:
            KIO::Scheduler::setJobPriority(kioJob, 5);
            break;
        default:
            break;
        }

        // Set the meta data for this job...
        kioJob->setMetaData(metaData);

        // Create the reply...
        reply = new KDEPrivate::AccessManagerReply(op, req, kioJob, d->emitReadyReadOnMetaDataChange, this);
        //qDebug() << reply << reqUrl;
    }

    if (ignoreContentDisposition && reply) {
        //qDebug() << "Content-Disposition WILL BE IGNORED!";
        reply->setIgnoreContentDisposition(ignoreContentDisposition);
    }

    return reply;
}

static inline
void moveMetaData(KIO::MetaData &metaData, const QString &metaDataKey, QNetworkRequest &request, const QByteArray &requestKey)
{
    if (request.hasRawHeader(requestKey)) {
        metaData.insert(metaDataKey, QString::fromUtf8(request.rawHeader(requestKey)));
        request.setRawHeader(requestKey, QByteArray());
    }
}

void AccessManager::AccessManagerPrivate::setMetaDataForRequest(QNetworkRequest request, KIO::MetaData &metaData)
{
    // Add any meta data specified within request...
    QVariant userMetaData = request.attribute(static_cast<QNetworkRequest::Attribute>(MetaData));
    if (userMetaData.isValid() && userMetaData.type() == QVariant::Map) {
        metaData += userMetaData.toMap();
    }

    metaData.insert(QStringLiteral("PropagateHttpHeader"), QStringLiteral("true"));

    moveMetaData(metaData, QStringLiteral("UserAgent"),    request, QByteArrayLiteral("User-Agent"));
    moveMetaData(metaData, QStringLiteral("accept"),       request, QByteArrayLiteral("Accept"));
    moveMetaData(metaData, QStringLiteral("Charsets"),     request, QByteArrayLiteral("Accept-Charset"));
    moveMetaData(metaData, QStringLiteral("Languages"),    request, QByteArrayLiteral("Accept-Language"));
    moveMetaData(metaData, QStringLiteral("referrer"),     request, QByteArrayLiteral("Referer")); //Don't try to correct spelling!
    moveMetaData(metaData, QStringLiteral("content-type"), request, QByteArrayLiteral("Content-Type"));

    if (request.attribute(QNetworkRequest::AuthenticationReuseAttribute) == QNetworkRequest::Manual) {
        metaData.insert(QStringLiteral("no-preemptive-auth-reuse"), QStringLiteral("true"));
    }

    request.setRawHeader("Content-Length", QByteArray());
    request.setRawHeader("Connection", QByteArray());
    request.setRawHeader("If-None-Match", QByteArray());
    request.setRawHeader("If-Modified-Since", QByteArray());
    request.setRawHeader("x-kdewebkit-ignore-disposition", QByteArray());

    QStringList customHeaders;
    const QList<QByteArray> list = request.rawHeaderList();
    for (const QByteArray &key : list) {
        const QByteArray value = request.rawHeader(key);
        if (value.length()) {
            customHeaders << (QString::fromUtf8(key) + QLatin1String(": ") + QString::fromUtf8(value));
        }
    }

    if (!customHeaders.isEmpty()) {
        metaData.insert(QStringLiteral("customHTTPHeader"), customHeaders.join(QLatin1String("\r\n")));
    }

    // Append per request meta data, if any...
    if (!requestMetaData.isEmpty()) {
        metaData += requestMetaData;
        // Clear per request meta data...
        requestMetaData.clear();
    }

    // Append per session meta data, if any...
    if (!sessionMetaData.isEmpty()) {
        metaData += sessionMetaData;
    }
}

using namespace KIO::Integration;

static QSsl::SslProtocol qSslProtocolFromString(const QString &str)
{
    if (str.compare(QStringLiteral("SSLv3"), Qt::CaseInsensitive) == 0) {
        return QSsl::SslV3;
    }

    if (str.compare(QStringLiteral("SSLv2"), Qt::CaseInsensitive) == 0) {
        return QSsl::SslV2;
    }

    if (str.compare(QStringLiteral("TLSv1"), Qt::CaseInsensitive) == 0) {
        return QSsl::TlsV1_0;
    }

    return QSsl::AnyProtocol;
}

bool KIO::Integration::sslConfigFromMetaData(const KIO::MetaData &metadata, QSslConfiguration &sslconfig)
{
    bool success = false;

    if (metadata.value(QStringLiteral("ssl_in_use")) == QLatin1String("TRUE")) {
        const QSsl::SslProtocol sslProto = qSslProtocolFromString(metadata.value(QStringLiteral("ssl_protocol_version")));
        QList<QSslCipher> cipherList;
        cipherList << QSslCipher(metadata.value(QStringLiteral("ssl_cipher_name")), sslProto);
        sslconfig.setCaCertificates(QSslCertificate::fromData(metadata.value(QStringLiteral("ssl_peer_chain")).toUtf8()));
        sslconfig.setCiphers(cipherList);
        sslconfig.setProtocol(sslProto);
        success = sslconfig.isNull();
    }

    return success;
}

CookieJar::CookieJar(QObject *parent)
    : QNetworkCookieJar(parent), d(new CookieJar::CookieJarPrivate)
{
    reparseConfiguration();
}

CookieJar::~CookieJar()
{
    delete d;
}

WId CookieJar::windowId() const
{
    return d->windowId;
}

bool CookieJar::isCookieStorageDisabled() const
{
    return d->isStorageDisabled;
}

QList<QNetworkCookie> CookieJar::cookiesForUrl(const QUrl &url) const
{
    QList<QNetworkCookie> cookieList;

    if (!d->isEnabled) {
        return cookieList;
    }
    QDBusInterface kcookiejar(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"));
    QDBusReply<QString> reply = kcookiejar.call(QStringLiteral("findDOMCookies"), url.toString(QUrl::RemoveUserInfo), (qlonglong)d->windowId);

    if (!reply.isValid()) {
        qCWarning(KIO_WIDGETS) << "Unable to communicate with the cookiejar!";
        return cookieList;
    }

    const QString cookieStr = reply.value();
    const QStringList cookies = cookieStr.split(QStringLiteral("; "), Qt::SkipEmptyParts);
    for (const QString &cookie : cookies) {
        const int index = cookie.indexOf(QL1C('='));
        const QStringRef name = cookie.leftRef(index);
        const QStringRef value = cookie.rightRef((cookie.length() - index - 1));
        cookieList << QNetworkCookie(name.toUtf8(), value.toUtf8());
        //qDebug() << "cookie: name=" << name << ", value=" << value;
    }

    return cookieList;
}

bool CookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    if (!d->isEnabled) {
        return false;
    }

    QDBusInterface kcookiejar(QStringLiteral("org.kde.kcookiejar5"), QStringLiteral("/modules/kcookiejar"), QStringLiteral("org.kde.KCookieServer"));
    for (const QNetworkCookie &cookie : cookieList) {
        QByteArray cookieHeader("Set-Cookie: ");
        if (d->isStorageDisabled && !cookie.isSessionCookie()) {
            QNetworkCookie sessionCookie(cookie);
            sessionCookie.setExpirationDate(QDateTime());
            cookieHeader += sessionCookie.toRawForm();
        } else {
            cookieHeader += cookie.toRawForm();
        }
        kcookiejar.call(QStringLiteral("addCookies"), url.toString(QUrl::RemoveUserInfo), cookieHeader, (qlonglong)d->windowId);
        //qDebug() << "[" << d->windowId << "]" << cookieHeader << " from " << url;
    }

    return !kcookiejar.lastError().isValid();
}

void CookieJar::setDisableCookieStorage(bool disable)
{
    d->isStorageDisabled = disable;
}

void CookieJar::setWindowId(WId id)
{
    d->windowId = id;
}

void CookieJar::reparseConfiguration()
{
    KConfigGroup cfg = KSharedConfig::openConfig(QStringLiteral("kcookiejarrc"), KConfig::NoGlobals)->group("Cookie Policy");
    d->isEnabled = cfg.readEntry("Cookies", true);
}

