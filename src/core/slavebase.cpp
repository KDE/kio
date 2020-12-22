/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2007 Thiago Macieira <thiago@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slavebase.h"

#include <config-kiocore.h>

#include <stdlib.h>
#include <qplatformdefs.h>
#include <signal.h>
#ifdef Q_OS_WIN
#include <process.h>
#endif

#include <QtGlobal>
#include <QFile>
#include <QList>
#include <QDateTime>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QDataStream>
#include <QMap>

#include <KConfig>
#include <KConfigGroup>
#include <KCrash>
#include <KLocalizedString>

#include "kremoteencoding.h"

#include "kioglobal_p.h"
#include "connection_p.h"
#include "commands_p.h"
#include "ioslave_defaults.h"
#include "slaveinterface.h"
#include "kpasswdserverclient.h"
#include "kiocoredebug.h"

#ifdef Q_OS_UNIX
#include <KAuth>
#endif

#if KIO_ASSERT_SLAVE_STATES
#define KIO_STATE_ASSERT(cond, where, what) Q_ASSERT_X(cond, where, what)
#else
#define KIO_STATE_ASSERT(cond, where, what) do { if (!(cond)) qCWarning(KIO_CORE) << what; } while (false)
#endif

extern "C" {
    static void sigpipe_handler(int sig);
}

using namespace KIO;

typedef QList<QByteArray> AuthKeysList;
typedef QMap<QString, QByteArray> AuthKeysMap;
#define KIO_DATA QByteArray data; QDataStream stream( &data, QIODevice::WriteOnly ); stream
#define KIO_FILESIZE_T(x) quint64(x)
static const int KIO_MAX_ENTRIES_PER_BATCH = 200;
static const int KIO_MAX_SEND_BATCH_TIME = 300;

namespace KIO
{

class SlaveBasePrivate
{
public:
    SlaveBase * const q;
    explicit SlaveBasePrivate(SlaveBase *owner)
        : q(owner)
        , nextTimeoutMsecs(0)
        , m_passwdServerClient(nullptr)
        , m_confirmationAsked(false)
        , m_privilegeOperationStatus(OperationNotAllowed)
    {
        if (!qEnvironmentVariableIsEmpty("KIOSLAVE_ENABLE_TESTMODE")) {
            QStandardPaths::setTestModeEnabled(true);
        }
        pendingListEntries.reserve(KIO_MAX_ENTRIES_PER_BATCH);
        appConnection.setReadMode(Connection::ReadMode::Polled);
    }
    ~SlaveBasePrivate()
    {
        delete m_passwdServerClient;
    }

    UDSEntryList pendingListEntries;
    QElapsedTimer m_timeSinceLastBatch;
    Connection appConnection;
    QString poolSocket;
    bool isConnectedToApp;

    QString slaveid;
    bool resume: 1;
    bool needSendCanResume: 1;
    bool onHold: 1;
    bool wasKilled: 1;
    bool inOpenLoop: 1;
    bool exit_loop: 1;
    MetaData configData;
    KConfig *config = nullptr;
    KConfigGroup *configGroup = nullptr;
    QMap<QString, QVariant> mapConfig;
    QUrl onHoldUrl;

    QElapsedTimer lastTimeout;
    QElapsedTimer nextTimeout;
    qint64 nextTimeoutMsecs;
    KIO::filesize_t totalSize;
    KRemoteEncoding *remotefile = nullptr;
    enum { Idle, InsideMethod, FinishedCalled, ErrorCalled } m_state;
    bool m_finalityCommand = true; // whether finished() or error() may/must be called
    QByteArray timeoutData;

    KPasswdServerClient *m_passwdServerClient = nullptr;
    bool m_rootEntryListed = false;

    bool m_confirmationAsked;
    QSet<QString> m_tempAuths;
    QString m_warningCaption;
    QString m_warningMessage;
    int m_privilegeOperationStatus;

    void updateTempAuthStatus()
    {
#ifdef Q_OS_UNIX
        QSet<QString>::iterator it = m_tempAuths.begin();
        while (it != m_tempAuths.end()) {
            KAuth::Action action(*it);
            if (action.status() != KAuth::Action::AuthorizedStatus) {
                it = m_tempAuths.erase(it);
            } else {
                ++it;
            }
        }
#endif
    }

    bool hasTempAuth() const
    {
        return !m_tempAuths.isEmpty();
    }

    // Reconstructs configGroup from configData and mIncomingMetaData
    void rebuildConfig()
    {
        mapConfig.clear();

        // mIncomingMetaData cascades over config, so we write config first,
        // to let it be overwritten
        MetaData::ConstIterator end = configData.constEnd();
        for (MetaData::ConstIterator it = configData.constBegin(); it != end; ++it) {
            mapConfig.insert(it.key(), it->toUtf8());
        }

        end = q->mIncomingMetaData.constEnd();
        for (MetaData::ConstIterator it = q->mIncomingMetaData.constBegin(); it != end; ++it) {
            mapConfig.insert(it.key(), it->toUtf8());
        }

        delete configGroup;
        configGroup = nullptr;
        delete config;
        config = nullptr;
    }

