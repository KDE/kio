// -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 2; c-file-style: "stroustrup" -*-
/*  This file is part of the KDE libraries
    Copyright (C) 2000-2006 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

/*
    Recommended reading explaining FTP details and quirks:
      http://cr.yp.to/ftp.html  (by D.J. Bernstein)

    RFC:
      RFC  959 "File Transfer Protocol (FTP)"
      RFC 1635 "How to Use Anonymous FTP"
      RFC 2428 "FTP Extensions for IPv6 and NATs" (defines EPRT and EPSV)
*/

#include <config-kioslave-ftp.h>

#define  KIO_FTP_PRIVATE_INCLUDE
#include "ftp.h"

#include <utime.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QTcpServer>
#include <QtNetwork/QSslSocket>
#include <QtNetwork/QAuthenticator>
#include <qmimedatabase.h>
#include <qplatformdefs.h>

#include <QDebug>
#include <kio/ioslave_defaults.h>
#include <kremoteencoding.h>
#include <kconfiggroup.h>

#if HAVE_STRTOLL
  #define charToLongLong(a) strtoll(a, 0, 10)
#else
  #define charToLongLong(a) strtol(a, 0, 10)
#endif

#define FTP_LOGIN   "anonymous"
#define FTP_PASSWD  "anonymous@"

//#undef  kDebug
#define ENABLE_CAN_RESUME

static QString ftpCleanPath(const QString& path)
{
    if (path.endsWith(QLatin1String(";type=A"), Qt::CaseInsensitive) ||
        path.endsWith(QLatin1String(";type=I"), Qt::CaseInsensitive) ||
        path.endsWith(QLatin1String(";type=D"), Qt::CaseInsensitive)) {
        return path.left((path.length() - qstrlen(";type=X")));
    }

    return path;
}

static char ftpModeFromPath(const QString& path, char defaultMode = '\0')
{
    const int index = path.lastIndexOf(QLatin1String(";type="));

    if (index > -1 && (index+6) < path.size()) {
        const QChar mode = path.at(index+6);
        // kio_ftp supports only A (ASCII) and I(BINARY) modes.
        if (mode == QLatin1Char('A') || mode == QLatin1Char('a') ||
            mode == QLatin1Char('I') || mode == QLatin1Char('i')) {
            return mode.toUpper().toLatin1();
        }
    }

    return defaultMode;
}

static bool supportedProxyScheme(const QString& scheme)
{
    return (scheme == QLatin1String("ftp") || scheme == QLatin1String("socks"));
}

static bool isSocksProxy()
{
    return (QNetworkProxy::applicationProxy().type() == QNetworkProxy::Socks5Proxy);
}


// JPF: somebody should find a better solution for this or move this to KIO
// JPF: anyhow, in KDE 3.2.0 I found diffent MAX_IPC_SIZE definitions!
namespace KIO {
    enum buffersizes
    {  /**
        * largest buffer size that should be used to transfer data between
        * KIO slaves using the data() function
        */
        maximumIpcSize = 32 * 1024,
        /**
         * this is a reasonable value for an initial read() that a KIO slave
         * can do to obtain data via a slow network connection.
         */
        initialIpcSize =  2 * 1024,
        /**
         * recommended size of a data block passed to findBufferFileType()
         */
        minimumMimeSize =     1024
    };

    // JPF: this helper was derived from write_all in file.cc (FileProtocol).
    static // JPF: in ftp.cc we make it static
    /**
     * This helper handles some special issues (blocking and interrupted
     * system call) when writing to a file handle.
     *
     * @return 0 on success or an error code on failure (ERR_COULD_NOT_WRITE,
     * ERR_DISK_FULL, ERR_CONNECTION_BROKEN).
     */
   int WriteToFile(int fd, const char *buf, size_t len)
   {
      while (len > 0)
      {  // JPF: shouldn't there be a KDE_write?
         ssize_t written = write(fd, buf, len);
         if (written >= 0)
         {   buf += written;
             len -= written;
             continue;
         }
         switch(errno)
         {   case EINTR:   continue;
             case EPIPE:   return ERR_CONNECTION_BROKEN;
             case ENOSPC:  return ERR_DISK_FULL;
             default:      return ERR_COULD_NOT_WRITE;
         }
      }
      return 0;
   }
}

KIO::filesize_t Ftp::UnknownSize = (KIO::filesize_t)-1;

using namespace KIO;

extern "C" Q_DECL_EXPORT int kdemain( int argc, char **argv )
{
  QCoreApplication app(argc, argv);
  app.setApplicationName(QLatin1String("kio_ftp"));

  // qDebug() << "Starting " << getpid();

  if (argc != 4)
  {
     fprintf(stderr, "Usage: kio_ftp protocol domain-socket1 domain-socket2\n");
     exit(-1);
  }

  Ftp slave(argv[2], argv[3]);
  slave.dispatchLoop();

  // qDebug() << "Done";
  return 0;
}

//===============================================================================
// Ftp
//===============================================================================

Ftp::Ftp( const QByteArray &pool, const QByteArray &app )
    : SlaveBase( "ftp", pool, app )
{
  // init the socket data
  m_data = m_control = NULL;
  m_server = NULL;
  ftpCloseControlConnection();

  // init other members
  m_port = 0;
  m_socketProxyAuth = 0;
}


Ftp::~Ftp()
{
  // qDebug();
  closeConnection();
}

/**
 * This closes a data connection opened by ftpOpenDataConnection().
 */
void Ftp::ftpCloseDataConnection()
{
    delete m_data;
    m_data = NULL;
    delete m_server;
    m_server = NULL;
}

/**
 * This closes a control connection opened by ftpOpenControlConnection() and reinits the
 * related states.  This method gets called from the constructor with m_control = NULL.
 */
void Ftp::ftpCloseControlConnection()
{
  m_extControl = 0;
  delete m_control;
  m_control = NULL;
  m_cDataMode = 0;
  m_bLoggedOn = false;    // logon needs control connction
  m_bTextMode = false;
  m_bBusy = false;
}

/**
 * Returns the last response from the server (iOffset >= 0)  -or-  reads a new response
 * (iOffset < 0). The result is returned (with iOffset chars skipped for iOffset > 0).
 */
const char* Ftp::ftpResponse(int iOffset)
{
  Q_ASSERT(m_control != NULL);    // must have control connection socket
  const char *pTxt = m_lastControlLine.data();

  // read the next line ...
  if(iOffset < 0)
  {
    int  iMore = 0;
    m_iRespCode = 0;

    if (!pTxt) return 0; // avoid using a NULL when calling atoi.

    // If the server sends a multiline response starting with
    // "nnn-text" we loop here until a final "nnn text" line is
    // reached. Only data from the final line will be stored.
    do {
      while (!m_control->canReadLine() && m_control->waitForReadyRead((readTimeout() * 1000))) {}
      m_lastControlLine = m_control->readLine();
      pTxt = m_lastControlLine.data();
      int iCode  = atoi(pTxt);
      if (iMore == 0) {
          // first line
          // qDebug() << "    > " << pTxt;
          if(iCode >= 100) {
              m_iRespCode = iCode;
              if (pTxt[3] == '-') {
                  // marker for a multiple line response
                  iMore = iCode;
              }
          } else {
              qWarning() << "Cannot parse valid code from line" << pTxt;
          }
      } else {
          // multi-line
          // qDebug() << "    > " << pTxt;
          if (iCode >= 100 && iCode == iMore && pTxt[3] == ' ') {
              iMore = 0;
          }
      }
    } while(iMore != 0);
    // qDebug() << "resp> " << pTxt;

    m_iRespType = (m_iRespCode > 0) ? m_iRespCode / 100 : 0;
  }

  // return text with offset ...
  while(iOffset-- > 0 && pTxt[0])
    pTxt++;
  return pTxt;
}


void Ftp::closeConnection()
{
  if(m_control != NULL || m_data != NULL)
    // qDebug() << "m_bLoggedOn=" << m_bLoggedOn << " m_bBusy=" << m_bBusy;

  if(m_bBusy)              // ftpCloseCommand not called
  {
    qWarning() << "Abandoned data stream";
    ftpCloseDataConnection();
  }

  if(m_bLoggedOn)           // send quit
  {
    if( !ftpSendCmd( "quit", 0 ) || (m_iRespType != 2) )
      qWarning() << "QUIT returned error: " << m_iRespCode;
  }

  // close the data and control connections ...
  ftpCloseDataConnection();
  ftpCloseControlConnection();
}

void Ftp::setHost( const QString& _host, quint16 _port, const QString& _user,
                   const QString& _pass )
{
  // qDebug() << _host << "port=" << _port << "user=" << _user;

  m_proxyURL.clear();
  m_proxyUrls = config()->readEntry("ProxyUrls", QStringList());
  // qDebug() << "proxy urls:" << m_proxyUrls;

  if ( m_host != _host || m_port != _port ||
       m_user != _user || m_pass != _pass )
    closeConnection();

  m_host = _host;
  m_port = _port;
  m_user = _user;
  m_pass = _pass;
}

void Ftp::openConnection()
{
  ftpOpenConnection(loginExplicit);
}

bool Ftp::ftpOpenConnection (LoginMode loginMode)
{
  // check for implicit login if we are already logged on ...
  if(loginMode == loginImplicit && m_bLoggedOn)
  {
    Q_ASSERT(m_control != NULL);    // must have control connection socket
    return true;
  }

  // qDebug() << "host=" << m_host << ", port=" << m_port << ", user=" << m_user << "password= [password hidden]";

  infoMessage( i18n("Opening connection to host %1", m_host) );

  if ( m_host.isEmpty() )
  {
    error( ERR_UNKNOWN_HOST, QString() );
    return false;
  }

  Q_ASSERT( !m_bLoggedOn );

  m_initialPath.clear();
  m_currentPath.clear();

  if (!ftpOpenControlConnection() )
    return false;          // error emitted by ftpOpenControlConnection
  infoMessage( i18n("Connected to host %1", m_host) );

  bool userNameChanged = false;
  if(loginMode != loginDefered)
  {
    m_bLoggedOn = ftpLogin(&userNameChanged);
    if( !m_bLoggedOn )
      return false;       // error emitted by ftpLogin
  }

  m_bTextMode = config()->readEntry("textmode", false);
  connected();

  // Redirected due to credential change...
  if (userNameChanged && m_bLoggedOn)
  {
    QUrl realURL;
    realURL.setScheme( "ftp" );
    if (m_user != FTP_LOGIN)
      realURL.setUserName( m_user );
    if (m_pass != FTP_PASSWD)
      realURL.setPassword( m_pass );
    realURL.setHost( m_host );
    if ( m_port > 0 && m_port != DEFAULT_FTP_PORT )
      realURL.setPort( m_port );
    if ( m_initialPath.isEmpty() )
      m_initialPath = '/';
    realURL.setPath( m_initialPath );
    // qDebug() << "User name changed! Redirecting to" << realURL;
    redirection( realURL );
    finished();
    return false;
  }

  return true;
}


