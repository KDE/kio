/*
 * This file is part of the KDE project.
 *
 * Copyright (C) 2009 - 2012 Dawit Alemayehu <adawit @ kde.org>
 * Copyright (C) 2008 - 2009 Urs Wolfer <uwolfer @ kde.org>
 * Copyright (C) 2007 Trolltech ASA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "accessmanager.h"

#include "accessmanagerreply_p.h"
#include "job.h"
#include "kjobwidgets.h"
#include "scheduler.h"

#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <kprotocolinfo.h>
#include <klocalizedstring.h>

#include <QtCore/QUrl>
#include <QNetworkCookie>
#include <QPointer>
#include <QWidget>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusReply>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslCipher>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslConfiguration>

#define QL1S(x)   QLatin1String(x)
#define QL1C(x)   QLatin1Char(x)

#if QT_VERSION >= 0x040800
static QNetworkRequest::Attribute gSynchronousNetworkRequestAttribute = QNetworkRequest::SynchronousRequestAttribute;
#else // QtWebkit hack to use the internal attribute
static QNetworkRequest::Attribute gSynchronousNetworkRequestAttribute = static_cast<QNetworkRequest::Attribute>(QNetworkRequest::HttpPipeliningWasUsedAttribute + 7);
#endif




static qint64 sizeFromRequest(const QNetworkRequest& req)
{
    const QVariant size = req.header(QNetworkRequest::ContentLengthHeader);
    if (!size.isValid())
        return -1;
    bool ok = false;
    const qlonglong value = size.toLongLong(&ok);
    return (ok ? value : -1);
}

namespace KIO {

class AccessManager::AccessManagerPrivate
{
public:
    AccessManagerPrivate()
      : externalContentAllowed(true),
        emitReadyReadOnMetaDataChange(false),
        window(0)
    {}

    void setMetaDataForRequest(QNetworkRequest request, KIO::MetaData& metaData);

    bool externalContentAllowed;
    bool emitReadyReadOnMetaDataChange;
    KIO::MetaData requestMetaData;
    KIO::MetaData sessionMetaData;
    QPointer<QWidget> window;
};

namespace Integration {

class CookieJar::CookieJarPrivate
{
public:
  CookieJarPrivate()
    : windowId((WId)-1),
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
              :QNetworkAccessManager(parent), d(new AccessManager::AccessManagerPrivate())
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

#ifndef KDE_NO_DEPRECATED
void AccessManager::setCookieJarWindowId(WId id)
{
    QWidget* window = QWidget::find(id);
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

void AccessManager::setWindow(QWidget* widget)
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

#ifndef KDE_NO_DEPRECATED
WId AccessManager::cookieJarWindowid() const
{
    KIO::Integration::CookieJar *jar = qobject_cast<KIO::Integration::CookieJar *> (cookieJar());
    if (jar)
        return jar->windowId();

    return 0;
}
#endif

QWidget* AccessManager::window() const
{
    return d->window;
}

KIO::MetaData& AccessManager::requestMetaData()
{
    return d->requestMetaData;
}

KIO::MetaData& AccessManager::sessionMetaData()
{
    return d->sessionMetaData;
}

void AccessManager::putReplyOnHold(QNetworkReply* reply)
{
    KDEPrivate::AccessManagerReply* r = qobject_cast<KDEPrivate::AccessManagerReply*>(reply);
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

    KIO::SimpleJob *kioJob = 0;

    switch (op) {
        case HeadOperation: {
            //qDebug() << "HeadOperation:" << reqUrl;
            kioJob = KIO::mimetype(reqUrl, KIO::HideProgressInfo);
            break;
        }
        case GetOperation: {
            //qDebug() << "GetOperation:" << reqUrl;
            if (!reqUrl.path().isEmpty() || reqUrl.host().isEmpty())
                kioJob = KIO::storedGet(reqUrl, KIO::NoReload, KIO::HideProgressInfo);
            else
                kioJob = KIO::stat(reqUrl, KIO::HideProgressInfo);

            // WORKAROUND: Avoid the brain damaged stuff QtWebKit does when a POST
            // operation is redirected! See BR# 268694.
            metaData.remove(QL1S("content-type")); // Remove the content-type from a GET/HEAD request!
            break;
        }
        case PutOperation: {
            //qDebug() << "PutOperation:" << reqUrl;
            if (outgoingData)
                kioJob = KIO::storedPut(outgoingData->readAll(), reqUrl, -1, KIO::HideProgressInfo);
            else
                kioJob = KIO::put(reqUrl, -1, KIO::HideProgressInfo);
            break;
        }
        case PostOperation: {
            kioJob = KIO::storedHttpPost(outgoingData, reqUrl, sizeFromRequest(req), KIO::HideProgressInfo);
            if (!metaData.contains(QL1S("content-type")))  {
                const QVariant header = req.header(QNetworkRequest::ContentTypeHeader);
                if (header.isValid()) {
                    metaData.insert(QL1S("content-type"),
                                    (QL1S("Content-Type: ") + header.toString()));
                } else {
                    metaData.insert(QL1S("content-type"),
                                    QL1S("Content-Type: application/x-www-form-urlencoded"));
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
            const QByteArray& method = req.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray();
            //qDebug() << "CustomOperation:" << reqUrl << "method:" << method << "outgoing data:" << outgoingData;

            if (method.isEmpty()) {
                return new KDEPrivate::AccessManagerReply(op, req, QNetworkReply::ProtocolUnknownError, i18n("Unknown HTTP verb."), this);
            }

            if (outgoingData)
                kioJob = KIO::http_post(reqUrl, outgoingData, sizeFromRequest(req), KIO::HideProgressInfo);
            else
                kioJob = KIO::get(reqUrl, KIO::NoReload, KIO::HideProgressInfo);

            metaData.insert(QL1S("CustomHTTPMethod"), method);
            break;
        }
        default: {
            qWarning() << "Unsupported KIO operation requested! Defering to QNetworkAccessManager...";
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
            if(StoredTransferJob* storedJob = qobject_cast< KIO::StoredTransferJob* >(kioJob))
                data = storedJob->data();
            reply = new KDEPrivate::AccessManagerReply(op, req, data, kioJob->url(), kioJob->metaData(), this);
            //qDebug() << "Synchronous XHR:" << reply << reqUrl;
        } else {
            qWarning() << "Failed to create a synchronous XHR for" << reqUrl;
            reply = new KDEPrivate::AccessManagerReply(op, req, QNetworkReply::UnknownNetworkError, kioJob->errorText(), this);
        }
    } else {
        // Set the window on the the KIO ui delegate
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

void AccessManager::AccessManagerPrivate::setMetaDataForRequest(QNetworkRequest request, KIO::MetaData& metaData)
{
    // Add any meta data specified within request...
    QVariant userMetaData = request.attribute (static_cast<QNetworkRequest::Attribute>(MetaData));
    if (userMetaData.isValid() && userMetaData.type() == QVariant::Map)
        metaData += userMetaData.toMap();

    metaData.insert(QL1S("PropagateHttpHeader"), QL1S("true"));

    if (request.hasRawHeader("User-Agent")) {
        metaData.insert(QL1S("UserAgent"), request.rawHeader("User-Agent"));
        request.setRawHeader("User-Agent", QByteArray());
    }

    if (request.hasRawHeader("Accept")) {
        metaData.insert(QL1S("accept"), request.rawHeader("Accept"));
        request.setRawHeader("Accept", QByteArray());
    }

    if (request.hasRawHeader("Accept-Charset")) {
        metaData.insert(QL1S("Charsets"), request.rawHeader("Accept-Charset"));
        request.setRawHeader("Accept-Charset", QByteArray());
    }

    if (request.hasRawHeader("Accept-Language")) {
        metaData.insert(QL1S("Languages"), request.rawHeader("Accept-Language"));
        request.setRawHeader("Accept-Language", QByteArray());
    }

    if (request.hasRawHeader("Referer")) {
        metaData.insert(QL1S("referrer"), request.rawHeader("Referer"));
        request.setRawHeader("Referer", QByteArray());
    }

    if (request.hasRawHeader("Content-Type")) {
        metaData.insert(QL1S("content-type"), request.rawHeader("Content-Type"));
        request.setRawHeader("Content-Type", QByteArray());
    }

    if (request.attribute(QNetworkRequest::AuthenticationReuseAttribute) == QNetworkRequest::Manual) {
        metaData.insert(QL1S("no-preemptive-auth-reuse"), QL1S("true"));
    }

    request.setRawHeader("Content-Length", QByteArray());
    request.setRawHeader("Connection", QByteArray());
    request.setRawHeader("If-None-Match", QByteArray());
    request.setRawHeader("If-Modified-Since", QByteArray());
    request.setRawHeader("x-kdewebkit-ignore-disposition", QByteArray());

    QStringList customHeaders;
    Q_FOREACH(const QByteArray &key, request.rawHeaderList()) {
        const QByteArray value = request.rawHeader(key);
        if (value.length())
            customHeaders << (key + QL1S(": ") + value);
    }

    if (!customHeaders.isEmpty()) {
        metaData.insert(QL1S("customHTTPHeader"), customHeaders.join("\r\n"));
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

static QSsl::SslProtocol qSslProtocolFromString(const QString& str)
{
    if (str.compare(QLatin1String("SSLv3"), Qt::CaseInsensitive) == 0) {
        return QSsl::SslV3;
    }

    if (str.compare(QLatin1String("SSLv2"), Qt::CaseInsensitive) == 0) {
        return QSsl::SslV2;
    }

    if (str.compare(QLatin1String("TLSv1"), Qt::CaseInsensitive) == 0) {
        return QSsl::TlsV1_0;
    }

    return QSsl::AnyProtocol;
}

bool KIO::Integration::sslConfigFromMetaData(const KIO::MetaData& metadata, QSslConfiguration& sslconfig)
{
    bool success = false;

    if (metadata.contains(QL1S("ssl_in_use"))) {
        const QSsl::SslProtocol sslProto = qSslProtocolFromString(metadata.value(QL1S("ssl_protocol_version")));
        QList<QSslCipher> cipherList;
        cipherList << QSslCipher(metadata.value(QL1S("ssl_cipher_name")), sslProto);
        sslconfig.setCaCertificates(QSslCertificate::fromData(metadata.value(QL1S("ssl_peer_chain")).toUtf8()));
        sslconfig.setCiphers(cipherList);
        sslconfig.setProtocol(sslProto);
        success = sslconfig.isNull();
    }

    return success;
}

CookieJar::CookieJar(QObject* parent)
          :QNetworkCookieJar(parent), d(new CookieJar::CookieJarPrivate)
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
    QDBusInterface kcookiejar("org.kde.kded5", "/modules/kcookiejar", "org.kde.KCookieServer");
    QDBusReply<QString> reply = kcookiejar.call("findDOMCookies", url.toString(QUrl::RemoveUserInfo), (qlonglong)d->windowId);

    if (!reply.isValid()) {
        qWarning() << "Unable to communicate with the cookiejar!";
        return cookieList;
    }

    const QString cookieStr = reply.value();
    const QStringList cookies = cookieStr.split(QL1S("; "), QString::SkipEmptyParts);
    Q_FOREACH(const QString& cookie, cookies) {
        const int index = cookie.indexOf(QL1C('='));
        const QString name = cookie.left(index);
        const QString value = cookie.right((cookie.length() - index - 1));
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

    QDBusInterface kcookiejar("org.kde.kded5", "/modules/kcookiejar", "org.kde.KCookieServer");
    Q_FOREACH(const QNetworkCookie &cookie, cookieList) {
        QByteArray cookieHeader ("Set-Cookie: ");
        if (d->isStorageDisabled && !cookie.isSessionCookie()) {
            QNetworkCookie sessionCookie(cookie);
            sessionCookie.setExpirationDate(QDateTime());
            cookieHeader += sessionCookie.toRawForm();
        } else {
            cookieHeader += cookie.toRawForm();
        }
        kcookiejar.call("addCookies", url.toString(QUrl::RemoveUserInfo), cookieHeader, (qlonglong)d->windowId);
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
    KConfigGroup cfg = KSharedConfig::openConfig("kcookiejarrc", KConfig::NoGlobals)->group("Cookie Policy");
    d->isEnabled = cfg.readEntry("Cookies", true);
}