    bool finalState() const
    {
        return ((m_state == FinishedCalled) || (m_state == ErrorCalled));
    }

    void verifyState(const char *cmdName)
    {
        KIO_STATE_ASSERT(finalState(),
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("%1 did not call finished() or error()! Please fix the %2 KIO slave")
                                        .arg(QLatin1String(cmdName))
                                        .arg(QCoreApplication::applicationName())));
        // Force the command into finished state. We'll not reach this for Debug builds
        // that fail the assertion. For Release builds we'll have made sure that the
        // command is actually finished after the verification regardless of what
        // the slave did.
        if (!finalState()) {
            q->finished();
        }
    }

    void verifyErrorFinishedNotCalled(const char *cmdName)
    {
        KIO_STATE_ASSERT(!finalState(),
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("%1 called finished() or error(), but it's not supposed to! Please fix the %2 KIO slave")
                                        .arg(QLatin1String(cmdName))
                                        .arg(QCoreApplication::applicationName())));
    }

    KPasswdServerClient *passwdServerClient()
    {
        if (!m_passwdServerClient) {
            m_passwdServerClient = new KPasswdServerClient;
        }

        return m_passwdServerClient;
    }
};

}

static SlaveBase *globalSlave;

static volatile bool slaveWriteError = false;

static const char *s_protocol;

#ifdef Q_OS_UNIX
extern "C" {
    static void genericsig_handler(int sigNumber)
    {
        ::signal(sigNumber, SIG_IGN);
        //WABA: Don't do anything that requires malloc, we can deadlock on it since
        //a SIGTERM signal can come in while we are in malloc/free.
        //qDebug()<<"kioslave : exiting due to signal "<<sigNumber;
        //set the flag which will be checked in dispatchLoop() and which *should* be checked
        //in lengthy operations in the various slaves
        if (globalSlave != nullptr) {
            globalSlave->setKillFlag();
        }
        ::signal(SIGALRM, SIG_DFL);
        alarm(5);  //generate an alarm signal in 5 seconds, in this time the slave has to exit
    }
}
#endif

//////////////

SlaveBase::SlaveBase(const QByteArray &protocol,
                     const QByteArray &pool_socket,
                     const QByteArray &app_socket)
    : mProtocol(protocol),
      d(new SlaveBasePrivate(this))

{
    Q_ASSERT(!app_socket.isEmpty());
    d->poolSocket = QFile::decodeName(pool_socket);
    s_protocol = protocol.data();

    KCrash::initialize();

#ifdef Q_OS_UNIX
    struct sigaction act;
    act.sa_handler = sigpipe_handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGPIPE, &act, nullptr);

    ::signal(SIGINT, &genericsig_handler);
    ::signal(SIGQUIT, &genericsig_handler);
    ::signal(SIGTERM, &genericsig_handler);
#endif

    globalSlave = this;

    d->isConnectedToApp = true;

    // by kahl for netmgr (need a way to identify slaves)
    d->slaveid = QString::fromUtf8(protocol) + QString::number(getpid());
    d->resume = false;
    d->needSendCanResume = false;
    d->mapConfig = QMap<QString, QVariant>();
    d->onHold = false;
    d->wasKilled = false;
//    d->processed_size = 0;
    d->totalSize = 0;
    connectSlave(QFile::decodeName(app_socket));

    d->remotefile = nullptr;
    d->inOpenLoop = false;
    d->exit_loop = false;
}

SlaveBase::~SlaveBase()
{
    delete d->configGroup;
    delete d->config;
    delete d->remotefile;
    delete d;
    s_protocol = "";
}

void SlaveBase::dispatchLoop()
{
    while (!d->exit_loop) {
        if (d->nextTimeout.isValid() && (d->nextTimeout.hasExpired(d->nextTimeoutMsecs))) {
            QByteArray data = d->timeoutData;
            d->nextTimeout.invalidate();
            d->timeoutData = QByteArray();
            special(data);
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
                disconnectSlave();
                d->isConnectedToApp = false;
                closeConnection();
                d->updateTempAuthStatus();
                connectSlave(d->poolSocket);
            } else {
                break;
            }
        }

        //I think we get here when we were killed in dispatch() and not in select()
        if (wasKilled()) {
            //qDebug() << "slave was killed, returning";
            break;
        }

        // execute deferred deletes
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }

    // execute deferred deletes
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void SlaveBase::connectSlave(const QString &address)
{
    d->appConnection.connectToRemote(QUrl(address));

    if (!d->appConnection.inited()) {
        /*qDebug() << "failed to connect to" << address << endl
                      << "Reason:" << d->appConnection.errorString();*/
        exit();
    }

    d->inOpenLoop = false;
}