/**
 * Called by @ref openConnection. It opens the control connection to the ftp server.
 *
 * @return true on success.
 */
bool Ftp::ftpOpenControlConnection()
{
  if (m_proxyUrls.isEmpty())
      return ftpOpenControlConnection(m_host, m_port);

  int errorCode = 0;
  QString errorMessage;

  Q_FOREACH (const QString& proxyUrl, m_proxyUrls) {
    const QUrl url(proxyUrl);
    const QString scheme(url.scheme());

    if (!supportedProxyScheme(scheme)) {
      // TODO: Need a new error code to indicate unsupported URL scheme.
      errorCode = ERR_COULD_NOT_CONNECT;
      errorMessage = url.toString();
      continue;
    }

    if (scheme == QLatin1String("socks")) {
      // qDebug() << "Connecting to SOCKS proxy @" << url;
      const int proxyPort = url.port();
      QNetworkProxy proxy (QNetworkProxy::Socks5Proxy, url.host(), (proxyPort == -1 ? 0 : proxyPort));
      QNetworkProxy::setApplicationProxy(proxy);
      if (ftpOpenControlConnection(m_host, m_port)) {
        return true;
      }
      QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    } else {
      if (ftpOpenControlConnection(url.host(), url.port())) {
        m_proxyURL = url;
        return true;
      }
    }
  }

  if (errorCode) {
    error(errorCode, errorMessage);
  }

  return false;
}

bool Ftp::ftpOpenControlConnection( const QString &host, int port )
{
  // implicitly close, then try to open a new connection ...
  closeConnection();
  QString sErrorMsg;

  // now connect to the server and read the login message ...
  if (port == 0)
    port = 21;                  // default FTP port
  m_control = synchronousConnectToHost(host, port);
  connect(m_control, SIGNAL(proxyAuthenticationRequired(QNetworkProxy,QAuthenticator*)),
          this, SLOT(proxyAuthentication(QNetworkProxy,QAuthenticator*)));
  int iErrorCode = m_control->state() == QAbstractSocket::ConnectedState ? 0 : ERR_COULD_NOT_CONNECT;

  // on connect success try to read the server message...
  if(iErrorCode == 0)
  {
    const char* psz = ftpResponse(-1);
    if(m_iRespType != 2)
    { // login not successful, do we have an message text?
      if(psz[0])
        sErrorMsg = i18n("%1.\n\nReason: %2", host, psz);
      iErrorCode = ERR_COULD_NOT_CONNECT;
    }
  }
  else
  {
    if (m_control->error() == QAbstractSocket::HostNotFoundError)
      iErrorCode = ERR_UNKNOWN_HOST;

    sErrorMsg = QString("%1: %2").arg(host).arg(m_control->errorString());
  }

  // if there was a problem - report it ...
  if(iErrorCode == 0)             // OK, return success
    return true;
  closeConnection();              // clean-up on error
  error(iErrorCode, sErrorMsg);
  return false;
}

/**
 * Called by @ref openConnection. It logs us in.
 * @ref m_initialPath is set to the current working directory
 * if logging on was successful.
 *
 * @return true on success.
 */
bool Ftp::ftpLogin(bool* userChanged)
{
  infoMessage( i18n("Sending login information") );

  Q_ASSERT( !m_bLoggedOn );

  QString user (m_user);
  QString pass (m_pass);

  if ( config()->readEntry("EnableAutoLogin", false) )
  {
    QString au = config()->readEntry("autoLoginUser");
    if ( !au.isEmpty() )
    {
        user = au;
        pass = config()->readEntry("autoLoginPass");
    }
  }

  AuthInfo info;
  info.url.setScheme( "ftp" );
  info.url.setHost( m_host );
  if ( m_port > 0 && m_port != DEFAULT_FTP_PORT )
      info.url.setPort( m_port );
  if (!user.isEmpty())
      info.url.setUserName(user);

  // Check for cached authentication first and fallback to
  // anonymous login when no stored credentials are found.
  if (!config()->readEntry("TryAnonymousLoginFirst", false) &&
      pass.isEmpty() && checkCachedAuthentication(info))
  {
      user = info.username;
      pass = info.password;
  }

  // Try anonymous login if both username/password
  // information is blank.
  if (user.isEmpty() && pass.isEmpty())
  {
      user = FTP_LOGIN;
      pass = FTP_PASSWD;
  }

  QByteArray tempbuf;
  QString lastServerResponse;
  int failedAuth = 0;
  bool promptForRetry = false;

  // Give the user the option to login anonymously...
  info.setExtraField(QLatin1String("anonymous"), false);

  do
  {
    // Check the cache and/or prompt user for password if 1st
    // login attempt failed OR the user supplied a login name,
    // but no password.
    if ( failedAuth > 0 || (!user.isEmpty() && pass.isEmpty()) )
    {
      QString errorMsg;
      // qDebug() << "Prompting user for login info...";

      // Ask user if we should retry after when login fails!
      if( failedAuth > 0 && promptForRetry)
      {
        errorMsg = i18n("Message sent:\nLogin using username=%1 and "
                        "password=[hidden]\n\nServer replied:\n%2\n\n"
                        , user, lastServerResponse);
      }

      if ( user != FTP_LOGIN )
        info.username = user;

      info.prompt = i18n("You need to supply a username and a password "
                         "to access this site.");
      info.commentLabel = i18n( "Site:" );
      info.comment = i18n("<b>%1</b>",  m_host );
      info.keepPassword = true; // Prompt the user for persistence as well.
      info.setModified(false);  // Default the modified flag since we reuse authinfo.

      bool disablePassDlg = config()->readEntry( "DisablePassDlg", false );
      if ( disablePassDlg || !openPasswordDialog( info, errorMsg ) )
      {
        error( ERR_USER_CANCELED, m_host );
        return false;
      }
      else
      {
        // User can decide go anonymous using checkbox
        if( info.getExtraField( "anonymous" ).toBool() )
        {
          user = FTP_LOGIN;
          pass = FTP_PASSWD;
        }
        else
        {
          user = info.username;
          pass = info.password;
        }
        promptForRetry = true;
      }
    }

    tempbuf = "USER ";
    tempbuf += user.toLatin1();
    if ( m_proxyURL.isValid() )
    {
      tempbuf += '@';
      tempbuf += m_host.toLatin1();
      if ( m_port > 0 && m_port != DEFAULT_FTP_PORT )
      {
        tempbuf += ':';
        tempbuf += QString::number(m_port).toLatin1();
      }
    }

    // qDebug() << "Sending Login name: " << tempbuf;

    bool loggedIn = ( ftpSendCmd(tempbuf) && (m_iRespCode == 230) );
    bool needPass = (m_iRespCode == 331);
    // Prompt user for login info if we do not
    // get back a "230" or "331".
    if ( !loggedIn && !needPass )
    {
      lastServerResponse = ftpResponse(0);
      // qDebug() << "Login failed: " << lastServerResponse;
      ++failedAuth;
      continue;  // Well we failed, prompt the user please!!
    }

    if( needPass )
    {
      tempbuf = "PASS ";
      tempbuf += pass.toLatin1();
      // qDebug() << "Sending Login password: " << "[protected]";
      loggedIn = ( ftpSendCmd(tempbuf) && (m_iRespCode == 230) );
    }

    if ( loggedIn )
    {
      // Make sure the user name changed flag is properly set.
      if (userChanged)
        *userChanged = (!m_user.isEmpty() && (m_user != user));

      // Do not cache the default login!!
      if( user != FTP_LOGIN && pass != FTP_PASSWD )
      {
        // Update the username in case it was changed during login.
        if (!m_user.isEmpty()) {
            info.url.setUserName(user);
            m_user = user;
        }

        // Cache the password if the user requested it.
        if (info.keepPassword) {
            cacheAuthentication(info);
        }
      }
      failedAuth = -1;
    }
    else
    {
        // some servers don't let you login anymore
        // if you fail login once, so restart the connection here
        lastServerResponse = ftpResponse(0);
        if (!ftpOpenControlConnection())
        {
            return false;
        }
    }
  } while( ++failedAuth );


  // qDebug() << "Login OK";
  infoMessage( i18n("Login OK") );

  // Okay, we're logged in. If this is IIS 4, switch dir listing style to Unix:
  // Thanks to jk@soegaard.net (Jens Kristian Sgaard) for this hint
  if( ftpSendCmd("SYST") && (m_iRespType == 2) )
  {
    if( !qstrncmp( ftpResponse(0), "215 Windows_NT", 14 ) ) // should do for any version
    {
      ftpSendCmd( "site dirstyle" );
      // Check if it was already in Unix style
      // Patch from Keith Refson <Keith.Refson@earth.ox.ac.uk>
      if( !qstrncmp( ftpResponse(0), "200 MSDOS-like directory output is on", 37 ))
         //It was in Unix style already!
         ftpSendCmd( "site dirstyle" );
      // windows won't support chmod before KDE konquers their desktop...
      m_extControl |= chmodUnknown;
    }
  }
  else
    qWarning() << "SYST failed";

  if ( config()->readEntry ("EnableAutoLoginMacro", false) )
    ftpAutoLoginMacro ();

  // Get the current working directory
  // qDebug() << "Searching for pwd";
  if( !ftpSendCmd("PWD") || (m_iRespType != 2) )
  {
    // qDebug() << "Couldn't issue pwd command";
    error( ERR_COULD_NOT_LOGIN, i18n("Could not login to %1.", m_host) ); // or anything better ?
    return false;
  }

  QString sTmp = remoteEncoding()->decode( ftpResponse(3) );
  int iBeg = sTmp.indexOf('"');
  int iEnd = sTmp.lastIndexOf('"');
  if(iBeg > 0 && iBeg < iEnd)
  {
    m_initialPath = sTmp.mid(iBeg+1, iEnd-iBeg-1);
    if(m_initialPath[0] != '/') m_initialPath.prepend('/');
    // qDebug() << "Initial path set to: " << m_initialPath;
    m_currentPath = m_initialPath;
  }
  return true;
}

void Ftp::ftpAutoLoginMacro ()
{
  QString macro = metaData( "autoLoginMacro" );

  if ( macro.isEmpty() )
    return;

  const QStringList list = macro.split('\n',QString::SkipEmptyParts);

  for(QStringList::const_iterator it = list.begin() ; it != list.end() ; ++it )
  {
    if ( (*it).startsWith(QLatin1String("init")) )
    {
      const QStringList list2 = macro.split( '\\',QString::SkipEmptyParts);
      it = list2.begin();
      ++it;  // ignore the macro name

      for( ; it != list2.end() ; ++it )
      {
        // TODO: Add support for arbitrary commands
        // besides simply changing directory!!
        if ( (*it).startsWith( QLatin1String("cwd") ) )
          (void)ftpFolder( (*it).mid(4), false );
      }

      break;
    }
  }
}


