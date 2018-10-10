/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2013 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "transferjob.h"
#include "job_p.h"
#include "slave.h"
#include <kurlauthorized.h>
#include <QDebug>

using namespace KIO;

static const int MAX_READ_BUF_SIZE = (64 * 1024);       // 64 KB at a time seems reasonable...

TransferJob::TransferJob(TransferJobPrivate &dd)
    : SimpleJob(dd)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_PUT) {
        d->m_extraFlags |= JobPrivate::EF_TransferJobDataSent;
    }

    if (d->m_outgoingDataSource) {
        connect(d->m_outgoingDataSource, SIGNAL(readChannelFinished()),
                SLOT(slotIODeviceClosedBeforeStart()));
    }

}

TransferJob::~TransferJob()
{
}

// Slave sends data
void TransferJob::slotData(const QByteArray &_data)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_GET && !d->m_isMimetypeEmitted) {
        qCWarning(KIO_CORE) << "mimeType() not emitted when sending first data!; job URL ="
                   << d->m_url << "data size =" << _data.size();
    }
    // shut up the warning, HACK: downside is that it changes the meaning of the variable
    d->m_isMimetypeEmitted = true;

    if (d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error()) {
        emit data(this, _data);
    }
}

void KIO::TransferJob::setTotalSize(KIO::filesize_t bytes)
{
    setTotalAmount(KJob::Bytes, bytes);
}

// Slave got a redirection request
void TransferJob::slotRedirection(const QUrl &url)
{
    Q_D(TransferJob);
    //qDebug() << url;
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), d->m_url, url)) {
        qCWarning(KIO_CORE) << "Redirection from" << d->m_url << "to" << url << "REJECTED!";
        return;
    }

    // Some websites keep redirecting to themselves where each redirection
    // acts as the stage in a state-machine. We define "endless redirections"
    // as 5 redirections to the same URL.
    if (d->m_redirectionList.count(url) > 5) {
        //qDebug() << "CYCLIC REDIRECTION!";
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
        emit redirection(this, d->m_redirectionURL);
    }
}

