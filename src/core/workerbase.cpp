/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>
    SPDX-FileCopyrightText: 2019-2022 Harald Sitter <sitter@kde.org>
*/

#include "workerbase.h"
#include "authinfo.h"
#include "ioworker_defaults.h"
#include "kremoteencoding.h"
#include "workerbase_p.h"
#include "workerinterface_p.h"

#include <commands_p.h>

#include <QThread>

#ifndef Q_OS_ANDROID
#include <KCrash>
#endif

#include <KLocalizedString>

extern "C" {
static void sigpipe_handler(int sig);
}

static volatile bool slaveWriteError = false;

#ifdef Q_OS_UNIX
static KIO::WorkerBase *globalSlave;

extern "C" {
static void genericsig_handler(int sigNumber)
{
    ::signal(sigNumber, SIG_IGN);
    // WABA: Don't do anything that requires malloc, we can deadlock on it since
    // a SIGTERM signal can come in while we are in malloc/free.
    // qDebug()<<"kioslave : exiting due to signal "<<sigNumber;
    // set the flag which will be checked in dispatchLoop() and which *should* be checked
    // in lengthy operations in the various slaves
    if (globalSlave != nullptr) {
        globalSlave->setKillFlag();
    }
    ::signal(SIGALRM, SIG_DFL);
    alarm(5); // generate an alarm signal in 5 seconds, in this time the slave has to exit
}
}
#endif

namespace KIO
{

WorkerBase::WorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket)
    : d(new WorkerBasePrivate(/*protocol, poolSocket, appSocket,*/ this))
{
    d->mProtocol = protocol;
    Q_ASSERT(!appSocket.isEmpty());
    d->poolSocket = QFile::decodeName(poolSocket);

    if (QThread::currentThread() == qApp->thread()) {
#ifndef Q_OS_ANDROID
        KCrash::initialize();
#endif

#ifdef Q_OS_UNIX
        struct sigaction act;
        act.sa_handler = sigpipe_handler;
        sigemptyset(&act.sa_mask);
        act.sa_flags = 0;
        sigaction(SIGPIPE, &act, nullptr);

        ::signal(SIGINT, &genericsig_handler);
        ::signal(SIGQUIT, &genericsig_handler);
        ::signal(SIGTERM, &genericsig_handler);

        globalSlave = this;
#endif
    }

    d->isConnectedToApp = true;

    // by kahl for netmgr (need a way to identify slaves)
    d->slaveid = QString::fromUtf8(protocol) + QString::number(getpid());
    d->resume = false;
    d->needSendCanResume = false;
    d->mapConfig = QMap<QString, QVariant>();
    d->onHold = false;
    //    d->processed_size = 0;
    d->totalSize = 0;
    connectWorker(QFile::decodeName(appSocket));

    d->remotefile = nullptr;
    d->inOpenLoop = false;
}

WorkerBase::~WorkerBase() = default;