void SlaveBase::disconnectSlave()
{
    d->appConnection.close();
}

void SlaveBase::setMetaData(const QString &key, const QString &value)
{
    mOutgoingMetaData.insert(key, value); // replaces existing key if already there
}

QString SlaveBase::metaData(const QString &key) const
{
    auto it = mIncomingMetaData.find(key);
    if (it != mIncomingMetaData.end()) {
        return *it;
    }
    return d->configData.value(key);
}

MetaData SlaveBase::allMetaData() const
{
    return mIncomingMetaData;
}

bool SlaveBase::hasMetaData(const QString &key) const
{
    if (mIncomingMetaData.contains(key)) {
        return true;
    }
    if (d->configData.contains(key)) {
        return true;
    }
    return false;
}

QMap<QString, QVariant> SlaveBase::mapConfig() const
{
    return d->mapConfig;
}

bool SlaveBase::configValue(const QString &key, bool defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toBool();
}

int SlaveBase::configValue(const QString &key, int defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toInt();
}

QString SlaveBase::configValue(const QString &key, const QString &defaultValue) const
{
    return d->mapConfig.value(key, defaultValue).toString();
}

KConfigGroup *SlaveBase::config()
{
    if (!d->config) {
        d->config = new KConfig(QString(), KConfig::SimpleConfig);

        d->configGroup = new KConfigGroup(d->config, QString());

        auto end = d->mapConfig.cend();
        for (auto it = d->mapConfig.cbegin(); it != end; ++it)
        {
            d->configGroup->writeEntry(it.key(), it->toString().toUtf8(), KConfigGroup::WriteConfigFlags());
        }
    }

    return d->configGroup;
}

void SlaveBase::sendMetaData()
{
    sendAndKeepMetaData();
    mOutgoingMetaData.clear();
}

void SlaveBase::sendAndKeepMetaData()
{
    if (!mOutgoingMetaData.isEmpty()) {
        KIO_DATA << mOutgoingMetaData;

        send(INF_META_DATA, data);
    }
}

KRemoteEncoding *SlaveBase::remoteEncoding()
{
    if (d->remotefile) {
        return d->remotefile;
    }

    const QByteArray charset(metaData(QStringLiteral("Charset")).toLatin1());
    return (d->remotefile = new KRemoteEncoding(charset.constData()));
}

void SlaveBase::data(const QByteArray &data)
{
    sendMetaData();
    send(MSG_DATA, data);
}

void SlaveBase::dataReq()
{
    //sendMetaData();
    if (d->needSendCanResume) {
        canResume(0);
    }
    send(MSG_DATA_REQ);
}

void SlaveBase::opened()
{
    sendMetaData();
    send(MSG_OPENED);
    d->inOpenLoop = true;
}

void SlaveBase::error(int _errid, const QString &_text)
{
    KIO_STATE_ASSERT(d->m_finalityCommand,
                     Q_FUNC_INFO,
                     qUtf8Printable(QStringLiteral("error() was called, but it's not supposed to! Please fix the %1 KIO slave")
                                    .arg(QCoreApplication::applicationName())));

    if (d->m_state == d->ErrorCalled) {
        KIO_STATE_ASSERT(false,
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("error() called twice! Please fix the %1 KIO slave")
                                        .arg(QCoreApplication::applicationName())));
        return;
    } else if (d->m_state == d->FinishedCalled) {
        KIO_STATE_ASSERT(false,
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("error() called after finished()! Please fix the %1 KIO slave")
                                        .arg(QCoreApplication::applicationName())));
        return;
    }

    d->m_state = d->ErrorCalled;
    mIncomingMetaData.clear(); // Clear meta data
    d->rebuildConfig();
    mOutgoingMetaData.clear();
    KIO_DATA << static_cast<qint32>(_errid) << _text;

    send(MSG_ERROR, data);
    //reset
    d->totalSize = 0;
    d->inOpenLoop = false;
    d->m_confirmationAsked = false;
    d->m_privilegeOperationStatus = OperationNotAllowed;
}

void SlaveBase::connected()
{
    send(MSG_CONNECTED);
}