/**
 * ftpSendCmd - send a command (@p cmd) and read response
 *
 * @param maxretries number of time it should retry. Since it recursively
 * calls itself if it can't read the answer (this happens especially after
 * timeouts), we need to limit the recursiveness ;-)
 *
 * return true if any response received, false on error
 */
bool Ftp::ftpSendCmd( const QByteArray& cmd, int maxretries )
{
  Q_ASSERT(m_control != NULL);    // must have control connection socket

  if ( cmd.indexOf( '\r' ) != -1 || cmd.indexOf( '\n' ) != -1)
  {
    qWarning() << "Invalid command received (contains CR or LF):"
                    << cmd.data();
    error( ERR_UNSUPPORTED_ACTION, m_host );
    return false;
  }

  // Don't print out the password...
  bool isPassCmd = (cmd.left(4).toLower() == "pass");
#if 0
  if ( !isPassCmd )
    qDebug() << "send> " << cmd.data();
  else
    qDebug() << "send> pass [protected]";
#endif

  // Send the message...
  QByteArray buf = cmd;
  buf += "\r\n";      // Yes, must use CR/LF - see http://cr.yp.to/ftp/request.html
  int num = m_control->write(buf);
  while (m_control->bytesToWrite() && m_control->waitForBytesWritten()) {}

  // If we were able to successfully send the command, then we will
  // attempt to read the response. Otherwise, take action to re-attempt
  // the login based on the maximum number of retries specified...
  if( num > 0 )
    ftpResponse(-1);
  else
  {
    m_iRespType = m_iRespCode = 0;
  }

  // If respCh is NULL or the response is 421 (Timed-out), we try to re-send
  // the command based on the value of maxretries.
  if( (m_iRespType <= 0) || (m_iRespCode == 421) )
  {
    // We have not yet logged on...
    if (!m_bLoggedOn)
    {
      // The command was sent from the ftpLogin function, i.e. we are actually
      // attempting to login in. NOTE: If we already sent the username, we
      // return false and let the user decide whether (s)he wants to start from
      // the beginning...
      if (maxretries > 0 && !isPassCmd)
      {
        closeConnection ();
        if( ftpOpenConnection(loginDefered) )
          ftpSendCmd ( cmd, maxretries - 1 );
      }

      return false;
    }
    else
    {
      if ( maxretries < 1 )
        return false;
      else
      {
        // qDebug() << "Was not able to communicate with " << m_host
        //              << "Attempting to re-establish connection.";

        closeConnection(); // Close the old connection...
        openConnection();  // Attempt to re-establish a new connection...

        if (!m_bLoggedOn)
        {
          if (m_control != NULL)  // if openConnection succeeded ...
          {
            // qDebug() << "Login failure, aborting";
            error (ERR_COULD_NOT_LOGIN, m_host);
            closeConnection ();
          }
          return false;
        }

        // qDebug() << "Logged back in, re-issuing command";

        // If we were able to login, resend the command...
        if (maxretries)
          maxretries--;

        return ftpSendCmd( cmd, maxretries );
      }
    }
  }

  return true;
}

/*
 * ftpOpenPASVDataConnection - set up data connection, using PASV mode
 *
 * return 0 if successful, ERR_INTERNAL otherwise
 * doesn't set error message, since non-pasv mode will always be tried if
 * this one fails
 */
int Ftp::ftpOpenPASVDataConnection()
{
  Q_ASSERT(m_control != NULL);    // must have control connection socket
  Q_ASSERT(m_data == NULL);       // ... but no data connection

  // Check that we can do PASV
  QHostAddress address = m_control->peerAddress();
  if (address.protocol() != QAbstractSocket::IPv4Protocol)
    return ERR_INTERNAL;       // no PASV for non-PF_INET connections

 if (m_extControl & pasvUnknown)
    return ERR_INTERNAL;       // already tried and got "unknown command"

  m_bPasv = true;

  /* Let's PASsiVe*/
  if( !ftpSendCmd("PASV") || (m_iRespType != 2) )
  {
    // qDebug() << "PASV attempt failed";
    // unknown command?
    if( m_iRespType == 5 )
    {
        // qDebug() << "disabling use of PASV";
        m_extControl |= pasvUnknown;
    }
    return ERR_INTERNAL;
  }

  // The usual answer is '227 Entering Passive Mode. (160,39,200,55,6,245)'
  // but anonftpd gives '227 =160,39,200,55,6,245'
  int i[6];
  const char *start = strchr(ftpResponse(3), '(');
  if ( !start )
    start = strchr(ftpResponse(3), '=');
  if ( !start ||
       ( sscanf(start, "(%d,%d,%d,%d,%d,%d)",&i[0], &i[1], &i[2], &i[3], &i[4], &i[5]) != 6 &&
         sscanf(start, "=%d,%d,%d,%d,%d,%d", &i[0], &i[1], &i[2], &i[3], &i[4], &i[5]) != 6 ) )
  {
    qCritical() << "parsing IP and port numbers failed. String parsed: " << start;
    return ERR_INTERNAL;
  }

  // we ignore the host part on purpose for two reasons
  // a) it might be wrong anyway
  // b) it would make us being suceptible to a port scanning attack

  // now connect the data socket ...
  quint16 port = i[4] << 8 | i[5];
  const QString host = (isSocksProxy() ? m_host : address.toString());
  m_data = synchronousConnectToHost(host, port);

  return m_data->state() == QAbstractSocket::ConnectedState ? 0 : ERR_INTERNAL;
}

/*
 * ftpOpenEPSVDataConnection - opens a data connection via EPSV
 */
int Ftp::ftpOpenEPSVDataConnection()
{
  Q_ASSERT(m_control != NULL);    // must have control connection socket
  Q_ASSERT(m_data == NULL);       // ... but no data connection

  QHostAddress address = m_control->peerAddress();
  int portnum;

  if (m_extControl & epsvUnknown)
    return ERR_INTERNAL;

  m_bPasv = true;
  if( !ftpSendCmd("EPSV") || (m_iRespType != 2) )
  {
    // unknown command?
    if( m_iRespType == 5 )
    {
       // qDebug() << "disabling use of EPSV";
       m_extControl |= epsvUnknown;
    }
    return ERR_INTERNAL;
  }

  const char *start = strchr(ftpResponse(3), '|');
  if ( !start || sscanf(start, "|||%d|", &portnum) != 1)
    return ERR_INTERNAL;

  const QString host = (isSocksProxy() ? m_host : address.toString());
  m_data = synchronousConnectToHost(host, portnum);
  return m_data->isOpen() ? 0 : ERR_INTERNAL;
}

/*
 * ftpOpenDataConnection - set up data connection
 *
 * The routine calls several ftpOpenXxxxConnection() helpers to find
 * the best connection mode. If a helper cannot connect if returns
 * ERR_INTERNAL - so this is not really an error! All other error
 * codes are treated as fatal, e.g. they are passed back to the caller
 * who is responsible for calling error(). ftpOpenPortDataConnection
 * can be called as last try and it does never return ERR_INTERNAL.
 *
 * @return 0 if successful, err code otherwise
 */
int Ftp::ftpOpenDataConnection()
{
  // make sure that we are logged on and have no data connection...
  Q_ASSERT( m_bLoggedOn );
  ftpCloseDataConnection();

  int  iErrCode = 0;
  int  iErrCodePASV = 0;  // Remember error code from PASV

  // First try passive (EPSV & PASV) modes
  if( !config()->readEntry("DisablePassiveMode", false) )
  {
    iErrCode = ftpOpenPASVDataConnection();
    if(iErrCode == 0)
      return 0; // success
    iErrCodePASV = iErrCode;
    ftpCloseDataConnection();

    if( !config()->readEntry("DisableEPSV", false) )
    {
      iErrCode = ftpOpenEPSVDataConnection();
      if(iErrCode == 0)
        return 0; // success
      ftpCloseDataConnection();
    }

    // if we sent EPSV ALL already and it was accepted, then we can't
    // use active connections any more
    if (m_extControl & epsvAllSent)
      return iErrCodePASV;
  }

  // fall back to port mode
  iErrCode = ftpOpenPortDataConnection();
  if(iErrCode == 0)
    return 0; // success

  ftpCloseDataConnection();
  // prefer to return the error code from PASV if any, since that's what should have worked in the first place
  return iErrCodePASV ? iErrCodePASV : iErrCode;
}

/*
 * ftpOpenPortDataConnection - set up data connection
 *
 * @return 0 if successful, err code otherwise (but never ERR_INTERNAL
 *         because this is the last connection mode that is tried)
 */
int Ftp::ftpOpenPortDataConnection()
{
  Q_ASSERT(m_control != NULL);    // must have control connection socket
  Q_ASSERT(m_data == NULL);       // ... but no data connection

  m_bPasv = false;
  if (m_extControl & eprtUnknown)
    return ERR_INTERNAL;

  if (!m_server) {
    m_server = new QTcpServer;
    m_server->listen(QHostAddress::Any, 0);
  }

  if (!m_server->isListening()) {
    delete m_server;
    m_server = NULL;
    return ERR_COULD_NOT_LISTEN;
  }

  m_server->setMaxPendingConnections(1);

  QString command;
  QHostAddress localAddress = m_control->localAddress();
  if (localAddress.protocol() == QAbstractSocket::IPv4Protocol)
  {
    struct
    {
      quint32 ip4;
      quint16 port;
    } data;
    data.ip4 = localAddress.toIPv4Address();
    data.port = m_server->serverPort();

    unsigned char *pData = reinterpret_cast<unsigned char*>(&data);
    command.sprintf("PORT %d,%d,%d,%d,%d,%d",pData[3],pData[2],pData[1],pData[0],pData[5],pData[4]);
  }
  else if (localAddress.protocol() == QAbstractSocket::IPv6Protocol)
  {
    command = QString("EPRT |2|%2|%3|").arg(localAddress.toString()).arg(m_server->serverPort());
  }

  if( ftpSendCmd(command.toLatin1()) && (m_iRespType == 2) )
  {
    return 0;
  }

  delete m_server;
  m_server = NULL;
  return ERR_INTERNAL;
}

