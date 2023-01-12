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

void Slave::accept()
{
    m_slaveconnserver->setNextPendingConnection(m_connection);
    m_slaveconnserver->deleteLater();
    m_slaveconnserver = nullptr;

    connect(m_connection, &Connection::readyRead, this, &Slave::gotInput);
}

void Slave::timeout()
{
    if (m_dead) { // already dead? then slaveDied was emitted and we are done
        return;
    }
    if (m_connection->isConnected()) {
        return;
    }

    /*qDebug() << "worker failed to connect to application pid=" << m_pid
                 << " protocol=" << m_protocol;*/
    if (m_pid && KIOPrivate::isProcessAlive(m_pid)) {
        int delta_t = m_contact_started.elapsed() / 1000;
        // qDebug() << "worker is slow... pid=" << m_pid << " t=" << delta_t;
        if (delta_t < s_slaveConnectionTimeoutMax) {
            QTimer::singleShot(1000 * s_slaveConnectionTimeoutMin, this, &Slave::timeout);
            return;
        }
    }
    // qDebug() << "Houston, we lost our worker, pid=" << m_pid;
    m_connection->close();
    m_dead = true;
    QString arg = m_protocol;
    if (!m_host.isEmpty()) {
        arg += QLatin1String("://") + m_host;
    }
    // qDebug() << "worker failed to connect pid =" << m_pid << arg;

    ref();
    // Tell the job about the problem.
    Q_EMIT error(ERR_WORKER_DIED, arg);
    // Tell the scheduler about the problem.
    Q_EMIT slaveDied(this);
    // After the above signal we're dead!!
    deref();
}

Slave::Slave(const QString &protocol, QObject *parent)
    : SlaveInterface(parent)
    , m_protocol(protocol)
    , m_slaveProtocol(protocol)
    , m_slaveconnserver(new KIO::ConnectionServer)
{
    m_contact_started.start();
    m_slaveconnserver->setParent(this);
    m_slaveconnserver->listenForRemote();
    if (!m_slaveconnserver->isListening()) {
        qCWarning(KIO_CORE) << "KIO Connection server not listening, could not connect";
    }
    m_connection = new Connection(this);
    connect(m_slaveconnserver, &ConnectionServer::newConnection, this, &Slave::accept);
}

Slave::~Slave()
{
    // qDebug() << "destructing slave object pid =" << m_pid;
    delete m_slaveconnserver;
}

QString Slave::protocol() const
{
    return m_protocol;
}

void Slave::setProtocol(const QString &protocol)
{
    m_protocol = protocol;
}

QString Slave::slaveProtocol() const
{
    return m_slaveProtocol;
}

QString Slave::host() const
{
    return m_host;
}

quint16 Slave::port() const
{
    return m_port;
}

QString Slave::user() const
{
    return m_user;
}

QString Slave::passwd() const
{
    return m_passwd;
}

void Slave::setIdle()
{
    m_idleSince.start();
}

void Slave::ref()
{
    m_refCount++;
}

void Slave::deref()
{
    m_refCount--;
    if (!m_refCount) {
        aboutToDelete();
        delete this; // yes it reads funny, but it's too late for a deleteLater() here, no event loop anymore
    }
}

void Slave::aboutToDelete()
{
    m_connection->disconnect(this);
    this->disconnect();
}

void Slave::setWorkerThread(WorkerThread *thread)
{
    m_workerThread = thread;
}

int Slave::idleTime() const
{
    if (!m_idleSince.isValid()) {
        return 0;
    }
    return m_idleSince.elapsed() / 1000;
}

void Slave::setPID(qint64 pid)
{
    m_pid = pid;
}

qint64 Slave::slave_pid() const
{
    return m_pid;
}

void Slave::setJob(KIO::SimpleJob *job)
{
    if (!m_sslMetaData.isEmpty()) {
        Q_EMIT metaData(m_sslMetaData);
    }
    m_job = job;
}

KIO::SimpleJob *Slave::job() const
{
    return m_job;
}

bool Slave::isAlive() const
{
    return !m_dead;
}

// TODO KF6: remove, unused
void Slave::hold(const QUrl &url)
{
    ref();
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << url;
        m_connection->send(CMD_SLAVE_HOLD, data);
        m_connection->close();
        m_dead = true;
        Q_EMIT slaveDied(this);
    }
    deref();
}

void Slave::suspend()
{
    m_connection->suspend();
}

void Slave::resume()
{
    m_connection->resume();
}

bool Slave::suspended()
{
    return m_connection->suspended();
}

void Slave::send(int cmd, const QByteArray &arr)
{
    m_connection->send(cmd, arr);
}

void Slave::gotInput()
{
    if (m_dead) { // already dead? then slaveDied was emitted and we are done
        return;
    }
    ref();
    if (!dispatch()) {
        m_connection->close();
        m_dead = true;
        QString arg = m_protocol;
        if (!m_host.isEmpty()) {
            arg += QLatin1String("://") + m_host;
        }
        // qDebug() << "worker died pid =" << m_pid << arg;
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
    m_dead = true; // OO can be such simple.
    if (m_pid) {
        qCDebug(KIO_CORE) << "killing worker process pid" << m_pid << "(" << m_protocol + QLatin1String("://") + m_host << ")";
        KIOPrivate::sendTerminateSignal(m_pid);
        m_pid = 0;
    } else if (m_workerThread) {
        qCDebug(KIO_CORE) << "aborting worker thread for " << m_protocol + QLatin1String("://") + m_host;
        m_workerThread->abort();
    }
    deref();
}

void Slave::setHost(const QString &host, quint16 port, const QString &user, const QString &passwd)
{
    m_host = host;
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_sslMetaData.clear();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << m_host << m_port << m_user << m_passwd;
    m_connection->send(CMD_HOST, data);
}

void Slave::resetHost()
{
    m_sslMetaData.clear();
    m_host = QStringLiteral("<reset>");
}

void Slave::setConfig(const MetaData &config)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << config;
    m_connection->send(CMD_CONFIG, data);
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
    QUrl slaveAddress = slave->m_slaveconnserver->address();
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
    QStringList searchPaths = KLibexec::kdeFrameworksPaths(QStringLiteral("libexec/kf6"));
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