void SlaveBase::finished()
{
    if (!d->pendingListEntries.isEmpty()) {
        if (!d->m_rootEntryListed) {
            qCWarning(KIO_CORE) << "UDSEntry for '.' not found, creating a default one. Please fix the" << QCoreApplication::applicationName() << "KIO slave";
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

    KIO_STATE_ASSERT(d->m_finalityCommand,
                     Q_FUNC_INFO,
                     qUtf8Printable(QStringLiteral("finished() was called, but it's not supposed to! Please fix the %2 KIO slave")
                                    .arg(QCoreApplication::applicationName())));

    if (d->m_state == d->FinishedCalled) {
        KIO_STATE_ASSERT(false,
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("finished() called twice! Please fix the %1 KIO slave")
                                        .arg(QCoreApplication::applicationName())));
        return;
    } else if (d->m_state == d->ErrorCalled) {
        KIO_STATE_ASSERT(false,
                         Q_FUNC_INFO,
                         qUtf8Printable(QStringLiteral("finished() called after error()! Please fix the %1 KIO slave")
                                        .arg(QCoreApplication::applicationName())));
        return;
    }

    d->m_state = d->FinishedCalled;
    mIncomingMetaData.clear(); // Clear meta data
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

void SlaveBase::needSubUrlData()
{
    send(MSG_NEED_SUBURL_DATA);
}

void SlaveBase::slaveStatus(const QString &host, bool connected)
{
    qint64 pid = getpid();
    qint8 b = connected ? 1 : 0;
    KIO_DATA << pid << mProtocol << host << b << d->onHold << d->onHoldUrl << d->hasTempAuth();
    send(MSG_SLAVE_STATUS_V2, data);
}

void SlaveBase::canResume()
{
    send(MSG_CANRESUME);
}

void SlaveBase::totalSize(KIO::filesize_t _bytes)
{
    KIO_DATA << KIO_FILESIZE_T(_bytes);
    send(INF_TOTAL_SIZE, data);

    //this one is usually called before the first item is listed in listDir()
    d->totalSize = _bytes;
}

void SlaveBase::processedSize(KIO::filesize_t _bytes)
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
        KIO_DATA << KIO_FILESIZE_T(_bytes);
        send(INF_PROCESSED_SIZE, data);
        d->lastTimeout.start();
    }

    //    d->processed_size = _bytes;
}

void SlaveBase::written(KIO::filesize_t _bytes)
{
    KIO_DATA << KIO_FILESIZE_T(_bytes);
    send(MSG_WRITTEN, data);
}

void SlaveBase::position(KIO::filesize_t _pos)
{
    KIO_DATA << KIO_FILESIZE_T(_pos);
    send(INF_POSITION, data);
}

void SlaveBase::truncated(KIO::filesize_t _length)
{
    KIO_DATA << KIO_FILESIZE_T(_length);
    send(INF_TRUNCATED, data);
}

void SlaveBase::processedPercent(float /* percent */)
{
    //qDebug() << "STUB";
}

void SlaveBase::speed(unsigned long _bytes_per_second)
{
    KIO_DATA << static_cast<quint32>(_bytes_per_second);
    send(INF_SPEED, data);
}

void SlaveBase::redirection(const QUrl &_url)
{
    KIO_DATA << _url;
    send(INF_REDIRECTION, data);
}

void SlaveBase::errorPage()
{
    send(INF_ERROR_PAGE);
}

static bool isSubCommand(int cmd)
{
    return ((cmd == CMD_REPARSECONFIGURATION) ||
            (cmd == CMD_META_DATA) ||
            (cmd == CMD_CONFIG) ||
            (cmd == CMD_SUBURL) ||
            (cmd == CMD_SLAVE_STATUS) ||
            (cmd == CMD_SLAVE_CONNECT) ||
            (cmd == CMD_SLAVE_HOLD) ||
            (cmd == CMD_MULTI_GET));
}

void SlaveBase::mimeType(const QString &_type)
{
    //qDebug() << _type;
    int cmd;
    do {
        // Send the meta-data each time we send the MIME type.
        if (!mOutgoingMetaData.isEmpty()) {
            //qDebug() << "emitting meta data";
            KIO_DATA << mOutgoingMetaData;
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
                //qDebug() << "read error";
                exit();
            }
            //qDebug() << "got" << cmd;
            if (cmd == CMD_HOST) { // Ignore.
                continue;
            }
            if (!isSubCommand(cmd)) {
                break;
            }

            dispatch(cmd, data);
        }
    } while (cmd != CMD_NONE);
    mOutgoingMetaData.clear();
}

void SlaveBase::exit()
{
    d->exit_loop = true;
    // Using ::exit() here is too much (crashes in qdbus's qglobalstatic object),
    // so let's cleanly exit dispatchLoop() instead.
    // Update: we do need to call exit(), otherwise a long download (get()) would
    // keep going until it ends, even though the application exited.
    ::exit(255);
}

void SlaveBase::warning(const QString &_msg)
{
    KIO_DATA << _msg;
    send(INF_WARNING, data);
}

