/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "workerinterface_p.h"

#include "commands_p.h"
#include "connection_p.h"
#include "hostinfo.h"
#include "kiocoredebug.h"
#include "usernotificationhandler_p.h"
#include "workerbase.h"

#include <KLocalizedString>

#include <QDataStream>
#include <QDateTime>

using namespace KIO;

Q_GLOBAL_STATIC(UserNotificationHandler, globalUserNotificationHandler)

WorkerInterface::WorkerInterface(QObject *parent)
    : QObject(parent)
{
    connect(&m_speed_timer, &QTimer::timeout, this, &WorkerInterface::calcSpeed);
}

WorkerInterface::~WorkerInterface()
{
    // Note: no Debug() here (scheduler is deleted very late)

    delete m_connection;
}

static KIO::filesize_t readFilesize_t(QDataStream &stream)
{
    KIO::filesize_t result;
    stream >> result;
    return result;
}

bool WorkerInterface::dispatch()
{
    Q_ASSERT(m_connection);

    int cmd;
    QByteArray data;

    int ret = m_connection->read(&cmd, data);
    if (ret == -1) {
        return false;
    }

    return dispatch(cmd, data);
}

void WorkerInterface::calcSpeed()
{
    if (m_worker_calcs_speed || !m_connection->isConnected()) { // killing a job results in disconnection but the timer never stops
        m_speed_timer.stop();
        return;
    }

    const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    const qint64 diff = currentTime - m_start_time;
    if (diff - m_last_time >= 900) {
        m_last_time = diff;
        if (m_nums == max_nums) {
            // let's hope gcc can optimize that well enough
            // otherwise I'd try memcpy :)
            for (unsigned int i = 1; i < max_nums; ++i) {
                m_times[i - 1] = m_times[i];
                m_sizes[i - 1] = m_sizes[i];
            }
            m_nums--;
        }
        m_times[m_nums] = diff;
        m_sizes[m_nums++] = m_filesize - m_offset;

        KIO::filesize_t lspeed = 1000 * (m_sizes[m_nums - 1] - m_sizes[0]) / (m_times[m_nums - 1] - m_times[0]);

        // qDebug() << (long)m_filesize << diff
        //          << long(m_sizes[m_nums-1] - m_sizes[0])
        //          << m_times[m_nums-1] - m_times[0]
        //          << long(lspeed) << double(m_filesize) / diff
        //          << convertSize(lspeed)
        //          << convertSize(long(double(m_filesize) / diff) * 1000);

        if (!lspeed) {
            m_nums = 1;
            m_times[0] = diff;
            m_sizes[0] = m_filesize - m_offset;
        }
        Q_EMIT speed(lspeed);
    }
}

bool WorkerInterface::dispatch(int _cmd, const QByteArray &rawdata)
{
    // qDebug() << "dispatch " << _cmd;

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
        // qDebug() << "Finished [this = " << this << "]";
        m_offset = 0;
        m_speed_timer.stop();
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
        m_offset = readFilesize_t(stream);
        Q_EMIT canResume(m_offset);
        break;
    }
    case MSG_CANRESUME: // From the get job
        m_filesize = m_offset;
        Q_EMIT canResume(0); // the arg doesn't matter
        break;
    case MSG_ERROR:
        stream >> i >> str1;
        // qDebug() << "error " << i << " " << str1;
        Q_EMIT error(i, str1);
        break;
    case MSG_WORKER_STATUS: {
        qint64 pid;
        QByteArray protocol;
        stream >> pid >> protocol >> str1 >> b;
        Q_EMIT workerStatus(pid, protocol, str1, (b != 0));
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
        m_start_time = QDateTime::currentMSecsSinceEpoch();
        m_last_time = 0;
        m_filesize = m_offset;
        m_sizes[0] = m_filesize - m_offset;
        m_times[0] = 0;
        m_nums = 1;
        m_speed_timer.start(1000);
        m_worker_calcs_speed = false;
        Q_EMIT totalSize(size);
        break;
    }
    case INF_PROCESSED_SIZE: {
        KIO::filesize_t size = readFilesize_t(stream);
        Q_EMIT processedSize(size);
        m_filesize = size;
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
        m_worker_calcs_speed = true;
        m_speed_timer.stop();
        Q_EMIT speed(ul);
        break;
    case INF_ERROR_PAGE:
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
        if (!m_connection->suspended()) {
            m_connection->sendnow(CMD_NONE, QByteArray());
        }
        break;
    case INF_WARNING:
        stream >> str1;
        Q_EMIT warning(str1);
        break;
    case INF_MESSAGEBOX: {
        // qDebug() << "needs a msg box";
        QString text;
        QString title;
        QString primaryActionText;
        QString secondaryActionText;
        QString dontAskAgainName;
        int type;
        stream >> type >> text >> title >> primaryActionText >> secondaryActionText;
        if (stream.atEnd()) {
            messageBox(type, text, title, primaryActionText, secondaryActionText);
        } else {
            stream >> dontAskAgainName;
            messageBox(type, text, title, primaryActionText, secondaryActionText, dontAskAgainName);
        }
        break;
    }
    case INF_INFOMESSAGE: {
        QString msg;
        stream >> msg;
        Q_EMIT infoMessage(msg);
        break;
    }
    case INF_SSLERROR: {
        QVariantMap sslErrorData;
        stream >> sslErrorData;
        globalUserNotificationHandler->sslError(this, sslErrorData);
        break;
    }
    case INF_META_DATA: {
        MetaData m;
        stream >> m;
        if (auto it = m.constFind(QStringLiteral("privilege_conf_details")); it != m.cend()) {
            // see WORKER_MESSAGEBOX_DETAILS_HACK
            m_messageBoxDetails = it.value();
        }
        Q_EMIT metaData(m);
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
        qCWarning(KIO_CORE) << "Worker sends unknown command (" << _cmd << "), dropping worker.";
        return false;
    }
    return true;
}

