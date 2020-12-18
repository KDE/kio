/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "slave.h"

#include <qplatformdefs.h>
#include <stdio.h>

#include <QFile>
#include <QStandardPaths>
#include <QTimer>
#include <QProcess>
#include <QElapsedTimer>

#include <QDBusConnection>
#include <KLocalizedString>

#include <KDEInitInterface>
#include <klauncher_interface.h>
#include <KPluginLoader>

#include "dataprotocol_p.h"
#include "connection_p.h"
#include "commands_p.h"
#include "connectionserver.h"
#include "kioglobal_p.h"
#include <kprotocolinfo.h>
#include <config-kiocore.h> // CMAKE_INSTALL_FULL_LIBEXECDIR_KF5

#include "slaveinterface_p.h"
#include "kiocoredebug.h"

using namespace KIO;

#define SLAVE_CONNECTION_TIMEOUT_MIN       2

// Without debug info we consider it an error if the slave doesn't connect
// within 10 seconds.
// With debug info we give the slave an hour so that developers have a chance
// to debug their slave.
#ifdef NDEBUG
#define SLAVE_CONNECTION_TIMEOUT_MAX      10
#else
#define SLAVE_CONNECTION_TIMEOUT_MAX    3600
#endif

static QThreadStorage<org::kde::KSlaveLauncher *> s_kslaveLauncher;

static org::kde::KSlaveLauncher *klauncher()
{
    KDEInitInterface::ensureKdeinitRunning();
    if (!s_kslaveLauncher.hasLocalData()) {
        org::kde::KSlaveLauncher *launcher = new org::kde::KSlaveLauncher(QStringLiteral("org.kde.klauncher5"),
                QStringLiteral("/KLauncher"),
                QDBusConnection::sessionBus());
        s_kslaveLauncher.setLocalData(launcher);
        return launcher;
    }
    return s_kslaveLauncher.localData();
}

// In such case we start the slave via QProcess.
// It's possible to force this by setting the env. variable
// KDE_FORK_SLAVES, Clearcase seems to require this.
static QBasicAtomicInt bForkSlaves =
#if KIO_FORK_SLAVES
    Q_BASIC_ATOMIC_INITIALIZER(1);
#else
    Q_BASIC_ATOMIC_INITIALIZER(-1);
#endif

static bool forkSlaves()
{
    // In such case we start the slave via QProcess.
    // It's possible to force this by setting the env. variable
    // KDE_FORK_SLAVES, Clearcase seems to require this.
    if (bForkSlaves.loadRelaxed() == -1) {
        bool fork = qEnvironmentVariableIsSet("KDE_FORK_SLAVES");

        // no dbus? => fork slaves as we can't talk to klauncher
        if (!fork) {
            fork = !QDBusConnection::sessionBus().interface();
        }

#ifdef Q_OS_UNIX
        if (!fork) {
            // check the UID of klauncher
            QDBusReply<uint> reply = QDBusConnection::sessionBus().interface()->serviceUid(klauncher()->service());
            // if reply is not valid, fork, most likely klauncher can not be run or is not installed
            // fallback: if there's an klauncher process owned by a different user: still fork
            if (!reply.isValid() || getuid() != reply) {
                fork = true;
            }
        }
#endif

        bForkSlaves.testAndSetRelaxed(-1, fork ? 1 : 0);
    }
    return bForkSlaves.loadRelaxed() == 1;
}

namespace KIO
{

/**
 * @internal
 */
class SlavePrivate: public SlaveInterfacePrivate
{
public:
    explicit SlavePrivate(const QString &protocol) :
        m_protocol(protocol),
        m_slaveProtocol(protocol),
        slaveconnserver(new KIO::ConnectionServer),
        m_job(nullptr),
        m_pid(0),
        m_port(0),
        contacted(false),
        dead(false),
        m_refCount(1)
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