void SlaveBase::infoMessage(const QString &_msg)
{
    KIO_DATA << _msg;
    send(INF_INFOMESSAGE, data);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
bool SlaveBase::requestNetwork(const QString &host)
{
    KIO_DATA << host << d->slaveid;
    send(MSG_NET_REQUEST, data);

    if (waitForAnswer(INF_NETWORK_STATUS, 0, data) != -1) {
        bool status;
        QDataStream stream(data);
        stream >> status;
        return status;
    } else {
        return false;
    }
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
void SlaveBase::dropNetwork(const QString &host)
{
    KIO_DATA << host << d->slaveid;
    send(MSG_NET_DROP, data);
}
#endif

void SlaveBase::statEntry(const UDSEntry &entry)
{
    KIO_DATA << entry;
    send(MSG_STAT_ENTRY, data);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 0)
void SlaveBase::listEntry(const UDSEntry &entry, bool _ready)
{
    if (_ready) {
        // #366795: many slaves don't create an entry for ".", so we keep track if they do
        // and we provide a fallback in finished() otherwise.
        if (entry.stringValue(KIO::UDSEntry::UDS_NAME) == QLatin1Char('.')) {
            d->m_rootEntryListed = true;
        }
        listEntries(d->pendingListEntries);
        d->pendingListEntries.clear();
    } else {
        listEntry(entry);
    }
}
#endif

void SlaveBase::listEntry(const UDSEntry &entry)
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

void SlaveBase::listEntries(const UDSEntryList &list)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    for (const UDSEntry &entry : list) {
        stream << entry;
    }

    send(MSG_LIST_ENTRIES, data);
}

static void sigpipe_handler(int)
{
    // We ignore a SIGPIPE in slaves.
    // A SIGPIPE can happen in two cases:
    // 1) Communication error with application.
    // 2) Communication error with network.
    slaveWriteError = true;

    // Don't add anything else here, especially no debug output
}

void SlaveBase::setHost(QString const &, quint16, QString const &, QString const &)
{
}

KIOCORE_EXPORT QString KIO::unsupportedActionErrorString(const QString &protocol, int cmd)
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
    case CMD_SUBURL:
        return i18n("Using sub-URLs with %1 is not supported.", protocol);
    case CMD_MULTI_GET:
        return i18n("Multiple get is not supported with protocol %1.", protocol);
    case CMD_OPEN:
        return i18n("Opening files is not supported with protocol %1.", protocol);
    default:
        return i18n("Protocol %1 does not support action %2.", protocol, cmd);
    }/*end switch*/
}

void SlaveBase::openConnection()
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_CONNECT));
}
void SlaveBase::closeConnection()
{ } // No response!
void SlaveBase::stat(QUrl const &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_STAT));
}
void SlaveBase::put(QUrl const &, int, JobFlags)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_PUT));
}
void SlaveBase::special(const QByteArray &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SPECIAL));
}
void SlaveBase::listDir(QUrl const &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_LISTDIR));
}
void SlaveBase::get(QUrl const &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_GET));
}
void SlaveBase::open(QUrl const &, QIODevice::OpenMode)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_OPEN));
}
void SlaveBase::read(KIO::filesize_t)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_READ));
}
void SlaveBase::write(const QByteArray &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_WRITE));
}
void SlaveBase::seek(KIO::filesize_t)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SEEK));
}
void SlaveBase::close()
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_CLOSE));
}
void SlaveBase::mimetype(QUrl const &url)
{
    get(url);
}
void SlaveBase::rename(QUrl const &, QUrl const &, JobFlags)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_RENAME));
}
void SlaveBase::symlink(QString const &, QUrl const &, JobFlags)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SYMLINK));
}
void SlaveBase::copy(QUrl const &, QUrl const &, int, JobFlags)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_COPY));
}
void SlaveBase::del(QUrl const &, bool)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_DEL));
}
void SlaveBase::setLinkDest(const QUrl &, const QString &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SETLINKDEST));
}
void SlaveBase::mkdir(QUrl const &, int)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_MKDIR));
}
void SlaveBase::chmod(QUrl const &, int)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_CHMOD));
}
void SlaveBase::setModificationTime(QUrl const &, const QDateTime &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SETMODIFICATIONTIME));
}
void SlaveBase::chown(QUrl const &, const QString &, const QString &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_CHOWN));
}
void SlaveBase::setSubUrl(QUrl const &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_SUBURL));
}
void SlaveBase::multiGet(const QByteArray &)
{
    error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_MULTI_GET));
}

void SlaveBase::slave_status()
{
    slaveStatus(QString(), false);
}

void SlaveBase::reparseConfiguration()
{
    delete d->remotefile;
    d->remotefile = nullptr;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 24)
bool SlaveBase::openPasswordDialog(AuthInfo &info, const QString &errorMsg)
{
    const int errorCode = openPasswordDialogV2(info, errorMsg);
    return errorCode == KJob::NoError;
}
#endif

int SlaveBase::openPasswordDialogV2(AuthInfo &info, const QString &errorMsg)
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

    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    const int errCode = passwdServerClient->queryAuthInfo(&dlgInfo, errorMessage, windowId, userTimestamp);
    if (errCode == KJob::NoError) {
        info = dlgInfo;
    }
    return errCode;
}