void TransferJob::slotFinished()
{
    Q_D(TransferJob);

    //qDebug() << d->m_url;
    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {

        //qDebug() << "Redirection to" << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);
        }

        if (queryMetaData(QStringLiteral("redirect-to-get")) == QLatin1String("true")) {
            d->m_command = CMD_GET;
            d->m_outgoingMetaData.remove(QStringLiteral("CustomHTTPMethod"));
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
                qint8 iOverwrite, iResume;
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

void TransferJob::sendAsyncData(const QByteArray &dataForSlave)
{
    Q_D(TransferJob);
    if (d->m_extraFlags & JobPrivate::EF_TransferJobNeedData) {
        if (d->m_slave) {
            d->m_slave->send(MSG_DATA, dataForSlave);
        }
        if (d->m_extraFlags & JobPrivate::EF_TransferJobDataSent) { // put job -> emit progress
            KIO::filesize_t size = processedAmount(KJob::Bytes) + dataForSlave.size();
            setProcessedAmount(KJob::Bytes, size);
        }
    }

    d->m_extraFlags &= ~JobPrivate::EF_TransferJobNeedData;
}

#ifndef KIOCORE_NO_DEPRECATED
void TransferJob::setReportDataSent(bool enabled)
{
    Q_D(TransferJob);
    if (enabled) {
        d->m_extraFlags |= JobPrivate::EF_TransferJobDataSent;
    } else {
        d->m_extraFlags &= ~JobPrivate::EF_TransferJobDataSent;
    }
}
#endif

#ifndef KIOCORE_NO_DEPRECATED
bool TransferJob::reportDataSent() const
{
    return (d_func()->m_extraFlags & JobPrivate::EF_TransferJobDataSent);
}
#endif

QString TransferJob::mimetype() const
{
    return d_func()->m_mimetype;
}

QUrl TransferJob::redirectUrl() const
{
    return d_func()->m_redirectionURL;
}

// Slave requests data
void TransferJob::slotDataReq()
{
    Q_D(TransferJob);
    QByteArray dataForSlave;

    d->m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (!d->staticData.isEmpty()) {
        dataForSlave = d->staticData;
        d->staticData.clear();
    } else {
        emit dataReq(this, dataForSlave);

        if (d->m_extraFlags & JobPrivate::EF_TransferJobAsync) {
            return;
        }
    }

    static const int max_size = 14 * 1024 * 1024;
    if (dataForSlave.size() > max_size) {
        //qDebug() << "send" << dataForSlave.size() / 1024 / 1024 << "MB of data in TransferJob::dataReq. This needs to be split, which requires a copy. Fix the application.";
        d->staticData = QByteArray(dataForSlave.data() + max_size,  dataForSlave.size() - max_size);
        dataForSlave.truncate(max_size);
    }

    sendAsyncData(dataForSlave);

    if (d->m_subJob) {
        // Bitburger protocol in action
        d->internalSuspend(); // Wait for more data from subJob.
        d->m_subJob->d_func()->internalResume(); // Ask for more!
    }
}

void TransferJob::slotMimetype(const QString &type)
{
    Q_D(TransferJob);
    d->m_mimetype = type;
    if (d->m_command == CMD_GET && d->m_isMimetypeEmitted) {
        qCWarning(KIO_CORE) << "mimetype() emitted again, or after sending first data!; job URL =" << d->m_url;
    }
    d->m_isMimetypeEmitted = true;
    emit mimetype(this, type);
}

void TransferJobPrivate::internalSuspend()
{
    m_internalSuspended = true;
    if (m_slave) {
        m_slave->suspend();
    }
}

void TransferJobPrivate::internalResume()
{
    m_internalSuspended = false;
    if (m_slave && !q_func()->isSuspended()) {
        m_slave->resume();
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

bool TransferJob::isErrorPage() const
{
    return d_func()->m_errorPage;
}

void TransferJobPrivate::start(Slave *slave)
{
    Q_Q(TransferJob);
    Q_ASSERT(slave);
    JobPrivate::emitTransferring(q, m_url);
    q->connect(slave, SIGNAL(data(QByteArray)),
               SLOT(slotData(QByteArray)));

    if (m_outgoingDataSource) {
        if (m_extraFlags & JobPrivate::EF_TransferJobAsync) {
            q->connect(m_outgoingDataSource, SIGNAL(readyRead()),
                    SLOT(slotDataReqFromDevice()));
            q->connect(m_outgoingDataSource, SIGNAL(readChannelFinished()),
                    SLOT(slotIODeviceClosed()));
            // We don't really need to disconnect since we're never checking
            // m_closedBeforeStart again but it's the proper thing to do logically
            QObject::disconnect(m_outgoingDataSource, SIGNAL(readChannelFinished()),
                                q, SLOT(slotIODeviceClosedBeforeStart()));
            if (m_closedBeforeStart) {
                QMetaObject::invokeMethod(q, "slotIODeviceClosed", Qt::QueuedConnection);
            } else if (m_outgoingDataSource->bytesAvailable() > 0) {
                QMetaObject::invokeMethod(q, "slotDataReqFromDevice", Qt::QueuedConnection);
            }
        } else {
            q->connect(slave, SIGNAL(dataReq()),
                    SLOT(slotDataReqFromDevice()));
        }
    } else
        q->connect(slave, SIGNAL(dataReq()),
                   SLOT(slotDataReq()));

    q->connect(slave, SIGNAL(redirection(QUrl)),
               SLOT(slotRedirection(QUrl)));

    q->connect(slave, SIGNAL(mimeType(QString)),
               SLOT(slotMimetype(QString)));

    q->connect(slave, SIGNAL(errorPage()),
               SLOT(slotErrorPage()));

    q->connect(slave, SIGNAL(needSubUrlData()),
               SLOT(slotNeedSubUrlData()));

    q->connect(slave, SIGNAL(canResume(KIO::filesize_t)),
               SLOT(slotCanResume(KIO::filesize_t)));

    if (slave->suspended()) {
        m_mimetype = QStringLiteral("unknown");
        // WABA: The slave was put on hold. Resume operation.
        slave->resume();
    }

    SimpleJobPrivate::start(slave);
    if (m_internalSuspended) {
        slave->suspend();
    }
}

void TransferJobPrivate::slotNeedSubUrlData()
{
    Q_Q(TransferJob);
    // Job needs data from subURL.
    m_subJob = KIO::get(m_subUrl, NoReload, HideProgressInfo);
    internalSuspend(); // Put job on hold until we have some data.
    q->connect(m_subJob, SIGNAL(data(KIO::Job*,QByteArray)),
               SLOT(slotSubUrlData(KIO::Job*,QByteArray)));
    q->addSubjob(m_subJob);
}

void TransferJobPrivate::slotSubUrlData(KIO::Job *, const QByteArray &data)
{
    // The Alternating Bitburg protocol in action again.
    staticData = data;
    m_subJob->d_func()->internalSuspend(); // Put job on hold until we have delivered the data.
    internalResume(); // Activate ourselves again.
}

void TransferJob::slotMetaData(const KIO::MetaData &_metaData)
{
    Q_D(TransferJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

void TransferJobPrivate::slotErrorPage()
{
    m_errorPage = true;
}

void TransferJobPrivate::slotCanResume(KIO::filesize_t offset)
{
    Q_Q(TransferJob);
    emit q->canResume(q, offset);
}

void TransferJobPrivate::slotDataReqFromDevice()
{
    Q_Q(TransferJob);

    bool done = false;
    QByteArray dataForSlave;

    m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (m_outgoingDataSource) {
        dataForSlave.resize(MAX_READ_BUF_SIZE);

        //Code inspired in QNonContiguousByteDevice
        qint64 bytesRead = m_outgoingDataSource->read(dataForSlave.data(), MAX_READ_BUF_SIZE);
        if (bytesRead >= 0) {
            dataForSlave.resize(bytesRead);
        } else {
            dataForSlave.clear();
        }
        done = ((bytesRead == -1) || (bytesRead == 0 && m_outgoingDataSource->atEnd() && !m_outgoingDataSource->isSequential()));
    }

    if (dataForSlave.isEmpty()) {
        emit q->dataReq(q, dataForSlave);
        if (!done && (m_extraFlags & JobPrivate::EF_TransferJobAsync)) {
            return;
        }
    }

    q->sendAsyncData(dataForSlave);

    if (m_subJob) {
        // Bitburger protocol in action
        internalSuspend(); // Wait for more data from subJob.
        m_subJob->d_func()->internalResume(); // Ask for more!
    }
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

    //We send an empty data array to indicate the stream is over
    q->sendAsyncData(QByteArray());

    if (m_subJob) {
        // Bitburger protocol in action
        internalSuspend(); // Wait for more data from subJob.
        m_subJob->d_func()->internalResume(); // Ask for more!
    }
}

void TransferJob::slotResult(KJob *job)
{
    Q_D(TransferJob);
    // This can only be our suburl.
    Q_ASSERT(job == d->m_subJob);

    SimpleJob::slotResult(job);

    if (!error() && job == d->m_subJob) {
        d->m_subJob = nullptr; // No action required
        d->internalResume(); // Make sure we get the remaining data.
    }
}

void TransferJob::setModificationTime(const QDateTime &mtime)
{
    addMetaData(QStringLiteral("modified"), mtime.toString(Qt::ISODate));
}

TransferJob *KIO::get(const QUrl &url, LoadType reload, JobFlags flags)
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    TransferJob *job = TransferJobPrivate::newJob(url, CMD_GET, packedArgs,
                       QByteArray(), flags);
    if (reload == Reload) {
        job->addMetaData(QStringLiteral("cache"), QStringLiteral("reload"));
    }
    return job;
}

#include "moc_transferjob.cpp"
