/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slave.h"

#include <qplatformdefs.h>
#include <stdio.h>

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <KLibexec>
#include <KLocalizedString>

#include "commands_p.h"
#include "connection_p.h"
#include "connectionserver.h"
#include "dataprotocol_p.h"
#include "kioglobal_p.h"
#include <config-kiocore.h> // KDE_INSTALL_FULL_LIBEXECDIR_KF
#include <kprotocolinfo.h>

#include "kiocoredebug.h"
#include "slavebase.h"
#include "slaveinterface_p.h"
#include "workerfactory.h"
#include "workerthread_p.h"

using namespace KIO;

static constexpr int s_slaveConnectionTimeoutMin = 2;

// Without debug info we consider it an error if the slave doesn't connect
// within 10 seconds.
// With debug info we give the slave an hour so that developers have a chance
// to debug their slave.
#ifdef NDEBUG
static constexpr int s_slaveConnectionTimeoutMax = 10;
#else
static constexpr int s_slaveConnectionTimeoutMax = 3600;
#endif

namespace KIO
{
/**
 * @internal
 */
class SlavePrivate : public SlaveInterfacePrivate
{
public:
    explicit SlavePrivate(const QString &protocol)
        : m_protocol(protocol)
        , m_slaveProtocol(protocol)
        , slaveconnserver(new KIO::ConnectionServer)
        , m_job(nullptr)
        , m_pid(0)
        , m_port(0)
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 103)
        , contacted(false)
#endif
        , dead(false)
        , m_refCount(1)
    {
        contact_started.start();
        slaveconnserver->listenForRemote();
        if (!slaveconnserver->isListening()) {
            qCWarning(KIO_CORE) << "KIO Connection server not listening, could not connect";
        }
    }
    ~SlavePrivate() override
    {
        delete slaveconnserver;
    }

    WorkerThread *m_workerThread = nullptr; // only set for in-process workers
    QString m_protocol;
    QString m_slaveProtocol;
    QString m_host;
    QString m_user;
    QString m_passwd;
    KIO::ConnectionServer *slaveconnserver;
    KIO::SimpleJob *m_job;
    qint64 m_pid; // only set for out-of-process workers
    quint16 m_port;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 103)
    bool contacted;
#endif
    bool dead;
    QElapsedTimer contact_started;
    QElapsedTimer m_idleSince;
    int m_refCount;
};
}

void Slave::accept()
{
    Q_D(Slave);
    d->slaveconnserver->setNextPendingConnection(d->connection);
    d->slaveconnserver->deleteLater();
    d->slaveconnserver = nullptr;

    connect(d->connection, &Connection::readyRead, this, &Slave::gotInput);
}

void Slave::timeout()
{
    Q_D(Slave);
    if (d->dead) { // already dead? then slaveDied was emitted and we are done
        return;
    }
    if (d->connection->isConnected()) {
        return;
    }

    /*qDebug() << "worker failed to connect to application pid=" << d->m_pid
                 << " protocol=" << d->m_protocol;*/
    if (d->m_pid && KIOPrivate::isProcessAlive(d->m_pid)) {
        int delta_t = d->contact_started.elapsed() / 1000;
        // qDebug() << "worker is slow... pid=" << d->m_pid << " t=" << delta_t;
        if (delta_t < s_slaveConnectionTimeoutMax) {
            QTimer::singleShot(1000 * s_slaveConnectionTimeoutMin, this, &Slave::timeout);
            return;
        }
    }
    // qDebug() << "Houston, we lost our worker, pid=" << d->m_pid;
    d->connection->close();
    d->dead = true;
    QString arg = d->m_protocol;
    if (!d->m_host.isEmpty()) {
        arg += QLatin1String("://") + d->m_host;
    }
    // qDebug() << "worker failed to connect pid =" << d->m_pid << arg;

    ref();
    // Tell the job about the problem.
    Q_EMIT error(ERR_WORKER_DIED, arg);
    // Tell the scheduler about the problem.
    Q_EMIT slaveDied(this);
    // After the above signal we're dead!!
    deref();
}

Slave::Slave(const QString &protocol, QObject *parent)
    : SlaveInterface(*new SlavePrivate(protocol), parent)
{
    Q_D(Slave);
    d->slaveconnserver->setParent(this);
    d->connection = new Connection(this);
    connect(d->slaveconnserver, &ConnectionServer::newConnection, this, &Slave::accept);
}

Slave::~Slave()
{
    // qDebug() << "destructing slave object pid =" << d->m_pid;
    // delete d;
}

