/*
   This file is part of the KDE libraries
   Copyright (c) 1999 Waldo Bastian <bastian@kde.org>
   Copyright (c) 2013 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
 */

#include "idleslave.h"
#include "connection_p.h"
#include "commands_p.h" // CMD_*
#include "slaveinterface.h" // MSG_*

using namespace KIO;

class KIO::IdleSlavePrivate
{
public:
    KIO::Connection mConn;
    QString mProtocol;
    QString mHost;
    bool mConnected;
    Q_PID mPid;
    QDateTime mBirthDate;
    bool mOnHold;
    QUrl mUrl;
};

IdleSlave::IdleSlave(QObject *parent)
    : QObject(parent), d(new IdleSlavePrivate)
{
    QObject::connect(&d->mConn, SIGNAL(readyRead()), this, SLOT(gotInput()));
    // Send it a SLAVE_STATUS command.
    d->mConn.send(CMD_SLAVE_STATUS);
    d->mPid = 0;
    d->mBirthDate = QDateTime::currentDateTime();
    d->mOnHold = false;
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
    } else if (cmd != MSG_SLAVE_STATUS) {
        qCritical() << "Unexpected data from KIO slave.";
        deleteLater();
    } else {
        QDataStream stream(data);
        Q_PID pid;
        QByteArray protocol;
        QString host;
        qint8 b;
        stream >> pid >> protocol >> host >> b;
        // Overload with (bool) onHold, (QUrl) url.
        if (!stream.atEnd())
        {
            QUrl url;
            stream >> url;
            d->mOnHold = true;
            d->mUrl = url;
        }

        d->mPid = pid;
        d->mConnected = (b != 0);
        d->mProtocol = QString::fromLatin1(protocol);
        d->mHost = host;
        emit statusUpdate(this);
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

Q_PID IdleSlave::pid() const
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
    if (!d->mOnHold)
        return false;
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

Connection* IdleSlave::connection() const
{
    return &d->mConn;
}
