/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2008 Alex Merry <alex.merry @ kdemail.net>
    SPDX-FileCopyrightText: 2008-2009 Urs Wolfer <uwolfer @ kde.org>
    SPDX-FileCopyrightText: 2009-2012 Dawit Alemayehu <adawit @ kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "accessmanagerreply_p.h"
#include "accessmanager.h"
#include "job.h"
#include "scheduler.h"
#include "kio_widgets_debug.h"

#include <kurlauthorized.h>
#include <kprotocolinfo.h>
#include <QMimeDatabase>

#include <QtMath>
#include <QSslConfiguration>

#define QL1S(x)  QLatin1String(x)
#define QL1C(x)  QLatin1Char(x)

namespace KDEPrivate
{

AccessManagerReply::AccessManagerReply(const QNetworkAccessManager::Operation op,
                                       const QNetworkRequest &request,
                                       KIO::SimpleJob *kioJob,
                                       bool emitReadyReadOnMetaDataChange,
                                       QObject *parent)
    : QNetworkReply(parent),
      m_offset(0),
      m_metaDataRead(false),
      m_ignoreContentDisposition(false),
      m_emitReadyReadOnMetaDataChange(emitReadyReadOnMetaDataChange),
      m_kioJob(kioJob)

{
    setRequest(request);
    setOpenMode(QIODevice::ReadOnly);
    setUrl(request.url());
    setOperation(op);
    setError(NoError, QString());

    if (!request.sslConfiguration().isNull()) {
        setSslConfiguration(request.sslConfiguration());
    }

    connect(kioJob, SIGNAL(redirection(KIO::Job*,QUrl)), SLOT(slotRedirection(KIO::Job*,QUrl)));
    connect(kioJob, QOverload<KJob*,ulong>::of(&KJob::percent), this, &AccessManagerReply::slotPercent);

    if (qobject_cast<KIO::StatJob *>(kioJob)) {
        connect(kioJob, &KJob::result, this, &AccessManagerReply::slotStatResult);
    } else {
        connect(kioJob, &KJob::result, this, &AccessManagerReply::slotResult);
        connect(kioJob, SIGNAL(data(KIO::Job*,QByteArray)),
                SLOT(slotData(KIO::Job*,QByteArray)));
        connect(kioJob, SIGNAL(mimeTypeFound(KIO::Job*,QString)),
                SLOT(slotMimeType(KIO::Job*,QString)));
    }
}

AccessManagerReply::AccessManagerReply(const QNetworkAccessManager::Operation op,
                                       const QNetworkRequest &request,
                                       const QByteArray &data,
                                       const QUrl &url,
                                       const KIO::MetaData &metaData,
                                       QObject *parent)
    : QNetworkReply(parent),
      m_data(data),
      m_offset(0),
      m_ignoreContentDisposition(false),
      m_emitReadyReadOnMetaDataChange(false)
{
    setRequest(request);
    setOpenMode(QIODevice::ReadOnly);
    setUrl((url.isValid() ? url : request.url()));
    setOperation(op);
    setHeaderFromMetaData(metaData);

    if (!request.sslConfiguration().isNull()) {
        setSslConfiguration(request.sslConfiguration());
    }

    setError(NoError, QString());
    emitFinished(true, Qt::QueuedConnection);
}

AccessManagerReply::AccessManagerReply(const QNetworkAccessManager::Operation op,
                                       const QNetworkRequest &request,
                                       QNetworkReply::NetworkError errorCode,
                                       const QString &errorMessage,
                                       QObject *parent)
    : QNetworkReply(parent),
      m_offset(0)
{
    setRequest(request);
    setOpenMode(QIODevice::ReadOnly);
    setUrl(request.url());
    setOperation(op);
    setError(static_cast<QNetworkReply::NetworkError>(errorCode), errorMessage);
    const auto networkError = error();
    if (networkError != QNetworkReply::NoError) {
        QMetaObject::invokeMethod(this, "error", Qt::QueuedConnection, Q_ARG(QNetworkReply::NetworkError, networkError));
    }

    emitFinished(true, Qt::QueuedConnection);
}

AccessManagerReply::~AccessManagerReply()
{
}

void AccessManagerReply::abort()
{
    if (m_kioJob) {
        m_kioJob.data()->disconnect(this);
    }
    m_kioJob.clear();
    m_data.clear();
    m_offset = 0;
    m_metaDataRead = false;
}

qint64 AccessManagerReply::bytesAvailable() const
{
    return (QNetworkReply::bytesAvailable() + m_data.length() - m_offset);
}

qint64 AccessManagerReply::readData(char *data, qint64 maxSize)
{
    const qint64 length = qMin(qint64(m_data.length() - m_offset), maxSize);

    if (length <= 0) {
        return 0;
    }

    memcpy(data, m_data.constData() + m_offset, length);
    m_offset += length;

    if (m_data.length() == m_offset) {
        m_data.clear();
        m_offset = 0;
    }

    return length;
}

bool AccessManagerReply::ignoreContentDisposition(const KIO::MetaData &metaData)
{
    if (m_ignoreContentDisposition) {
        return true;
    }

    if (!metaData.contains(QLatin1String("content-disposition-type"))) {
        return true;
    }

    bool ok = false;
    const int statusCode = attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&ok);
    if (!ok || statusCode < 200 || statusCode > 299) {
        return true;
    }

