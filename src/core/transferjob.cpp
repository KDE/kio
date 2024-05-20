/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "transferjob.h"
#include "job_p.h"
#include "worker_p.h"
#include <QDebug>
#include <kurlauthorized.h>

using namespace KIO;

static const int MAX_READ_BUF_SIZE = (64 * 1024); // 64 KB at a time seems reasonable...

TransferJob::TransferJob(TransferJobPrivate &dd)
    : SimpleJob(dd)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_PUT) {
        d->m_extraFlags |= JobPrivate::EF_TransferJobDataSent;
    }

    if (d->m_outgoingDataSource) {
        d->m_readChannelFinishedConnection = connect(d->m_outgoingDataSource, &QIODevice::readChannelFinished, this, [d]() {
            d->slotIODeviceClosedBeforeStart();
        });
    }
}

TransferJob::~TransferJob()
{
}

// Worker sends data
void TransferJob::slotData(const QByteArray &_data)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_GET && !d->m_isMimetypeEmitted) {
        qCWarning(KIO_CORE) << "mimeType() not emitted when sending first data!; job URL =" << d->m_url << "data size =" << _data.size();
    }
    // shut up the warning, HACK: downside is that it changes the meaning of the variable
    d->m_isMimetypeEmitted = true;

    if (d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error()) {
        Q_EMIT data(this, _data);
    }
}

void KIO::TransferJob::setTotalSize(KIO::filesize_t bytes)
{
    setTotalAmount(KJob::Bytes, bytes);
}

// Worker got a redirection request
void TransferJob::slotRedirection(const QUrl &url)
{
    Q_D(TransferJob);
    // qDebug() << url;
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), d->m_url, url)) {
        qCWarning(KIO_CORE) << "Redirection from" << d->m_url << "to" << url << "REJECTED!";
        return;
    }

    // Some websites keep redirecting to themselves where each redirection
    // acts as the stage in a state-machine. We define "endless redirections"
    // as 5 redirections to the same URL.
    if (d->m_redirectionList.count(url) > 5) {
        // qDebug() << "CYCLIC REDIRECTION!";
        setError(ERR_CYCLIC_LINK);
        setErrorText(d->m_url.toDisplayString());
    } else {
        d->m_redirectionURL = url; // We'll remember that when the job finishes
        d->m_redirectionList.append(url);
        QString sslInUse = queryMetaData(QStringLiteral("ssl_in_use"));
        if (!sslInUse.isNull()) { // the key is present
            addMetaData(QStringLiteral("ssl_was_in_use"), sslInUse);
        } else {
            addMetaData(QStringLiteral("ssl_was_in_use"), QStringLiteral("FALSE"));
        }
        // Tell the user that we haven't finished yet
        Q_EMIT redirection(this, d->m_redirectionURL);
    }
}