    QString m_protocol;
    QString m_slaveProtocol;
    QString m_host;
    QString m_user;
    QString m_passwd;
    KIO::ConnectionServer *slaveconnserver;
    KIO::SimpleJob *m_job;
    qint64 m_pid;
    quint16 m_port;
    bool contacted;
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
    if (d->dead) { //already dead? then slaveDied was emitted and we are done
        return;
    }
    if (d->connection->isConnected()) {
        return;
    }

    /*qDebug() << "slave failed to connect to application pid=" << d->m_pid
                 << " protocol=" << d->m_protocol;*/
    if (d->m_pid && KIOPrivate::isProcessAlive(d->m_pid)) {
        int delta_t = d->contact_started.elapsed() / 1000;
        //qDebug() << "slave is slow... pid=" << d->m_pid << " t=" << delta_t;
        if (delta_t < SLAVE_CONNECTION_TIMEOUT_MAX) {
            QTimer::singleShot(1000 * SLAVE_CONNECTION_TIMEOUT_MIN, this, &Slave::timeout);
            return;
        }
    }
    //qDebug() << "Houston, we lost our slave, pid=" << d->m_pid;
    d->connection->close();
    d->dead = true;
    QString arg = d->m_protocol;
    if (!d->m_host.isEmpty()) {
        arg += QLatin1String("://") + d->m_host;
    }
    //qDebug() << "slave died pid = " << d->m_pid;

    ref();
    // Tell the job about the problem.
    emit error(ERR_SLAVE_DIED, arg);
    // Tell the scheduler about the problem.
    emit slaveDied(this);
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
    //qDebug() << "destructing slave object pid = " << d->m_pid;
    //delete d;
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

bool Slave::isConnected()
{
    Q_D(Slave);
    return d->contacted;
}

void Slave::setConnected(bool c)
{
    Q_D(Slave);
    d->contacted = c;
}

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
        emit metaData(d->sslMetaData);
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
        emit slaveDied(this);
    }
    deref();
    // Call KSlaveLauncher::waitForSlave(pid);
    {
        klauncher()->waitForSlave(d->m_pid);
    }
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
    if (d->dead) { //already dead? then slaveDied was emitted and we are done
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
        //qDebug() << "slave died pid = " << d->m_pid;
        // Tell the job about the problem.
        emit error(ERR_SLAVE_DIED, arg);
        // Tell the scheduler about the problem.
        emit slaveDied(this);
    }
    deref();
    // Here we might be dead!!
}

void Slave::kill()
{
    Q_D(Slave);
    d->dead = true; // OO can be such simple.
    //qDebug() << "killing slave pid" << d->m_pid
    //         << "(" << d->m_protocol + QLatin1String("://") + d->m_host << ")";
    if (d->m_pid) {
        KIOPrivate::sendTerminateSignal(d->m_pid);
        d->m_pid = 0;
    }
}

void Slave::setHost(const QString &host, quint16 port,
                    const QString &user, const QString &passwd)
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