    return false;
}

void AccessManagerReply::setHeaderFromMetaData(const KIO::MetaData &_metaData)
{
    if (_metaData.isEmpty()) {
        return;
    }

    KIO::MetaData metaData(_metaData);

    // Set the encryption attribute and values...
    QSslConfiguration sslConfig;
    const bool isEncrypted = KIO::Integration::sslConfigFromMetaData(metaData, sslConfig);
    setAttribute(QNetworkRequest::ConnectionEncryptedAttribute, isEncrypted);
    if (isEncrypted) {
        setSslConfiguration(sslConfig);
    }

    // Set the raw header information...
    const QStringList httpHeaders(metaData.value(QStringLiteral("HTTP-Headers")).split(QL1C('\n'), Qt::SkipEmptyParts));
    if (httpHeaders.isEmpty()) {
        const auto charSetIt = metaData.constFind(QStringLiteral("charset"));
        if (charSetIt != metaData.constEnd()) {
            QString mimeType = header(QNetworkRequest::ContentTypeHeader).toString();
            mimeType += QLatin1String(" ; charset=") + *charSetIt;
            //qDebug() << "changed content-type to" << mimeType;
            setHeader(QNetworkRequest::ContentTypeHeader, mimeType.toUtf8());
        }
    } else {
        for (const QString &httpHeader : httpHeaders) {
            int index = httpHeader.indexOf(QL1C(':'));
            // Handle HTTP status line...
            if (index == -1) {
                // Except for the status line, all HTTP header must be an nvpair of
                // type "<name>:<value>"
                if (!httpHeader.startsWith(QLatin1String("HTTP/"), Qt::CaseInsensitive)) {
                    continue;
                }

                QStringList statusLineAttrs(httpHeader.split(QL1C(' '), Qt::SkipEmptyParts));
                if (statusLineAttrs.count() > 1) {
                    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusLineAttrs.at(1));
                }

                if (statusLineAttrs.count() > 2) {
                    setAttribute(QNetworkRequest::HttpReasonPhraseAttribute, statusLineAttrs.at(2));
                }

                continue;
            }

            const QStringRef headerName = httpHeader.leftRef(index);
            QString headerValue = httpHeader.mid(index + 1);

            // Ignore cookie header since it is handled by the http ioslave.
            if (headerName.startsWith(QLatin1String("set-cookie"), Qt::CaseInsensitive)) {
                continue;
            }

            if (headerName.startsWith(QLatin1String("content-disposition"), Qt::CaseInsensitive) &&
                    ignoreContentDisposition(metaData)) {
                continue;
            }

            // Without overriding the corrected mime-type sent by kio_http, add
            // back the "charset=" portion of the content-type header if present.
            if (headerName.startsWith(QLatin1String("content-type"), Qt::CaseInsensitive)) {

                QString mimeType(header(QNetworkRequest::ContentTypeHeader).toString());

                if (m_ignoreContentDisposition) {
                    // If the server returned application/octet-stream, try to determine the
                    // real content type from the disposition filename.
                    if (mimeType == QLatin1String("application/octet-stream")) {
                        const QString fileName(metaData.value(QStringLiteral("content-disposition-filename")));
                        QMimeDatabase db;
                        QMimeType mime = db.mimeTypeForFile((fileName.isEmpty() ? url().path() : fileName), QMimeDatabase::MatchExtension);
                        mimeType = mime.name();
                    }
                    metaData.remove(QStringLiteral("content-disposition-type"));
                    metaData.remove(QStringLiteral("content-disposition-filename"));
                }

                if (!headerValue.contains(mimeType, Qt::CaseInsensitive)) {
                    index = headerValue.indexOf(QL1C(';'));
                    if (index == -1) {
                        headerValue = mimeType;
                    } else {
                        headerValue.replace(0, index, mimeType);
                    }
                    //qDebug() << "Changed mime-type from" << mimeType << "to" << headerValue;
                }
            }
            setRawHeader(headerName.trimmed().toUtf8(), headerValue.trimmed().toUtf8());
        }
    }

    // Set the returned meta data as attribute...
    setAttribute(static_cast<QNetworkRequest::Attribute>(KIO::AccessManager::MetaData), metaData.toVariant());
}

