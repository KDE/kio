/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2000 Waldo Bastian <bastian@kde.org>
 *  Copyright (c) 2000 David Faure <faure@kde.org>
 *  Copyright (c) 2000 Stephan Kulow <coolo@kde.org>
 *  Copyright (c) 2007 Thiago Macieira <thiago@kde.org>
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
 *
 **/

#include "slavebase.h"

#include <config-kiocore.h>

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <QtCore/QFile>
#include <QtCore/QList>
#include <QtCore/QDateTime>
#include <QtCore/QCoreApplication>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>

#include "kremoteencoding.h"

#include "connection_p.h"
#include "commands_p.h"
#include "ioslave_defaults.h"
#include "slaveinterface.h"
#include "kpasswdserver_p.h"

#ifndef NDEBUG
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#endif

extern "C" {
    static void sigsegv_handler(int sig);
    static void sigpipe_handler(int sig);
}

using namespace KIO;

typedef QList<QByteArray> AuthKeysList;
typedef QMap<QString,QByteArray> AuthKeysMap;
#define KIO_DATA QByteArray data; QDataStream stream( &data, QIODevice::WriteOnly ); stream
#define KIO_FILESIZE_T(x) quint64(x)
static const int KIO_MAX_ENTRIES_PER_BATCH = 200;
static const int KIO_MAX_SEND_BATCH_TIME = 300;

namespace KIO {

class SlaveBasePrivate {
public:
    SlaveBase* q;
    SlaveBasePrivate(SlaveBase* owner): q(owner), m_passwdServer(0)
    {
        pendingListEntries.reserve(KIO_MAX_ENTRIES_PER_BATCH);
    }
    ~SlaveBasePrivate() { delete m_passwdServer; }

    UDSEntryList pendingListEntries;
    QTime m_timeSinceLastBatch;
    Connection appConnection;
    QString poolSocket;
    bool isConnectedToApp;
    static qlonglong s_seqNr;

    QString slaveid;
    bool resume:1;
    bool needSendCanResume:1;
    bool onHold:1;
    bool wasKilled:1;
    bool inOpenLoop:1;
    bool exit_loop:1;
    MetaData configData;
    KConfig *config;
    KConfigGroup *configGroup;
    QUrl onHoldUrl;

    QDateTime lastTimeout;
    QDateTime nextTimeout;
    KIO::filesize_t totalSize;
    KIO::filesize_t sentListEntries;
    KRemoteEncoding *remotefile;
    enum { Idle, InsideMethod, FinishedCalled, ErrorCalled } m_state;
    QByteArray timeoutData;

    KPasswdServer* m_passwdServer;

    // Reconstructs configGroup from configData and mIncomingMetaData
    void rebuildConfig()
    {
        configGroup->deleteGroup(KConfigGroup::WriteConfigFlags());

        // mIncomingMetaData cascades over config, so we write config first,
        // to let it be overwritten
        MetaData::ConstIterator end = configData.constEnd();
        for (MetaData::ConstIterator it = configData.constBegin(); it != end; ++it)
            configGroup->writeEntry(it.key(), it->toUtf8(), KConfigGroup::WriteConfigFlags());

        end = q->mIncomingMetaData.constEnd();
        for (MetaData::ConstIterator it = q->mIncomingMetaData.constBegin(); it != end; ++it)
            configGroup->writeEntry(it.key(), it->toUtf8(), KConfigGroup::WriteConfigFlags());
    }

    void verifyState(const char* cmdName)
    {
        if ((m_state != FinishedCalled) && (m_state != ErrorCalled)){
            qWarning() << cmdName << "did not call finished() or error()! Please fix the KIO slave.";
        }
    }

    void verifyErrorFinishedNotCalled(const char* cmdName)
    {
        if (m_state == FinishedCalled || m_state == ErrorCalled) {
            qWarning() << cmdName << "called finished() or error(), but it's not supposed to! Please fix the KIO slave.";
        }
    }

    KPasswdServer* passwdServer()
    {
        if (!m_passwdServer) {
            m_passwdServer = new KPasswdServer;
        }

        return m_passwdServer;
    }
};

}

static SlaveBase *globalSlave;
qlonglong SlaveBasePrivate::s_seqNr;