QString Slave::protocol()
{
    Q_D(Slave);
    return d->m_protocol;
}

void Slave::setProtocol(const QString &protocol)
{
    Q_D(Slave);
    d->m_protocol = protocol;
}

QString Slave::slaveProtocol()
{
    Q_D(Slave);
    return d->m_slaveProtocol;
}

QString Slave::host()
{
    Q_D(Slave);
    return d->m_host;
}

quint16 Slave::port()
{
    Q_D(Slave);
    return d->m_port;
}

QString Slave::user()
{
    Q_D(Slave);
    return d->m_user;
}

QString Slave::passwd()
{
    Q_D(Slave);
    return d->m_passwd;
}

void Slave::setIdle()
{
    Q_D(Slave);
    d->m_idleSince.start();
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 103)
bool Slave::isConnected()
{
    Q_D(Slave);
    return d->contacted;
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 103)
void Slave::setConnected(bool c)
{
    Q_D(Slave);
    d->contacted = c;
}
#endif

void Slave::ref()
{
    Q_D(Slave);
    d->m_refCount++;
}

void Slave::deref()
{
    Q_D(Slave);
    d->m_refCount--;
    if (!d->m_refCount) {
        aboutToDelete();
        delete this; // yes it reads funny, but it's too late for a deleteLater() here, no event loop anymore
    }
}

void Slave::aboutToDelete()
{
    Q_D(Slave);
    d->connection->disconnect(this);
    this->disconnect();
}

void Slave::setWorkerThread(WorkerThread *thread)
{
    Q_D(Slave);
    d->m_workerThread = thread;
}

int Slave::idleTime()
{
    Q_D(Slave);
    if (!d->m_idleSince.isValid()) {
        return 0;
    }
    return d->m_idleSince.elapsed() / 1000;
}

void Slave::setPID(qint64 pid)
{
    Q_D(Slave);
    d->m_pid = pid;
}

qint64 Slave::slave_pid()
{
    Q_D(Slave);
    return d->m_pid;
}

void Slave::setJob(KIO::SimpleJob *job)
{
    Q_D(Slave);
    if (!d->sslMetaData.isEmpty()) {
        Q_EMIT metaData(d->sslMetaData);
    }
    d->m_job = job;
}

KIO::SimpleJob *Slave::job() const
{
    Q_D(const Slave);
    return d->m_job;
}

bool Slave::isAlive()
{
    Q_D(Slave);
    return !d->dead;
}

// TODO KF6: remove, unused
void Slave::hold(const QUrl &url)
{
    Q_D(Slave);
    ref();
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << url;
        d->connection->send(CMD_SLAVE_HOLD, data);
        d->connection->close();
        d->dead = true;
        Q_EMIT slaveDied(this);
    }
    deref();
}

void Slave::suspend()
{
    Q_D(Slave);
    d->connection->suspend();
}

void Slave::resume()
{
    Q_D(Slave);
    d->connection->resume();
}

bool Slave::suspended()
{
    Q_D(Slave);
    return d->connection->suspended();
}

void Slave::send(int cmd, const QByteArray &arr)
{
    Q_D(Slave);
    d->connection->send(cmd, arr);
}

void Slave::gotInput()
{
    Q_D(Slave);
    if (d->dead) { // already dead? then slaveDied was emitted and we are done
        return;
    }
    ref();
    if (!dispatch()) {
        d->connection->close();
        d->dead = true;
        QString arg = d->m_protocol;
        if (!d->m_host.isEmpty()) {
            arg += QLatin1String("://") + d->m_host;
        }
        // qDebug() << "worker died pid =" << d->m_pid << arg;
        // Tell the job about the problem.
        Q_EMIT error(ERR_WORKER_DIED, arg);
        // Tell the scheduler about the problem.
        Q_EMIT slaveDied(this);
    }
    deref();
    // Here we might be dead!!
}

void Slave::kill()
{
    Q_D(Slave);
    d->dead = true; // OO can be such simple.
    if (d->m_pid) {
        qCDebug(KIO_CORE) << "killing worker process pid" << d->m_pid << "(" << d->m_protocol + QLatin1String("://") + d->m_host << ")";
        KIOPrivate::sendTerminateSignal(d->m_pid);
        d->m_pid = 0;
    } else if (d->m_workerThread) {
        qCDebug(KIO_CORE) << "aborting worker thread for " << d->m_protocol + QLatin1String("://") + d->m_host;
        d->m_workerThread->abort();
    }
    deref();
}