void AccessManagerReply::setIgnoreContentDisposition(bool on)
{
    //qDebug() << on;
    m_ignoreContentDisposition = on;
}

void AccessManagerReply::putOnHold()
{
    if (!m_kioJob || isFinished()) {
        return;
    }

    //qDebug() << m_kioJob << m_data;
    m_kioJob.data()->disconnect(this);
    m_kioJob.data()->putOnHold();
    m_kioJob.clear();
    KIO::Scheduler::publishSlaveOnHold();
}

bool AccessManagerReply::isLocalRequest(const QUrl &url)
{
    const QString scheme(url.scheme());
    return (KProtocolInfo::isKnownProtocol(scheme) &&
            KProtocolInfo::protocolClass(scheme).compare(QStringLiteral(":local"), Qt::CaseInsensitive) == 0);
}

void AccessManagerReply::readHttpResponseHeaders(KIO::Job *job)
{
    if (!job || m_metaDataRead) {
        return;
    }

    KIO::MetaData metaData(job->metaData());
    if (metaData.isEmpty()) {
        // Allow handling of local resources such as man pages and file url...
        if (isLocalRequest(url())) {
            setHeader(QNetworkRequest::ContentLengthHeader, job->totalAmount(KJob::Bytes));
            setAttribute(QNetworkRequest::HttpStatusCodeAttribute, QStringLiteral("200"));
            Q_EMIT metaDataChanged();
        }
        return;
    }

    setHeaderFromMetaData(metaData);
    m_metaDataRead = true;
    Q_EMIT metaDataChanged();
}

int AccessManagerReply::jobError(KJob *kJob)
{
    const int errCode = kJob->error();
    switch (errCode) {
    case 0:
        break; // No error;
    case KIO::ERR_SLAVE_DEFINED:
    case KIO::ERR_NO_CONTENT: // Sent by a 204 response is not an error condition.
        setError(QNetworkReply::NoError, kJob->errorText());
        break;
    case KIO::ERR_IS_DIRECTORY:
        // This error condition can happen if you click on an ftp link that points
        // to a directory instead of a file, e.g. ftp://ftp.kde.org/pub
        setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("inode/directory"));
        setError(QNetworkReply::NoError, kJob->errorText());
        break;
    case KIO::ERR_CANNOT_CONNECT:
        setError(QNetworkReply::ConnectionRefusedError, kJob->errorText());
        break;
    case KIO::ERR_UNKNOWN_HOST:
        setError(QNetworkReply::HostNotFoundError, kJob->errorText());
        break;
    case KIO::ERR_SERVER_TIMEOUT:
        setError(QNetworkReply::TimeoutError, kJob->errorText());
        break;
    case KIO::ERR_USER_CANCELED:
    case KIO::ERR_ABORTED:
        setError(QNetworkReply::OperationCanceledError, kJob->errorText());
        break;
    case KIO::ERR_UNKNOWN_PROXY_HOST:
        setError(QNetworkReply::ProxyNotFoundError, kJob->errorText());
        break;
    case KIO::ERR_ACCESS_DENIED:
        setError(QNetworkReply::ContentAccessDenied, kJob->errorText());
        break;
    case KIO::ERR_WRITE_ACCESS_DENIED:
        setError(QNetworkReply::ContentOperationNotPermittedError, kJob->errorText());
        break;
    case KIO::ERR_DOES_NOT_EXIST:
        setError(QNetworkReply::ContentNotFoundError, kJob->errorText());
        break;
    case KIO::ERR_CANNOT_AUTHENTICATE:
        setError(QNetworkReply::AuthenticationRequiredError, kJob->errorText());
        break;
    case KIO::ERR_UNSUPPORTED_PROTOCOL:
    case KIO::ERR_NO_SOURCE_PROTOCOL:
        setError(QNetworkReply::ProtocolUnknownError, kJob->errorText());
        break;
    case KIO::ERR_CONNECTION_BROKEN:
        setError(QNetworkReply::RemoteHostClosedError, kJob->errorText());
        break;
    case KIO::ERR_UNSUPPORTED_ACTION:
        setError(QNetworkReply::ProtocolInvalidOperationError, kJob->errorText());
        break;
    default:
        setError(QNetworkReply::UnknownNetworkError, kJob->errorText());
    }

    return errCode;
}

