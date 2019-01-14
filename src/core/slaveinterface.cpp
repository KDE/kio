/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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

#include "slaveinterface.h"
#include "slaveinterface_p.h"
#include "usernotificationhandler_p.h"

#include "slavebase.h"
#include "connection_p.h"
#include "commands_p.h"
#include "hostinfo.h"
#include <qplatformdefs.h>
#include <signal.h>
#include <klocalizedstring.h>
#include <time.h>

#include <QDebug>

#include <QDBusConnection>
#include <QPointer>
#include <QDataStream>
#include <QSslCertificate>
#include <QSslError>

using namespace KIO;

Q_GLOBAL_STATIC(UserNotificationHandler, globalUserNotificationHandler)

SlaveInterface::SlaveInterface(SlaveInterfacePrivate &dd, QObject *parent)
    : QObject(parent), d_ptr(&dd)
{
    connect(&d_ptr->speed_timer, &QTimer::timeout, this, &SlaveInterface::calcSpeed);
    d_ptr->transfer_details.reserve(max_count);
}

SlaveInterface::~SlaveInterface()
{
    // Note: no Debug() here (scheduler is deleted very late)

    delete d_ptr;
}

void SlaveInterface::setConnection(Connection *connection)
{
    Q_D(SlaveInterface);
    d->connection = connection;
}

Connection *SlaveInterface::connection() const
{
    const Q_D(SlaveInterface);
    return d->connection;
}

static KIO::filesize_t readFilesize_t(QDataStream &stream)
{
    KIO::filesize_t result;
    stream >> result;
    return result;
}

bool SlaveInterface::dispatch()
{
    Q_D(SlaveInterface);
    Q_ASSERT(d->connection);

    int cmd;
    QByteArray data;

    int ret = d->connection->read(&cmd, data);
    if (ret == -1) {
        return false;
    }

    return dispatch(cmd, data);
}

void SlaveInterface::calcSpeed()
{
    Q_D(SlaveInterface);
    if (d->slave_calcs_speed || !d->connection->isConnected()) { // killing a job results in disconnection but the timer never stops
        d->speed_timer.stop();
        return;
    }

    // Note for future reference: A list is maintained for sizes and times.
    // Minimum list size is 1 and maximum list size is 8. Delta is calculated
    // using first and last item from the list.

    const qint64 elapsed_time = d->elapsed_timer.elapsed();
    if (elapsed_time >= 900) {
        if (d->transfer_details.count() == max_count) {
            d->transfer_details.removeFirst();
        }

        const SlaveInterfacePrivate::TransferInfo first = d->transfer_details.first();
        const SlaveInterfacePrivate::TransferInfo last = {elapsed_time, (d->filesize - d->offset)};
        KIO::filesize_t lspeed = 1000 * (last.size - first.size) / (last.time - first.time);
        if (!lspeed) {
            d->transfer_details.clear();
        }
        d->transfer_details.append(last);

        emit speed(lspeed);
    }
}

