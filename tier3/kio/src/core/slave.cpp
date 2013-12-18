/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2000 Waldo Bastian <bastian@kde.org>
 *                2000 Stephan Kulow <coolo@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "slave.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>

#include <QtCore/QFile>
#include <QtCore/QTimer>
#include <QDBusConnection>
#include <QtCore/QProcess>

#include <klocalizedstring.h>

#include <kdeinit_interface.h>
#include <klauncher_interface.h>
#include <klibrary.h>

#include "dataprotocol_p.h"
#include "kservice.h"
#include "connection_p.h"
#include "commands_p.h"
#include "connectionserver.h"
#include <kprotocolinfo.h>
#include <config-kiocore.h> // CMAKE_INSTALL_PREFIX

#include "slaveinterface_p.h"

using namespace KIO;

#define SLAVE_CONNECTION_TIMEOUT_MIN	   2

// Without debug info we consider it an error if the slave doesn't connect
// within 10 seconds.
// With debug info we give the slave an hour so that developers have a chance
// to debug their slave.
#ifdef NDEBUG
#define SLAVE_CONNECTION_TIMEOUT_MAX      10
#else
#define SLAVE_CONNECTION_TIMEOUT_MAX    3600
#endif

Q_GLOBAL_STATIC_WITH_ARGS(org::kde::KLauncher, klauncherIface,
                          (QString::fromLatin1("org.kde.klauncher5"), QString::fromLatin1("/KLauncher"), QDBusConnection::sessionBus()))

static org::kde::KLauncher *klauncher()
{
    KDEInitInterface::ensureKdeinitRunning();
    return ::klauncherIface();
}


namespace KIO {

  /**
   * @internal
   */
    class SlavePrivate: public SlaveInterfacePrivate
    {
    public:
        SlavePrivate(const QString &protocol) :
            m_protocol(protocol),
            m_slaveProtocol(protocol),
            slaveconnserver(new KIO::ConnectionServer),
            m_job(0),
            m_pid(0),
            m_port(0),
            contacted(false),
            dead(false),
            m_refCount(1)
        {
            contact_started = QDateTime::currentDateTime();
            m_idleSince = QDateTime();
            slaveconnserver->listenForRemote();
            if ( !slaveconnserver->isListening() )
                qWarning() << "Connection server not listening, could not connect";
        }
        ~SlavePrivate()
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
        pid_t m_pid;
        quint16 m_port;
        bool contacted;
        bool dead;
        QDateTime contact_started;
        QDateTime m_idleSince;
        int m_refCount;
  };
}

void Slave::accept()
{
    Q_D(Slave);
    d->slaveconnserver->setNextPendingConnection(d->connection);
    d->slaveconnserver->deleteLater();
    d->slaveconnserver = 0;

    connect(d->connection, SIGNAL(readyRead()), SLOT(gotInput()));
}

void Slave::timeout()
{
    Q_D(Slave);
   if (d->dead) //already dead? then slaveDied was emitted and we are done
      return;
   if (d->connection->isConnected())
      return;

   /*qDebug() << "slave failed to connect to application pid=" << d->m_pid
                << " protocol=" << d->m_protocol;*/
   if (d->m_pid && (::kill(d->m_pid, 0) == 0))
   {
      int delta_t = d->contact_started.secsTo(QDateTime::currentDateTime());
      //qDebug() << "slave is slow... pid=" << d->m_pid << " t=" << delta_t;
      if (delta_t < SLAVE_CONNECTION_TIMEOUT_MAX)
      {
         QTimer::singleShot(1000*SLAVE_CONNECTION_TIMEOUT_MIN, this, SLOT(timeout()));
         return;
      }
   }
   //qDebug() << "Houston, we lost our slave, pid=" << d->m_pid;
   d->connection->close();
   d->dead = true;
   QString arg = d->m_protocol;
   if (!d->m_host.isEmpty())
      arg += "://"+d->m_host;
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
    connect(d->slaveconnserver, SIGNAL(newConnection()), SLOT(accept()));
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

void Slave::setProtocol(const QString & protocol)
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
    d->m_idleSince = QDateTime::currentDateTime();
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
        d->connection->disconnect(this);
        this->disconnect();
        deleteLater();
    }
}

int Slave::idleTime()
{
    Q_D(Slave);
    if (d->m_idleSince.isNull()) {
        return 0;
    }
    return d->m_idleSince.secsTo(QDateTime::currentDateTime());
}

void Slave::setPID(pid_t pid)
{
    Q_D(Slave);
    d->m_pid = pid;
}

int Slave::slave_pid()
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
        QDataStream stream( &data, QIODevice::WriteOnly );
        stream << url;
        d->connection->send( CMD_SLAVE_HOLD, data );
        d->connection->close();
        d->dead = true;
        emit slaveDied(this);
    }
    deref();
    // Call KLauncher::waitForSlave(pid);
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
    if (d->dead) //already dead? then slaveDied was emitted and we are done
        return;
    ref();
    if (!dispatch())
    {
        d->connection->close();
        d->dead = true;
        QString arg = d->m_protocol;
        if (!d->m_host.isEmpty())
            arg += "://"+d->m_host;
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
    /*qDebug() << "killing slave pid" << d->m_pid
                 << "(" << QString(d->m_protocol) + "://" + d->m_host << ")";*/
    if (d->m_pid)
    {
#ifndef _WIN32_WCE
       ::kill(d->m_pid, SIGTERM);
#else
        ::kill(d->m_pid, SIGKILL);
#endif
       d->m_pid = 0;
    }
}