int SlaveBase::messageBox(MessageBoxType type, const QString &text, const QString &caption,
                          const QString &buttonYes, const QString &buttonNo)
{
    return messageBox(text, type, caption, buttonYes, buttonNo, QString());
}

int SlaveBase::messageBox(const QString &text, MessageBoxType type, const QString &caption,
                          const QString &_buttonYes, const QString &_buttonNo,
                          const QString &dontAskAgainName)
{
    QString buttonYes = _buttonYes.isNull() ? i18n("&Yes") : _buttonYes;
    QString buttonNo = _buttonNo.isNull() ? i18n("&No") : _buttonNo;
    //qDebug() << "messageBox " << type << " " << text << " - " << caption << buttonYes << buttonNo;
    KIO_DATA << static_cast<qint32>(type) << text << caption << buttonYes << buttonNo << dontAskAgainName;
    send(INF_MESSAGEBOX, data);
    if (waitForAnswer(CMD_MESSAGEBOXANSWER, 0, data) != -1) {
        QDataStream stream(data);
        int answer;
        stream >> answer;
        //qDebug() << "got messagebox answer" << answer;
        return answer;
    } else {
        return 0;    // communication failure
    }
}

bool SlaveBase::canResume(KIO::filesize_t offset)
{
    //qDebug() << "offset=" << KIO::number(offset);
    d->needSendCanResume = false;
    KIO_DATA << KIO_FILESIZE_T(offset);
    send(MSG_RESUME, data);
    if (offset) {
        int cmd;
        if (waitForAnswer(CMD_RESUMEANSWER, CMD_NONE, data, &cmd) != -1) {
            //qDebug() << "returning" << (cmd == CMD_RESUMEANSWER);
            return cmd == CMD_RESUMEANSWER;
        } else {
            return false;
        }
    } else { // No resuming possible -> no answer to wait for
        return true;
    }
}