void AccessManagerReply::slotData(KIO::Job *kioJob, const QByteArray &data)
{
    Q_UNUSED(kioJob);
    if (data.isEmpty()) {
        return;
    }

    qint64 newSizeWithOffset = m_data.size() + data.size();
    if (newSizeWithOffset <= m_data.capacity()) {
        // Already enough space
    } else if (newSizeWithOffset - m_offset <= m_data.capacity()) {
        // We get enough space with ::remove.
        m_data.remove(0, m_offset);
        m_offset = 0;
    } else {
        // We have to resize the array, which implies an expensive memmove.
        // Do it ourselves to save m_offset bytes.
        QByteArray newData;
        // Leave some free space to avoid that every slotData call results in
        // a reallocation. qNextPowerOfTwo is what QByteArray does internally.
        newData.reserve(qNextPowerOfTwo(newSizeWithOffset - m_offset));
        newData.append(m_data.constData() + m_offset, m_data.size() - m_offset);
        m_data = newData;
        m_offset = 0;
    }

    m_data += data;

    Q_EMIT readyRead();
}

void AccessManagerReply::slotMimeType(KIO::Job *kioJob, const QString &mimeType)
{
    //qDebug() << kioJob << mimeType;
    setHeader(QNetworkRequest::ContentTypeHeader, mimeType.toUtf8());
    readHttpResponseHeaders(kioJob);
    if (m_emitReadyReadOnMetaDataChange) {
        Q_EMIT readyRead();
    }
}

void AccessManagerReply::slotResult(KJob *kJob)
{
    const int errcode = jobError(kJob);

    const QUrl redirectUrl = attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (!redirectUrl.isValid()) {
        setAttribute(static_cast<QNetworkRequest::Attribute>(KIO::AccessManager::KioError), errcode);
        if (errcode && errcode != KIO::ERR_NO_CONTENT) {
            const auto networkError = error();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            Q_EMIT error(networkError);
#else
            Q_EMIT errorOccurred(networkError);
#endif
        }
    }

    // Make sure HTTP response headers are always set.
    if (!m_metaDataRead) {
        readHttpResponseHeaders(qobject_cast<KIO::Job *>(kJob));
    }

    emitFinished(true);
}

void AccessManagerReply::slotStatResult(KJob *kJob)
{
    if (jobError(kJob)) {
        const auto networkError = error();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        Q_EMIT error(networkError);
#else
        Q_EMIT errorOccurred(networkError);
#endif
        emitFinished(true);
        return;
    }

    KIO::StatJob *statJob = qobject_cast<KIO::StatJob *>(kJob);
    Q_ASSERT(statJob);

    KIO::UDSEntry entry =  statJob->statResult();
    QString mimeType = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
    if (mimeType.isEmpty() && entry.isDir()) {
        mimeType = QStringLiteral("inode/directory");
    }

    if (!mimeType.isEmpty()) {
        setHeader(QNetworkRequest::ContentTypeHeader, mimeType.toUtf8());
    }

    emitFinished(true);
}

void AccessManagerReply::slotRedirection(KIO::Job *job, const QUrl &u)
{
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), url(), u)) {
        qCWarning(KIO_WIDGETS) << "Redirection from" << url() << "to" << u << "REJECTED by policy!";
        setError(QNetworkReply::ContentAccessDenied, u.toString());
        const auto networkError = error();
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        Q_EMIT error(networkError);
#else
        Q_EMIT errorOccurred(networkError);
#endif
        return;
    }
    setAttribute(QNetworkRequest::RedirectionTargetAttribute, QUrl(u));
    if (job->queryMetaData(QStringLiteral("redirect-to-get")) == QL1S("true")) {
        setOperation(QNetworkAccessManager::GetOperation);
    }
}

void AccessManagerReply::slotPercent(KJob *job, unsigned long percent)
{
    qulonglong bytesTotal = job->totalAmount(KJob::Bytes);
    qulonglong bytesProcessed = (bytesTotal * percent) / 100;
    if (operation() == QNetworkAccessManager::PutOperation ||
            operation() == QNetworkAccessManager::PostOperation) {
        Q_EMIT uploadProgress(bytesProcessed, bytesTotal);
        return;
    }
    Q_EMIT downloadProgress(bytesProcessed, bytesTotal);
}

void AccessManagerReply::emitFinished(bool state, Qt::ConnectionType type)
{
    setFinished(state);
    Q_EMIT QMetaObject::invokeMethod(this, "finished", type);
}

}