static volatile bool slaveWriteError = false;

static const char *s_protocol;

#ifdef Q_OS_UNIX
extern "C" {
static void genericsig_handler(int sigNumber)
{
   ::signal(sigNumber,SIG_IGN);
   //WABA: Don't do anything that requires malloc, we can deadlock on it since
   //a SIGTERM signal can come in while we are in malloc/free.
   //qDebug()<<"kioslave : exiting due to signal "<<sigNumber;
   //set the flag which will be checked in dispatchLoop() and which *should* be checked
   //in lengthy operations in the various slaves
   if (globalSlave!=0)
      globalSlave->setKillFlag();
   ::signal(SIGALRM,SIG_DFL);
   alarm(5);  //generate an alarm signal in 5 seconds, in this time the slave has to exit
}
}
#endif

//////////////

SlaveBase::SlaveBase( const QByteArray &protocol,
                      const QByteArray &pool_socket,
                      const QByteArray &app_socket )
    : mProtocol(protocol),
      d(new SlaveBasePrivate(this))

{
    d->poolSocket = QFile::decodeName(pool_socket);
    s_protocol = protocol.data();
#ifdef Q_OS_UNIX
    if (qgetenv("KDE_DEBUG").isEmpty())
    {
        ::signal(SIGSEGV,&sigsegv_handler);
        ::signal(SIGILL,&sigsegv_handler);
        ::signal(SIGTRAP,&sigsegv_handler);
        ::signal(SIGABRT,&sigsegv_handler);
        ::signal(SIGBUS,&sigsegv_handler);
        ::signal(SIGALRM,&sigsegv_handler);
        ::signal(SIGFPE,&sigsegv_handler);
#ifdef SIGPOLL
        ::signal(SIGPOLL, &sigsegv_handler);
#endif
#ifdef SIGSYS
        ::signal(SIGSYS, &sigsegv_handler);
#endif
#ifdef SIGVTALRM
        ::signal(SIGVTALRM, &sigsegv_handler);
#endif
#ifdef SIGXCPU
        ::signal(SIGXCPU, &sigsegv_handler);
#endif
#ifdef SIGXFSZ
        ::signal(SIGXFSZ, &sigsegv_handler);
#endif
    }

    struct sigaction act;
    act.sa_handler = sigpipe_handler;
    sigemptyset( &act.sa_mask );
    act.sa_flags = 0;
    sigaction( SIGPIPE, &act, 0 );

    ::signal(SIGINT,&genericsig_handler);
    ::signal(SIGQUIT,&genericsig_handler);
    ::signal(SIGTERM,&genericsig_handler);
#endif

    globalSlave=this;

    d->isConnectedToApp = true;

    // by kahl for netmgr (need a way to identify slaves)
    d->slaveid = protocol;
    d->slaveid += QString::number(getpid());
    d->resume = false;
    d->needSendCanResume = false;
    d->config = new KConfig(QString(), KConfig::SimpleConfig);
    // The KConfigGroup needs the KConfig to exist during its whole lifetime.
    d->configGroup = new KConfigGroup(d->config, QString());
    d->onHold = false;
    d->wasKilled=false;
//    d->processed_size = 0;
    d->totalSize=0;
    d->sentListEntries=0;
    connectSlave(QFile::decodeName(app_socket));

    d->remotefile = 0;
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
        if (d->nextTimeout.isValid() && (d->nextTimeout < QDateTime::currentDateTime())) {
            QByteArray data = d->timeoutData;
            d->nextTimeout = QDateTime();
            d->timeoutData = QByteArray();
            special(data);
        }

        Q_ASSERT(d->appConnection.inited());

        int ms = -1;
        if (d->nextTimeout.isValid())
            ms = qMax<int>(QDateTime::currentDateTime().msecsTo(d->nextTimeout), 1);

        int ret = -1;
        if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(ms)) {
            // dispatch application messages
            int cmd;
            QByteArray data;
            ret = d->appConnection.read(&cmd, data);

            if (ret != -1) {
                if (d->inOpenLoop)
                    dispatchOpenCommand(cmd, data);
                else
                    dispatch(cmd, data);
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
        QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
    }

    // execute deferred deletes
    QCoreApplication::sendPostedEvents(NULL, QEvent::DeferredDelete);
}