void TransferJob::slotFinished()
{
    Q_D(TransferJob);

    // qDebug() << d->m_url;
    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {
        // qDebug() << "Redirection to" << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            Q_EMIT permanentRedirection(this, d->m_url, d->m_redirectionURL);
        }

        if (queryMetaData(QStringLiteral("redirect-to-get")) == QLatin1String("true")) {
            d->m_command = CMD_GET;
            d->m_outgoingMetaData.remove(QStringLiteral("content-type"));
        }

        if (d->m_redirectionHandlingEnabled) {
            // Honour the redirection
            // We take the approach of "redirecting this same job"
            // Another solution would be to create a subjob, but the same problem
            // happens (unpacking+repacking)
            d->staticData.truncate(0);
            d->m_incomingMetaData.clear();
            if (queryMetaData(QStringLiteral("cache")) != QLatin1String("reload")) {
                addMetaData(QStringLiteral("cache"), QStringLiteral("refresh"));
            }
            d->m_internalSuspended = false;
            // The very tricky part is the packed arguments business
            QUrl dummyUrl;
            QDataStream istream(d->m_packedArgs);
            switch (d->m_command) {
            case CMD_GET:
            case CMD_STAT:
            case CMD_DEL: {
                d->m_packedArgs.truncate(0);
                QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
                stream << d->m_redirectionURL;
                break;
            }
            case CMD_PUT: {
                int permissions;
                qint8 iOverwrite;
                qint8 iResume;
                istream >> dummyUrl >> iOverwrite >> iResume >> permissions;
                d->m_packedArgs.truncate(0);
                QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
                stream << d->m_redirectionURL << iOverwrite << iResume << permissions;
                break;
            }
            case CMD_SPECIAL: {
                int specialcmd;
                istream >> specialcmd;
                if (specialcmd == 1) { // HTTP POST
                    d->m_outgoingMetaData.remove(QStringLiteral("content-type"));
                    addMetaData(QStringLiteral("cache"), QStringLiteral("reload"));
                    d->m_packedArgs.truncate(0);
                    QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
                    stream << d->m_redirectionURL;
                    d->m_command = CMD_GET;
                }
                break;
            }
            }
            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    SimpleJob::slotFinished();
}

void TransferJob::setAsyncDataEnabled(bool enabled)
{
    Q_D(TransferJob);
    if (enabled) {
        d->m_extraFlags |= JobPrivate::EF_TransferJobAsync;
    } else {
        d->m_extraFlags &= ~JobPrivate::EF_TransferJobAsync;
    }
}

void TransferJob::sendAsyncData(const QByteArray &dataForWorker)
{
    Q_D(TransferJob);
    if (d->m_extraFlags & JobPrivate::EF_TransferJobNeedData) {
        if (d->m_worker) {
            d->m_worker->send(MSG_DATA, dataForWorker);
        }
        if (d->m_extraFlags & JobPrivate::EF_TransferJobDataSent) { // put job -> emit progress
            KIO::filesize_t size = processedAmount(KJob::Bytes) + dataForWorker.size();
            setProcessedAmount(KJob::Bytes, size);
        }
    }

    d->m_extraFlags &= ~JobPrivate::EF_TransferJobNeedData;
}

QString TransferJob::mimetype() const
{
    return d_func()->m_mimetype;
}

QUrl TransferJob::redirectUrl() const
{
    return d_func()->m_redirectionURL;
}

// Worker requests data
void TransferJob::slotDataReq()
{
    Q_D(TransferJob);
    QByteArray dataForWorker;

    d->m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (!d->staticData.isEmpty()) {
        dataForWorker = d->staticData;
        d->staticData.clear();
    } else {
        Q_EMIT dataReq(this, dataForWorker);

        if (d->m_extraFlags & JobPrivate::EF_TransferJobAsync) {
            return;
        }
    }

    static const int max_size = 14 * 1024 * 1024;
    if (dataForWorker.size() > max_size) {
        // qDebug() << "send" << dataForWorker.size() / 1024 / 1024 << "MB of data in TransferJob::dataReq. This needs to be split, which requires a copy. Fix
        // the application.";
        d->staticData = QByteArray(dataForWorker.data() + max_size, dataForWorker.size() - max_size);
        dataForWorker.truncate(max_size);
    }

    sendAsyncData(dataForWorker);
}

void TransferJob::slotMimetype(const QString &type)
{
    Q_D(TransferJob);
    d->m_mimetype = type;
    if (d->m_command == CMD_GET && d->m_isMimetypeEmitted) {
        qCWarning(KIO_CORE) << "mimetype() emitted again, or after sending first data!; job URL =" << d->m_url;
    }
    d->m_isMimetypeEmitted = true;
    Q_EMIT mimeTypeFound(this, type);
}

void TransferJobPrivate::internalSuspend()
{
    m_internalSuspended = true;
    if (m_worker) {
        m_worker->suspend();
    }
}

void TransferJobPrivate::internalResume()
{
    m_internalSuspended = false;
    if (m_worker && !q_func()->isSuspended()) {
        m_worker->resume();
    }
}

bool TransferJob::doResume()
{
    Q_D(TransferJob);
    if (!SimpleJob::doResume()) {
        return false;
    }
    if (d->m_internalSuspended) {
        d->internalSuspend();
    }
    return true;
}

#if KIOCORE_ENABLE_DEPRECATED_SINCE(6, 3)
bool TransferJob::isErrorPage() const
{
    return false;
}
#endif

void TransferJobPrivate::start(Worker *worker)
{
    Q_Q(TransferJob);
    Q_ASSERT(worker);
    JobPrivate::emitTransferring(q, m_url);
    q->connect(worker, &WorkerInterface::data, q, &TransferJob::slotData);

    if (m_outgoingDataSource) {
        if (m_extraFlags & JobPrivate::EF_TransferJobAsync) {
            auto dataReqFunc = [this]() {
                slotDataReqFromDevice();
            };
            q->connect(m_outgoingDataSource, &QIODevice::readyRead, q, dataReqFunc);
            auto ioClosedFunc = [this]() {
                slotIODeviceClosed();
            };
            q->connect(m_outgoingDataSource, &QIODevice::readChannelFinished, q, ioClosedFunc);
            // We don't really need to disconnect since we're never checking
            // m_closedBeforeStart again but it's the proper thing to do logically
            QObject::disconnect(m_readChannelFinishedConnection);
            if (m_closedBeforeStart) {
                QMetaObject::invokeMethod(q, ioClosedFunc, Qt::QueuedConnection);
            } else if (m_outgoingDataSource->bytesAvailable() > 0) {
                QMetaObject::invokeMethod(q, dataReqFunc, Qt::QueuedConnection);
            }
        } else {
            q->connect(worker, &WorkerInterface::dataReq, q, [this]() {
                slotDataReqFromDevice();
            });
        }
    } else {
        q->connect(worker, &WorkerInterface::dataReq, q, &TransferJob::slotDataReq);
    }

    q->connect(worker, &WorkerInterface::redirection, q, &TransferJob::slotRedirection);

    q->connect(worker, &WorkerInterface::mimeType, q, &TransferJob::slotMimetype);

    q->connect(worker, &WorkerInterface::canResume, q, [q](KIO::filesize_t offset) {
        Q_EMIT q->canResume(q, offset);
    });

    if (worker->suspended()) {
        m_mimetype = QStringLiteral("unknown");
        // WABA: The worker was put on hold. Resume operation.
        worker->resume();
    }

    SimpleJobPrivate::start(worker);
    if (m_internalSuspended) {
        worker->suspend();
    }
}

void TransferJobPrivate::slotDataReqFromDevice()
{
    Q_Q(TransferJob);

    bool done = false;
    QByteArray dataForWorker;

    m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (m_outgoingDataSource) {
        dataForWorker.resize(MAX_READ_BUF_SIZE);

        // Code inspired in QNonContiguousByteDevice
        qint64 bytesRead = m_outgoingDataSource->read(dataForWorker.data(), MAX_READ_BUF_SIZE);
        if (bytesRead >= 0) {
            dataForWorker.resize(bytesRead);
        } else {
            dataForWorker.clear();
        }
        done = ((bytesRead == -1) || (bytesRead == 0 && m_outgoingDataSource->atEnd() && !m_outgoingDataSource->isSequential()));
    }

    if (dataForWorker.isEmpty()) {
        Q_EMIT q->dataReq(q, dataForWorker);
        if (!done && (m_extraFlags & JobPrivate::EF_TransferJobAsync)) {
            return;
        }
    }

    q->sendAsyncData(dataForWorker);
}

void TransferJobPrivate::slotIODeviceClosedBeforeStart()
{
    m_closedBeforeStart = true;
}

void TransferJobPrivate::slotIODeviceClosed()
{
    Q_Q(TransferJob);
    const QByteArray remainder = m_outgoingDataSource->readAll();
    if (!remainder.isEmpty()) {
        m_extraFlags |= JobPrivate::EF_TransferJobNeedData;
        q->sendAsyncData(remainder);
    }

    m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    // We send an empty data array to indicate the stream is over
    q->sendAsyncData(QByteArray());
}

void TransferJob::setModificationTime(const QDateTime &mtime)
{
    addMetaData(QStringLiteral("modified"), mtime.toString(Qt::ISODate));
}

TransferJob *KIO::get(const QUrl &url, LoadType reload, JobFlags flags)
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    TransferJob *job = TransferJobPrivate::newJob(url, CMD_GET, packedArgs, QByteArray(), flags);
    if (reload == Reload) {
        job->addMetaData(QStringLiteral("cache"), QStringLiteral("reload"));
    }
    return job;
}

#include "moc_transferjob.cpp"