void WorkerBase::dispatchLoop()
{
    while (!d->exit_loop) {
        if (d->nextTimeout.isValid() && (d->nextTimeout.hasExpired(d->nextTimeoutMsecs))) {
            QByteArray data = d->timeoutData;
            d->nextTimeout.invalidate();
            d->timeoutData = QByteArray();
            // do *not* finalize here
            WorkerResult result = special(data);
            if (!result.success()) {
                qWarning(KIO_CORE) << "TimeoutSpecialCommand failed with" << result.error() << result.errorString();
            }
        }

        Q_ASSERT(d->appConnection.inited());

        int ms = -1;
        if (d->nextTimeout.isValid()) {
            ms = qMax<int>(d->nextTimeoutMsecs - d->nextTimeout.elapsed(), 1);
        }

        int ret = -1;
        if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(ms)) {
            // dispatch application messages
            int cmd;
            QByteArray data;
            ret = d->appConnection.read(&cmd, data);

            if (ret != -1) {
                if (d->inOpenLoop) {
                    dispatchOpenCommand(cmd, data);
                } else {
                    dispatch(cmd, data);
                }
            }
        } else {
            ret = d->appConnection.isConnected() ? 0 : -1;
        }

        if (ret == -1) { // some error occurred, perhaps no more application
            // When the app exits, should the slave be put back in the pool ?
            if (!d->exit_loop && d->isConnectedToApp && !d->poolSocket.isEmpty()) {
                disconnectWorker();
                d->isConnectedToApp = false;
                closeConnection();
                d->updateTempAuthStatus();
                connectWorker(d->poolSocket);
            } else {
                break;
            }
        }

        // I think we get here when we were killed in dispatch() and not in select()
        if (wasKilled()) {
            // qDebug() << "worker was killed, returning";
            break;
        }

        // execute deferred deletes
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // execute deferred deletes
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void WorkerBase::connectWorker(const QString &address)
{
    d->appConnection.connectToRemote(QUrl(address));

    if (!d->appConnection.inited()) {
        /*qDebug() << "failed to connect to" << address << endl
                      << "Reason:" << d->appConnection.errorString();*/
        exit();
    }

    d->inOpenLoop = false;
}

void WorkerBase::disconnectWorker()
{
    d->appConnection.close();
}

void WorkerBase::setMetaData(const QString &key, const QString &value)
{
    d->mOutgoingMetaData.insert(key, value); // replaces existing key if already there
}

QString WorkerBase::metaData(const QString &key) const
{
    auto it = d->mIncomingMetaData.find(key);
    if (it != d->mIncomingMetaData.end()) {
        return *it;
    }
    return d->configData.value(key);
}

MetaData WorkerBase::allMetaData() const
{
    return d->mIncomingMetaData;
}

bool WorkerBase::hasMetaData(const QString &key) const
{
    if (d->mIncomingMetaData.contains(key)) {
        return true;
    }
    if (d->configData.contains(key)) {
        return true;
    }
    return false;
}

QMap<QString, QVariant> WorkerBase::mapConfig() const
{
    return d->mapConfig;
}

bool WorkerBase::configValue(const QString &key, bool defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toBool();
}

int WorkerBase::configValue(const QString &key, int defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toInt();
}

QString WorkerBase::configValue(const QString &key, const QString &defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toString();
}

KConfigGroup *WorkerBase::config()
{
    if (!d->config) {
        d->config = new KConfig(QString(), KConfig::SimpleConfig);

        d->configGroup = new KConfigGroup(d->config, QString());

        auto end = d->mapConfig.cend();
        for (auto it = d->mapConfig.cbegin(); it != end; ++it) {
            d->configGroup->writeEntry(it.key(), it->toString().toUtf8(), KConfigGroup::WriteConfigFlags());
        }
    }

    return d->configGroup;
}

void WorkerBase::sendMetaData()
{
    sendAndKeepMetaData();
    d->mOutgoingMetaData.clear();
}

void WorkerBase::sendAndKeepMetaData()
{
    if (!d->mOutgoingMetaData.isEmpty()) {
        KIO_DATA << d->mOutgoingMetaData;

        send(INF_META_DATA, data);
    }
}

KRemoteEncoding *WorkerBase::remoteEncoding()
{
    if (d->remotefile) {
        return d->remotefile;
    }

    const QByteArray charset(metaData(QStringLiteral("Charset")).toLatin1());
    return (d->remotefile = new KRemoteEncoding(charset.constData()));
}

void WorkerBase::data(const QByteArray &data)
{
    sendMetaData();
    send(MSG_DATA, data);
}

void WorkerBase::dataReq()
{
    // sendMetaData();
    if (d->needSendCanResume) {
        canResume(0);
    }
    send(MSG_DATA_REQ);
}

void WorkerBase::workerStatus(const QString &host, bool connected)
{
    qint64 pid = getpid();
    qint8 b = connected ? 1 : 0;
    KIO_DATA << pid << d->mProtocol << host << b << d->onHold << d->onHoldUrl << d->hasTempAuth();
    send(MSG_WORKER_STATUS, data);
}

void WorkerBase::canResume()
{
    send(MSG_CANRESUME);
}

void WorkerBase::totalSize(KIO::filesize_t _bytes)
{
    KIO_DATA << static_cast<quint64>(_bytes);
    send(INF_TOTAL_SIZE, data);

    // this one is usually called before the first item is listed in listDir()
    d->totalSize = _bytes;
}

void WorkerBase::processedSize(KIO::filesize_t _bytes)
{
    bool emitSignal = false;

    if (_bytes == d->totalSize) {
        emitSignal = true;
    } else {
        if (d->lastTimeout.isValid()) {
            emitSignal = d->lastTimeout.hasExpired(100); // emit size 10 times a second
        } else {
            emitSignal = true;
        }
    }

    if (emitSignal) {
        KIO_DATA << static_cast<quint64>(_bytes);
        send(INF_PROCESSED_SIZE, data);
        d->lastTimeout.start();
    }

    //    d->processed_size = _bytes;
}

void WorkerBase::written(KIO::filesize_t _bytes)
{
    KIO_DATA << static_cast<quint64>(_bytes);
    send(MSG_WRITTEN, data);
}

void WorkerBase::position(KIO::filesize_t _pos)
{
    KIO_DATA << static_cast<quint64>(_pos);
    send(INF_POSITION, data);
}

void WorkerBase::truncated(KIO::filesize_t _length)
{
    KIO_DATA << static_cast<quint64>(_length);
    send(INF_TRUNCATED, data);
}

void WorkerBase::speed(unsigned long _bytes_per_second)
{
    KIO_DATA << static_cast<quint32>(_bytes_per_second);
    send(INF_SPEED, data);
}

void WorkerBase::redirection(const QUrl &_url)
{
    KIO_DATA << _url;
    send(INF_REDIRECTION, data);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(6, 3)
void WorkerBase::errorPage()
{
}
#endif

static bool isSubCommand(int cmd)
{
    /* clang-format off */
    return cmd == CMD_REPARSECONFIGURATION
        || cmd == CMD_META_DATA
        || cmd == CMD_CONFIG
        || cmd == CMD_WORKER_STATUS;
    /* clang-format on */
}

void WorkerBase::mimeType(const QString &_type)
{
    qCDebug(KIO_CORE) << "detected mimetype" << _type;
    int cmd = CMD_NONE;
    do {
        if (wasKilled()) {
            break;
        }

        // Send the meta-data each time we send the MIME type.
        if (!d->mOutgoingMetaData.isEmpty()) {
            qCDebug(KIO_CORE) << "sending mimetype meta data";
            KIO_DATA << d->mOutgoingMetaData;
            send(INF_META_DATA, data);
        }
        KIO_DATA << _type;
        send(INF_MIME_TYPE, data);
        while (true) {
            cmd = 0;
            int ret = -1;
            if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(-1)) {
                ret = d->appConnection.read(&cmd, data);
            }
            if (ret == -1) {
                qCDebug(KIO_CORE) << "read error on app connection while sending mimetype";
                exit();
                break;
            }
            qCDebug(KIO_CORE) << "got reply after sending mimetype" << cmd;
            if (cmd == CMD_HOST) { // Ignore.
                continue;
            }
            if (!isSubCommand(cmd)) {
                break;
            }

            dispatch(cmd, data);
        }
    } while (cmd != CMD_NONE);
    d->mOutgoingMetaData.clear();
}

void WorkerBase::exit() // possibly called from another thread, only use atomics in here
{
    d->exit_loop = true;
    if (d->runInThread) {
        d->wasKilled = true;
    } else {
        // Using ::exit() here is too much (crashes in qdbus's qglobalstatic object),
        // so let's cleanly exit dispatchLoop() instead.
        // Update: we do need to call exit(), otherwise a long download (get()) would
        // keep going until it ends, even though the application exited.
        ::exit(255);
    }
}

void WorkerBase::warning(const QString &_msg)
{
    KIO_DATA << _msg;
    send(INF_WARNING, data);
}

void WorkerBase::infoMessage(const QString &_msg)
{
    KIO_DATA << _msg;
    send(INF_INFOMESSAGE, data);
}

void WorkerBase::statEntry(const UDSEntry &entry)
{
    KIO_DATA << entry;
    send(MSG_STAT_ENTRY, data);
}

void WorkerBase::listEntry(const UDSEntry &entry)
{
    // #366795: many slaves don't create an entry for ".", so we keep track if they do
    // and we provide a fallback in finished() otherwise.
    if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == QLatin1Char('.')) {
        d->m_rootEntryListed = true;
    }

    // We start measuring the time from the point we start filling the list
    if (d->pendingListEntries.isEmpty()) {
        d->m_timeSinceLastBatch.restart();
    }

    d->pendingListEntries.append(entry);

    // If more then KIO_MAX_SEND_BATCH_TIME time is passed, emit the current batch
    // Also emit if we have piled up a large number of entries already, to save memory (and time)
    if (d->m_timeSinceLastBatch.elapsed() > KIO_MAX_SEND_BATCH_TIME || d->pendingListEntries.size() > KIO_MAX_ENTRIES_PER_BATCH) {
        listEntries(d->pendingListEntries);
        d->pendingListEntries.clear();

        // Restart time
        d->m_timeSinceLastBatch.restart();
    }
}