bool Ftp::ftpOpenCommand( const char *_command, const QString & _path, char _mode,
                          int errorcode, KIO::fileoffset_t _offset )
{
  int errCode = 0;
  if( !ftpDataMode(ftpModeFromPath(_path, _mode)) )
    errCode = ERR_COULD_NOT_CONNECT;
  else
    errCode = ftpOpenDataConnection();

  if(errCode != 0)
  {
    error(errCode, m_host);
    return false;
  }

  if ( _offset > 0 ) {
    // send rest command if offset > 0, this applies to retr and stor commands
    char buf[100];
    sprintf(buf, "rest %lld", _offset);
    if ( !ftpSendCmd( buf ) )
       return false;
    if( m_iRespType != 3 )
    {
      error( ERR_CANNOT_RESUME, _path ); // should never happen
      return false;
    }
  }

  QByteArray tmp = _command;
  QString errormessage;

  if ( !_path.isEmpty() ) {
    tmp += ' ';
    tmp += remoteEncoding()->encode(ftpCleanPath(_path));
  }

  if( !ftpSendCmd( tmp ) || (m_iRespType != 1) )
  {
    if( _offset > 0 && qstrcmp(_command, "retr") == 0 && (m_iRespType == 4) )
      errorcode = ERR_CANNOT_RESUME;
    // The error here depends on the command
    errormessage = _path;
  }

  else
  {
    // Only now we know for sure that we can resume
    if ( _offset > 0 && qstrcmp(_command, "retr") == 0 )
      canResume();

    if(m_server && !m_data) {
      // qDebug() << "waiting for connection from remote.";
      m_server->waitForNewConnection(connectTimeout() * 1000);
      m_data = m_server->nextPendingConnection();
    }

    if(m_data) {
      // qDebug() << "connected with remote.";
      m_bBusy = true;              // cleared in ftpCloseCommand
      return true;
    }

    // qDebug() << "no connection received from remote.";
    errorcode=ERR_COULD_NOT_ACCEPT;
    errormessage=m_host;
    return false;
  }

  error(errorcode, errormessage);
  return false;
}


bool Ftp::ftpCloseCommand()
{
  // first close data sockets (if opened), then read response that
  // we got for whatever was used in ftpOpenCommand ( should be 226 )
  ftpCloseDataConnection();

  if(!m_bBusy)
    return true;

  // qDebug() << "ftpCloseCommand: reading command result";
  m_bBusy = false;

  if(!ftpResponse(-1) || (m_iRespType != 2) )
  {
    // qDebug() << "ftpCloseCommand: no transfer complete message";
    return false;
  }
  return true;
}

void Ftp::mkdir( const QUrl & url, int permissions )
{
  if( !ftpOpenConnection(loginImplicit) )
        return;

  const QByteArray encodedPath (remoteEncoding()->encode(url));
  const QString path = QString::fromLatin1(encodedPath.constData(), encodedPath.size());

  if( !ftpSendCmd( (QByteArray ("mkd ") + encodedPath) ) || (m_iRespType != 2) )
  {
    QString currentPath( m_currentPath );

    // Check whether or not mkdir failed because
    // the directory already exists...
    if( ftpFolder( path, false ) )
    {
      error( ERR_DIR_ALREADY_EXIST, path );
      // Change the directory back to what it was...
      (void) ftpFolder( currentPath, false );
      return;
    }

    error( ERR_COULD_NOT_MKDIR, path );
    return;
  }

  if ( permissions != -1 )
  {
    // chmod the dir we just created, ignoring errors.
    (void) ftpChmod( path, permissions );
  }

  finished();
}

void Ftp::rename( const QUrl& src, const QUrl& dst, KIO::JobFlags flags )
{
  if( !ftpOpenConnection(loginImplicit) )
        return;

  // The actual functionality is in ftpRename because put needs it
  if ( ftpRename( src.path(), dst.path(), flags ) )
    finished();
}

bool Ftp::ftpRename(const QString & src, const QString & dst, KIO::JobFlags jobFlags)
{
    Q_ASSERT(m_bLoggedOn);

    // Must check if dst already exists, RNFR+RNTO overwrites by default (#127793).
    if (!(jobFlags & KIO::Overwrite)) {
        if (ftpFileExists(dst)) {
            error(ERR_FILE_ALREADY_EXIST, dst);
            return false;
        }
    }

    if (ftpFolder(dst, false)) {
        error(ERR_DIR_ALREADY_EXIST, dst);
        return false;
    }

    // CD into parent folder
    const int pos = src.lastIndexOf('/');
    if (pos >= 0) {
        if(!ftpFolder(src.left(pos+1), false))
            return false;
    }

    QByteArray from_cmd = "RNFR ";
    from_cmd += remoteEncoding()->encode(src.mid(pos+1));
    if (!ftpSendCmd(from_cmd) || (m_iRespType != 3)) {
        error( ERR_CANNOT_RENAME, src );
        return false;
    }

    QByteArray to_cmd = "RNTO ";
    to_cmd += remoteEncoding()->encode(dst);
    if (!ftpSendCmd(to_cmd) || (m_iRespType != 2)) {
        error( ERR_CANNOT_RENAME, src );
        return false;
    }

    return true;
}

void Ftp::del( const QUrl& url, bool isfile )
{
  if( !ftpOpenConnection(loginImplicit) )
        return;

  // When deleting a directory, we must exit from it first
  // The last command probably went into it (to stat it)
  if ( !isfile )
    ftpFolder(remoteEncoding()->directory(url), false); // ignore errors

  QByteArray cmd = isfile ? "DELE " : "RMD ";
  cmd += remoteEncoding()->encode(url);

  if( !ftpSendCmd( cmd ) || (m_iRespType != 2) )
    error( ERR_CANNOT_DELETE, url.path() );
  else
    finished();
}

bool Ftp::ftpChmod( const QString & path, int permissions )
{
  Q_ASSERT( m_bLoggedOn );

  if(m_extControl & chmodUnknown)      // previous errors?
    return false;

  // we need to do bit AND 777 to get permissions, in case
  // we were sent a full mode (unlikely)
  QString cmd = QString::fromLatin1("SITE CHMOD ") + QString::number( permissions & 511, 8 /*octal*/ ) + ' ';
  cmd += path;

  ftpSendCmd(remoteEncoding()->encode(cmd));
  if(m_iRespType == 2)
     return true;

  if(m_iRespCode == 500)
  {
    m_extControl |= chmodUnknown;
    // qDebug() << "ftpChmod: CHMOD not supported - disabling";
  }
  return false;
}

void Ftp::chmod( const QUrl & url, int permissions )
{
  if( !ftpOpenConnection(loginImplicit) )
        return;

  if ( !ftpChmod( url.path(), permissions ) )
    error( ERR_CANNOT_CHMOD, url.path() );
  else
    finished();
}

void Ftp::ftpCreateUDSEntry( const QString & filename, const FtpEntry& ftpEnt, UDSEntry& entry, bool isDir )
{
  Q_ASSERT(entry.count() == 0); // by contract :-)

  entry.insert( KIO::UDSEntry::UDS_NAME, filename );
  entry.insert( KIO::UDSEntry::UDS_SIZE, ftpEnt.size );
  entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, ftpEnt.date.toTime_t());
  entry.insert( KIO::UDSEntry::UDS_ACCESS, ftpEnt.access );
  entry.insert( KIO::UDSEntry::UDS_USER, ftpEnt.owner );
  if ( !ftpEnt.group.isEmpty() )
  {
    entry.insert( KIO::UDSEntry::UDS_GROUP, ftpEnt.group );
  }

  if ( !ftpEnt.link.isEmpty() )
  {
    entry.insert( KIO::UDSEntry::UDS_LINK_DEST, ftpEnt.link );

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForUrl(QUrl("ftp://host/" + filename));
    // Links on ftp sites are often links to dirs, and we have no way to check
    // that. Let's do like Netscape : assume dirs generally.
    // But we do this only when the mimetype can't be known from the filename.
    // --> we do better than Netscape :-)
    if (mime.isDefault()) {
      // qDebug() << "Setting guessed mime type to inode/directory for " << filename;
      entry.insert( KIO::UDSEntry::UDS_GUESSED_MIME_TYPE, QString::fromLatin1( "inode/directory" ) );
      isDir = true;
    }
  }

  entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : ftpEnt.type );
  // entry.insert KIO::UDSEntry::UDS_ACCESS_TIME,buff.st_atime);
  // entry.insert KIO::UDSEntry::UDS_CREATION_TIME,buff.st_ctime);
}


void Ftp::ftpShortStatAnswer( const QString& filename, bool isDir )
{
    UDSEntry entry;


    entry.insert( KIO::UDSEntry::UDS_NAME, filename );
    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : S_IFREG );
    entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
    if (isDir) {
      entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, QLatin1String("inode/directory"));
    }
    // No details about size, ownership, group, etc.

    statEntry(entry);
    finished();
}

void Ftp::ftpStatAnswerNotFound( const QString & path, const QString & filename )
{
    // Only do the 'hack' below if we want to download an existing file (i.e. when looking at the "source")
    // When e.g. uploading a file, we still need stat() to return "not found"
    // when the file doesn't exist.
    QString statSide = metaData("statSide");
    // qDebug() << "statSide=" << statSide;
    if ( statSide == "source" )
    {
        // qDebug() << "Not found, but assuming found, because some servers don't allow listing";
        // MS Server is incapable of handling "list <blah>" in a case insensitive way
        // But "retr <blah>" works. So lie in stat(), to get going...
        //
        // There's also the case of ftp://ftp2.3ddownloads.com/90380/linuxgames/loki/patches/ut/ut-patch-436.run
        // where listing permissions are denied, but downloading is still possible.
        ftpShortStatAnswer( filename, false /*file, not dir*/ );

        return;
    }

    error( ERR_DOES_NOT_EXIST, path );
}