void SlaveBase::connectSlave(const QString &address)
{
    d->appConnection.connectToRemote(QUrl(address));

    if (!d->appConnection.inited())
    {
        /*qDebug() << "failed to connect to" << address << endl
                      << "Reason:" << d->appConnection.errorString();*/
        exit();
        return;
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
   if (mIncomingMetaData.contains(key))
      return mIncomingMetaData[key];
   if (d->configData.contains(key))
      return d->configData[key];
   return QString();
}

MetaData SlaveBase::allMetaData() const
{
    return mIncomingMetaData;
}

bool SlaveBase::hasMetaData(const QString &key) const
{
   if (mIncomingMetaData.contains(key))
      return true;
   if (d->configData.contains(key))
      return true;
   return false;
}

KConfigGroup *SlaveBase::config()
{
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
   if (d->remotefile)
      return d->remotefile;

   const QByteArray charset (metaData(QLatin1String("Charset")).toLatin1());
   return (d->remotefile = new KRemoteEncoding( charset ));
}

void SlaveBase::data( const QByteArray &data )
{
   sendMetaData();
   send( MSG_DATA, data );
}

void SlaveBase::dataReq( )
{
   //sendMetaData();
   if (d->needSendCanResume)
      canResume(0);
   send( MSG_DATA_REQ );
}

void SlaveBase::opened()
{
   sendMetaData();
   send( MSG_OPENED );
   d->inOpenLoop = true;
}

void SlaveBase::error( int _errid, const QString &_text )
{
    if (d->m_state == d->ErrorCalled) {
        qWarning() << "error() called twice! Please fix the KIO slave.";
        return;
    } else if (d->m_state == d->FinishedCalled) {
        qWarning() << "error() called after finished()! Please fix the KIO slave.";
        return;
    }

    d->m_state = d->ErrorCalled;
    mIncomingMetaData.clear(); // Clear meta data
    d->rebuildConfig();
    mOutgoingMetaData.clear();
    KIO_DATA << (qint32) _errid << _text;

    send( MSG_ERROR, data );
    //reset
    d->sentListEntries=0;
    d->totalSize=0;
    d->inOpenLoop=false;
}

void SlaveBase::connected()
{
    send( MSG_CONNECTED );
}

void SlaveBase::finished()
{
    if (!d->pendingListEntries.isEmpty()) {
        listEntries(d->pendingListEntries);
        d->pendingListEntries.clear();
    }

    if (d->m_state == d->FinishedCalled) {
        qWarning() << "finished() called twice! Please fix the KIO slave.";
        return;
    } else if (d->m_state == d->ErrorCalled) {
        qWarning() << "finished() called after error()! Please fix the KIO slave.";
        return;
    }

    d->m_state = d->FinishedCalled;
    mIncomingMetaData.clear(); // Clear meta data
    d->rebuildConfig();
    sendMetaData();
    send( MSG_FINISHED );

    // reset
    d->sentListEntries=0;
    d->totalSize=0;
    d->inOpenLoop=false;
}

void SlaveBase::needSubUrlData()
{
    send( MSG_NEED_SUBURL_DATA );
}

/*
 * Map pid_t to a signed integer type that makes sense for QByteArray;
 * only the most common sizes 16 bit and 32 bit are special-cased.
 */
template<int T> struct PIDType { typedef pid_t PID_t; } ;
template<> struct PIDType<2> { typedef qint16 PID_t; } ;
template<> struct PIDType<4> { typedef qint32 PID_t; } ;

void SlaveBase::slaveStatus( const QString &host, bool connected )
{
    pid_t pid = getpid();
    qint8 b = connected ? 1 : 0;
    KIO_DATA << (PIDType<sizeof(pid_t)>::PID_t)pid << mProtocol << host << b;
    if (d->onHold)
       stream << d->onHoldUrl;
    send( MSG_SLAVE_STATUS, data );
}

void SlaveBase::canResume()
{
    send( MSG_CANRESUME );
}

void SlaveBase::totalSize( KIO::filesize_t _bytes )
{
    KIO_DATA << KIO_FILESIZE_T(_bytes);
    send( INF_TOTAL_SIZE, data );

    //this one is usually called before the first item is listed in listDir()
    d->totalSize=_bytes;
    d->sentListEntries=0;
}

void SlaveBase::processedSize(KIO::filesize_t _bytes)
{
    bool emitSignal = false;

    QDateTime now = QDateTime::currentDateTime();

    if (_bytes == d->totalSize)
        emitSignal=true;
    else {
        if (d->lastTimeout.isValid())
            emitSignal = d->lastTimeout.msecsTo(now); // emit size 10 times a second
        else
            emitSignal = true;
    }

    if (emitSignal) {
        KIO_DATA << KIO_FILESIZE_T(_bytes);
        send(INF_PROCESSED_SIZE, data);
        d->lastTimeout = now;
    }

    //    d->processed_size = _bytes;
}

void SlaveBase::written( KIO::filesize_t _bytes )
{
    KIO_DATA << KIO_FILESIZE_T(_bytes);
    send( MSG_WRITTEN, data );
}

void SlaveBase::position( KIO::filesize_t _pos )
{
    KIO_DATA << KIO_FILESIZE_T(_pos);
    send( INF_POSITION, data );
}

void SlaveBase::processedPercent( float /* percent */ )
{
  //qDebug() << "STUB";
}


void SlaveBase::speed( unsigned long _bytes_per_second )
{
    KIO_DATA << (quint32) _bytes_per_second;
    send( INF_SPEED, data );
}

void SlaveBase::redirection( const QUrl& _url )
{
    KIO_DATA << _url;
    send( INF_REDIRECTION, data );
}

void SlaveBase::errorPage()
{
    send( INF_ERROR_PAGE );
}

static bool isSubCommand(int cmd)
{
   return ( (cmd == CMD_REPARSECONFIGURATION) ||
            (cmd == CMD_META_DATA) ||
            (cmd == CMD_CONFIG) ||
            (cmd == CMD_SUBURL) ||
            (cmd == CMD_SLAVE_STATUS) ||
            (cmd == CMD_SLAVE_CONNECT) ||
            (cmd == CMD_SLAVE_HOLD) ||
            (cmd == CMD_MULTI_GET));
}

void SlaveBase::mimeType( const QString &_type)
{
  //qDebug() << _type;
  int cmd;
  do
  {
    // Send the meta-data each time we send the mime-type.
    if (!mOutgoingMetaData.isEmpty())
    {
      //qDebug() << "emitting meta data";
      KIO_DATA << mOutgoingMetaData;
      send( INF_META_DATA, data );
    }
    KIO_DATA << _type;
    send( INF_MIME_TYPE, data );
    while(true)
    {
       cmd = 0;
       int ret = -1;
       if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(-1)) {
           ret = d->appConnection.read( &cmd, data );
       }
       if (ret == -1) {
           //qDebug() << "read error";
           exit();
           return;
       }
       //qDebug() << "got" << cmd;
       if ( cmd == CMD_HOST) // Ignore.
          continue;
       if (!isSubCommand(cmd))
          break;

       dispatch( cmd, data );
    }
  }
  while (cmd != CMD_NONE);
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

void SlaveBase::warning( const QString &_msg)
{
    KIO_DATA << _msg;
    send( INF_WARNING, data );
}

void SlaveBase::infoMessage( const QString &_msg)
{
    KIO_DATA << _msg;
    send( INF_INFOMESSAGE, data );
}

bool SlaveBase::requestNetwork(const QString& host)
{
    KIO_DATA << host << d->slaveid;
    send( MSG_NET_REQUEST, data );

    if ( waitForAnswer( INF_NETWORK_STATUS, 0, data ) != -1 )
    {
        bool status;
        QDataStream stream( data );
        stream >> status;
        return status;
    } else
        return false;
}

void SlaveBase::dropNetwork(const QString& host)
{
    KIO_DATA << host << d->slaveid;
    send( MSG_NET_DROP, data );
}

void SlaveBase::statEntry( const UDSEntry& entry )
{
    KIO_DATA << entry;
    send( MSG_STAT_ENTRY, data );
}

#ifndef KDE_NO_DEPRECATED
void SlaveBase::listEntry( const UDSEntry& entry, bool _ready )
{
    if (_ready) {
        listEntries(d->pendingListEntries);
        d->pendingListEntries.clear();
    } else {
        listEntry(entry);
    }
}
#endif

void SlaveBase::listEntry(const UDSEntry &entry)
{
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

void SlaveBase::listEntries( const UDSEntryList& list )
{
    KIO_DATA << (quint32)list.count();

    foreach(const UDSEntry &entry, list) {
        stream << entry;
    }

    send( MSG_LIST_ENTRIES, data);
    d->sentListEntries+=(uint)list.count();
}

static void sigsegv_handler(int sig)
{
#ifdef Q_OS_UNIX
    ::signal(sig,SIG_DFL); // Next one kills

    //Kill us if we deadlock
    ::signal(SIGALRM,SIG_DFL);
    alarm(5);  //generate an alarm signal in 5 seconds, in this time the slave has to exit

    // Debug and printf should be avoided because they might
    // call malloc.. and get in a nice recursive malloc loop
    char buffer[120];
    qsnprintf(buffer, sizeof(buffer), "kioslave: ####### CRASH ###### protocol = %s pid = %d signal = %d\n", s_protocol, getpid(), sig);
    write(2, buffer, strlen(buffer));
#ifndef NDEBUG
#if HAVE_BACKTRACE
    void* trace[256];
    int n = backtrace(trace, 256);
    if (n)
      backtrace_symbols_fd(trace, n, 2);
#endif
#endif
    ::exit(1);
#endif
}

static void sigpipe_handler (int)
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

KIOCORE_EXPORT QString KIO::unsupportedActionErrorString(const QString &protocol, int cmd) {
  switch (cmd) {
    case CMD_CONNECT:
      return i18n("Opening connections is not supported with the protocol %1." , protocol);
    case CMD_DISCONNECT:
      return i18n("Closing connections is not supported with the protocol %1." , protocol);
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

void SlaveBase::openConnection(void)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_CONNECT)); }
void SlaveBase::closeConnection(void)
{ } // No response!
void SlaveBase::stat(QUrl const &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_STAT)); }
void SlaveBase::put(QUrl const &, int, JobFlags )
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_PUT)); }
void SlaveBase::special(const QByteArray &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SPECIAL)); }
void SlaveBase::listDir(QUrl const &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_LISTDIR)); }
void SlaveBase::get(QUrl const & )
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_GET)); }
void SlaveBase::open(QUrl const &, QIODevice::OpenMode)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_OPEN)); }
void SlaveBase::read(KIO::filesize_t)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_READ)); }
void SlaveBase::write(const QByteArray &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_WRITE)); }
void SlaveBase::seek(KIO::filesize_t)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SEEK)); }
void SlaveBase::close()
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_CLOSE)); }
void SlaveBase::mimetype(QUrl const &url)
{ get(url); }
void SlaveBase::rename(QUrl const &, QUrl const &, JobFlags)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_RENAME)); }
void SlaveBase::symlink(QString const &, QUrl const &, JobFlags)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SYMLINK)); }
void SlaveBase::copy(QUrl const &, QUrl const &, int, JobFlags)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_COPY)); }
void SlaveBase::del(QUrl const &, bool)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_DEL)); }
void SlaveBase::setLinkDest(const QUrl &, const QString&)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SETLINKDEST)); }
void SlaveBase::mkdir(QUrl const &, int)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_MKDIR)); }
void SlaveBase::chmod(QUrl const &, int)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_CHMOD)); }
void SlaveBase::setModificationTime(QUrl const &, const QDateTime&)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SETMODIFICATIONTIME)); }
void SlaveBase::chown(QUrl const &, const QString &, const QString &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_CHOWN)); }
void SlaveBase::setSubUrl(QUrl const &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_SUBURL)); }
void SlaveBase::multiGet(const QByteArray &)
{ error(  ERR_UNSUPPORTED_ACTION, unsupportedActionErrorString(mProtocol, CMD_MULTI_GET)); }


