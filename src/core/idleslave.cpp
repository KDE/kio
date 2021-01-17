/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "idleslave.h"
#include "connection_p.h"
#include "commands_p.h" // CMD_*
#include "slaveinterface.h" // MSG_*

#include <QDataStream>

using namespace KIO;

class KIO::IdleSlavePrivate
{
public:
    KIO::Connection mConn;
    QString mProtocol;
    QString mHost;
    bool mConnected;
    qint64 mPid;
    QDateTime mBirthDate;
    bool mOnHold;
    QUrl mUrl;
    bool mHasTempAuth;
};

IdleSlave::IdleSlave(QObject *parent)
    : QObject(parent), d(new IdleSlavePrivate)
{
    QObject::connect(&d->mConn, &Connection::readyRead, this, &IdleSlave::gotInput);
    // Send it a SLAVE_STATUS command.
    d->mConn.send(CMD_SLAVE_STATUS);
    d->mPid = 0;
    d->mBirthDate = QDateTime::currentDateTime();
    d->mOnHold = false;
    d->mHasTempAuth = false;
}

IdleSlave::~IdleSlave()
{
}

void IdleSlave::gotInput()
{
    int cmd;
    QByteArray data;
    if (d->mConn.read(&cmd, data) == -1) {
        // Communication problem with slave.
        //qCritical() << "No communication with KIO slave.";
        deleteLater();
    } else if (cmd == MSG_SLAVE_ACK) {
        deleteLater();
    } else if (cmd != MSG_SLAVE_STATUS_V2
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 45)
        && cmd != MSG_SLAVE_STATUS
#endif
                                  ) {
        qCritical() << "Unexpected data from KIO slave.";
        deleteLater();
    } else {
        QDataStream stream(data);
        qint64 pid;
        QByteArray protocol;
        QString host;
        qint8 b;
        // Overload with (bool) onHold, (QUrl) url.
        stream >> pid >> protocol >> host >> b;
        if (cmd == MSG_SLAVE_STATUS_V2) {
            QUrl url;
            bool onHold;
            bool tempAuth;
            stream >> onHold >> url >> tempAuth;
            d->mHasTempAuth = tempAuth;
            if (onHold) {
                d->mOnHold = onHold;
                d->mUrl = url;
            }
        } else { // compat code for KF < 5.45. TODO KF6: remove
            if (!stream.atEnd()) {
                QUrl url;
                stream >> url;
                d->mOnHold = true;
                d->mUrl = url;
            }
        }
        d->mPid = pid;
        d->mConnected = (b != 0);
        d->mProtocol = QString::fromLatin1(protocol);
        d->mHost = host;
        Q_EMIT statusUpdate(this);
    }
}

void IdleSlave::connect(const QString &app_socket)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << app_socket;
    d->mConn.send(CMD_SLAVE_CONNECT, data);
    // Timeout!
}

qint64 IdleSlave::pid() const
{
    return d->mPid;
}

void IdleSlave::reparseConfiguration()
{
    d->mConn.send(CMD_REPARSECONFIGURATION);
}

bool IdleSlave::match(const QString &protocol, const QString &host, bool needConnected) const
{
    if (d->mOnHold || protocol != d->mProtocol) {
        return false;
    }
    if (host.isEmpty()) {
        return true;
    }
    return (host == d->mHost) && (!needConnected || d->mConnected);
}

bool IdleSlave::onHold(const QUrl &url) const
{
    if (!d->mOnHold) {
        return false;
    }
    return (url == d->mUrl);
}

int IdleSlave::age(const QDateTime &now) const
{
    return d->mBirthDate.secsTo(now);
}

QString IdleSlave::protocol() const
{
    return d->mProtocol;
}

Connection *IdleSlave::connection() const
{
    return &d->mConn;
}

bool IdleSlave::hasTempAuthorization() const
{
    return d->mHasTempAuth;
}