bool SlaveInterface::dispatch(int _cmd, const QByteArray &rawdata)
{
    Q_D(SlaveInterface);
    //qDebug() << "dispatch " << _cmd;

    QDataStream stream(rawdata);

    QString str1;
    qint32 i;
    qint8 b;
    quint32 ul;

    switch (_cmd) {
    case MSG_DATA:
        emit data(rawdata);
        break;
    case MSG_DATA_REQ:
        emit dataReq();
        break;
    case MSG_OPENED:
        emit open();
        break;
    case MSG_FINISHED:
        //qDebug() << "Finished [this = " << this << "]";
        d->offset = 0;
        d->speed_timer.stop();
        emit finished();
        break;
    case MSG_STAT_ENTRY: {
        UDSEntry entry;
        stream >> entry;
        emit statEntry(entry);
        break;
    }
    case MSG_LIST_ENTRIES: {
        UDSEntryList list;
        UDSEntry entry;

        while (!stream.atEnd()) {
            stream >> entry;
            list.append(entry);
        }

        emit listEntries(list);
        break;
    }
    case MSG_RESUME: { // From the put job
        d->offset = readFilesize_t(stream);
        emit canResume(d->offset);
        break;
    }
    case MSG_CANRESUME: // From the get job
        d->filesize = d->offset;
        emit canResume(0); // the arg doesn't matter
        break;
    case MSG_ERROR:
        stream >> i >> str1;
        //qDebug() << "error " << i << " " << str1;
        emit error(i, str1);
        break;
    case MSG_SLAVE_STATUS:
    case MSG_SLAVE_STATUS_V2: {
        qint64 pid;
        QByteArray protocol;
        stream >> pid >> protocol >> str1 >> b;
        emit slaveStatus(pid, protocol, str1, (b != 0));
        break;
    }
    case MSG_CONNECTED:
        emit connected();
        break;
    case MSG_WRITTEN: {
        KIO::filesize_t size = readFilesize_t(stream);
        emit written(size);
        break;
    }
    case INF_TOTAL_SIZE: {
        KIO::filesize_t size = readFilesize_t(stream);
        d->filesize = d->offset;
        d->transfer_details.append({0, 0});
        d->speed_timer.start(1000);
        d->elapsed_timer.start();
        d->slave_calcs_speed = false;
        emit totalSize(size);
        break;
    }
    case INF_PROCESSED_SIZE: {
        KIO::filesize_t size = readFilesize_t(stream);
        emit processedSize(size);
        d->filesize = size;
        break;
    }
    case INF_POSITION: {
        KIO::filesize_t pos = readFilesize_t(stream);
        emit position(pos);
        break;
    }
    case INF_SPEED:
        stream >> ul;
        d->slave_calcs_speed = true;
        d->speed_timer.stop();
        emit speed(ul);
        break;
    case INF_GETTING_FILE:
        break;
    case INF_ERROR_PAGE:
        emit errorPage();
        break;
    case INF_REDIRECTION: {
        QUrl url;
        stream >> url;
        emit redirection(url);
        break;
    }
    case INF_MIME_TYPE:
        stream >> str1;
        emit mimeType(str1);
        if (!d->connection->suspended()) {
            d->connection->sendnow(CMD_NONE, QByteArray());
        }
        break;
    case INF_WARNING:
        stream >> str1;
        emit warning(str1);
        break;
    case INF_MESSAGEBOX: {
        //qDebug() << "needs a msg box";
        QString text, caption, buttonYes, buttonNo, dontAskAgainName;
        int type;
        stream >> type >> text >> caption >> buttonYes >> buttonNo;
        if (stream.atEnd()) {
            messageBox(type, text, caption, buttonYes, buttonNo);
        } else {
            stream >> dontAskAgainName;
            messageBox(type, text, caption, buttonYes, buttonNo, dontAskAgainName);
        }
        break;
    }
    case INF_INFOMESSAGE: {
        QString msg;
        stream >> msg;
        emit infoMessage(msg);
        break;
    }
    case INF_META_DATA: {
        MetaData m;
        stream >> m;
        if (m.contains(QStringLiteral("ssl_in_use"))) {
            const QLatin1String ssl_("ssl_");
            const MetaData constM = m;
            for (MetaData::ConstIterator it = constM.lowerBound(ssl_); it != constM.constEnd(); ++it) {
                if (it.key().startsWith(ssl_)) {
                    d->sslMetaData.insert(it.key(), it.value());
                } else {
                    // we're past the ssl_* entries; remember that QMap is ordered.
                    break;
                }
            }
        }
        emit metaData(m);
        break;
    }
    case MSG_NET_REQUEST: {
        QString host;
        QString slaveid;
        stream >> host >> slaveid;
        requestNetwork(host, slaveid);
        break;
    }
    case MSG_NET_DROP: {
        QString host;
        QString slaveid;
        stream >> host >> slaveid;
        dropNetwork(host, slaveid);
        break;
    }
    case MSG_NEED_SUBURL_DATA: {
        emit needSubUrlData();
        break;
    }
    case MSG_HOST_INFO_REQ: {
        QString hostName;
        stream >> hostName;
        HostInfo::lookupHost(hostName, this, SLOT(slotHostInfo(QHostInfo)));
        break;
    }
    case MSG_PRIVILEGE_EXEC:
        emit privilegeOperationRequested();
        break;
    default:
        qCWarning(KIO_CORE) << "Slave sends unknown command (" << _cmd << "), dropping slave";
        return false;
    }
    return true;
}