void Slave::setHost(const QString &host, quint16 port, const QString &user, const QString &passwd)
{
    Q_D(Slave);
    d->m_host = host;
    d->m_port = port;
    d->m_user = user;
    d->m_passwd = passwd;
    d->sslMetaData.clear();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << d->m_host << d->m_port << d->m_user << d->m_passwd;
    d->connection->send(CMD_HOST, data);
}

void Slave::resetHost()
{
    Q_D(Slave);
    d->sslMetaData.clear();
    d->m_host = QStringLiteral("<reset>");
}

void Slave::setConfig(const MetaData &config)
{
    Q_D(Slave);
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << config;
    d->connection->send(CMD_CONFIG, data);
}

// TODO KF6: return std::unique_ptr
Slave *Slave::createSlave(const QString &protocol, const QUrl &url, int &error, QString &error_text)
{
    Q_UNUSED(url)
    // qDebug() << "createSlave" << protocol << "for" << url;
    // Firstly take into account all special slaves
    if (protocol == QLatin1String("data")) {
        return new DataProtocol();
    }

    const QString _name = KProtocolInfo::exec(protocol);
    if (_name.isEmpty()) {
        error_text = i18n("Unknown protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        return nullptr;
    }

    // find the KIO worker using QPluginLoader; kioslave would do this
    // anyway, but if it doesn't exist, we want to be able to return
    // a useful error message immediately
    QPluginLoader loader(_name);
    const QString lib_path = loader.fileName();
    if (lib_path.isEmpty()) {
        error_text = i18n("Can not find a KIO worker for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        return nullptr;
    }

    Slave *slave = new Slave(protocol);
    QUrl slaveAddress = slave->d_func()->slaveconnserver->address();
    if (slaveAddress.isEmpty()) {
        error_text = i18n("Can not create a socket for launching a KIO worker for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        delete slave;
        return nullptr;
    }

    // Threads are enabled by default, set KIO_ENABLE_WORKER_THREADS=0 to disable them
    const auto useThreads = []() {
        return qgetenv("KIO_ENABLE_WORKER_THREADS") != "0";
    };
    static bool bUseThreads = useThreads();

    // Threads have performance benefits, but degrade robustness
    // (a worker crashing kills the app). So let's only enable the feature for kio_file, for now.
    if (protocol == QLatin1String("admin") || (bUseThreads && protocol == QLatin1String("file"))) {
        auto *factory = qobject_cast<WorkerFactory *>(loader.instance());
        if (factory) {
            auto *thread = new WorkerThread(slave, factory, slaveAddress.toString().toLocal8Bit());
            thread->start();
            slave->setWorkerThread(thread);
            return slave;
        } else {
            qCWarning(KIO_CORE) << lib_path << "doesn't implement WorkerFactory?";
        }
    }

    const QStringList args = QStringList{lib_path, protocol, QString(), slaveAddress.toString()};
    // qDebug() << "kioslave" << ", " << lib_path << ", " << protocol << ", " << QString() << ", " << slaveAddress;

    // search paths
    QStringList searchPaths = KLibexec::kdeFrameworksPaths(QStringLiteral("libexec/kf" QT_STRINGIFY(QT_VERSION_MAJOR)));
    searchPaths.append(QFile::decodeName(KDE_INSTALL_FULL_LIBEXECDIR_KF)); // look at our installation location
    QString kioslaveExecutable = QStandardPaths::findExecutable(QStringLiteral("kioslave5"), searchPaths);
    if (kioslaveExecutable.isEmpty()) {
        // Fallback to PATH. On win32 we install to bin/ which tests outside
        // KIO cannot not find at the time ctest is run because it
        // isn't the same as applicationDirPath().
        kioslaveExecutable = QStandardPaths::findExecutable(QStringLiteral("kioslave5"));
    }
    if (kioslaveExecutable.isEmpty()) {
        error_text = i18n("Can not find 'kioslave5' executable at '%1'", searchPaths.join(QLatin1String(", ")));
        error = KIO::ERR_CANNOT_CREATE_WORKER;
        delete slave;
        return nullptr;
    }

    qint64 pid = 0;
    QProcess::startDetached(kioslaveExecutable, args, QString(), &pid);
    slave->setPID(pid);

    return slave;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 88)
Slave *Slave::holdSlave(const QString &protocol, const QUrl &url)
{
    Q_UNUSED(protocol)
    Q_UNUSED(url)
    return nullptr;
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 88)
bool Slave::checkForHeldSlave(const QUrl &)
{
    return false;
}
#endif