void WorkerInterface::setOffset(KIO::filesize_t o)
{
    m_offset = o;
}

KIO::filesize_t WorkerInterface::offset() const
{
    return m_offset;
}

void WorkerInterface::sendResumeAnswer(bool resume)
{
    // qDebug() << "ok for resuming:" << resume;
    m_connection->sendnow(resume ? CMD_RESUMEANSWER : CMD_NONE, QByteArray());
}

void WorkerInterface::sendMessageBoxAnswer(int result)
{
    if (!m_connection) {
        return;
    }

    if (m_connection->suspended()) {
        m_connection->resume();
    }
    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << result;
    m_connection->sendnow(CMD_MESSAGEBOXANSWER, packedArgs);
    // qDebug() << "message box answer" << result;
}

void WorkerInterface::sendSslErrorAnswer(int result)
{
    if (!m_connection) {
        return;
    }

    if (m_connection->suspended()) {
        m_connection->resume();
    }
    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    stream << result;
    m_connection->sendnow(CMD_SSLERRORANSWER, packedArgs);
    // qDebug() << "message box answer" << result;
}

void WorkerInterface::messageBox(int type, const QString &text, const QString &title, const QString &primaryActionText, const QString &secondaryActionText)
{
    messageBox(type, text, title, primaryActionText, secondaryActionText, QString());
}

void WorkerInterface::messageBox(int type,
                                 const QString &text,
                                 const QString &title,
                                 const QString &primaryActionText,
                                 const QString &secondaryActionText,
                                 const QString &dontAskAgainName)
{
    if (m_connection) {
        m_connection->suspend();
    }

    QHash<UserNotificationHandler::MessageBoxDataType, QVariant> data;
    data.insert(UserNotificationHandler::MSG_TEXT, text);
    data.insert(UserNotificationHandler::MSG_TITLE, title);
    data.insert(UserNotificationHandler::MSG_PRIMARYACTION_TEXT, primaryActionText);
    data.insert(UserNotificationHandler::MSG_SECONDARYACTION_TEXT, secondaryActionText);
    data.insert(UserNotificationHandler::MSG_DONT_ASK_AGAIN, dontAskAgainName);

    // SMELL: the braindead way to support button icons
    // TODO: Fix this in KIO::WorkerBase.
    if (primaryActionText == i18n("&Details")) {
        data.insert(UserNotificationHandler::MSG_PRIMARYACTION_ICON, QLatin1String("help-about"));
    } else if (primaryActionText == i18n("&Forever")) {
        data.insert(UserNotificationHandler::MSG_PRIMARYACTION_ICON, QLatin1String("flag-green"));
    }

    if (secondaryActionText == i18n("Co&ntinue")) {
        data.insert(UserNotificationHandler::MSG_SECONDARYACTION_ICON, QLatin1String("arrow-right"));
    } else if (secondaryActionText == i18n("&Current Session only")) {
        data.insert(UserNotificationHandler::MSG_SECONDARYACTION_ICON, QLatin1String("chronometer"));
    }

    if (type == KIO::WorkerBase::WarningContinueCancelDetailed) { // see WORKER_MESSAGEBOX_DETAILS_HACK
        data.insert(UserNotificationHandler::MSG_DETAILS, m_messageBoxDetails);
    }

    globalUserNotificationHandler()->requestMessageBox(this, type, data);
}

void WorkerInterface::slotHostInfo(const QHostInfo &info)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << info.hostName() << info.addresses() << info.error() << info.errorString();
    m_connection->send(CMD_HOST_INFO, data);
}

#include "moc_workerinterface_p.cpp"
