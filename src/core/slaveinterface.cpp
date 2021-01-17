/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slaveinterface.h"
#include "slaveinterface_p.h"
#include "usernotificationhandler_p.h"

#include "slavebase.h"
#include "connection_p.h"
#include "commands_p.h"
#include "hostinfo.h"
#include <signal.h>
#include <KLocalizedString>
#include <time.h>

#include <QDebug>
#include <QDateTime>
#include <QDataStream>

using namespace KIO;

Q_GLOBAL_STATIC(UserNotificationHandler, globalUserNotificationHandler)

SlaveInterface::SlaveInterface(SlaveInterfacePrivate &dd, QObject *parent)
    : QObject(parent), d_ptr(&dd)
{
    connect(&d_ptr->speed_timer, &QTimer::timeout, this, &SlaveInterface::calcSpeed);
}

SlaveInterface::~SlaveInterface()
{
    // Note: no Debug() here (scheduler is deleted very late)

    delete d_ptr;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
void SlaveInterface::setConnection(Connection *connection)
{
    Q_D(SlaveInterface);
    d->connection = connection;
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
Connection *SlaveInterface::connection() const
{
    const Q_D(SlaveInterface);
    return d->connection;
}
#endif

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

    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 diff = currentTime - d->start_time;
    if (diff - d->last_time >= 900) {
        d->last_time = diff;
        if (d->nums == max_nums) {
            // let's hope gcc can optimize that well enough
            // otherwise I'd try memcpy :)
            for (unsigned int i = 1; i < max_nums; ++i) {
                d->times[i - 1] = d->times[i];
                d->sizes[i - 1] = d->sizes[i];
            }
            d->nums--;
        }
        d->times[d->nums] = diff;
        d->sizes[d->nums++] = d->filesize - d->offset;

        KIO::filesize_t lspeed = 1000 * (d->sizes[d->nums - 1] - d->sizes[0]) / (d->times[d->nums - 1] - d->times[0]);

//qDebug() << (long)d->filesize << diff
//          << long(d->sizes[d->nums-1] - d->sizes[0])
//          << d->times[d->nums-1] - d->times[0]
//          << long(lspeed) << double(d->filesize) / diff
//          << convertSize(lspeed)
//          << convertSize(long(double(d->filesize) / diff) * 1000);

        if (!lspeed) {
            d->nums = 1;
            d->times[0] = diff;
            d->sizes[0] = d->filesize - d->offset;
        }
        Q_EMIT speed(lspeed);
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
        Q_EMIT data(rawdata);
        break;
    case MSG_DATA_REQ:
        Q_EMIT dataReq();
        break;
    case MSG_OPENED:
        Q_EMIT open();
        break;
    case MSG_FINISHED:
        //qDebug() << "Finished [this = " << this << "]";
        d->offset = 0;
        d->speed_timer.stop();
        Q_EMIT finished();
        break;
    case MSG_STAT_ENTRY: {
        UDSEntry entry;
        stream >> entry;
        Q_EMIT statEntry(entry);
        break;
    }
    case MSG_LIST_ENTRIES: {
        UDSEntryList list;
        UDSEntry entry;

        while (!stream.atEnd()) {
            stream >> entry;
            list.append(entry);
        }

        Q_EMIT listEntries(list);
        break;
    }
    case MSG_RESUME: { // From the put job
        d->offset = readFilesize_t(stream);
        Q_EMIT canResume(d->offset);
        break;
    }
    case MSG_CANRESUME: // From the get job
        d->filesize = d->offset;
        Q_EMIT canResume(0); // the arg doesn't matter
        break;
    case MSG_ERROR:
        stream >> i >> str1;
        //qDebug() << "error " << i << " " << str1;
        Q_EMIT error(i, str1);
        break;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 45)
    case MSG_SLAVE_STATUS:
#endif
    case MSG_SLAVE_STATUS_V2: {
        qint64 pid;
        QByteArray protocol;
        stream >> pid >> protocol >> str1 >> b;
        Q_EMIT slaveStatus(pid, protocol, str1, (b != 0));
        break;
    }
    case MSG_CONNECTED:
        Q_EMIT connected();
        break;
    case MSG_WRITTEN: {
        KIO::filesize_t size = readFilesize_t(stream);
        Q_EMIT written(size);
        break;
    }
    case INF_TOTAL_SIZE: {
        KIO::filesize_t size = readFilesize_t(stream);
        d->start_time = QDateTime::currentMSecsSinceEpoch();
        d->last_time = 0;
        d->filesize = d->offset;
        d->sizes[0] = d->filesize - d->offset;
        d->times[0] = 0;
        d->nums = 1;
        d->speed_timer.start(1000);
        d->slave_calcs_speed = false;
        Q_EMIT totalSize(size);
        break;
    }
    case INF_PROCESSED_SIZE: {
        KIO::filesize_t size = readFilesize_t(stream);
        Q_EMIT processedSize(size);
        d->filesize = size;
        break;
    }
    case INF_POSITION: {
        KIO::filesize_t pos = readFilesize_t(stream);
        Q_EMIT position(pos);
        break;
    }
    case INF_TRUNCATED: {
        KIO::filesize_t length = readFilesize_t(stream);
        Q_EMIT truncated(length);
        break;
    }
    case INF_SPEED:
        stream >> ul;
        d->slave_calcs_speed = true;
        d->speed_timer.stop();
        Q_EMIT speed(ul);
        break;
#if KIOCORE_BUILD_DEPRECATED_SINCE(3, 0)
    case INF_GETTING_FILE:
        break;
#endif
    case INF_ERROR_PAGE:
        Q_EMIT errorPage();
        break;
    case INF_REDIRECTION: {
        QUrl url;
        stream >> url;
        Q_EMIT redirection(url);
        break;
    }
    case INF_MIME_TYPE:
        stream >> str1;
        Q_EMIT mimeType(str1);
        if (!d->connection->suspended()) {
            d->connection->sendnow(CMD_NONE, QByteArray());
        }
        break;
    case INF_WARNING:
        stream >> str1;
        Q_EMIT warning(str1);
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
        Q_EMIT infoMessage(msg);
        break;
    }
    case INF_META_DATA: {
        MetaData m;
        stream >> m;
        if (m.contains(QLatin1String("ssl_in_use"))) {
            const QLatin1String ssl_("ssl_");
            const MetaData &constM = m;
            for (MetaData::ConstIterator it = constM.lowerBound(ssl_); it != constM.constEnd(); ++it) {
                if (it.key().startsWith(ssl_)) {
                    d->sslMetaData.insert(it.key(), it.value());
                } else {
                    // we're past the ssl_* entries; remember that QMap is ordered.
                    break;
                }
            }
        } else if (m.contains(QStringLiteral("privilege_conf_details"))) { // KF6 TODO Remove this conditional.
            d->privilegeConfMetaData = m;
        }
        Q_EMIT metaData(m);
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
        Q_EMIT needSubUrlData();
        break;
    }
    case MSG_HOST_INFO_REQ: {
        QString hostName;
        stream >> hostName;
        HostInfo::lookupHost(hostName, this, SLOT(slotHostInfo(QHostInfo)));
        break;
    }
    case MSG_PRIVILEGE_EXEC:
        Q_EMIT privilegeOperationRequested();
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
    } else if (type == KIO::SlaveBase::WarningContinueCancelDetailed) { // KF6 TODO Remove
        data.insert(UserNotificationHandler::MSG_META_DATA, d->privilegeConfMetaData.toVariant());
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