void SlaveInterface::setOffset(KIO::filesize_t o)
{
    Q_D(SlaveInterface);
    d->offset = o;
}

KIO::filesize_t SlaveInterface::offset() const
{
    const Q_D(SlaveInterface);
    return d->offset;
}

void SlaveInterface::requestNetwork(const QString &host, const QString &slaveid)
{
    Q_D(SlaveInterface);
    Q_UNUSED(host);
    Q_UNUSED(slaveid);
    //qDebug() << "requestNetwork " << host << slaveid;

    // This is old stuff. We just always return true...

    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << true;
    d->connection->sendnow(INF_NETWORK_STATUS, packedArgs);
}

void SlaveInterface::dropNetwork(const QString &host, const QString &slaveid)
{
    Q_UNUSED(host);
    Q_UNUSED(slaveid);
    //qDebug() << "dropNetwork " << host << slaveid;
}

void SlaveInterface::sendResumeAnswer(bool resume)
{
    Q_D(SlaveInterface);
    //qDebug() << "ok for resuming:" << resume;
    d->connection->sendnow(resume ? CMD_RESUMEANSWER : CMD_NONE, QByteArray());
}

void SlaveInterface::sendMessageBoxAnswer(int result)
{
    Q_D(SlaveInterface);
    if (!d->connection) {
        return;
    }

    if (d->connection->suspended()) {
        d->connection->resume();
    }
    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << result;
    d->connection->sendnow(CMD_MESSAGEBOXANSWER, packedArgs);
    // qDebug() << "message box answer" << result;
}

void SlaveInterface::messageBox(int type, const QString &text, const QString &_caption,
                                const QString &buttonYes, const QString &buttonNo)
{
    messageBox(type, text, _caption, buttonYes, buttonNo, QString());
}

void SlaveInterface::messageBox(int type, const QString &text, const QString &caption,
                                const QString &buttonYes, const QString &buttonNo, const QString &dontAskAgainName)
{
    Q_D(SlaveInterface);
    if (d->connection) {
        d->connection->suspend();
    }

    QHash<UserNotificationHandler::MessageBoxDataType, QVariant> data;
    data.insert(UserNotificationHandler::MSG_TEXT, text);
    data.insert(UserNotificationHandler::MSG_CAPTION, caption);
    data.insert(UserNotificationHandler::MSG_YES_BUTTON_TEXT, buttonYes);
    data.insert(UserNotificationHandler::MSG_NO_BUTTON_TEXT, buttonNo);
    data.insert(UserNotificationHandler::MSG_DONT_ASK_AGAIN, dontAskAgainName);

    // SMELL: the braindead way to support button icons
    // TODO: Fix this in KIO::SlaveBase.
    if (buttonYes == i18n("&Details")) {
        data.insert(UserNotificationHandler::MSG_YES_BUTTON_ICON, QLatin1String("help-about"));
    } else if (buttonYes == i18n("&Forever")) {
        data.insert(UserNotificationHandler::MSG_YES_BUTTON_ICON, QLatin1String("flag-green"));
    }

    if (buttonNo == i18n("Co&ntinue")) {
        data.insert(UserNotificationHandler::MSG_NO_BUTTON_ICON, QLatin1String("arrow-right"));
    } else if (buttonNo == i18n("&Current Session only")) {
        data.insert(UserNotificationHandler::MSG_NO_BUTTON_ICON, QLatin1String("chronometer"));
    }

    if (type == KIO::SlaveBase::SSLMessageBox) {
        data.insert(UserNotificationHandler::MSG_META_DATA, d->sslMetaData.toVariant());
    }

    globalUserNotificationHandler()->requestMessageBox(this, type, data);
}

void SlaveInterfacePrivate::slotHostInfo(const QHostInfo &info)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream <<  info.hostName() << info.addresses() << info.error() << info.errorString();
    connection->send(CMD_HOST_INFO, data);
}

#include "moc_slaveinterface.cpp"