void Slave::setHost( const QString &host, quint16 port,
                     const QString &user, const QString &passwd)
{
    Q_D(Slave);
    d->m_host = host;
    d->m_port = port;
    d->m_user = user;
    d->m_passwd = passwd;
    d->sslMetaData.clear();

    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );
    stream << d->m_host << d->m_port << d->m_user << d->m_passwd;
    d->connection->send( CMD_HOST, data );
}

void Slave::resetHost()
{
    Q_D(Slave);
    d->sslMetaData.clear();
    d->m_host = "<reset>";
}

void Slave::setConfig(const MetaData &config)
{
    Q_D(Slave);
    QByteArray data;
    QDataStream stream( &data, QIODevice::WriteOnly );
    stream << config;
    d->connection->send( CMD_CONFIG, data );
}

Slave* Slave::createSlave( const QString &protocol, const QUrl& url, int& error, QString& error_text )
{
    //qDebug() << "createSlave" << protocol << "for" << url;
    // Firstly take into account all special slaves
    if (protocol == "data")
        return new DataProtocol();
    Slave *slave = new Slave(protocol);
    QUrl slaveAddress = slave->d_func()->slaveconnserver->address();

#ifdef Q_OS_UNIX
    // In such case we start the slave via QProcess.
    // It's possible to force this by setting the env. variable
    // KDE_FORK_SLAVES, Clearcase seems to require this.
    static bool bForkSlaves = !qgetenv("KDE_FORK_SLAVES").isEmpty();

    if (!bForkSlaves)
    {
       // check the UID of klauncher
       QDBusReply<uint> reply = QDBusConnection::sessionBus().interface()->serviceUid(klauncher()->service());
       if (reply.isValid() && getuid() != reply)
          bForkSlaves = true;
    }

    if (bForkSlaves)
    {
       QString _name = KProtocolInfo::exec(protocol);
       if (_name.isEmpty())
       {
          error_text = i18n("Unknown protocol '%1'.", protocol);
          error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
          delete slave;
          return 0;
       }
       KLibrary lib(_name);
       QString lib_path = lib.fileName();
       if (lib_path.isEmpty())
       {
          error_text = i18n("Can not find io-slave for protocol '%1'.", protocol);
          error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
          delete slave;
          return 0;
       }

       const QStringList args = QStringList() << lib_path << protocol << "" << slaveAddress.toString();
       //qDebug() << "kioslave" << ", " << lib_path << ", " << protocol << ", " << QString() << ", " << slaveAddress;

       const QString kioslave = CMAKE_INSTALL_PREFIX "/" LIBEXEC_INSTALL_DIR "/kioslave";
       if (kioslave.isEmpty()) {
          error_text = i18n("Can not find 'kioslave' executable");
          error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
          delete slave;
          return 0;

       }
       QProcess::startDetached(kioslave, args);

       return slave;
    }
#endif

    QString errorStr;
    QDBusReply<int> reply = klauncher()->requestSlave(protocol, url.host(), slaveAddress.toString(), errorStr);
    if (!reply.isValid()) {
	error_text = i18n("Cannot talk to klauncher: %1", klauncher()->lastError().message() );
	error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
        delete slave;
        return 0;
    }
    pid_t pid = reply;
    if (!pid)
    {
        error_text = i18n("Unable to create io-slave:\nklauncher said: %1", errorStr);
        error = KIO::ERR_CANNOT_LAUNCH_PROCESS;
        delete slave;
        return 0;
    }
    slave->setPID(pid);
    QTimer::singleShot(1000*SLAVE_CONNECTION_TIMEOUT_MIN, slave, SLOT(timeout()));
    return slave;
}

Slave* Slave::holdSlave(const QString &protocol, const QUrl& url)
{
    //qDebug() << "holdSlave" << protocol << "for" << url;
    // Firstly take into account all special slaves
    if (protocol == "data")
        return 0;
    Slave *slave = new Slave(protocol);
    QUrl slaveAddress = slave->d_func()->slaveconnserver->address();
    QDBusReply<int> reply = klauncher()->requestHoldSlave(url.toString(), slaveAddress.toString());
    if (!reply.isValid()) {
        delete slave;
        return 0;
    }
    pid_t pid = reply;
    if (!pid)
    {
        delete slave;
        return 0;
    }
    slave->setPID(pid);
    QTimer::singleShot(1000*SLAVE_CONNECTION_TIMEOUT_MIN, slave, SLOT(timeout()));
    return slave;
}

bool Slave::checkForHeldSlave(const QUrl &url)
{
    return klauncher()->checkForHeldSlave(url.toString());
}