void WorkerBase::listEntries(const UDSEntryList &list)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    for (const UDSEntry &entry : list) {
        stream << entry;
    }

    send(MSG_LIST_ENTRIES, data);
}

void WorkerBase::appConnectionMade()
{
} // No response!

void WorkerBase::setHost(QString const &, quint16, QString const &, QString const &)
{
} // No response!

WorkerResult WorkerBase::openConnection()
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CONNECT));
}

void WorkerBase::closeConnection()
{
} // No response!

WorkerResult WorkerBase::stat(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_STAT));
}

WorkerResult WorkerBase::put(QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_PUT));
}

WorkerResult WorkerBase::special(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SPECIAL));
}

WorkerResult WorkerBase::listDir(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_LISTDIR));
}

WorkerResult WorkerBase::get(QUrl const &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_GET));
}

WorkerResult WorkerBase::open(QUrl const &, QIODevice::OpenMode)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_OPEN));
}

WorkerResult WorkerBase::read(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_READ));
}

WorkerResult WorkerBase::write(const QByteArray &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_WRITE));
}

WorkerResult WorkerBase::seek(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SEEK));
}

WorkerResult WorkerBase::truncate(KIO::filesize_t)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_TRUNCATE));
}

WorkerResult WorkerBase::close()
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CLOSE));
}