void SlaveBase::slave_status()
{ slaveStatus( QString(), false ); }

void SlaveBase::reparseConfiguration()
{
    delete d->remotefile;
    d->remotefile = 0;
}

bool SlaveBase::openPasswordDialog( AuthInfo& info, const QString &errorMsg )
{
    const long windowId = metaData(QLatin1String("window-id")).toLong();
    const unsigned long userTimestamp = metaData(QLatin1String("user-timestamp")).toULong();
    QString errorMessage;
    if (metaData(QLatin1String("no-auth-prompt")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        errorMessage = QLatin1String("<NoAuthPrompt>");
    } else {
        errorMessage = errorMsg;
    }

    AuthInfo dlgInfo (info);
    // Make sure the modified flag is not set.
    dlgInfo.setModified(false);
    // Prevent queryAuthInfo from caching the user supplied password since
    // we need the ioslaves to first authenticate against the server with
    // it to ensure it is valid.
    dlgInfo.setExtraField(QLatin1String("skip-caching-on-query"), true);

    KPasswdServer* passwdServer = d->passwdServer();

    if (passwdServer) {
        qlonglong seqNr = passwdServer->queryAuthInfo(dlgInfo, errorMessage, windowId,
                                                      SlaveBasePrivate::s_seqNr, userTimestamp);
        if (seqNr > 0) {
            SlaveBasePrivate::s_seqNr = seqNr;
            if (dlgInfo.isModified()) {
                info = dlgInfo;
                return true;
            }
        }
    }

    return false;
}

int SlaveBase::messageBox( MessageBoxType type, const QString &text, const QString &caption,
                           const QString &buttonYes, const QString &buttonNo )
{
    return messageBox( text, type, caption, buttonYes, buttonNo, QString() );
}

int SlaveBase::messageBox( const QString &text, MessageBoxType type, const QString &caption,
                           const QString &buttonYes, const QString &buttonNo,
                           const QString &dontAskAgainName )
{
    //qDebug() << "messageBox " << type << " " << text << " - " << caption << buttonYes << buttonNo;
    KIO_DATA << (qint32)type << text << caption << buttonYes << buttonNo << dontAskAgainName;
    send( INF_MESSAGEBOX, data );
    if ( waitForAnswer( CMD_MESSAGEBOXANSWER, 0, data ) != -1 )
    {
        QDataStream stream( data );
        int answer;
        stream >> answer;
        //qDebug() << "got messagebox answer" << answer;
        return answer;
    } else
        return 0; // communication failure
}

bool SlaveBase::canResume( KIO::filesize_t offset )
{
    //qDebug() << "offset=" << KIO::number(offset);
    d->needSendCanResume = false;
    KIO_DATA << KIO_FILESIZE_T(offset);
    send( MSG_RESUME, data );
    if ( offset )
    {
        int cmd;
        if ( waitForAnswer( CMD_RESUMEANSWER, CMD_NONE, data, &cmd ) != -1 )
        {
            //qDebug() << "returning" << (cmd == CMD_RESUMEANSWER);
            return cmd == CMD_RESUMEANSWER;
        } else
            return false;
    }
    else // No resuming possible -> no answer to wait for
        return true;
}



int SlaveBase::waitForAnswer( int expected1, int expected2, QByteArray & data, int *pCmd )
{
    int cmd, result = -1;
    for (;;)
    {
        if (d->appConnection.hasTaskAvailable() || d->appConnection.waitForIncomingTask(-1)) {
            result = d->appConnection.read( &cmd, data );
        }
        if (result == -1) {
            //qDebug() << "read error.";
            return -1;
        }

        if ( cmd == expected1 || cmd == expected2 )
        {
            if ( pCmd ) *pCmd = cmd;
            return result;
        }
        if ( isSubCommand(cmd) )
        {
            dispatch( cmd, data );
        }
        else
        {
            qFatal("Fatal Error: Got cmd %d, while waiting for an answer!", cmd);
        }
    }
}


int SlaveBase::readData( QByteArray &buffer)
{
   int result = waitForAnswer( MSG_DATA, 0, buffer );
   //qDebug() << "readData: length = " << result << " ";
   return result;
}

void SlaveBase::setTimeoutSpecialCommand(int timeout, const QByteArray &data)
{
   if (timeout > 0)
      d->nextTimeout = QDateTime::currentDateTime().addSecs(timeout);
   else if (timeout == 0)
      d->nextTimeout = QDateTime::currentDateTime().addSecs(1); // Immediate timeout
   else
      d->nextTimeout = QDateTime(); // Canceled

   d->timeoutData = data;
}

void SlaveBase::dispatch( int command, const QByteArray &data )
{
    QDataStream stream( data );

    QUrl url;
    int i;

    switch( command ) {
    case CMD_HOST: {
        // Reset s_seqNr, see kpasswdserver/DESIGN
        SlaveBasePrivate::s_seqNr = 0;
        QString passwd;
        QString host, user;
        quint16 port;
        stream >> host >> port >> user >> passwd;
        d->m_state = d->InsideMethod;
        setHost( host, port, user, passwd );
        d->verifyErrorFinishedNotCalled("setHost()");
        d->m_state = d->Idle;
    } break;
    case CMD_CONNECT: {
        openConnection( );
    } break;
    case CMD_DISCONNECT: {
        closeConnection( );
    } break;
    case CMD_SLAVE_STATUS: {
        d->m_state = d->InsideMethod;
        slave_status();
        // TODO verify that the slave has called slaveStatus()?
        d->verifyErrorFinishedNotCalled("slave_status()");
        d->m_state = d->Idle;
    } break;
    case CMD_SLAVE_CONNECT: {
        d->onHold = false;
        QString app_socket;
        QDataStream stream( data );
        stream >> app_socket;
        d->appConnection.send( MSG_SLAVE_ACK );
        disconnectSlave();
        d->isConnectedToApp = true;
        connectSlave(app_socket);
        virtual_hook(AppConnectionMade, 0);
    } break;
    case CMD_SLAVE_HOLD: {
        QUrl url;
        QDataStream stream( data );
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
        reparseConfiguration();
        d->verifyErrorFinishedNotCalled("reparseConfiguration()");
        d->m_state = d->Idle;
    } break;
    case CMD_CONFIG: {
        stream >> d->configData;
        d->rebuildConfig();
#if 0 //TODO: decide what to do in KDE 4.1
        KSocks::setConfig(d->configGroup);
#endif
        delete d->remotefile;
        d->remotefile = 0;
    } break;
    case CMD_GET: {
        stream >> url;
        d->m_state = d->InsideMethod;
        get( url );
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
        if ( iOverwrite != 0 ) flags |= Overwrite;
        if ( iResume != 0 ) flags |= Resume;

        // Remember that we need to send canResume(), TransferJob is expecting
        // it. Well, in theory this shouldn't be done if resume is true.
        //   (the resume bool is currently unused)
        d->needSendCanResume = true   /* !resume */;

        d->m_state = d->InsideMethod;
        put( url, permissions, flags);
        d->verifyState("put()");
        d->m_state = d->Idle;
    } break;
    case CMD_STAT: {
        stream >> url;
        d->m_state = d->InsideMethod;
        stat( url ); //krazy:exclude=syscalls
        d->verifyState("stat()");
        d->m_state = d->Idle;
    } break;
    case CMD_MIMETYPE: {
        stream >> url;
        d->m_state = d->InsideMethod;
        mimetype( url );
        d->verifyState("mimetype()");
        d->m_state = d->Idle;
    } break;
    case CMD_LISTDIR: {
        stream >> url;
        d->m_state = d->InsideMethod;
        listDir( url );
        d->verifyState("listDir()");
        d->m_state = d->Idle;
    } break;
    case CMD_MKDIR: {
        stream >> url >> i;
        d->m_state = d->InsideMethod;
        mkdir( url, i ); //krazy:exclude=syscalls
        d->verifyState("mkdir()");
        d->m_state = d->Idle;
    } break;
    case CMD_RENAME: {
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> iOverwrite;
        JobFlags flags;
        if ( iOverwrite != 0 ) flags |= Overwrite;
        d->m_state = d->InsideMethod;
        rename( url, url2, flags ); //krazy:exclude=syscalls
        d->verifyState("rename()");
        d->m_state = d->Idle;
    } break;
    case CMD_SYMLINK: {
        qint8 iOverwrite;
        QString target;
        stream >> target >> url >> iOverwrite;
        JobFlags flags;
        if ( iOverwrite != 0 ) flags |= Overwrite;
        d->m_state = d->InsideMethod;
        symlink( target, url, flags );
        d->verifyState("symlink()");
        d->m_state = d->Idle;
    } break;
    case CMD_COPY: {
        int permissions;
        qint8 iOverwrite;
        QUrl url2;
        stream >> url >> url2 >> permissions >> iOverwrite;
        JobFlags flags;
        if ( iOverwrite != 0 ) flags |= Overwrite;
        d->m_state = d->InsideMethod;
        copy( url, url2, permissions, flags );
        d->verifyState("copy()");
        d->m_state = d->Idle;
    } break;
    case CMD_DEL: {
        qint8 isFile;
        stream >> url >> isFile;
        d->m_state = d->InsideMethod;
        del( url, isFile != 0);
        d->verifyState("del()");
        d->m_state = d->Idle;
    } break;
    case CMD_CHMOD: {
        stream >> url >> i;
        d->m_state = d->InsideMethod;
        chmod( url, i);
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
        special( data );
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
        qWarning() << "Got unexpected CMD_NONE!";
    } break;
    case CMD_MULTI_GET: {
        d->m_state = d->InsideMethod;
        multiGet( data );
        d->verifyState("multiGet()");
        d->m_state = d->Idle;
    } break;
    default: {
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KDE.
    } break;
    }
}

bool SlaveBase::checkCachedAuthentication( AuthInfo& info )
{
    KPasswdServer* passwdServer = d->passwdServer();
    return (passwdServer &&
            passwdServer->checkAuthInfo(info, metaData(QLatin1String("window-id")).toLong(),
                                        metaData(QLatin1String("user-timestamp")).toULong()));
}

void SlaveBase::dispatchOpenCommand( int command, const QByteArray &data )
{
    QDataStream stream( data );

    switch( command ) {
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
    }
    case CMD_NONE:
        break;
    case CMD_CLOSE:
        close();                // must call finish(), which will set d->inOpenLoop=false
        break;
    default:
        // Some command we don't understand.
        // Just ignore it, it may come from some future version of KDE.
        break;
    }
}