void Ftp::stat(const QUrl &url)
{
  // qDebug() << "path=" << url.path();
  if( !ftpOpenConnection(loginImplicit) )
        return;

  const QString path = ftpCleanPath( QDir::cleanPath( url.path() ) );
  // qDebug() << "cleaned path=" << path;

  // We can't stat root, but we know it's a dir.
  if( path.isEmpty() || path == "/" )
  {
    UDSEntry entry;
    //entry.insert( KIO::UDSEntry::UDS_NAME, UDSField( QString() ) );
    entry.insert( KIO::UDSEntry::UDS_NAME, QString::fromLatin1( "." ) );
    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR );
    entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, QLatin1String("inode/directory"));
    entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
    entry.insert( KIO::UDSEntry::UDS_USER, QString::fromLatin1( "root" ) );
    entry.insert( KIO::UDSEntry::UDS_GROUP, QString::fromLatin1( "root" ) );
    // no size

    statEntry( entry );
    finished();
    return;
  }

  QUrl tempurl( url );
  tempurl.setPath( path ); // take the clean one
  QString listarg; // = tempurl.directory(QUrl::ObeyTrailingSlash);
  QString parentDir;
  QString filename = tempurl.fileName();
  Q_ASSERT(!filename.isEmpty());
  QString search = filename;

  // Try cwd into it, if it works it's a dir (and then we'll list the parent directory to get more info)
  // if it doesn't work, it's a file (and then we'll use dir filename)
  bool isDir = ftpFolder(path, false);

  // if we're only interested in "file or directory", we should stop here
  QString sDetails = metaData("details");
  int details = sDetails.isEmpty() ? 2 : sDetails.toInt();
  // qDebug() << "details=" << details;
  if ( details == 0 )
  {
     if ( !isDir && !ftpFileExists(path) ) // ok, not a dir -> is it a file ?
     {  // no -> it doesn't exist at all
        ftpStatAnswerNotFound( path, filename );
        return;
     }
     ftpShortStatAnswer( filename, isDir ); // successfully found a dir or a file -> done
     return;
  }

  if (!isDir)
  {
    // It is a file or it doesn't exist, try going to parent directory
    parentDir = tempurl.adjusted(QUrl::RemoveFilename).path();
    // With files we can do "LIST <filename>" to avoid listing the whole dir
    listarg = filename;
  }
  else
  {
    // --- New implementation:
    // Don't list the parent dir. Too slow, might not show it, etc.
    // Just return that it's a dir.
    UDSEntry entry;
    entry.insert( KIO::UDSEntry::UDS_NAME, filename );
    entry.insert( KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR );
    entry.insert( KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );
    // No clue about size, ownership, group, etc.

    statEntry(entry);
    finished();
    return;
  }

  // Now cwd the parent dir, to prepare for listing
  if( !ftpFolder(parentDir, true) )
    return;

  if( !ftpOpenCommand( "list", listarg, 'I', ERR_DOES_NOT_EXIST ) )
  {
    qCritical() << "COULD NOT LIST";
    return;
  }
  // qDebug() << "Starting of list was ok";

  Q_ASSERT( !search.isEmpty() && search != "/" );

  bool bFound = false;
  QUrl linkURL;
  FtpEntry ftpEnt;
  QList<FtpEntry> ftpValidateEntList;
  while (ftpReadDir(ftpEnt)) {
    if (!ftpEnt.name.isEmpty() && ftpEnt.name.at(0).isSpace()) {
      ftpValidateEntList.append(ftpEnt);
      continue;
    }

    // We look for search or filename, since some servers (e.g. ftp.tuwien.ac.at)
    // return only the filename when doing "dir /full/path/to/file"
    if (!bFound) {
      bFound = maybeEmitStatEntry(ftpEnt, search, filename, isDir);
    }
    // qDebug() << ftpEnt.name;
  }

  for (int i = 0, count = ftpValidateEntList.count(); i < count; ++i) {
    FtpEntry& ftpEnt = ftpValidateEntList[i];
    fixupEntryName(&ftpEnt);
    if (maybeEmitStatEntry(ftpEnt, search, filename, isDir)) {
      break;
    }
  }

  ftpCloseCommand();        // closes the data connection only

  if ( !bFound )
  {
    ftpStatAnswerNotFound( path, filename );
    return;
  }

  if ( !linkURL.isEmpty() )
  {
      if ( linkURL == url || linkURL == tempurl )
      {
          error( ERR_CYCLIC_LINK, linkURL.toString() );
          return;
      }
      Ftp::stat( linkURL );
      return;
  }

  // qDebug() << "stat : finished successfully";
  finished();
}

bool Ftp::maybeEmitStatEntry(FtpEntry& ftpEnt, const QString& search, const QString& filename, bool isDir)
{
    if ((search == ftpEnt.name || filename == ftpEnt.name) && !filename.isEmpty()) {
        UDSEntry entry;
        ftpCreateUDSEntry( filename, ftpEnt, entry, isDir );
        statEntry( entry );
        return true;
    }

    return false;
}

void Ftp::listDir( const QUrl &url )
{
  // qDebug() << url;
  if( !ftpOpenConnection(loginImplicit) )
        return;

  // No path specified ?
  QString path = url.path();
  if ( path.isEmpty() )
  {
    QUrl realURL;
    realURL.setScheme( "ftp" );
    realURL.setUserName( m_user );
    realURL.setPassword( m_pass );
    realURL.setHost( m_host );
    if ( m_port > 0 && m_port != DEFAULT_FTP_PORT )
        realURL.setPort( m_port );
    if ( m_initialPath.isEmpty() )
        m_initialPath = '/';
    realURL.setPath( m_initialPath );
    // qDebug() << "REDIRECTION to " << realURL;
    redirection( realURL );
    finished();
    return;
  }

    // qDebug() << "hunting for path" << path;

    if (!ftpOpenDir(path)) {
        if (ftpFileExists(path)) {
            error(ERR_IS_FILE, path);
        } else {
            // not sure which to emit
            //error( ERR_DOES_NOT_EXIST, path );
            error( ERR_CANNOT_ENTER_DIRECTORY, path );
        }
        return;
    }

  UDSEntry entry;
  FtpEntry  ftpEnt;
  QList<FtpEntry> ftpValidateEntList;
  while( ftpReadDir(ftpEnt) )
  {
    //qDebug() << ftpEnt.name;
    //Q_ASSERT( !ftpEnt.name.isEmpty() );
    if (!ftpEnt.name.isEmpty()) {
      if (ftpEnt.name.at(0).isSpace()) {
        ftpValidateEntList.append(ftpEnt);
        continue;
      }

      //if ( S_ISDIR( (mode_t)ftpEnt.type ) )
      //   qDebug() << "is a dir";
      //if ( !ftpEnt.link.isEmpty() )
      //   qDebug() << "is a link to " << ftpEnt.link;
      ftpCreateUDSEntry( ftpEnt.name, ftpEnt, entry, false );
      listEntry(entry);
      entry.clear();
    }
  }

  for (int i = 0, count = ftpValidateEntList.count(); i < count; ++i) {
    FtpEntry& ftpEnt = ftpValidateEntList[i];
    fixupEntryName(&ftpEnt);
    ftpCreateUDSEntry( ftpEnt.name, ftpEnt, entry, false );
    listEntry(entry);
    entry.clear();
  }

  ftpCloseCommand();        // closes the data connection only
  finished();
}

void Ftp::slave_status()
{
  // qDebug() << "Got slave_status host = " << (!m_host.toLatin1().isEmpty() ? m_host.toLatin1() : "[None]") << " [" << (m_bLoggedOn ? "Connected" : "Not connected") << "]";
  slaveStatus( m_host, m_bLoggedOn );
}

bool Ftp::ftpOpenDir( const QString & path )
{
  //QString path( _url.path(QUrl::RemoveTrailingSlash) );

  // We try to change to this directory first to see whether it really is a directory.
  // (And also to follow symlinks)
  QString tmp = path.isEmpty() ? QString("/") : path;

  // We get '550', whether it's a file or doesn't exist...
  if( !ftpFolder(tmp, false) )
      return false;

  // Don't use the path in the list command:
  // We changed into this directory anyway - so it's enough just to send "list".
  // We use '-a' because the application MAY be interested in dot files.
  // The only way to really know would be to have a metadata flag for this...
  // Since some windows ftp server seems not to support the -a argument, we use a fallback here.
  // In fact we have to use -la otherwise -a removes the default -l (e.g. ftp.trolltech.com)
  if( !ftpOpenCommand( "list -la", QString(), 'I', ERR_CANNOT_ENTER_DIRECTORY ) )
  {
    if ( !ftpOpenCommand( "list", QString(), 'I', ERR_CANNOT_ENTER_DIRECTORY ) )
    {
      qWarning() << "Can't open for listing";
      return false;
    }
  }
  // qDebug() << "Starting of list was ok";
  return true;
}