WorkerResult WorkerBase::mimetype(QUrl const &url)
{
    return get(url);
}

WorkerResult WorkerBase::rename(QUrl const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_RENAME));
}

WorkerResult WorkerBase::symlink(QString const &, QUrl const &, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SYMLINK));
}

WorkerResult WorkerBase::copy(QUrl const &, QUrl const &, int, JobFlags)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_COPY));
}

WorkerResult WorkerBase::del(QUrl const &, bool)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_DEL));
}

WorkerResult WorkerBase::mkdir(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_MKDIR));
}

WorkerResult WorkerBase::chmod(QUrl const &, int)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHMOD));
}

WorkerResult WorkerBase::setModificationTime(QUrl const &, const QDateTime &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_SETMODIFICATIONTIME));
}

WorkerResult WorkerBase::chown(QUrl const &, const QString &, const QString &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_CHOWN));
}

WorkerResult WorkerBase::fileSystemFreeSpace(const QUrl &)
{
    return WorkerResult::fail(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(d->protocolName(), CMD_FILESYSTEMFREESPACE));
}

void WorkerBase::worker_status()
{
    workerStatus(QString(), false);
}

void WorkerBase::reparseConfiguration()
{
    // base implementation called by bridge
}

int WorkerBase::openPasswordDialog(AuthInfo &info, const QString &errorMsg)
{
    const long windowId = metaData(QStringLiteral("window-id")).toLong();
    const unsigned long userTimestamp = metaData(QStringLiteral("user-timestamp")).toULong();
    QString errorMessage;
    if (metaData(QStringLiteral("no-auth-prompt")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        errorMessage = QStringLiteral("<NoAuthPrompt>");
    } else {
        errorMessage = errorMsg;
    }

    AuthInfo dlgInfo(info);
    // Make sure the modified flag is not set.
    dlgInfo.setModified(false);
    // Prevent queryAuthInfo from caching the user supplied password since
    // we need the ioslaves to first authenticate against the server with
    // it to ensure it is valid.
    dlgInfo.setExtraField(QStringLiteral("skip-caching-on-query"), true);

#ifdef WITH_QTDBUS
    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    const int errCode = passwdServerClient->queryAuthInfo(&dlgInfo, errorMessage, windowId, userTimestamp);
    if (errCode == KJob::NoError) {
        info = dlgInfo;
    }
    return errCode;
#else
    return KJob::NoError;
#endif
}

int WorkerBase::messageBox(MessageBoxType type, const QString &text, const QString &title, const QString &primaryActionText, const QString &secondaryActionText)
{
    return messageBox(text, type, title, primaryActionText, secondaryActionText, QString());
}

int WorkerBase::messageBox(const QString &text,
                           MessageBoxType type,
                           const QString &title,
                           const QString &primaryActionText,
                           const QString &secondaryActionText,
                           const QString &dontAskAgainName)
{
    KIO_DATA << static_cast<qint32>(type) << text << title << primaryActionText << secondaryActionText << dontAskAgainName;
    send(INF_MESSAGEBOX, data);
    if (waitForAnswer(CMD_MESSAGEBOXANSWER, 0, data) != -1) {
        QDataStream stream(data);
        int answer;
        stream >> answer;
        return answer;
    } else {
        return 0; // communication failure
    }
}

int WorkerBase::sslError(const QVariantMap &sslData)
{
    KIO_DATA << sslData;
    send(INF_SSLERROR, data);
    if (waitForAnswer(CMD_SSLERRORANSWER, 0, data) != -1) {
        QDataStream stream(data);
        int answer;
        stream >> answer;
        return answer;
    } else {
        return 0; // communication failure
    }
}

bool WorkerBase::canResume(KIO::filesize_t offset)
{
    // qDebug() << "offset=" << KIO::number(offset);
    d->needSendCanResume = false;
    KIO_DATA << static_cast<quint64>(offset);
    send(MSG_RESUME, data);
    if (offset) {
        int cmd;
        if (waitForAnswer(CMD_RESUMEANSWER, CMD_NONE, data, &cmd) != -1) {
            // qDebug() << "returning" << (cmd == CMD_RESUMEANSWER);
            return cmd == CMD_RESUMEANSWER;
        } else {
            return false;
        }
    } else { // No resuming possible -> no answer to wait for
        return true;
    }
}

int WorkerBase::waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd)
{
    int cmd = 0;
    int result = -1;
    for (;;) {
        if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(-1)) {
            result = d->appConnection.read(&cmd, data);
        }
        if (result == -1) {
            // qDebug() << "read error.";
            return -1;
        }

        if (cmd == expected1 || cmd == expected2) {
            if (pCmd) {
                *pCmd = cmd;
            }
            return result;
        }
        if (isSubCommand(cmd)) {
            dispatch(cmd, data);
        } else {
            qFatal("Fatal Error: Got cmd %d, while waiting for an answer!", cmd);
        }
    }
}