int SlaveBase::waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd)
{
    int cmd = 0;
    int result = -1;
    for (;;) {
        if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(-1)) {
            result = d->appConnection.read(&cmd, data);
        }
        if (result == -1) {
            //qDebug() << "read error.";
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

int SlaveBase::readData(QByteArray &buffer)
{
    int result = waitForAnswer(MSG_DATA, 0, buffer);
    //qDebug() << "readData: length = " << result << " ";
    return result;
}

void SlaveBase::setTimeoutSpecialCommand(int timeout, const QByteArray &data)
{
    if (timeout > 0) {
        d->nextTimeoutMsecs = timeout*1000; // from seconds to milliseconds
        d->nextTimeout.start();
    } else if (timeout == 0) {
        d->nextTimeoutMsecs = 1000;  // Immediate timeout
        d->nextTimeout.start();
    } else {
        d->nextTimeout.invalidate();  // Canceled
    }

    d->timeoutData = data;
}

void SlaveBase::dispatch(int command, const QByteArray &data)
{
    QDataStream stream(data);

    QUrl url;
    int i;

    d->m_finalityCommand = true; // default

    switch (command) {
    case CMD_HOST: {
        QString passwd;
        QString host, user;
        quint16 port;
        stream >> host >> port >> user >> passwd;
        d->m_state = d->InsideMethod;
        d->m_finalityCommand = false;
        setHost(host, port, user, passwd);
        d->m_state = d->Idle;
    } break;
    case CMD_CONNECT: {
        openConnection();
    } break;
    case CMD_DISCONNECT: {
        closeConnection();
    } break;
    case CMD_SLAVE_STATUS: {
        d->m_state = d->InsideMethod;
        d->m_finalityCommand = false;
        slave_status();
        // TODO verify that the slave has called slaveStatus()?
        d->m_state = d->Idle;
    } break;
    case CMD_SLAVE_CONNECT: {
        d->onHold = false;
        QString app_socket;
        QDataStream stream(data);
        stream >> app_socket;
        d->appConnection.send(MSG_SLAVE_ACK);
        disconnectSlave();
        d->isConnectedToApp = true;
        connectSlave(app_socket);
        virtual_hook(AppConnectionMade, nullptr);
    } break;
    case CMD_SLAVE_HOLD: {
        QUrl url;
        QDataStream stream(data);
        stream >> url;
        d->onHoldUrl = url;
        d->onHold = true;
        disconnectSlave();
        d->isConnectedToApp = false;
        // Do not close connection!
        connectSlave(d->poolSocket);
    } break;
    case CMD_REPARSECONFIGURATION: {
        d->m_state = d->InsideMethod;
        d->m_finalityCommand = false;
        reparseConfiguration();
        d->m_state = d->Idle;
    } break;
    case CMD_CONFIG: {
        stream >> d->configData;
        d->rebuildConfig();
        delete d->remotefile;
        d->remotefile = nullptr;
    } break;
    case CMD_GET: {
        stream >> url;
        d->m_state = d->InsideMethod;
        get(url);
        d->verifyState("get()");
        d->m_state = d->Idle;
    } break;
    case CMD_OPEN: {
        stream >> url >> i;
        QIODevice::OpenMode mode = QFlag(i);
        d->m_state = d->InsideMethod;
        open(url, mode); //krazy:exclude=syscalls
        d->m_state = d->Idle;
    } break;
    case CMD_PUT: {
        int permissions;
        qint8 iOverwrite, iResume;
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
        d->needSendCanResume = true   /* !resume */;

        d->m_state = d->InsideMethod;
        put(url, permissions, flags);
        d->verifyState("put()");
        d->m_state = d->Idle;
    } break;
    case CMD_STAT: {
        stream >> url;
        d->m_state = d->InsideMethod;
        stat(url);   //krazy:exclude=syscalls
        d->verifyState("stat()");
        d->m_state = d->Idle;
    } break;
    case CMD_MIMETYPE: {
        stream >> url;
        d->m_state = d->InsideMethod;
        mimetype(url);
        d->verifyState("mimetype()");
        d->m_state = d->Idle;
    } break;
    case CMD_LISTDIR: {
        stream >> url;
        d->m_state = d->InsideMethod;
        listDir(url);
        d->verifyState("listDir()");
        d->m_state = d->Idle;
    } break;
    case CMD_MKDIR: {
        stream >> url >> i;
        d->m_state = d->InsideMethod;
        mkdir(url, i);   //krazy:exclude=syscalls
        d->verifyState("mkdir()");
        d->m_state = d->Idle;
    } break;
    case CMD_RENAME: {
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        d->m_state = d->InsideMethod;
        rename(url, url2, flags);   //krazy:exclude=syscalls
        d->verifyState("rename()");
        d->m_state = d->Idle;
    } break;
    case CMD_SYMLINK: {
        qint8 iOverwrite;
        QString target;
        stream >> target >> url >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        d->m_state = d->InsideMethod;
        symlink(target, url, flags);
        d->verifyState("symlink()");
        d->m_state = d->Idle;
    } break;
    case CMD_COPY: {
        int permissions;
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> permissions >> iOverwrite;
        JobFlags flags;
        if (iOverwrite != 0) {
            flags |= Overwrite;
        }
        d->m_state = d->InsideMethod;
        copy(url, url2, permissions, flags);
        d->verifyState("copy()");
        d->m_state = d->Idle;
    } break;
    case CMD_DEL: {
        qint8 isFile;
        stream >> url >> isFile;
        d->m_state = d->InsideMethod;
        del(url, isFile != 0);
        d->verifyState("del()");
        d->m_state = d->Idle;
    } break;
    case CMD_CHMOD: {
        stream >> url >> i;
        d->m_state = d->InsideMethod;
        chmod(url, i);
        d->verifyState("chmod()");
        d->m_state = d->Idle;
    } break;
    case CMD_CHOWN: {
        QString owner, group;
        stream >> url >> owner >> group;
        d->m_state = d->InsideMethod;
        chown(url, owner, group);
        d->verifyState("chown()");
        d->m_state = d->Idle;
    } break;
    case CMD_SETMODIFICATIONTIME: {
        QDateTime dt;
        stream >> url >> dt;
        d->m_state = d->InsideMethod;
        setModificationTime(url, dt);
        d->verifyState("setModificationTime()");
        d->m_state = d->Idle;
    } break;
    case CMD_SPECIAL: {
        d->m_state = d->InsideMethod;
        special(data);
        d->verifyState("special()");
        d->m_state = d->Idle;
    } break;
    case CMD_META_DATA: {
        //qDebug() << "(" << getpid() << ") Incoming meta-data...";
        stream >> mIncomingMetaData;
        d->rebuildConfig();
    } break;
    case CMD_SUBURL: {
        stream >> url;
        d->m_state = d->InsideMethod;
        setSubUrl(url);
        d->verifyErrorFinishedNotCalled("setSubUrl()");
        d->m_state = d->Idle;
    } break;
    case CMD_NONE: {
        qCWarning(KIO_CORE) << "Got unexpected CMD_NONE!";
    } break;
    case CMD_MULTI_GET: {
        d->m_state = d->InsideMethod;
        multiGet(data);
        d->verifyState("multiGet()");
        d->m_state = d->Idle;
    } break;
    case CMD_FILESYSTEMFREESPACE: {
        stream >> url;

        void *data = static_cast<void *>(&url);

        d->m_state = d->InsideMethod;
        virtual_hook(GetFileSystemFreeSpace, data);
        d->verifyState("fileSystemFreeSpace()");
        d->m_state = d->Idle;
    } break;
    default: {
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KIO.
    } break;
    }
}

bool SlaveBase::checkCachedAuthentication(AuthInfo &info)
{
    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    return (passwdServerClient->checkAuthInfo(&info, metaData(QStringLiteral("window-id")).toLong(),
                                        metaData(QStringLiteral("user-timestamp")).toULong()));
}

void SlaveBase::dispatchOpenCommand(int command, const QByteArray &data)
{
    QDataStream stream(data);

    switch (command) {
    case CMD_READ: {
        KIO::filesize_t bytes;
        stream >> bytes;
        read(bytes);
        break;
    }
    case CMD_WRITE: {
        write(data);
        break;
    }
    case CMD_SEEK: {
        KIO::filesize_t offset;
        stream >> offset;
        seek(offset);
        break;
    }
    case CMD_TRUNCATE: {
        KIO::filesize_t length;
        stream >> length;
        void *data = static_cast<void *>(&length);
        virtual_hook(Truncate, data);
        break;
    }
    case CMD_NONE:
        break;
    case CMD_CLOSE:
        close();                // must call finish(), which will set d->inOpenLoop=false
        break;
    default:
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KIO.
        break;
    }
}

bool SlaveBase::cacheAuthentication(const AuthInfo &info)
{
    KPasswdServerClient *passwdServerClient = d->passwdServerClient();
    passwdServerClient->addAuthInfo(info, metaData(QStringLiteral("window-id")).toLongLong());
    return true;
}

int SlaveBase::connectTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_CONNECT_TIMEOUT;
}

int SlaveBase::proxyConnectTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ProxyConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_PROXY_CONNECT_TIMEOUT;
}