bool Ftp::ftpReadDir(FtpEntry& de)
{
  Q_ASSERT(m_data != NULL);

  // get a line from the data connecetion ...
  while( true )
  {
    while (!m_data->canReadLine() && m_data->waitForReadyRead((readTimeout() * 1000))) {}
    QByteArray data = m_data->readLine();
    if (data.size() == 0)
      break;

    const char* buffer = data.data();
    // qDebug() << "dir > " << buffer;

    //Normally the listing looks like
    // -rw-r--r--   1 dfaure   dfaure        102 Nov  9 12:30 log
    // but on Netware servers like ftp://ci-1.ci.pwr.wroc.pl/ it looks like (#76442)
    // d [RWCEAFMS] Admin                     512 Oct 13  2004 PSI

    // we should always get the following 5 fields ...
    const char *p_access, *p_junk, *p_owner, *p_group, *p_size;
    if( (p_access = strtok((char*)buffer," ")) == 0) continue;
    if( (p_junk  = strtok(NULL," ")) == 0) continue;
    if( (p_owner = strtok(NULL," ")) == 0) continue;
    if( (p_group = strtok(NULL," ")) == 0) continue;
    if( (p_size  = strtok(NULL," ")) == 0) continue;

    //qDebug() << "p_access=" << p_access << " p_junk=" << p_junk << " p_owner=" << p_owner << " p_group=" << p_group << " p_size=" << p_size;

    de.access = 0;
    if ( qstrlen( p_access ) == 1 && p_junk[0] == '[' ) { // Netware
      de.access = S_IRWXU | S_IRWXG | S_IRWXO; // unknown -> give all permissions
    }

    const char *p_date_1, *p_date_2, *p_date_3, *p_name;

    // A special hack for "/dev". A listing may look like this:
    // crw-rw-rw-   1 root     root       1,   5 Jun 29  1997 zero
    // So we just ignore the number in front of the ",". Ok, it is a hack :-)
    if ( strchr( p_size, ',' ) != 0L )
    {
      //qDebug() << "Size contains a ',' -> reading size again (/dev hack)";
      if ((p_size = strtok(NULL," ")) == 0)
        continue;
    }

    // Check whether the size we just read was really the size
    // or a month (this happens when the server lists no group)
    // Used to be the case on sunsite.uio.no, but not anymore
    // This is needed for the Netware case, too.
    if ( !isdigit( *p_size ) )
    {
      p_date_1 = p_size;
      p_size = p_group;
      p_group = 0;
      //qDebug() << "Size didn't have a digit -> size=" << p_size << " date_1=" << p_date_1;
    }
    else
    {
      p_date_1 = strtok(NULL," ");
      //qDebug() << "Size has a digit -> ok. p_date_1=" << p_date_1;
    }

    if ( p_date_1 != 0 &&
         (p_date_2 = strtok(NULL," ")) != 0 &&
         (p_date_3 = strtok(NULL," ")) != 0 &&
         (p_name = strtok(NULL,"\r\n")) != 0 )
    {
      {
        QByteArray tmp( p_name );
        if ( p_access[0] == 'l' )
        {
          int i = tmp.lastIndexOf( " -> " );
          if ( i != -1 ) {
            de.link = remoteEncoding()->decode(p_name + i + 4);
            tmp.truncate( i );
          }
          else
            de.link.clear();
        }
        else
          de.link.clear();

        if ( tmp[0] == '/' ) // listing on ftp://ftp.gnupg.org/ starts with '/'
          tmp.remove( 0, 1 );

        if (tmp.indexOf('/') != -1)
          continue; // Don't trick us!

        de.name     = remoteEncoding()->decode(tmp);
      }

      de.type = S_IFREG;
      switch ( p_access[0] ) {
      case 'd':
        de.type = S_IFDIR;
        break;
      case 's':
        de.type = S_IFSOCK;
        break;
      case 'b':
        de.type = S_IFBLK;
        break;
      case 'c':
        de.type = S_IFCHR;
        break;
      case 'l':
        de.type = S_IFREG;
        // we don't set S_IFLNK here.  de.link says it.
        break;
      default:
        break;
      }

      if ( p_access[1] == 'r' )
        de.access |= S_IRUSR;
      if ( p_access[2] == 'w' )
        de.access |= S_IWUSR;
      if ( p_access[3] == 'x' || p_access[3] == 's' )
        de.access |= S_IXUSR;
      if ( p_access[4] == 'r' )
        de.access |= S_IRGRP;
      if ( p_access[5] == 'w' )
        de.access |= S_IWGRP;
      if ( p_access[6] == 'x' || p_access[6] == 's' )
        de.access |= S_IXGRP;
      if ( p_access[7] == 'r' )
        de.access |= S_IROTH;
      if ( p_access[8] == 'w' )
        de.access |= S_IWOTH;
      if ( p_access[9] == 'x' || p_access[9] == 't' )
        de.access |= S_IXOTH;
      if ( p_access[3] == 's' || p_access[3] == 'S' )
        de.access |= S_ISUID;
      if ( p_access[6] == 's' || p_access[6] == 'S' )
        de.access |= S_ISGID;
      if ( p_access[9] == 't' || p_access[9] == 'T' )
        de.access |= S_ISVTX;

      de.owner    = remoteEncoding()->decode(p_owner);
      de.group    = remoteEncoding()->decode(p_group);
      de.size     = charToLongLong(p_size);

      // Parsing the date is somewhat tricky
      // Examples : "Oct  6 22:49", "May 13  1999"

      // First get current time - we need the current month and year
      QDateTime currentTime(QDateTime::currentDateTime());
      int currentMonth = currentTime.date().month();
      //qDebug() << "Current time :" << asctime( tmptr );
      // Reset time fields
      currentTime.setTime(QTime(0, 0, 0));
      // Get day number (always second field)
      int day = 0;
      int month = 0;
      int year = 0;
      int minute = 0;
      int hour = 0;
      if (p_date_2) {
        day = atoi(p_date_2);
      }
      // Get month from first field
      // NOTE : no, we don't want to use KLocale here
      // It seems all FTP servers use the English way
      //qDebug() << "Looking for month " << p_date_1;
      static const char * const s_months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
      for ( int c = 0 ; c < 12 ; c ++ )
        if (!qstrcmp(p_date_1, s_months[c])) {
          //qDebug() << "Found month " << c << " for " << p_date_1;
          month = c + 1;
          break;
        }

      // Parse third field
      if (qstrlen(p_date_3) == 4) { // 4 digits, looks like a year
        year = atoi(p_date_3) - 1900;
      } else {
        // otherwise, the year is implicit
        // according to man ls, this happens when it is between than 6 months
        // old and 1 hour in the future.
        // So the year is : current year if tm_mon <= currentMonth+1
        // otherwise current year minus one
        // (The +1 is a security for the "+1 hour" at the end of the month issue)
        if (month > currentMonth + 1) {
          year--;
        }

        // and p_date_3 contains probably a time
        char * semicolon;
        if (p_date_3 && (semicolon = (char*)strchr(p_date_3, ':'))) {
          *semicolon = '\0';
          minute = atoi(semicolon + 1);
          hour = atoi(p_date_3);
        } else {
          qWarning() << "Can't parse third field " << p_date_3;
        }
      }

      //qDebug() << asctime( tmptr );
      de.date = QDateTime(QDate(year, month, day), QTime(hour, minute));
      return true;
    }
  } // line invalid, loop to get another line
  return false;
}

//===============================================================================
// public: get           download file from server
// helper: ftpGet        called from get() and copy()
//===============================================================================
void Ftp::get( const QUrl & url )
{
  // qDebug() << url;

  int iError = 0;
  const StatusCode cs = ftpGet(iError, -1, url, 0);
  ftpCloseCommand();                        // must close command!

  if (cs == statusSuccess) {
     finished();
     return;
  }

  if (iError) {                            // can have only server side errs
     error(iError, url.path());
  }
}

Ftp::StatusCode Ftp::ftpGet(int& iError, int iCopyFile, const QUrl& url, KIO::fileoffset_t llOffset)
{
  // Calls error() by itself!
  if( !ftpOpenConnection(loginImplicit) )
    return statusServerError;

  // Try to find the size of the file (and check that it exists at
  // the same time). If we get back a 550, "File does not exist"
  // or "not a plain file", check if it is a directory. If it is a
  // directory, return an error; otherwise simply try to retrieve
  // the request...
  if ( !ftpSize( url.path(), '?' ) && (m_iRespCode == 550) &&
       ftpFolder(url.path(), false) )
  {
    // Ok it's a dir in fact
    // qDebug() << "it is a directory in fact";
    iError = ERR_IS_DIRECTORY;
    return statusServerError;
  }

  QString resumeOffset = metaData("resume");
  if ( !resumeOffset.isEmpty() )
  {
    llOffset = resumeOffset.toLongLong();
    // qDebug() << "got offset from metadata : " << llOffset;
  }

  if( !ftpOpenCommand("retr", url.path(), '?', ERR_CANNOT_OPEN_FOR_READING, llOffset) )
  {
    qWarning() << "Can't open for reading";
    return statusServerError;
  }

  // Read the size from the response string
  if(m_size == UnknownSize)
  {
    const char* psz = strrchr( ftpResponse(4), '(' );
    if(psz) m_size = charToLongLong(psz+1);
    if (!m_size) m_size = UnknownSize;
  }

  // Send the mime-type...
  if (iCopyFile == -1) {
    StatusCode status = ftpSendMimeType(iError, url);
    if (status != statusSuccess) {
        return status;
    }
  }

  KIO::filesize_t bytesLeft = 0;
  if ( m_size != UnknownSize ) {
    bytesLeft = m_size - llOffset;
    totalSize( m_size );  // emit the total size...
  }

  // qDebug() << "starting with offset=" << llOffset;
  KIO::fileoffset_t processed_size = llOffset;

  QByteArray array;
  char buffer[maximumIpcSize];
  // start with small data chunks in case of a slow data source (modem)
  // - unfortunately this has a negative impact on performance for large
  // - files - so we will increase the block size after a while ...
  int iBlockSize = initialIpcSize;
  int iBufferCur = 0;

  while(m_size == UnknownSize || bytesLeft > 0)
  {  // let the buffer size grow if the file is larger 64kByte ...
    if(processed_size-llOffset > 1024 * 64)
      iBlockSize = maximumIpcSize;

    // read the data and detect EOF or error ...
    if(iBlockSize+iBufferCur > (int)sizeof(buffer))
      iBlockSize = sizeof(buffer) - iBufferCur;
    if (m_data->bytesAvailable() == 0)
      m_data->waitForReadyRead((readTimeout() * 1000));
    int n = m_data->read( buffer+iBufferCur, iBlockSize );
    if(n <= 0)
    {   // this is how we detect EOF in case of unknown size
      if( m_size == UnknownSize && n == 0 )
        break;
      // unexpected eof. Happens when the daemon gets killed.
      iError = ERR_COULD_NOT_READ;
      return statusServerError;
    }
    processed_size += n;

    // collect very small data chunks in buffer before processing ...
    if(m_size != UnknownSize)
    {
      bytesLeft -= n;
      iBufferCur += n;
      if(iBufferCur < minimumMimeSize && bytesLeft > 0)
      {
        processedSize( processed_size );
        continue;
      }
      n = iBufferCur;
      iBufferCur = 0;
    }

    // write output file or pass to data pump ...
    if(iCopyFile == -1)
    {
        array = QByteArray::fromRawData(buffer, n);
        data( array );
        array.clear();
    }
    else if( (iError = WriteToFile(iCopyFile, buffer, n)) != 0)
       return statusClientError;              // client side error
    processedSize( processed_size );
  }

  // qDebug() << "done";
  if(iCopyFile == -1)          // must signal EOF to data pump ...
    data(array);               // array is empty and must be empty!

  processedSize( m_size == UnknownSize ? processed_size : m_size );
  return statusSuccess;
}

#if 0
  void Ftp::mimetype( const QUrl& url )
  {
    if( !ftpOpenConnection(loginImplicit) )
          return;

    if ( !ftpOpenCommand( "retr", url.path(), 'I', ERR_CANNOT_OPEN_FOR_READING, 0 ) ) {
      qWarning() << "Can't open for reading";
      return;
    }
    char buffer[ 2048 ];
    QByteArray array;
    // Get one chunk of data only and send it, KIO::Job will determine the
    // mimetype from it using KMimeMagic
    int n = m_data->read( buffer, 2048 );
    array.setRawData(buffer, n);
    data( array );
    array.resetRawData(buffer, n);

    // qDebug() << "aborting";
    ftpAbortTransfer();

    // qDebug() << "finished";
    finished();
    // qDebug() << "after finished";
  }

  void Ftp::ftpAbortTransfer()
  {
    // RFC 959, page 34-35
    // IAC (interpret as command) = 255 ; IP (interrupt process) = 254
    // DM = 242 (data mark)
     char msg[4];
     // 1. User system inserts the Telnet "Interrupt Process" (IP) signal
     //   in the Telnet stream.
     msg[0] = (char) 255; //IAC
     msg[1] = (char) 254; //IP
     (void) send(sControl, msg, 2, 0);
     // 2. User system sends the Telnet "Sync" signal.
     msg[0] = (char) 255; //IAC
     msg[1] = (char) 242; //DM
     if (send(sControl, msg, 2, MSG_OOB) != 2)
       ; // error...

     // Send ABOR
     // qDebug() << "send ABOR";
     QCString buf = "ABOR\r\n";
     if ( KSocks::self()->write( sControl, buf.data(), buf.length() ) <= 0 )  {
       error( ERR_COULD_NOT_WRITE, QString() );
       return;
     }

     //
     // qDebug() << "read resp";
     if ( readresp() != '2' )
     {
       error( ERR_COULD_NOT_READ, QString() );
       return;
     }

    // qDebug() << "close sockets";
    closeSockets();
  }
#endif

//===============================================================================
// public: put           upload file to server
// helper: ftpPut        called from put() and copy()
//===============================================================================
void Ftp::put(const QUrl& url, int permissions, KIO::JobFlags flags)
{
  // qDebug() << url;

  int iError = 0;                           // iError gets status
  const StatusCode cs = ftpPut(iError, -1, url, permissions, flags);
  ftpCloseCommand();                        // must close command!

  if (cs == statusSuccess) {
    finished();
    return;
  }

  if (iError) {                             // can have only server side errs
    error(iError, url.path());
  }
}