int WorkerBase::readData(QByteArray &buffer)
{
    int result = waitForAnswer(MSG_DATA, 0, buffer);
    // qDebug() << "readData: length = " << result << " ";
    return result;
}

void WorkerBase::setTimeoutSpecialCommand(int timeout, const QByteArray &data)
{
    if (timeout > 0) {
        d->nextTimeoutMsecs = timeout * 1000; // from seconds to milliseconds
        d->nextTimeout.start();
    } else if (timeout == 0) {
        d->nextTimeoutMsecs = 1000; // Immediate timeout
        d->nextTimeout.start();
    } else {
        d->nextTimeout.invalidate(); // Canceled
    }

    d->timeoutData = data;
}

bool WorkerBase::checkCachedAuthentication(AuthInfo &info)
{
#ifdef WITH_QTDBUS
    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    return (passwdServerClient->checkAuthInfo(&info, metaData(QStringLiteral("window-id")).toLong(), metaData(QStringLiteral("user-timestamp")).toULong()));
#else
    return false;
#endif
}

bool WorkerBase::cacheAuthentication(const AuthInfo &info)
{
#ifdef WITH_QTDBUS
    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    passwdServerClient->addAuthInfo(info, metaData(QStringLiteral("window-id")).toLongLong());
#endif
    return true;
}

int WorkerBase::connectTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_CONNECT_TIMEOUT;
}

int WorkerBase::proxyConnectTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ProxyConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_PROXY_CONNECT_TIMEOUT;
}

int WorkerBase::responseTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ResponseTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_RESPONSE_TIMEOUT;
}

int WorkerBase::readTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ReadTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_READ_TIMEOUT;
}

bool WorkerBase::wasKilled() const
{
    return d->wasKilled;
}

void WorkerBase::lookupHost(const QString &host)
{
    KIO_DATA << host;
    send(MSG_HOST_INFO_REQ, data);
}

int WorkerBase::waitForHostInfo(QHostInfo &info)
{
    QByteArray data;
    int result = waitForAnswer(CMD_HOST_INFO, 0, data);

    if (result == -1) {
        info.setError(QHostInfo::UnknownError);
        info.setErrorString(i18n("Unknown Error"));
        return result;
    }

    QDataStream stream(data);
    QString hostName;
    QList<QHostAddress> addresses;
    int error;
    QString errorString;

    stream >> hostName >> addresses >> error >> errorString;

    info.setHostName(hostName);
    info.setAddresses(addresses);
    info.setError(QHostInfo::HostInfoError(error));
    info.setErrorString(errorString);

    return result;
}