Slave *Slave::createSlave(const QString &protocol, const QUrl &url, int &error, QString &error_text)
{
    //qDebug() << "createSlave" << protocol << "for" << url;
    // Firstly take into account all special slaves
    if (protocol == QLatin1String("data")) {
        return new DataProtocol();
    }
    Slave *slave = new Slave(protocol);
    QUrl slaveAddress = slave->d_func()->slaveconnserver->address();
    if (slaveAddress.isEmpty()) {
        error_text = i18n("Can not create socket for launching io-slave for protocol '%1'.", protocol);
        error = KIO::ERR_CANNOT_CREATE_SLAVE;
        delete slave;
        return nullptr;
    }

    if (forkSlaves() == 1) {
        QString _name = KProtocolInfo::exec(protocol);
        if (_name.isEmpty()) {
            error_text = i18n("Unknown protocol '%1'.", protocol);
            error = KIO::ERR_CANNOT_CREATE_SLAVE;
            delete slave;
            return nullptr;
        }
        // find the kioslave using KPluginLoader; kioslave would do this
        // anyway, but if it doesn't exist, we want to be able to return
        // a useful error message immediately
        QString lib_path = KPluginLoader::findPlugin(_name);
        if (lib_path.isEmpty()) {
            error_text = i18n("Can not find io-slave for protocol '%1'.", protocol);
            error = KIO::ERR_CANNOT_CREATE_SLAVE;
            delete slave;
            return nullptr;
        }

        const QStringList args = QStringList{lib_path, protocol, QString(), slaveAddress.toString()};
        //qDebug() << "kioslave" << ", " << lib_path << ", " << protocol << ", " << QString() << ", " << slaveAddress;

        // look where libexec path is (can be set in qt.conf)
        const QString qlibexec = QLibraryInfo::location(QLibraryInfo::LibraryExecutablesPath);
        // on !win32 we use a kf5 suffix
        const QString qlibexecKF5 = QDir(qlibexec).filePath(QStringLiteral("kf5"));

        // search paths
        const QStringList searchPaths = QStringList()
            << QCoreApplication::applicationDirPath() // then look where our application binary is located
            << qlibexec
            << qlibexecKF5
            << QFile::decodeName(CMAKE_INSTALL_FULL_LIBEXECDIR_KF5); // look at our installation location
        QString kioslaveExecutable = QStandardPaths::findExecutable(QStringLiteral("kioslave5"), searchPaths);
        if (kioslaveExecutable.isEmpty()) {
            // Fallback to PATH. On win32 we install to bin/ which tests outside
            // KIO cannot not find at the time ctest is run because it
            // isn't the same as applicationDirPath().
            kioslaveExecutable = QStandardPaths::findExecutable(QStringLiteral("kioslave5"));
        }
        if (kioslaveExecutable.isEmpty()) {
            error_text = i18n("Can not find 'kioslave5' executable at '%1'", searchPaths.join(QLatin1String(", ")));
            error = KIO::ERR_CANNOT_CREATE_SLAVE;
            delete slave;
            return nullptr;
        }

        qint64 pid = 0;
        QProcess::startDetached(kioslaveExecutable, args, QString(), &pid);
        slave->setPID(pid);

        return slave;
    }

    QString errorStr;
    QDBusReply<int> reply = klauncher()->requestSlave(protocol, url.host(), slaveAddress.toString(), errorStr);
    if (!reply.isValid()) {
        error_text = i18n("Cannot talk to klauncher: %1", klauncher()->lastError().message());
        error = KIO::ERR_CANNOT_CREATE_SLAVE;
        delete slave;
        return nullptr;
    }
    qint64 pid = reply;
    if (!pid) {
        error_text = i18n("klauncher said: %1", errorStr);
        error = KIO::ERR_CANNOT_CREATE_SLAVE;
        delete slave;
        return nullptr;
    }
    slave->setPID(pid);
    QTimer::singleShot(1000 * SLAVE_CONNECTION_TIMEOUT_MIN, slave, &Slave::timeout);
    return slave;
}

Slave *Slave::holdSlave(const QString &protocol, const QUrl &url)
{
    //qDebug() << "holdSlave" << protocol << "for" << url;
    // Firstly take into account all special slaves
    if (protocol == QLatin1String("data")) {
        return nullptr;
    }

    if (forkSlaves()) {
        return nullptr;
    }

    Slave *slave = new Slave(protocol);
    QUrl slaveAddress = slave->d_func()->slaveconnserver->address();
    QDBusReply<int> reply = klauncher()->requestHoldSlave(url.toString(), slaveAddress.toString());
    if (!reply.isValid()) {
        delete slave;
        return nullptr;
    }
    qint64 pid = reply;
    if (!pid) {
        delete slave;
        return nullptr;
    }
    slave->setPID(pid);
    QTimer::singleShot(1000 * SLAVE_CONNECTION_TIMEOUT_MIN, slave, &Slave::timeout);
    return slave;
}

bool Slave::checkForHeldSlave(const QUrl &url)
{
    if (forkSlaves()) {
        return false;
    }

    return klauncher()->checkForHeldSlave(url.toString());
}