Ftp::StatusCode Ftp::ftpPut(int& iError, int iCopyFile, const QUrl& dest_url,
                            int permissions, KIO::JobFlags flags)
{
  if( !ftpOpenConnection(loginImplicit) )
    return statusServerError;

  // Don't use mark partial over anonymous FTP.
  // My incoming dir allows put but not rename...
  bool bMarkPartial;
  if (m_user.isEmpty () || m_user == FTP_LOGIN)
    bMarkPartial = false;
  else
    bMarkPartial = config()->readEntry("MarkPartial", true);

  QString dest_orig = dest_url.path();
  QString dest_part( dest_orig );
  dest_part += ".part";

  if ( ftpSize( dest_orig, 'I' ) )
  {
    if ( m_size == 0 )
    { // delete files with zero size
      QByteArray cmd = "DELE ";
      cmd += remoteEncoding()->encode(dest_orig);
      if( !ftpSendCmd( cmd ) || (m_iRespType != 2) )
      {
        iError = ERR_CANNOT_DELETE_PARTIAL;
        return statusServerError;
      }
    }
    else if ( !(flags & KIO::Overwrite) && !(flags & KIO::Resume) )
    {
       iError = ERR_FILE_ALREADY_EXIST;
       return statusServerError;
    }
    else if ( bMarkPartial )
    { // when using mark partial, append .part extension
      if ( !ftpRename( dest_orig, dest_part, KIO::Overwrite ) )
      {
        iError = ERR_CANNOT_RENAME_PARTIAL;
        return statusServerError;
      }
    }
    // Don't chmod an existing file
    permissions = -1;
  }
  else if ( bMarkPartial && ftpSize( dest_part, 'I' ) )
  { // file with extension .part exists
    if ( m_size == 0 )
    {  // delete files with zero size
      QByteArray cmd = "DELE ";
      cmd += remoteEncoding()->encode(dest_part);
      if ( !ftpSendCmd( cmd ) || (m_iRespType != 2) )
      {
        iError = ERR_CANNOT_DELETE_PARTIAL;
        return statusServerError;
      }
    }
    else if ( !(flags & KIO::Overwrite) && !(flags & KIO::Resume) )
    {
      flags |= canResume (m_size) ? KIO::Resume : KIO::DefaultFlags;
      if (!(flags & KIO::Resume))
      {
        iError = ERR_FILE_ALREADY_EXIST;
        return statusServerError;
      }
    }
  }
  else
    m_size = 0;

  QString dest;

  // if we are using marking of partial downloads -> add .part extension
  if ( bMarkPartial ) {
    // qDebug() << "Adding .part extension to " << dest_orig;
    dest = dest_part;
  } else
    dest = dest_orig;

  KIO::fileoffset_t offset = 0;

  // set the mode according to offset
  if( (flags & KIO::Resume) && m_size > 0 )
  {
    offset = m_size;
    if(iCopyFile != -1)
    {
      if(QT_LSEEK(iCopyFile, offset, SEEK_SET) < 0)
      {
        iError = ERR_CANNOT_RESUME;
        return statusClientError;
      }
    }
  }

  if (! ftpOpenCommand( "stor", dest, '?', ERR_COULD_NOT_WRITE, offset ) )
     return statusServerError;

  // qDebug() << "ftpPut: starting with offset=" << offset;
  KIO::fileoffset_t processed_size = offset;

  QByteArray buffer;
  int result;
  int iBlockSize = initialIpcSize;
  // Loop until we got 'dataEnd'
  do
  {
    if(iCopyFile == -1)
    {
      dataReq(); // Request for data
      result = readData( buffer );
    }
    else
    { // let the buffer size grow if the file is larger 64kByte ...
      if(processed_size-offset > 1024 * 64)
        iBlockSize = maximumIpcSize;
      buffer.resize(iBlockSize);
      result = QT_READ(iCopyFile, buffer.data(), buffer.size());
      if(result < 0)
        iError = ERR_COULD_NOT_WRITE;
      else
        buffer.resize(result);
    }

    if (result > 0)
    {
      m_data->write( buffer );
      while (m_data->bytesToWrite() && m_data->waitForBytesWritten()) {}
      processed_size += result;
      processedSize (processed_size);
    }
  }
  while ( result > 0 );

  if (result != 0) // error
  {
    ftpCloseCommand();               // don't care about errors
    // qDebug() << "Error during 'put'. Aborting.";
    if (bMarkPartial)
    {
      // Remove if smaller than minimum size
      if ( ftpSize( dest, 'I' ) &&
           ( processed_size < config()->readEntry("MinimumKeepSize", DEFAULT_MINIMUM_KEEP_SIZE) ) )
      {
        QByteArray cmd = "DELE ";
        cmd += remoteEncoding()->encode(dest);
        (void) ftpSendCmd( cmd );
      }
    }
    return statusServerError;
  }

  if ( !ftpCloseCommand() )
  {
    iError = ERR_COULD_NOT_WRITE;
    return statusServerError;
  }

  // after full download rename the file back to original name
  if ( bMarkPartial )
  {
    // qDebug() << "renaming dest (" << dest << ") back to dest_orig (" << dest_orig << ")";
    if ( !ftpRename( dest, dest_orig, KIO::Overwrite ) )
    {
      iError = ERR_CANNOT_RENAME_PARTIAL;
      return statusServerError;
    }
  }

  // set final permissions
  if ( permissions != -1 )
  {
    if ( m_user == FTP_LOGIN )
      // qDebug() << "Trying to chmod over anonymous FTP ???";
    // chmod the file we just put
    if ( ! ftpChmod( dest_orig, permissions ) )
    {
        // To be tested
        //if ( m_user != FTP_LOGIN )
        //    warning( i18n( "Could not change permissions for\n%1" ).arg( dest_orig ) );
    }
  }

  return statusSuccess;
}

/** Use the SIZE command to get the file size.
    Warning : the size depends on the transfer mode, hence the second arg. */
bool Ftp::ftpSize( const QString & path, char mode )
{
  m_size = UnknownSize;
  if( !ftpDataMode(mode) )
      return false;

  QByteArray buf;
  buf = "SIZE ";
  buf += remoteEncoding()->encode(path);
  if( !ftpSendCmd( buf ) || (m_iRespType != 2) )
    return false;

  // skip leading "213 " (response code)
  QByteArray psz (ftpResponse(4));
  if(psz.isEmpty())
    return false;
  bool ok = false;
  m_size = psz.trimmed().toLongLong(&ok);
  if (!ok) m_size = UnknownSize;
  return true;
}

bool Ftp::ftpFileExists(const QString& path)
{
  QByteArray buf;
  buf = "SIZE ";
  buf += remoteEncoding()->encode(path);
  if( !ftpSendCmd( buf ) || (m_iRespType != 2) )
    return false;

  // skip leading "213 " (response code)
  const char* psz = ftpResponse(4);
  return psz != 0;
}

// Today the differences between ASCII and BINARY are limited to
// CR or CR/LF line terminators. Many servers ignore ASCII (like
// win2003 -or- vsftp with default config). In the early days of
// computing, when even text-files had structure, this stuff was
// more important.
// Theoretically "list" could return different results in ASCII
// and BINARY mode. But again, most servers ignore ASCII here.
bool Ftp::ftpDataMode(char cMode)
{
  if(cMode == '?') cMode = m_bTextMode ? 'A' : 'I';
  else if(cMode == 'a') cMode = 'A';
  else if(cMode != 'A') cMode = 'I';

  // qDebug() << "want" << cMode << "has" << m_cDataMode;
  if(m_cDataMode == cMode)
    return true;

  QByteArray buf = "TYPE ";
  buf += cMode;
  if( !ftpSendCmd(buf) || (m_iRespType != 2) )
      return false;
  m_cDataMode = cMode;
  return true;
}


bool Ftp::ftpFolder(const QString& path, bool bReportError)
{
  QString newPath = path;
  int iLen = newPath.length();
  if(iLen > 1 && newPath[iLen-1] == '/') newPath.truncate(iLen-1);

  //qDebug() << "want" << newPath << "has" << m_currentPath;
  if(m_currentPath == newPath)
    return true;

  QByteArray tmp = "cwd ";
  tmp += remoteEncoding()->encode(newPath);
  if( !ftpSendCmd(tmp) )
    return false;                  // connection failure
  if(m_iRespType != 2)
  {
    if(bReportError)
      error(ERR_CANNOT_ENTER_DIRECTORY, path);
    return false;                  // not a folder
  }
  m_currentPath = newPath;
  return true;
}


//===============================================================================
// public: copy          don't use kio data pump if one side is a local file
// helper: ftpCopyPut    called from copy() on upload
// helper: ftpCopyGet    called from copy() on download
//===============================================================================
void Ftp::copy( const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags )
{
  int iError = 0;
  int iCopyFile = -1;
  StatusCode cs = statusSuccess;
  bool bSrcLocal = src.isLocalFile();
  bool bDestLocal = dest.isLocalFile();
  QString  sCopyFile;

  if(bSrcLocal && !bDestLocal)                    // File -> Ftp
  {
    sCopyFile = src.toLocalFile();
    // qDebug() << "local file" << sCopyFile << "-> ftp" << dest.path();
    cs = ftpCopyPut(iError, iCopyFile, sCopyFile, dest, permissions, flags);
    if( cs == statusServerError) sCopyFile = dest.toString();
  }
  else if(!bSrcLocal && bDestLocal)               // Ftp -> File
  {
    sCopyFile = dest.toLocalFile();
    // qDebug() << "ftp" << src.path() << "-> local file" << sCopyFile;
    cs = ftpCopyGet(iError, iCopyFile, sCopyFile, src, permissions, flags);
    if( cs == statusServerError ) sCopyFile = src.toString();
  }
  else {
    error( ERR_UNSUPPORTED_ACTION, QString() );
    return;
  }

  // perform clean-ups and report error (if any)
  if(iCopyFile != -1)
      QT_CLOSE(iCopyFile);
  ftpCloseCommand();                        // must close command!
  if(iError)
    error(iError, sCopyFile);
  else
    finished();
}


Ftp::StatusCode Ftp::ftpCopyPut(int& iError, int& iCopyFile, const QString &sCopyFile,
                                const QUrl& url, int permissions, KIO::JobFlags flags)
{
  // check if source is ok ...
  QFileInfo info(sCopyFile);
  bool bSrcExists = info.exists();
  if(bSrcExists)
  { if(info.isDir())
    {
      iError = ERR_IS_DIRECTORY;
      return statusClientError;
    }
  }
  else
  {
    iError = ERR_DOES_NOT_EXIST;
    return statusClientError;
  }

  iCopyFile = QT_OPEN(QFile::encodeName(sCopyFile).constData(), O_RDONLY);
  if(iCopyFile == -1)
  {
    iError = ERR_CANNOT_OPEN_FOR_READING;
    return statusClientError;
  }

  // delegate the real work (iError gets status) ...
  totalSize(info.size());
#ifdef  ENABLE_CAN_RESUME
  return ftpPut(iError, iCopyFile, url, permissions, flags & ~KIO::Resume);
#else
  return ftpPut(iError, iCopyFile, url, permissions, flags | KIO::Resume);
#endif
}