bool SlaveBase::cacheAuthentication( const AuthInfo& info )
{
    KPasswdServer* passwdServer = d->passwdServer();

    if (!passwdServer) {
        return false;
    }

    passwdServer->addAuthInfo(info, metaData(QLatin1String("window-id")).toLongLong());
    return true;
}

int SlaveBase::connectTimeout()
{
    bool ok;
    QString tmp = metaData(QLatin1String("ConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok)
       return result;
    return DEFAULT_CONNECT_TIMEOUT;
}

int SlaveBase::proxyConnectTimeout()
{
    bool ok;
    QString tmp = metaData(QLatin1String("ProxyConnectTimeout"));
    int result = tmp.toInt(&ok);
    if (ok)
       return result;
    return DEFAULT_PROXY_CONNECT_TIMEOUT;
}


int SlaveBase::responseTimeout()
{
    bool ok;
    QString tmp = metaData(QLatin1String("ResponseTimeout"));
    int result = tmp.toInt(&ok);
    if (ok)
       return result;
    return DEFAULT_RESPONSE_TIMEOUT;
}


int SlaveBase::readTimeout()
{
    bool ok;
    QString tmp = metaData(QLatin1String("ReadTimeout"));
    int result = tmp.toInt(&ok);
    if (ok)
       return result;
    return DEFAULT_READ_TIMEOUT;
}

bool SlaveBase::wasKilled() const
{
   return d->wasKilled;
}

void SlaveBase::setKillFlag()
{
   d->wasKilled=true;
}

void SlaveBase::send(int cmd, const QByteArray& arr )
{
   slaveWriteError = false;
   if (!d->appConnection.send(cmd, arr))
       // Note that slaveWriteError can also be set by sigpipe_handler
       slaveWriteError = true;
   if (slaveWriteError) exit();
}

void SlaveBase::virtual_hook( int, void* )
{ /*BASE::virtual_hook( id, data );*/ }

void SlaveBase::lookupHost(const QString& host)
{
    KIO_DATA << host;
    send(MSG_HOST_INFO_REQ, data);
}

int SlaveBase::waitForHostInfo(QHostInfo& info)
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