int SlaveBase::responseTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ResponseTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_RESPONSE_TIMEOUT;
}

int SlaveBase::readTimeout()
{
    bool ok;
    QString tmp = metaData(QStringLiteral("ReadTimeout"));
    int result = tmp.toInt(&ok);
    if (ok) {
        return result;
    }
    return DEFAULT_READ_TIMEOUT;
}

bool SlaveBase::wasKilled() const
{
    return d->wasKilled;
}

void SlaveBase::setKillFlag()
{
    d->wasKilled = true;
}

void SlaveBase::send(int cmd, const QByteArray &arr)
{
    slaveWriteError = false;
    if (!d->appConnection.send(cmd, arr))
        // Note that slaveWriteError can also be set by sigpipe_handler
    {
        slaveWriteError = true;
    }
    if (slaveWriteError) {
        exit();
    }
}

void SlaveBase::virtual_hook(int id, void *data)
{
    Q_UNUSED(data);

    switch(id) {
    case GetFileSystemFreeSpace: {
        error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_FILESYSTEMFREESPACE));
    } break;
    case Truncate: {
        error(ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(protocolName(), CMD_TRUNCATE));
    } break;
    }
}

void SlaveBase::lookupHost(const QString &host)
{
    KIO_DATA << host;
    send(MSG_HOST_INFO_REQ, data);
}

int SlaveBase::waitForHostInfo(QHostInfo &info)
{
    QByteArray data;
    int result = waitForAnswer(CMD_HOST_INFO, 0, data);

    if (result  == -1) {
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

PrivilegeOperationStatus SlaveBase::requestPrivilegeOperation(const QString &operationDetails)
{

    if (d->m_privilegeOperationStatus == OperationNotAllowed) {
        QByteArray buffer;
        send(MSG_PRIVILEGE_EXEC);
        waitForAnswer(MSG_PRIVILEGE_EXEC, 0, buffer);
        QDataStream ds(buffer);
        ds >> d->m_privilegeOperationStatus >> d->m_warningCaption >> d-> m_warningMessage;
    }

    if (metaData(QStringLiteral("UnitTesting")) != QLatin1String("true") &&
            d->m_privilegeOperationStatus == OperationAllowed &&
            !d->m_confirmationAsked) {
        //KF6 TODO Remove. We don't want to pass details as meta-data. Pass it as a parameter in messageBox().
        setMetaData(QStringLiteral("privilege_conf_details"), operationDetails);
        sendMetaData();

        int result = messageBox(d->m_warningMessage, WarningContinueCancelDetailed,
                                d->m_warningCaption, QString(), QString(), QString());
        d->m_privilegeOperationStatus = result == Continue ? OperationAllowed : OperationCanceled;
        d->m_confirmationAsked = true;
    }

    return KIO::PrivilegeOperationStatus(d->m_privilegeOperationStatus);
}

void SlaveBase::addTemporaryAuthorization(const QString &action)
{
    d->m_tempAuths.insert(action);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 66)
PrivilegeOperationStatus SlaveBase::requestPrivilegeOperation()
{
        return KIO::OperationNotAllowed;
}
#endif