PrivilegeOperationStatus WorkerBase::requestPrivilegeOperation(const QString &operationDetails)
{
    if (d->m_privilegeOperationStatus == OperationNotAllowed) {
        QByteArray buffer;
        send(MSG_PRIVILEGE_EXEC);
        waitForAnswer(MSG_PRIVILEGE_EXEC, 0, buffer);
        QDataStream ds(buffer);
        ds >> d->m_privilegeOperationStatus >> d->m_warningTitle >> d->m_warningMessage;
    }

    if (metaData(QStringLiteral("UnitTesting")) != QLatin1String("true") && d->m_privilegeOperationStatus == OperationAllowed && !d->m_confirmationAsked) {
        // WORKER_MESSAGEBOX_DETAILS_HACK
        // SlaveBase::messageBox() overloads miss a parameter to pass an details argument.
        // As workaround details are passed instead via metadata before and then cached by the WorkerInterface,
        // to be used in the upcoming messageBox call (needs WarningContinueCancelDetailed type)
        // TODO: add a messageBox() overload taking details and use here,
        // then remove or adapt all code marked with WORKER_MESSAGEBOX_DETAILS
        setMetaData(QStringLiteral("privilege_conf_details"), operationDetails);
        sendMetaData();

        int result = messageBox(d->m_warningMessage, WarningContinueCancelDetailed, d->m_warningTitle, QString(), QString(), QString());
        d->m_privilegeOperationStatus = result == Continue ? OperationAllowed : OperationCanceled;
        d->m_confirmationAsked = true;
    }

    return KIO::PrivilegeOperationStatus(d->m_privilegeOperationStatus);
}

void WorkerBase::addTemporaryAuthorization(const QString &action)
{
    d->m_tempAuths.insert(action);
}

class WorkerResultPrivate
{
public:
    bool success;
    int error;
    QString errorString;
};

WorkerResult::~WorkerResult() = default;

WorkerResult::WorkerResult(const WorkerResult &rhs)
    : d(std::make_unique<WorkerResultPrivate>(*rhs.d))
{
}

WorkerResult &WorkerResult::operator=(const WorkerResult &rhs)
{
    if (this == &rhs) {
        return *this;
    }
    d = std::make_unique<WorkerResultPrivate>(*rhs.d);
    return *this;
}

WorkerResult::WorkerResult(WorkerResult &&) noexcept = default;
WorkerResult &WorkerResult::operator=(WorkerResult &&) noexcept = default;

bool WorkerResult::success() const
{
    return d->success;
}

int WorkerResult::error() const
{
    return d->error;
}

QString WorkerResult::errorString() const
{
    return d->errorString;
}

Q_REQUIRED_RESULT WorkerResult WorkerResult::fail(int _error, const QString &_errorString)
{
    return WorkerResult(std::make_unique<WorkerResultPrivate>(WorkerResultPrivate{false, _error, _errorString}));
}

Q_REQUIRED_RESULT WorkerResult WorkerResult::pass()
{
    return WorkerResult(std::make_unique<WorkerResultPrivate>(WorkerResultPrivate{true, 0, QString()}));
}

WorkerResult::WorkerResult(std::unique_ptr<WorkerResultPrivate> &&dptr)
    : d(std::move(dptr))
{
}

void WorkerBase::setIncomingMetaData(const KIO::MetaData &metaData)
{
    d->mIncomingMetaData = metaData;
}

void WorkerBase::finalize(const WorkerResult &result)
{
    if (!result.success()) {
        error(result.error(), result.errorString());
        return;
    }
    finished();
}

void WorkerBase::maybeError(const WorkerResult &result)
{
    if (!result.success()) {
        error(result.error(), result.errorString());
    }
}

