/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Leo Savernik <l.savernik@aon.at>
    Derived from slave.cpp

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "dataslave_p.h"
#include "dataprotocol_p.h"
#include "commands_p.h"
#include "slavebase.h"

#include <KLocalizedString>

#include <QTimer>
#include <QDataStream>

using namespace KIO;

#define KIO_DATA_POLL_INTERVAL 0

// don't forget to sync DISPATCH_DECL in dataslave_p.h
#define DISPATCH_IMPL(type) \
    void DataSlave::dispatch_##type() { \
        if (_suspended) { \
            QueueStruct q(Queue_##type); \
            q.size = -1; \
            dispatchQueue.push_back(q); \
            if (!timer->isActive()) timer->start(KIO_DATA_POLL_INTERVAL); \
        } else \
            Q_EMIT type(); \
    }

// don't forget to sync DISPATCH_DECL1 in dataslave_p.h
#define DISPATCH_IMPL1(type, paramtype, paramname) \
    void DataSlave::dispatch_##type(paramtype paramname) { \
        if (_suspended) { \
            QueueStruct q(Queue_##type); \
            q.paramname = paramname; \
            dispatchQueue.push_back(q); \
            if (!timer->isActive()) timer->start(KIO_DATA_POLL_INTERVAL); \
        } else \
            Q_EMIT type(paramname); \
    }

DataSlave::DataSlave() :
    Slave(QStringLiteral("data"))
{
    //qDebug() << this;
    _suspended = false;
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DataSlave::dispatchNext);
}

DataSlave::~DataSlave()
{
    //qDebug() << this;
}

void DataSlave::hold(const QUrl &/*url*/)
{
    // ignored
}

void DataSlave::suspend()
{
    _suspended = true;
    //qDebug() << this;
    timer->stop();
}

void DataSlave::resume()
{
    _suspended = false;
    //qDebug() << this;
    // aarrrgh! This makes the once hyper fast and efficient data protocol
    // implementation slow as molasses. But it wouldn't work otherwise,
    // and I don't want to start messing around with threads
    timer->start(KIO_DATA_POLL_INTERVAL);
}

// finished is a special case. If we emit it right away, then
// TransferJob::start can delete the job even before the end of the method
void DataSlave::dispatch_finished()
{
    QueueStruct q(Queue_finished);
    q.size = -1;
    dispatchQueue.push_back(q);
    if (!timer->isActive()) {
        timer->start(KIO_DATA_POLL_INTERVAL);
    }
}

void DataSlave::dispatchNext()
{
    if (dispatchQueue.empty()) {
        timer->stop();
        return;
    }

    const QueueStruct &q = dispatchQueue.front();
    //qDebug() << this << "dispatching" << q.type << dispatchQueue.size() << "left";
    switch (q.type) {
    case Queue_mimeType:    Q_EMIT mimeType(q.s); break;
    case Queue_totalSize:   Q_EMIT totalSize(q.size); break;
    case Queue_sendMetaData:    sendMetaData(); break;
    case Queue_data:        Q_EMIT data(q.ba); break;
    case Queue_finished:    Q_EMIT finished(); break;
    }/*end switch*/

    dispatchQueue.pop_front();
}

void DataSlave::send(int cmd, const QByteArray &arr)
{
    QDataStream stream(arr);

    QUrl url;

    switch (cmd) {
    case CMD_GET: {
        stream >> url;
        get(url);
        break;
    }
    case CMD_MIMETYPE: {
        stream >> url;
        mimetype(url);
        break;
    }
    // ignore these (must not emit error, otherwise SIGSEGV occurs)
    case CMD_REPARSECONFIGURATION:
    case CMD_META_DATA:
    case CMD_SUBURL:
        break;
    default:
        Q_EMIT error(ERR_UNSUPPORTED_ACTION,
                   unsupportedActionErrorString(QStringLiteral("data"), cmd));
    }/*end switch*/
}

bool DataSlave::suspended()
{
    return _suspended;
}

void DataSlave::setHost(const QString &/*host*/, quint16 /*port*/,
                        const QString &/*user*/, const QString &/*passwd*/)
{
    // irrelevant -> will be ignored
}

void DataSlave::setConfig(const MetaData &/*config*/)
{
    // FIXME: decide to handle this directly or not at all
#if 0
    QByteArray data;
    QDataStream stream(data, QIODevice::WriteOnly);
    stream << config;
    slaveconn.send(CMD_CONFIG, data);
#endif
}

void DataSlave::setAllMetaData(const MetaData &md)
{
    meta_data = md;
}

void DataSlave::sendMetaData()
{
    Q_EMIT metaData(meta_data);
}

DISPATCH_IMPL1(mimeType, const QString &, s)
DISPATCH_IMPL1(totalSize, KIO::filesize_t, size)
DISPATCH_IMPL(sendMetaData)
DISPATCH_IMPL1(data, const QByteArray &, ba)

#undef DISPATCH_IMPL
#undef DISPATCH_IMPL1