Ftp::StatusCode Ftp::ftpCopyGet(int& iError, int& iCopyFile, const QString &sCopyFile,
                                const QUrl& url, int permissions, KIO::JobFlags flags)
{
  // check if destination is ok ...
  QFileInfo info(sCopyFile);
  const bool bDestExists = info.exists();
  if(bDestExists)
  { if(info.isDir())
    {
      iError = ERR_IS_DIRECTORY;
      return statusClientError;
    }
    if(!(flags & KIO::Overwrite))
    {
      iError = ERR_FILE_ALREADY_EXIST;
      return statusClientError;
    }
  }

  // do we have a ".part" file?
  const QString sPart = sCopyFile + QLatin1String(".part");
  bool bResume = false;
  QFileInfo sPartInfo(sPart);
  const bool bPartExists = sPartInfo.exists();
  const bool bMarkPartial = config()->readEntry("MarkPartial", true);
  const QString dest = bMarkPartial ? sPart : sCopyFile;
  if (bMarkPartial && bPartExists && sPartInfo.size() > 0)
  { // must not be a folder! please fix a similar bug in kio_file!!
    if(sPartInfo.isDir())
    {
      iError = ERR_DIR_ALREADY_EXIST;
      return statusClientError;                            // client side error
    }
    //doesn't work for copy? -> design flaw?
#ifdef  ENABLE_CAN_RESUME
    bResume = canResume(sPartInfo.size());
#else
    bResume = true;
#endif
  }

  if (bPartExists && !bResume)                  // get rid of an unwanted ".part" file
    QFile::remove(sPart);

  // WABA: Make sure that we keep writing permissions ourselves,
  // otherwise we can be in for a surprise on NFS.
  mode_t initialMode;
  if (permissions != -1)
    initialMode = permissions | S_IWUSR;
  else
    initialMode = 0666;

  // open the output file ...
  KIO::fileoffset_t hCopyOffset = 0;
  if (bResume) {
    iCopyFile = QT_OPEN(QFile::encodeName(sPart).constData(), O_RDWR);  // append if resuming
    hCopyOffset = QT_LSEEK(iCopyFile, 0, SEEK_END);
    if(hCopyOffset < 0)
    {
      iError = ERR_CANNOT_RESUME;
      return statusClientError;                            // client side error
    }
    // qDebug() << "resuming at " << hCopyOffset;
  }
  else {
    iCopyFile = QT_OPEN(QFile::encodeName(dest).constData(), O_CREAT | O_TRUNC | O_WRONLY, initialMode);
  }

  if(iCopyFile == -1)
  {
    // qDebug() << "### COULD NOT WRITE " << sCopyFile;
    iError = (errno == EACCES) ? ERR_WRITE_ACCESS_DENIED
                               : ERR_CANNOT_OPEN_FOR_WRITING;
    return statusClientError;
  }

  // delegate the real work (iError gets status) ...
  StatusCode iRes = ftpGet(iError, iCopyFile, url, hCopyOffset);
  if (QT_CLOSE(iCopyFile) && iRes == statusSuccess)
  {
    iError = ERR_COULD_NOT_WRITE;
    iRes = statusClientError;
  }
  iCopyFile = -1;

  // handle renaming or deletion of a partial file ...
  if(bMarkPartial)
  {
    if(iRes == statusSuccess)
    { // rename ".part" on success
      if (QFile::rename( sPart, sCopyFile ))
      {
        // If rename fails, try removing the destination first if it exists.
        if (!bDestExists || !(QFile::remove(sCopyFile) && QFile::rename(sPart, sCopyFile) == 0)) {
            // qDebug() << "cannot rename " << sPart << " to " << sCopyFile;
            iError = ERR_CANNOT_RENAME_PARTIAL;
            iRes = statusClientError;
        }
      }
    }
    else {
        sPartInfo.refresh();
        if (sPartInfo.exists()) { // should a very small ".part" be deleted?
            int size = config()->readEntry("MinimumKeepSize", DEFAULT_MINIMUM_KEEP_SIZE);
            if (sPartInfo.size() <  size)
                QFile::remove(sPart);
        }
    }
  }

  if (iRes == statusSuccess) {
      const QString mtimeStr = metaData("modified");
      if (!mtimeStr.isEmpty()) {
        QDateTime dt = QDateTime::fromString(mtimeStr, Qt::ISODate);
        if (dt.isValid()) {
          // qDebug() << "Updating modified timestamp to" << mtimeStr;
          struct utimbuf utbuf;
          utbuf.actime = sPartInfo.lastRead().toTime_t(); // access time, unchanged
          utbuf.modtime = dt.toTime_t(); // modification time
          ::utime(QFile::encodeName(sCopyFile).constData(), &utbuf);
        }
      }
  }

  return iRes;
}

Ftp::StatusCode Ftp::ftpSendMimeType(int& iError, const QUrl& url)
{
  const int totalSize = ((m_size == UnknownSize || m_size > 1024) ? 1024 : m_size);
  QByteArray buffer(totalSize, '\0');

  while (true) {
      // Wait for content to be available...
      if (m_data->bytesAvailable() == 0 && !m_data->waitForReadyRead((readTimeout() * 1000))) {
          iError = ERR_COULD_NOT_READ;
          return statusServerError;
      }

      const int bytesRead = m_data->peek(buffer.data(), totalSize);

      // If we got a -1, it must be an error so return an error.
      if (bytesRead == -1) {
          iError = ERR_COULD_NOT_READ;
          return statusServerError;
      }

      // If m_size is unknown, peek returns 0 (0 sized file ??), or peek returns size
      // equal to the size we want, then break.
      if (bytesRead == 0 || bytesRead == totalSize || m_size == UnknownSize) {
          break;
      }
  }

  if (!buffer.isEmpty()) {
      QMimeDatabase db;
      QMimeType mime = db.mimeTypeForFileNameAndData(url.path(), buffer);
      // qDebug() << "Emitting mimetype" << mime.name();
      mimeType(mime.name()); // emit the mime type...
  }

  return statusSuccess;
}

void Ftp::proxyAuthentication(const QNetworkProxy& proxy, QAuthenticator* authenticator)
{
    Q_UNUSED(proxy);
    // qDebug() << "Authenticator received -- realm:" << authenticator->realm() << "user:" << authenticator->user();

    AuthInfo info;
    info.url = m_proxyURL;
    info.realmValue = authenticator->realm();
    info.verifyPath = true;    //### whatever
    info.username = authenticator->user();

    const bool haveCachedCredentials = checkCachedAuthentication(info);

    // if m_socketProxyAuth is a valid pointer then authentication has been attempted before,
    // and it was not successful. see below and saveProxyAuthenticationForSocket().
    if (!haveCachedCredentials || m_socketProxyAuth) {
        // Save authentication info if the connection succeeds. We need to disconnect
        // this after saving the auth data (or an error) so we won't save garbage afterwards!
        connect(m_control, SIGNAL(connected()), this, SLOT(saveProxyAuthentication()));
        //### fillPromptInfo(&info);
        info.prompt = i18n("You need to supply a username and a password for "
                           "the proxy server listed below before you are allowed "
                           "to access any sites.");
        info.keepPassword = true;
        info.commentLabel = i18n("Proxy:");
        info.comment = i18n("<b>%1</b> at <b>%2</b>", info.realmValue, m_proxyURL.host());
        const bool dataEntered = openPasswordDialog(info, i18n("Proxy Authentication Failed."));
        if (!dataEntered) {
            // qDebug() << "looks like the user canceled proxy authentication.";
            error(ERR_USER_CANCELED, m_proxyURL.host());
            return;
        }
    }
    authenticator->setUser(info.username);
    authenticator->setPassword(info.password);
    authenticator->setOption(QLatin1String("keepalive"), info.keepPassword);

    if (m_socketProxyAuth) {
        *m_socketProxyAuth = *authenticator;
    } else {
        m_socketProxyAuth = new QAuthenticator(*authenticator);
    }

    m_proxyURL.setUserName(info.username);
    m_proxyURL.setPassword(info.password);
}

void Ftp::saveProxyAuthentication()
{
    // qDebug();
    disconnect(m_control, SIGNAL(connected()), this, SLOT(saveProxyAuthentication()));
    Q_ASSERT(m_socketProxyAuth);
    if (m_socketProxyAuth) {
        // qDebug() << "-- realm:" << m_socketProxyAuth->realm() << "user:" << m_socketProxyAuth->user();
        KIO::AuthInfo a;
        a.verifyPath = true;
        a.url = m_proxyURL;
        a.realmValue = m_socketProxyAuth->realm();
        a.username = m_socketProxyAuth->user();
        a.password = m_socketProxyAuth->password();
        a.keepPassword = m_socketProxyAuth->option(QLatin1String("keepalive")).toBool();
        cacheAuthentication(a);
    }
    delete m_socketProxyAuth;
    m_socketProxyAuth = 0;
}

void Ftp::fixupEntryName(FtpEntry* e)
{
    Q_ASSERT(e);
    if (e->type == S_IFDIR) {
        if (!ftpFolder(e->name, false)) {
            QString name (e->name.trimmed());
            if (ftpFolder(name, false)) {
                e->name = name;
                // qDebug() << "fixing up directory name from" << e->name << "to" << name;
            } else {
                int index = 0;
                while (e->name.at(index).isSpace()) {
                    index++;
                    name = e->name.mid(index);
                    if (ftpFolder(name, false)) {
                        // qDebug() << "fixing up directory name from" << e->name << "to" << name;
                        e->name = name;
                        break;
                    }
                }
            }
        }
    } else {
        if (!ftpFileExists(e->name)) {
            QString name (e->name.trimmed());
            if (ftpFileExists(name)) {
                e->name = name;
                // qDebug() << "fixing up filename from" << e->name << "to" << name;
            } else {
                int index = 0;
                while (e->name.at(index).isSpace()) {
                    index++;
                    name = e->name.mid(index);
                    if (ftpFileExists(name)) {
                        // qDebug() << "fixing up filename from" << e->name << "to" << name;
                        e->name = name;
                        break;
                    }
                }
            }
        }
    }
}

QTcpSocket *Ftp::synchronousConnectToHost(const QString &host, quint16 port)
{
    QTcpSocket *socket = new QSslSocket;
    socket->connectToHost(host, port);
    socket->waitForConnected(connectTimeout() * 1000);
    return socket;
}