void WorkerBase::dispatchOpenCommand(int command, const QByteArray &data)
{
    QDataStream stream(data);

    switch (command) {
    case CMD_READ: {
        KIO::filesize_t bytes;
        stream >> bytes;
        maybeError(read(bytes));
        break;
    }
    case CMD_WRITE: {
        maybeError(write(data));
        break;
    }
    case CMD_SEEK: {
        KIO::filesize_t offset;
        stream >> offset;
        maybeError(seek(offset));
        break;
    }
    case CMD_TRUNCATE: {
        KIO::filesize_t length;
        stream >> length;
        maybeError(truncate(length));
        break;
    }
    case CMD_NONE:
        break;
    case CMD_CLOSE:
        close(); // TODO? // must call finish(), which will set d->inOpenLoop=false
        break;
    default:
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KIO.
        break;
    }
}

void WorkerBase::dispatch(int command, const QByteArray &data)
{
    QDataStream stream(data);

    QUrl url;
    int i;

    switch (command) {
    case CMD_HOST: {
        QString passwd;
        QString host;
        QString user;
        quint16 port;
        stream >> host >> port >> user >> passwd;
        setHost(host, port, user, passwd);
        break;
    }
    case CMD_CONNECT: {
        const WorkerResult result = openConnection();
        if (!result.success()) {
            error(result.error(), result.errorString());
            return;
        }

        break;
    }
    case CMD_DISCONNECT: {
        closeConnection();
        break;
    }
    case CMD_WORKER_STATUS: {
        workerStatus(QString(), false);
        // TODO verify that the slave has called slaveStatus()?
        break;
    }
    case CMD_REPARSECONFIGURATION: {
        reparseConfiguration();
        break;
    }
    case CMD_CONFIG: {
        stream >> d->configData;
        d->rebuildConfig();
        delete d->remotefile;
        d->remotefile = nullptr;
        break;
    }
    case CMD_GET: {
        stream >> url;
        finalize(get(url));
        break;
    }
    case CMD_OPEN: {
        stream >> url >> i;
        QIODevice::OpenMode mode = QFlag(i);
        const WorkerResult result = open(url, mode);
        if (!result.success()) {
            error(result.error(), result.errorString());
            return;
        }

        break;
    }
    case CMD_PUT: {
        int permissions;
        qint8 iOverwrite;
        qint8 iResume;
        stream >> url >> iOverwrite >> iResume >> permissions;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        if (iResume != 0) {
            flags |= Resume;
        }

        // Remember that we need to send canResume(), TransferJob is expecting
        // it. Well, in theory this shouldn't be done if resume is true.
        //   (the resume bool is currently unused)
        d->needSendCanResume = true /* !resume */;

        finalize(put(url, permissions, flags));
        break;
    }
    case CMD_STAT: {
        stream >> url;
        finalize(stat(url));
        break;
    }
    case CMD_MIMETYPE: {
        stream >> url;
        finalize(mimetype(url));
        break;
    }
    case CMD_LISTDIR: {
        stream >> url;
        finalize(listDir(url));
        break;
    }
    case CMD_MKDIR: {
        stream >> url >> i;
        finalize(mkdir(url, 0 /*permissions TODO*/));
        break;
    }
    case CMD_RENAME: {
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        finalize(rename(url, url2, flags));
        break;
    }
    case CMD_SYMLINK: {
        qint8 iOverwrite;
        QString target;
        stream >> target >> url >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        finalize(symlink(target, url, flags));
        break;
    }
    case CMD_COPY: {
        int permissions;
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> permissions >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        finalize(copy(url, url2, permissions, flags));
        break;
    }
    case CMD_DEL: {
        qint8 isFile;
        stream >> url >> isFile;
        finalize(del(url, isFile != 0));
        break;
    }
    case CMD_CHMOD: {
        stream >> url >> i;
        finalize(chmod(url, i));
        break;
    }
    case CMD_CHOWN: {
        QString owner;
        QString group;
        stream >> url >> owner >> group;
        finalize(chown(url, owner, group));
        break;
    }
    case CMD_SETMODIFICATIONTIME: {
        QDateTime dt;
        stream >> url >> dt;
        finalize(setModificationTime(url, dt));
        break;
    }
    case CMD_SPECIAL: {
        finalize(special(data));
        break;
    }
    case CMD_META_DATA: {
        // qDebug() << "(" << getpid() << ") Incoming meta-data...";
        stream >> d->mIncomingMetaData;
        d->rebuildConfig();
        break;
    }
    case CMD_NONE: {
        qCWarning(KIO_CORE) << "Got unexpected CMD_NONE!";
        break;
    }
    case CMD_FILESYSTEMFREESPACE: {
        stream >> url;

        finalize(fileSystemFreeSpace(url));
        break;
    }
    default: {
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KIO.
        break;
    }
    }
}

void WorkerBase::send(int cmd, const QByteArray &arr)
{
    if (d->runInThread) {
        if (!d->appConnection.send(cmd, arr)) {
            exit();
        }
    } else {
        slaveWriteError = false;
        if (!d->appConnection.send(cmd, arr))
        // Note that slaveWriteError can also be set by sigpipe_handler
        {
            slaveWriteError = true;
        }
        if (slaveWriteError) {
            qCWarning(KIO_CORE) << "An error occurred during write. The worker terminates now.";
            exit();
        }
    }
}

void WorkerBase::setKillFlag()
{
    d->wasKilled = true;
}

void WorkerBase::setRunInThread(bool b)
{
    d->runInThread = b;
}

void WorkerBase::error(int _errid, const QString &_text)
{
    d->mIncomingMetaData.clear(); // Clear meta data
    d->rebuildConfig();
    d->mOutgoingMetaData.clear();
    KIO_DATA << static_cast<qint32>(_errid) << _text;

    send(MSG_ERROR, data);
    // reset
    d->totalSize = 0;
    d->inOpenLoop = false;
    d->m_confirmationAsked = false;
    d->m_privilegeOperationStatus = OperationNotAllowed;
}

void WorkerBase::finished()
{
    if (!d->pendingListEntries.isEmpty()) {
        if (!d->m_rootEntryListed) {
            qCWarning(KIO_CORE) << "UDSEntry for '.' not found, creating a default one. Please fix the" << QCoreApplication::applicationName() << "KIO worker.";
            KIO::UDSEntry entry;
            entry.reserve(4);
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, 0);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IXOTH);
            d->pendingListEntries.append(entry);
        }

        listEntries(d->pendingListEntries);
        d->pendingListEntries.clear();
    }

    d->mIncomingMetaData.clear(); // Clear meta data
    d->rebuildConfig();
    sendMetaData();
    send(MSG_FINISHED);

    // reset
    d->totalSize = 0;
    d->inOpenLoop = false;
    d->m_rootEntryListed = false;
    d->m_confirmationAsked = false;
    d->m_privilegeOperationStatus = OperationNotAllowed;
}

KIOCORE_EXPORT QString unsupportedActionErrorString(const QString &protocol, int cmd)
{
    switch (cmd) {
    case CMD_CONNECT:
        return i18n("Opening connections is not supported with the protocol %1.", protocol);
    case CMD_DISCONNECT:
        return i18n("Closing connections is not supported with the protocol %1.", protocol);
    case CMD_STAT:
        return i18n("Accessing files is not supported with the protocol %1.", protocol);
    case CMD_PUT:
        return i18n("Writing to %1 is not supported.", protocol);
    case CMD_SPECIAL:
        return i18n("There are no special actions available for protocol %1.", protocol);
    case CMD_LISTDIR:
        return i18n("Listing folders is not supported for protocol %1.", protocol);
    case CMD_GET:
        return i18n("Retrieving data from %1 is not supported.", protocol);
    case CMD_MIMETYPE:
        return i18n("Retrieving mime type information from %1 is not supported.", protocol);
    case CMD_RENAME:
        return i18n("Renaming or moving files within %1 is not supported.", protocol);
    case CMD_SYMLINK:
        return i18n("Creating symlinks is not supported with protocol %1.", protocol);
    case CMD_COPY:
        return i18n("Copying files within %1 is not supported.", protocol);
    case CMD_DEL:
        return i18n("Deleting files from %1 is not supported.", protocol);
    case CMD_MKDIR:
        return i18n("Creating folders is not supported with protocol %1.", protocol);
    case CMD_CHMOD:
        return i18n("Changing the attributes of files is not supported with protocol %1.", protocol);
    case CMD_CHOWN:
        return i18n("Changing the ownership of files is not supported with protocol %1.", protocol);
    case CMD_OPEN:
        return i18n("Opening files is not supported with protocol %1.", protocol);
    default:
        return i18n("Protocol %1 does not support action %2.", protocol, cmd);
    } /*end switch*/
}

} // namespace KIO

static void sigpipe_handler(int)
{
    // We ignore a SIGPIPE in slaves.
    // A SIGPIPE can happen in two cases:
    // 1) Communication error with application.
    // 2) Communication error with network.
    slaveWriteError = true;

    // Don't add anything else here, especially no debug output
}
