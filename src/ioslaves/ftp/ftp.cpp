/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2006 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2019 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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

#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

#include <QCoreApplication>
#include <QDir>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QTcpServer>
#include <QSslSocket>
#include <QAuthenticator>
#include <QMimeDatabase>

#include <QDebug>
#include <ioslave_defaults.h>
#include <KLocalizedString>
#include <kremoteencoding.h>
#include <KConfigGroup>

#include "kioglobal_p.h"

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_FTP)
Q_LOGGING_CATEGORY(KIO_FTP, "kf.kio.slaves.ftp", QtWarningMsg)

#if HAVE_STRTOLL
#define charToLongLong(a) strtoll(a, nullptr, 10)
#else
#define charToLongLong(a) strtol(a, nullptr, 10)
#endif

#define FTP_LOGIN   "anonymous"
#define FTP_PASSWD  "anonymous@"

#define ENABLE_CAN_RESUME

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.ftp" FILE "ftp.json")
};

static QString ftpCleanPath(const QString &path)
{
    if (path.endsWith(QLatin1String(";type=A"), Qt::CaseInsensitive) ||
            path.endsWith(QLatin1String(";type=I"), Qt::CaseInsensitive) ||
            path.endsWith(QLatin1String(";type=D"), Qt::CaseInsensitive)) {
        return path.left((path.length() - qstrlen(";type=X")));
    }

    return path;
}

static char ftpModeFromPath(const QString &path, char defaultMode = '\0')
{
    const int index = path.lastIndexOf(QLatin1String(";type="));

    if (index > -1 && (index + 6) < path.size()) {
        const QChar mode = path.at(index + 6);
        // kio_ftp supports only A (ASCII) and I(BINARY) modes.
        if (mode == QLatin1Char('A') || mode == QLatin1Char('a') ||
                mode == QLatin1Char('I') || mode == QLatin1Char('i')) {
            return mode.toUpper().toLatin1();
        }
    }

    return defaultMode;
}

static bool supportedProxyScheme(const QString &scheme)
{
    return (scheme == QLatin1String("ftp") || scheme == QLatin1String("socks"));
}

// JPF: somebody should find a better solution for this or move this to KIO
namespace KIO
{
enum buffersizes {
    /**
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
    minimumMimeSize =     1024,
};

// JPF: this helper was derived from write_all in file.cc (FileProtocol).
static // JPF: in ftp.cc we make it static
/**
 * This helper handles some special issues (blocking and interrupted
 * system call) when writing to a file handle.
 *
 * @return 0 on success or an error code on failure (ERR_CANNOT_WRITE,
 * ERR_DISK_FULL, ERR_CONNECTION_BROKEN).
 */
int WriteToFile(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        // JPF: shouldn't there be a KDE_write?
        ssize_t written = write(fd, buf, len);
        if (written >= 0) {
            buf += written;
            len -= written;
            continue;
        }
        switch (errno) {
        case EINTR:   continue;
        case EPIPE:   return ERR_CONNECTION_BROKEN;
        case ENOSPC:  return ERR_DISK_FULL;
        default:      return ERR_CANNOT_WRITE;
        }
    }
    return 0;
}
}

const KIO::filesize_t FtpInternal::UnknownSize = (KIO::filesize_t) - 1;

using namespace KIO;

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_ftp"));

    qCDebug(KIO_FTP) << "Starting";

    if (argc != 4) {
        fprintf(stderr, "Usage: kio_ftp protocol domain-socket1 domain-socket2\n");
        exit(-1);
    }

    Ftp slave(argv[2], argv[3]);
    slave.dispatchLoop();

    qCDebug(KIO_FTP) << "Done";
    return 0;
}

//===============================================================================
// FtpInternal
//===============================================================================

/**
 * This closes a data connection opened by ftpOpenDataConnection().
 */
void FtpInternal::ftpCloseDataConnection()
{
    delete m_data;
    m_data = nullptr;
    delete m_server;
    m_server = nullptr;
}

/**
 * This closes a control connection opened by ftpOpenControlConnection() and reinits the
 * related states.  This method gets called from the constructor with m_control = nullptr.
 */
void FtpInternal::ftpCloseControlConnection()
{
    m_extControl = 0;
    delete m_control;
    m_control = nullptr;
    m_cDataMode = 0;
    m_bLoggedOn = false;    // logon needs control connection
    m_bTextMode = false;
    m_bBusy = false;
}

/**
 * Returns the last response from the server (iOffset >= 0)  -or-  reads a new response
 * (iOffset < 0). The result is returned (with iOffset chars skipped for iOffset > 0).
 */
const char *FtpInternal::ftpResponse(int iOffset)
{
    Q_ASSERT(m_control);    // must have control connection socket
    const char *pTxt = m_lastControlLine.data();

    // read the next line ...
    if (iOffset < 0) {
        int  iMore = 0;
        m_iRespCode = 0;

        if (!pTxt) {
            return nullptr;    // avoid using a nullptr when calling atoi.
        }

        // If the server sends a multiline response starting with
        // "nnn-text" we loop here until a final "nnn text" line is
        // reached. Only data from the final line will be stored.
        do {
            while (!m_control->canReadLine() && m_control->waitForReadyRead((q->readTimeout() * 1000))) {}
            m_lastControlLine = m_control->readLine();
            pTxt = m_lastControlLine.data();
            int iCode  = atoi(pTxt);
            if (iMore == 0) {
                // first line
                qCDebug(KIO_FTP) << "    > " << pTxt;
                if (iCode >= 100) {
                    m_iRespCode = iCode;
                    if (pTxt[3] == '-') {
                        // marker for a multiple line response
                        iMore = iCode;
                    }
                } else {
                    qCWarning(KIO_FTP) << "Cannot parse valid code from line" << pTxt;
                }
            } else {
                // multi-line
                qCDebug(KIO_FTP) << "    > " << pTxt;
                if (iCode >= 100 && iCode == iMore && pTxt[3] == ' ') {
                    iMore = 0;
                }
            }
        } while (iMore != 0);
        qCDebug(KIO_FTP) << "resp> " << pTxt;

        m_iRespType = (m_iRespCode > 0) ? m_iRespCode / 100 : 0;
    }

    // return text with offset ...
    while (iOffset-- > 0 && pTxt[0]) {
        pTxt++;
    }
    return pTxt;
}

void FtpInternal::closeConnection()
{
    if (m_control || m_data)
        qCDebug(KIO_FTP) << "m_bLoggedOn=" << m_bLoggedOn << " m_bBusy=" << m_bBusy;

        if (m_bBusy) {           // ftpCloseCommand not called
            qCWarning(KIO_FTP) << "Abandoned data stream";
            ftpCloseDataConnection();
        }

    if (m_bLoggedOn) {        // send quit
        if (!ftpSendCmd(QByteArrayLiteral("quit"), 0) || (m_iRespType != 2)) {
            qCWarning(KIO_FTP) << "QUIT returned error: " << m_iRespCode;
        }
    }

    // close the data and control connections ...
    ftpCloseDataConnection();
    ftpCloseControlConnection();
}

FtpInternal::FtpInternal(Ftp *qptr)
    : QObject()
    , q(qptr)
{
    ftpCloseControlConnection();
}

FtpInternal::~FtpInternal()
{
    qCDebug(KIO_FTP);
    closeConnection();
}

void FtpInternal::setHost(const QString &_host, quint16 _port, const QString &_user,
                  const QString &_pass)
{
    qCDebug(KIO_FTP) << _host << "port=" << _port << "user=" << _user;

    m_proxyURL.clear();
    m_proxyUrls = q->mapConfig().value(QStringLiteral("ProxyUrls"), QString()).toString().split(QLatin1Char(','), Qt::SkipEmptyParts);

    qCDebug(KIO_FTP) << "proxy urls:" << m_proxyUrls;

    if (m_host != _host || m_port != _port ||
            m_user != _user || m_pass != _pass) {
        closeConnection();
    }

    m_host = _host;
    m_port = _port;
    m_user = _user;
    m_pass = _pass;
}

Result FtpInternal::openConnection()
{
    return ftpOpenConnection(LoginMode::Explicit);
}

Result FtpInternal::ftpOpenConnection(LoginMode loginMode)
{
    // check for implicit login if we are already logged on ...
    if (loginMode == LoginMode::Implicit && m_bLoggedOn) {
        Q_ASSERT(m_control);    // must have control connection socket
        return Result::pass();
    }

    qCDebug(KIO_FTP) << "host=" << m_host << ", port=" << m_port << ", user=" << m_user << "password= [password hidden]";

    q->infoMessage(i18n("Opening connection to host %1", m_host));

    if (m_host.isEmpty()) {
        return Result::fail(ERR_UNKNOWN_HOST);
    }

    Q_ASSERT(!m_bLoggedOn);

    m_initialPath.clear();
    m_currentPath.clear();

    const Result result = ftpOpenControlConnection();
    if (!result.success) {
        return result;
    }
    q->infoMessage(i18n("Connected to host %1", m_host));

    bool userNameChanged = false;
    if (loginMode != LoginMode::Deferred) {
        const Result result = ftpLogin(&userNameChanged);
        m_bLoggedOn = result.success;
        if (!m_bLoggedOn) {
            return result;
        }
    }

    m_bTextMode = q->configValue(QStringLiteral("textmode"), false);
    q->connected();

    // Redirected due to credential change...
    if (userNameChanged && m_bLoggedOn) {
        QUrl realURL;
        realURL.setScheme(QStringLiteral("ftp"));
        if (m_user != QLatin1String(FTP_LOGIN)) {
            realURL.setUserName(m_user);
        }
        if (m_pass != QLatin1String(FTP_PASSWD)) {
            realURL.setPassword(m_pass);
        }
        realURL.setHost(m_host);
        if (m_port > 0 && m_port != DEFAULT_FTP_PORT) {
            realURL.setPort(m_port);
        }
        if (m_initialPath.isEmpty()) {
            m_initialPath = QStringLiteral("/");
        }
        realURL.setPath(m_initialPath);
        qCDebug(KIO_FTP) << "User name changed! Redirecting to" << realURL;
        q->redirection(realURL);
        return Result::fail();
    }

    return Result::pass();
}

/**
 * Called by @ref openConnection. It opens the control connection to the ftp server.
 *
 * @return true on success.
 */
Result FtpInternal::ftpOpenControlConnection()
{
    if (m_proxyUrls.isEmpty()) {
        return ftpOpenControlConnection(m_host, m_port);
    }

    Result result = Result::fail();

    for (const QString &proxyUrl : qAsConst(m_proxyUrls)) {
        const QUrl url(proxyUrl);
        const QString scheme(url.scheme());

        if (!supportedProxyScheme(scheme)) {
            // TODO: Need a new error code to indicate unsupported URL scheme.
            result = Result::fail(ERR_CANNOT_CONNECT, url.toString());
            continue;
        }

        if (!isSocksProxyScheme(scheme)) {
            const Result result = ftpOpenControlConnection(url.host(), url.port());
            if (result.success) {
                return Result::pass();
            }
            continue;
        }

        qCDebug(KIO_FTP) << "Connecting to SOCKS proxy @" << url;
        m_proxyURL = url;
        result = ftpOpenControlConnection(m_host, m_port);
        if (result.success) {
            return result;
        }
        m_proxyURL.clear();
    }

    return result;
}

Result FtpInternal::ftpOpenControlConnection(const QString &host, int port)
{
    // implicitly close, then try to open a new connection ...
    closeConnection();
    QString sErrorMsg;

    // now connect to the server and read the login message ...
    if (port == 0) {
        port = 21;    // default FTP port
    }
    const auto connectionResult = synchronousConnectToHost(host, port);
    m_control = connectionResult.socket;

    int iErrorCode = m_control->state() == QAbstractSocket::ConnectedState ? 0 : ERR_CANNOT_CONNECT;
    if (!connectionResult.result.success) {
        qDebug() << "overriding error code!!1" << connectionResult.result.error;
        iErrorCode = connectionResult.result.error;
        sErrorMsg = connectionResult.result.errorString;
    }

    // on connect success try to read the server message...
    if (iErrorCode == 0) {
        const char *psz = ftpResponse(-1);
        if (m_iRespType != 2) {
            // login not successful, do we have an message text?
            if (psz[0]) {
                sErrorMsg = i18n("%1 (Error %2)", host, q->remoteEncoding()->decode(psz).trimmed());
            }
            iErrorCode = ERR_CANNOT_CONNECT;
        }
    } else {
        const auto socketError = m_control->error();
        if (socketError == QAbstractSocket::HostNotFoundError) {
            iErrorCode = ERR_UNKNOWN_HOST;
        }

        sErrorMsg = QStringLiteral("%1: %2").arg(host, m_control->errorString());
    }

    // if there was a problem - report it ...
    if (iErrorCode == 0) {          // OK, return success
        return Result::pass();
    }
    closeConnection();              // clean-up on error
    return Result::fail(iErrorCode, sErrorMsg);
}

/**
 * Called by @ref openConnection. It logs us in.
 * @ref m_initialPath is set to the current working directory
 * if logging on was successful.
 *
 * @return true on success.
 */
Result FtpInternal::ftpLogin(bool *userChanged)
{
    q->infoMessage(i18n("Sending login information"));

    Q_ASSERT(!m_bLoggedOn);

    QString user(m_user);
    QString pass(m_pass);

    if (q->configValue(QStringLiteral("EnableAutoLogin"), false)) {
        QString au = q->configValue(QStringLiteral("autoLoginUser"));
        if (!au.isEmpty()) {
            user = au;
            pass = q->configValue(QStringLiteral("autoLoginPass"));
        }
    }

    AuthInfo info;
    info.url.setScheme(QStringLiteral("ftp"));
    info.url.setHost(m_host);
    if (m_port > 0 && m_port != DEFAULT_FTP_PORT) {
        info.url.setPort(m_port);
    }
    if (!user.isEmpty()) {
        info.url.setUserName(user);
    }

    // Check for cached authentication first and fallback to
    // anonymous login when no stored credentials are found.
    if (!q->configValue(QStringLiteral("TryAnonymousLoginFirst"), false) &&
            pass.isEmpty() && q->checkCachedAuthentication(info)) {
        user = info.username;
        pass = info.password;
    }

    // Try anonymous login if both username/password
    // information is blank.
    if (user.isEmpty() && pass.isEmpty()) {
        user = QStringLiteral(FTP_LOGIN);
        pass = QStringLiteral(FTP_PASSWD);
    }

    QByteArray tempbuf;
    QString lastServerResponse;
    int failedAuth = 0;
    bool promptForRetry = false;

    // Give the user the option to login anonymously...
    info.setExtraField(QStringLiteral("anonymous"), false);

    do {
        // Check the cache and/or prompt user for password if 1st
        // login attempt failed OR the user supplied a login name,
        // but no password.
        if (failedAuth > 0 || (!user.isEmpty() && pass.isEmpty())) {
            QString errorMsg;
            qCDebug(KIO_FTP) << "Prompting user for login info...";

            // Ask user if we should retry after when login fails!
            if (failedAuth > 0 && promptForRetry) {
                errorMsg = i18n("Message sent:\nLogin using username=%1 and "
                                "password=[hidden]\n\nServer replied:\n%2\n\n"
                                , user, lastServerResponse);
            }

            if (user != QLatin1String(FTP_LOGIN)) {
                info.username = user;
            }

            info.prompt = i18n("You need to supply a username and a password "
                               "to access this site.");
            info.commentLabel = i18n("Site:");
            info.comment = i18n("<b>%1</b>",  m_host);
            info.keepPassword = true; // Prompt the user for persistence as well.
            info.setModified(false);  // Default the modified flag since we reuse authinfo.

            const bool disablePassDlg = q->configValue(QStringLiteral("DisablePassDlg"), false);
            if (disablePassDlg) {
                return Result::fail(ERR_USER_CANCELED, m_host);
            }
            const int errorCode = q->openPasswordDialogV2(info, errorMsg);
            if (errorCode) {
                return Result::fail(errorCode);
            } else {
                // User can decide go anonymous using checkbox
                if (info.getExtraField(QStringLiteral("anonymous")).toBool()) {
                    user = QStringLiteral(FTP_LOGIN);
                    pass = QStringLiteral(FTP_PASSWD);
                } else {
                    user = info.username;
                    pass = info.password;
                }
                promptForRetry = true;
            }
        }

        tempbuf = "USER " + user.toLatin1();
        if (m_proxyURL.isValid()) {
            tempbuf += '@' + m_host.toLatin1();
            if (m_port > 0 && m_port != DEFAULT_FTP_PORT) {
                tempbuf += ':' + QByteArray::number(m_port);
            }
        }

        qCDebug(KIO_FTP) << "Sending Login name: " << tempbuf;

        bool loggedIn = (ftpSendCmd(tempbuf) && (m_iRespCode == 230));
        bool needPass = (m_iRespCode == 331);
        // Prompt user for login info if we do not
        // get back a "230" or "331".
        if (!loggedIn && !needPass) {
            lastServerResponse = QString::fromUtf8(ftpResponse(0));
            qCDebug(KIO_FTP) << "Login failed: " << lastServerResponse;
            ++failedAuth;
            continue;  // Well we failed, prompt the user please!!
        }

        if (needPass) {
            tempbuf = "PASS " + pass.toLatin1();
            qCDebug(KIO_FTP) << "Sending Login password: " << "[protected]";
            loggedIn = (ftpSendCmd(tempbuf) && (m_iRespCode == 230));
        }

        if (loggedIn) {
            // Make sure the user name changed flag is properly set.
            if (userChanged) {
                *userChanged = (!m_user.isEmpty() && (m_user != user));
            }

            // Do not cache the default login!!
            if (user != QLatin1String(FTP_LOGIN) && pass != QLatin1String(FTP_PASSWD)) {
                // Update the username in case it was changed during login.
                if (!m_user.isEmpty()) {
                    info.url.setUserName(user);
                    m_user = user;
                }

                // Cache the password if the user requested it.
                if (info.keepPassword) {
                    q->cacheAuthentication(info);
                }
            }
            failedAuth = -1;
        } else {
            // some servers don't let you login anymore
            // if you fail login once, so restart the connection here
            lastServerResponse = QString::fromUtf8(ftpResponse(0));
            const Result result = ftpOpenControlConnection();
            if (!result.success) {
                return result;
            }
        }
    } while (++failedAuth);

    qCDebug(KIO_FTP) << "Login OK";
    q->infoMessage(i18n("Login OK"));

    // Okay, we're logged in. If this is IIS 4, switch dir listing style to Unix:
    // Thanks to jk@soegaard.net (Jens Kristian Sgaard) for this hint
    if (ftpSendCmd(QByteArrayLiteral("SYST")) && (m_iRespType == 2)) {
        if (!qstrncmp(ftpResponse(0), "215 Windows_NT", 14)) {  // should do for any version
            (void) ftpSendCmd(QByteArrayLiteral("site dirstyle"));
            // Check if it was already in Unix style
            // Patch from Keith Refson <Keith.Refson@earth.ox.ac.uk>
            if (!qstrncmp(ftpResponse(0), "200 MSDOS-like directory output is on", 37))
                //It was in Unix style already!
            {
                (void) ftpSendCmd(QByteArrayLiteral("site dirstyle"));
            }
            // windows won't support chmod before KDE konquers their desktop...
            m_extControl |= chmodUnknown;
        }
    } else {
        qCWarning(KIO_FTP) << "SYST failed";
    }

    if (q->configValue(QStringLiteral("EnableAutoLoginMacro"), false)) {
        ftpAutoLoginMacro();
    }

    // Get the current working directory
    qCDebug(KIO_FTP) << "Searching for pwd";
    if (!ftpSendCmd(QByteArrayLiteral("PWD")) || (m_iRespType != 2)) {
        qCDebug(KIO_FTP) << "Couldn't issue pwd command";
        return Result::fail(ERR_CANNOT_LOGIN, i18n("Could not login to %1.", m_host));   // or anything better ?
    }

    QString sTmp = q->remoteEncoding()->decode(ftpResponse(3));
    const int iBeg = sTmp.indexOf(QLatin1Char('"'));
    const int iEnd = sTmp.lastIndexOf(QLatin1Char('"'));
    if (iBeg > 0 && iBeg < iEnd) {
        m_initialPath = sTmp.mid(iBeg + 1, iEnd - iBeg - 1);
        if (!m_initialPath.startsWith(QLatin1Char('/'))) {
            m_initialPath.prepend(QLatin1Char('/'));
        }
        qCDebug(KIO_FTP) << "Initial path set to: " << m_initialPath;
        m_currentPath = m_initialPath;
    }

    return Result::pass();
}

void FtpInternal::ftpAutoLoginMacro()
{
    QString macro = q->metaData(QStringLiteral("autoLoginMacro"));

    if (macro.isEmpty()) {
        return;
    }

    const QStringList list = macro.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it) {
        if ((*it).startsWith(QLatin1String("init"))) {
            const QStringList list2 = macro.split(QLatin1Char('\\'), Qt::SkipEmptyParts);
            it = list2.begin();
            ++it;  // ignore the macro name

            for (; it != list2.end(); ++it) {
                // TODO: Add support for arbitrary commands
                // besides simply changing directory!!
                if ((*it).startsWith(QLatin1String("cwd"))) {
                    (void) ftpFolder((*it).mid(4));
                }
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
bool FtpInternal::ftpSendCmd(const QByteArray &cmd, int maxretries)
{
    Q_ASSERT(m_control);    // must have control connection socket

    if (cmd.indexOf('\r') != -1 || cmd.indexOf('\n') != -1) {
        qCWarning(KIO_FTP) << "Invalid command received (contains CR or LF):"
                   << cmd.data();
        return false;
    }

    // Don't print out the password...
    bool isPassCmd = (cmd.left(4).toLower() == "pass");

    // Send the message...
    const QByteArray buf = cmd + "\r\n";      // Yes, must use CR/LF - see http://cr.yp.to/ftp/request.html
    int num = m_control->write(buf);
    while (m_control->bytesToWrite() && m_control->waitForBytesWritten()) {}

    // If we were able to successfully send the command, then we will
    // attempt to read the response. Otherwise, take action to re-attempt
    // the login based on the maximum number of retries specified...
    if (num > 0) {
        ftpResponse(-1);
    } else {
        m_iRespType = m_iRespCode = 0;
    }

    // If respCh is NULL or the response is 421 (Timed-out), we try to re-send
    // the command based on the value of maxretries.
    if ((m_iRespType <= 0) || (m_iRespCode == 421)) {
        // We have not yet logged on...
        if (!m_bLoggedOn) {
            // The command was sent from the ftpLogin function, i.e. we are actually
            // attempting to login in. NOTE: If we already sent the username, we
            // return false and let the user decide whether (s)he wants to start from
            // the beginning...
            if (maxretries > 0 && !isPassCmd) {
                closeConnection();
                const auto result = ftpOpenConnection(LoginMode::Deferred);
                if (result.success && ftpSendCmd(cmd, maxretries - 1)) {
                    return true;
                }
            }

            return false;
        } else {
            if (maxretries < 1) {
                return false;
            } else {
                qCDebug(KIO_FTP) << "Was not able to communicate with " << m_host
                                 << "Attempting to re-establish connection.";

                closeConnection(); // Close the old connection...
                const Result openResult = openConnection();  // Attempt to re-establish a new connection...

                if (!openResult.success) {
                    if (m_control) { // if openConnection succeeded ...
                        qCDebug(KIO_FTP) << "Login failure, aborting";
                        closeConnection();
                    }
                    return false;
                }

                qCDebug(KIO_FTP) << "Logged back in, re-issuing command";

                // If we were able to login, resend the command...
                if (maxretries) {
                    maxretries--;
                }

                return ftpSendCmd(cmd, maxretries);
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
int FtpInternal::ftpOpenPASVDataConnection()
{
    Q_ASSERT(m_control);    // must have control connection socket
    Q_ASSERT(!m_data);      // ... but no data connection

    // Check that we can do PASV
    QHostAddress address = m_control->peerAddress();
    if (address.protocol() != QAbstractSocket::IPv4Protocol && !isSocksProxy()) {
        return ERR_INTERNAL;    // no PASV for non-PF_INET connections
    }

    if (m_extControl & pasvUnknown) {
        return ERR_INTERNAL;    // already tried and got "unknown command"
    }

    m_bPasv = true;

    /* Let's PASsiVe*/
    if (!ftpSendCmd(QByteArrayLiteral("PASV")) || (m_iRespType != 2)) {
        qCDebug(KIO_FTP) << "PASV attempt failed";
        // unknown command?
        if (m_iRespType == 5) {
            qCDebug(KIO_FTP) << "disabling use of PASV";
            m_extControl |= pasvUnknown;
        }
        return ERR_INTERNAL;
    }

    // The usual answer is '227 Entering Passive Mode. (160,39,200,55,6,245)'
    // but anonftpd gives '227 =160,39,200,55,6,245'
    int i[6];
    const char *start = strchr(ftpResponse(3), '(');
    if (!start) {
        start = strchr(ftpResponse(3), '=');
    }
    if (!start ||
            (sscanf(start, "(%d,%d,%d,%d,%d,%d)", &i[0], &i[1], &i[2], &i[3], &i[4], &i[5]) != 6 &&
             sscanf(start, "=%d,%d,%d,%d,%d,%d", &i[0], &i[1], &i[2], &i[3], &i[4], &i[5]) != 6)) {
        qCritical() << "parsing IP and port numbers failed. String parsed: " << start;
        return ERR_INTERNAL;
    }

    // we ignore the host part on purpose for two reasons
    // a) it might be wrong anyway
    // b) it would make us being susceptible to a port scanning attack

    // now connect the data socket ...
    quint16 port = i[4] << 8 | i[5];
    const QString host = (isSocksProxy() ? m_host : address.toString());
    const auto connectionResult = synchronousConnectToHost(host, port);
    m_data = connectionResult.socket;
    if (!connectionResult.result.success) {
        return connectionResult.result.error;
    }

    return m_data->state() == QAbstractSocket::ConnectedState ? 0 : ERR_INTERNAL;
}

/*
 * ftpOpenEPSVDataConnection - opens a data connection via EPSV
 */
int FtpInternal::ftpOpenEPSVDataConnection()
{
    Q_ASSERT(m_control);    // must have control connection socket
    Q_ASSERT(!m_data);      // ... but no data connection

    QHostAddress address = m_control->peerAddress();
    int portnum;

    if (m_extControl & epsvUnknown) {
        return ERR_INTERNAL;
    }

    m_bPasv = true;
    if (!ftpSendCmd(QByteArrayLiteral("EPSV")) || (m_iRespType != 2)) {
        // unknown command?
        if (m_iRespType == 5) {
            qCDebug(KIO_FTP) << "disabling use of EPSV";
            m_extControl |= epsvUnknown;
        }
        return ERR_INTERNAL;
    }

    const char *start = strchr(ftpResponse(3), '|');
    if (!start || sscanf(start, "|||%d|", &portnum) != 1) {
        return ERR_INTERNAL;
    }
    Q_ASSERT(portnum > 0);

    const QString host = (isSocksProxy() ? m_host : address.toString());
    const auto connectionResult = synchronousConnectToHost(host, static_cast<quint16>(portnum));
    m_data = connectionResult.socket;
    if (!connectionResult.result.success) {
        return connectionResult.result.error;
    }
    return m_data->state() == QAbstractSocket::ConnectedState ? 0 : ERR_INTERNAL;
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
int FtpInternal::ftpOpenDataConnection()
{
    // make sure that we are logged on and have no data connection...
    Q_ASSERT(m_bLoggedOn);
    ftpCloseDataConnection();

    int  iErrCode = 0;
    int  iErrCodePASV = 0;  // Remember error code from PASV

    // First try passive (EPSV & PASV) modes
    if (!q->configValue(QStringLiteral("DisablePassiveMode"), false)) {
        iErrCode = ftpOpenPASVDataConnection();
        if (iErrCode == 0) {
            return 0;    // success
        }
        iErrCodePASV = iErrCode;
        ftpCloseDataConnection();

        if (!q->configValue(QStringLiteral("DisableEPSV"), false)) {
            iErrCode = ftpOpenEPSVDataConnection();
            if (iErrCode == 0) {
                return 0;    // success
            }
            ftpCloseDataConnection();
        }

        // if we sent EPSV ALL already and it was accepted, then we can't
        // use active connections any more
        if (m_extControl & epsvAllSent) {
            return iErrCodePASV;
        }
    }

    // fall back to port mode
    iErrCode = ftpOpenPortDataConnection();
    if (iErrCode == 0) {
        return 0;    // success
    }

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
int FtpInternal::ftpOpenPortDataConnection()
{
    Q_ASSERT(m_control);    // must have control connection socket
    Q_ASSERT(!m_data);      // ... but no data connection

    m_bPasv = false;
    if (m_extControl & eprtUnknown) {
        return ERR_INTERNAL;
    }

    if (!m_server) {
        m_server = new QTcpServer;
        m_server->listen(QHostAddress::Any, 0);
    }

    if (!m_server->isListening()) {
        delete m_server;
        m_server = nullptr;
        return ERR_CANNOT_LISTEN;
    }

    m_server->setMaxPendingConnections(1);

    QString command;
    QHostAddress localAddress = m_control->localAddress();
    if (localAddress.protocol() == QAbstractSocket::IPv4Protocol) {
        struct {
            quint32 ip4;
            quint16 port;
        } data;
        data.ip4 = localAddress.toIPv4Address();
        data.port = m_server->serverPort();

        unsigned char *pData = reinterpret_cast<unsigned char *>(&data);
        command = QStringLiteral("PORT %1,%2,%3,%4,%5,%6").arg(pData[3]).arg(pData[2]).arg(pData[1]).arg(pData[0]).arg(pData[5]).arg(pData[4]);
    } else if (localAddress.protocol() == QAbstractSocket::IPv6Protocol) {
        command = QStringLiteral("EPRT |2|%2|%3|").arg(localAddress.toString()).arg(m_server->serverPort());
    }

    if (ftpSendCmd(command.toLatin1()) && (m_iRespType == 2)) {
        return 0;
    }

    delete m_server;
    m_server = nullptr;
    return ERR_INTERNAL;
}

Result FtpInternal::ftpOpenCommand(const char *_command, const QString &_path, char _mode,
                         int errorcode, KIO::fileoffset_t _offset)
{
    int errCode = 0;
    if (!ftpDataMode(ftpModeFromPath(_path, _mode))) {
        errCode = ERR_CANNOT_CONNECT;
    } else {
        errCode = ftpOpenDataConnection();
    }

    if (errCode != 0) {
        return Result::fail(errCode, m_host);
    }

    if (_offset > 0) {
        // send rest command if offset > 0, this applies to retr and stor commands
        char buf[100];
        sprintf(buf, "rest %lld", _offset);
        if (!ftpSendCmd(buf)) {
            return Result::fail();
        }
        if (m_iRespType != 3) {
            return Result::fail(ERR_CANNOT_RESUME, _path);   // should never happen
        }
    }

    QByteArray tmp = _command;
    QString errormessage;

    if (!_path.isEmpty()) {
        tmp += ' ' + q->remoteEncoding()->encode(ftpCleanPath(_path));
    }

    if (!ftpSendCmd(tmp) || (m_iRespType != 1)) {
        if (_offset > 0 && qstrcmp(_command, "retr") == 0 && (m_iRespType == 4)) {
            errorcode = ERR_CANNOT_RESUME;
        }
        // The error code here depends on the command
        errormessage = _path + i18n("\nThe server said: \"%1\"", QString::fromUtf8(ftpResponse(0)).trimmed());
    }

    else {
        // Only now we know for sure that we can resume
        if (_offset > 0 && qstrcmp(_command, "retr") == 0) {
            q->canResume();
        }

        if (m_server && !m_data) {
            qCDebug(KIO_FTP) << "waiting for connection from remote.";
            m_server->waitForNewConnection(q->connectTimeout() * 1000);
            m_data = m_server->nextPendingConnection();
        }

        if (m_data) {
            qCDebug(KIO_FTP) << "connected with remote.";
            m_bBusy = true;              // cleared in ftpCloseCommand
            return Result::pass();
        }

        qCDebug(KIO_FTP) << "no connection received from remote.";
        errorcode = ERR_CANNOT_ACCEPT;
        errormessage = m_host;
    }

    if (errorcode != KJob::NoError) {
        return Result::fail(errorcode, errormessage);
    }
    return Result::fail();
}

bool FtpInternal::ftpCloseCommand()
{
    // first close data sockets (if opened), then read response that
    // we got for whatever was used in ftpOpenCommand ( should be 226 )
    ftpCloseDataConnection();

    if (!m_bBusy) {
        return true;
    }

    qCDebug(KIO_FTP) << "ftpCloseCommand: reading command result";
    m_bBusy = false;

    if (!ftpResponse(-1) || (m_iRespType != 2)) {
        qCDebug(KIO_FTP) << "ftpCloseCommand: no transfer complete message";
        return false;
    }
    return true;
}

Result FtpInternal::mkdir(const QUrl &url, int permissions)
{
    auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    const QByteArray encodedPath(q->remoteEncoding()->encode(url));
    const QString path = QString::fromLatin1(encodedPath.constData(), encodedPath.size());

    if (!ftpSendCmd((QByteArrayLiteral("mkd ") + encodedPath)) || (m_iRespType != 2)) {
        QString currentPath(m_currentPath);

        // Check whether or not mkdir failed because
        // the directory already exists...
        if (ftpFolder(path)) {
            const QString &failedPath = path;
            // Change the directory back to what it was...
            (void) ftpFolder(currentPath);
            return Result::fail(ERR_DIR_ALREADY_EXIST, failedPath);
        }

        return Result::fail(ERR_CANNOT_MKDIR, path);
    }

    if (permissions != -1) {
        // chmod the dir we just created, ignoring errors.
        (void) ftpChmod(path, permissions);
    }

    return Result::pass();
}

Result FtpInternal::rename(const QUrl &src, const QUrl &dst, KIO::JobFlags flags)
{
    const auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    // The actual functionality is in ftpRename because put needs it
    return ftpRename(src.path(), dst.path(), flags);
}

Result FtpInternal::ftpRename(const QString &src, const QString &dst, KIO::JobFlags jobFlags)
{
    Q_ASSERT(m_bLoggedOn);

    // Must check if dst already exists, RNFR+RNTO overwrites by default (#127793).
    if (!(jobFlags & KIO::Overwrite)) {
        if (ftpFileExists(dst)) {
            return Result::fail(ERR_FILE_ALREADY_EXIST, dst);
        }
    }

    if (ftpFolder(dst)) {
        return Result::fail(ERR_DIR_ALREADY_EXIST, dst);
    }

    // CD into parent folder
    const int pos = src.lastIndexOf(QLatin1Char('/'));
    if (pos >= 0) {
        if (!ftpFolder(src.left(pos + 1))) {
            return Result::fail(ERR_CANNOT_ENTER_DIRECTORY, src);
        }
    }

    const QByteArray from_cmd = "RNFR " + q->remoteEncoding()->encode(src.mid(pos + 1));
    if (!ftpSendCmd(from_cmd) || (m_iRespType != 3)) {
        return Result::fail(ERR_CANNOT_RENAME, src);
    }

    const QByteArray to_cmd = "RNTO " + q->remoteEncoding()->encode(dst);
    if (!ftpSendCmd(to_cmd) || (m_iRespType != 2)) {
        return Result::fail(ERR_CANNOT_RENAME, src);
    }

    return Result::pass();
}

Result FtpInternal::del(const QUrl &url, bool isfile)
{
    auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    // When deleting a directory, we must exit from it first
    // The last command probably went into it (to stat it)
    if (!isfile) {
        (void) ftpFolder(q->remoteEncoding()->decode(q->remoteEncoding()->directory(url)));    // ignore errors
    }

    const QByteArray cmd = (isfile ? "DELE " : "RMD ") + q->remoteEncoding()->encode(url);

    if (!ftpSendCmd(cmd) || (m_iRespType != 2)) {
        return Result::fail(ERR_CANNOT_DELETE, url.path());
    }

    return Result::pass();
}

bool FtpInternal::ftpChmod(const QString &path, int permissions)
{
    Q_ASSERT(m_bLoggedOn);

    if (m_extControl & chmodUnknown) {   // previous errors?
        return false;
    }

    // we need to do bit AND 777 to get permissions, in case
    // we were sent a full mode (unlikely)
    const QByteArray cmd = "SITE CHMOD " + QByteArray::number(permissions & 0777/*octal*/, 8 /*octal*/) + ' ' + q->remoteEncoding()->encode(path);

    if (ftpSendCmd(cmd)) {
        qCDebug(KIO_FTP) << "ftpChmod: Failed to issue chmod";
        return false;
    }

    if (m_iRespType == 2) {
        return true;
    }

    if (m_iRespCode == 500) {
        m_extControl |= chmodUnknown;
        qCDebug(KIO_FTP) << "ftpChmod: CHMOD not supported - disabling";
    }
    return false;
}

Result FtpInternal::chmod(const QUrl &url, int permissions)
{
    const auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    if (!ftpChmod(url.path(), permissions)) {
        return Result::fail(ERR_CANNOT_CHMOD, url.path());
    }

    return Result::pass();
}

void FtpInternal::ftpCreateUDSEntry(const QString &filename, const FtpEntry &ftpEnt, UDSEntry &entry, bool isDir)
{
    Q_ASSERT(entry.count() == 0); // by contract :-)

    entry.reserve(9);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, ftpEnt.size);
    entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, ftpEnt.date.toSecsSinceEpoch());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, ftpEnt.access);
    entry.fastInsert(KIO::UDSEntry::UDS_USER, ftpEnt.owner);
    if (!ftpEnt.group.isEmpty()) {
        entry.fastInsert(KIO::UDSEntry::UDS_GROUP, ftpEnt.group);
    }

    if (!ftpEnt.link.isEmpty()) {
        entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, ftpEnt.link);

        QMimeDatabase db;
        QMimeType mime = db.mimeTypeForUrl(QUrl(QLatin1String("ftp://host/") + filename));
        // Links on ftp sites are often links to dirs, and we have no way to check
        // that. Let's do like Netscape : assume dirs generally.
        // But we do this only when the MIME type can't be known from the filename.
        // --> we do better than Netscape :-)
        if (mime.isDefault()) {
            qCDebug(KIO_FTP) << "Setting guessed MIME type to inode/directory for " << filename;
            entry.fastInsert(KIO::UDSEntry::UDS_GUESSED_MIME_TYPE, QStringLiteral("inode/directory"));
            isDir = true;
        }
    }

    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : ftpEnt.type);
    // entry.insert KIO::UDSEntry::UDS_ACCESS_TIME,buff.st_atime);
    // entry.insert KIO::UDSEntry::UDS_CREATION_TIME,buff.st_ctime);
}

void FtpInternal::ftpShortStatAnswer(const QString &filename, bool isDir)
{
    UDSEntry entry;

    entry.reserve(4);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, isDir ? S_IFDIR : S_IFREG);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
    if (isDir) {
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    }
    // No details about size, ownership, group, etc.

    q->statEntry(entry);
}

Result FtpInternal::ftpStatAnswerNotFound(const QString &path, const QString &filename)
{
    // Only do the 'hack' below if we want to download an existing file (i.e. when looking at the "source")
    // When e.g. uploading a file, we still need stat() to return "not found"
    // when the file doesn't exist.
    QString statSide = q->metaData(QStringLiteral("statSide"));
    qCDebug(KIO_FTP) << "statSide=" << statSide;
    if (statSide == QLatin1String("source")) {
        qCDebug(KIO_FTP) << "Not found, but assuming found, because some servers don't allow listing";
        // MS Server is incapable of handling "list <blah>" in a case insensitive way
        // But "retr <blah>" works. So lie in stat(), to get going...
        //
        // There's also the case of ftp://ftp2.3ddownloads.com/90380/linuxgames/loki/patches/ut/ut-patch-436.run
        // where listing permissions are denied, but downloading is still possible.
        ftpShortStatAnswer(filename, false /*file, not dir*/);

        return Result::pass();
    }

    return Result::fail(ERR_DOES_NOT_EXIST, path);
}

Result FtpInternal::stat(const QUrl &url)
{
    qCDebug(KIO_FTP) << "path=" << url.path();
    auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    const QString path = ftpCleanPath(QDir::cleanPath(url.path()));
    qCDebug(KIO_FTP) << "cleaned path=" << path;

    // We can't stat root, but we know it's a dir.
    if (path.isEmpty() || path == QLatin1String("/")) {
        UDSEntry entry;
        entry.reserve(6);
        //entry.insert( KIO::UDSEntry::UDS_NAME, UDSField( QString() ) );
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        entry.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("root"));
        entry.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("root"));
        // no size

        q->statEntry(entry);
        return Result::pass();
    }

    QUrl tempurl(url);
    tempurl.setPath(path);   // take the clean one
    QString listarg; // = tempurl.directory(QUrl::ObeyTrailingSlash);
    QString parentDir;
    const QString filename = tempurl.fileName();
    Q_ASSERT(!filename.isEmpty());

    // Try cwd into it, if it works it's a dir (and then we'll list the parent directory to get more info)
    // if it doesn't work, it's a file (and then we'll use dir filename)
    bool isDir = ftpFolder(path);

    // if we're only interested in "file or directory", we should stop here
    QString sDetails = q->metaData(QStringLiteral("details"));
    int details = sDetails.isEmpty() ? 2 : sDetails.toInt();
    qCDebug(KIO_FTP) << "details=" << details;
    if (details == 0) {
        if (!isDir && !ftpFileExists(path)) { // ok, not a dir -> is it a file ?
            // no -> it doesn't exist at all
            return ftpStatAnswerNotFound(path, filename);
        }
        ftpShortStatAnswer(filename, isDir);
        return Result::pass();   // successfully found a dir or a file -> done
    }

    if (!isDir) {
        // It is a file or it doesn't exist, try going to parent directory
        parentDir = tempurl.adjusted(QUrl::RemoveFilename).path();
        // With files we can do "LIST <filename>" to avoid listing the whole dir
        listarg = filename;
    } else {
        // --- New implementation:
        // Don't list the parent dir. Too slow, might not show it, etc.
        // Just return that it's a dir.
        UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, filename);
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
        // No clue about size, ownership, group, etc.

        q->statEntry(entry);
        return Result::pass();
    }

    // Now cwd the parent dir, to prepare for listing
    if (!ftpFolder(parentDir)) {
        return Result::fail(ERR_CANNOT_ENTER_DIRECTORY, parentDir);
    }

    result = ftpOpenCommand("list", listarg, 'I', ERR_DOES_NOT_EXIST);
    if (!result.success) {
        qCritical() << "COULD NOT LIST";
        return result;
    }
    qCDebug(KIO_FTP) << "Starting of list was ok";

    Q_ASSERT(!filename.isEmpty() && filename != QLatin1String("/"));

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
            bFound = maybeEmitStatEntry(ftpEnt, filename, isDir);
        }
        qCDebug(KIO_FTP) << ftpEnt.name;
    }

    for (int i = 0, count = ftpValidateEntList.count(); i < count; ++i) {
        FtpEntry &ftpEnt = ftpValidateEntList[i];
        fixupEntryName(&ftpEnt);
        if (maybeEmitStatEntry(ftpEnt, filename, isDir)) {
            break;
        }
    }

    ftpCloseCommand();        // closes the data connection only

    if (!bFound) {
        return ftpStatAnswerNotFound(path, filename);
    }

    if (!linkURL.isEmpty()) {
        if (linkURL == url || linkURL == tempurl) {
            return Result::fail(ERR_CYCLIC_LINK, linkURL.toString());
        }
        return FtpInternal::stat(linkURL);
    }

    qCDebug(KIO_FTP) << "stat : finished successfully";;
    return Result::pass();
}

bool FtpInternal::maybeEmitStatEntry(FtpEntry &ftpEnt, const QString &filename, bool isDir)
{
    if (filename == ftpEnt.name && !filename.isEmpty()) {
        UDSEntry entry;
        ftpCreateUDSEntry(filename, ftpEnt, entry, isDir);
        q->statEntry(entry);
        return true;
    }

    return false;
}

Result FtpInternal::listDir(const QUrl &url)
{
    qCDebug(KIO_FTP) << url;
    auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    // No path specified ?
    QString path = url.path();
    if (path.isEmpty()) {
        QUrl realURL;
        realURL.setScheme(QStringLiteral("ftp"));
        realURL.setUserName(m_user);
        realURL.setPassword(m_pass);
        realURL.setHost(m_host);
        if (m_port > 0 && m_port != DEFAULT_FTP_PORT) {
            realURL.setPort(m_port);
        }
        if (m_initialPath.isEmpty()) {
            m_initialPath = QStringLiteral("/");
        }
        realURL.setPath(m_initialPath);
        qCDebug(KIO_FTP) << "REDIRECTION to " << realURL;
        q->redirection(realURL);
        return Result::pass();
    }

    qCDebug(KIO_FTP) << "hunting for path" << path;

    result = ftpOpenDir(path);
    if (!result.success) {
        if (ftpFileExists(path)) {
            return Result::fail(ERR_IS_FILE, path);
        }
        // not sure which to emit
        //error( ERR_DOES_NOT_EXIST, path );
        return Result::fail(ERR_CANNOT_ENTER_DIRECTORY, path);
    }

    UDSEntry entry;
    FtpEntry  ftpEnt;
    QList<FtpEntry> ftpValidateEntList;
    while (ftpReadDir(ftpEnt)) {
        qCDebug(KIO_FTP) << ftpEnt.name;
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
            ftpCreateUDSEntry(ftpEnt.name, ftpEnt, entry, false);
            q->listEntry(entry);
            entry.clear();
        }
    }

    for (int i = 0, count = ftpValidateEntList.count(); i < count; ++i) {
        FtpEntry &ftpEnt = ftpValidateEntList[i];
        fixupEntryName(&ftpEnt);
        ftpCreateUDSEntry(ftpEnt.name, ftpEnt, entry, false);
        q->listEntry(entry);
        entry.clear();
    }

    ftpCloseCommand();        // closes the data connection only
    return Result::pass();
}

void FtpInternal::slave_status()
{
    qCDebug(KIO_FTP) << "Got slave_status host = " << (!m_host.toLatin1().isEmpty() ? m_host.toLatin1() : "[None]") << " [" << (m_bLoggedOn ? "Connected" : "Not connected") << "]";
    q->slaveStatus(m_host, m_bLoggedOn);
}

Result FtpInternal::ftpOpenDir(const QString &path)
{
    //QString path( _url.path(QUrl::RemoveTrailingSlash) );

    // We try to change to this directory first to see whether it really is a directory.
    // (And also to follow symlinks)
    QString tmp = path.isEmpty() ? QStringLiteral("/") : path;

    // We get '550', whether it's a file or doesn't exist...
    if (!ftpFolder(tmp)) {
        return Result::fail();
    }

    // Don't use the path in the list command:
    // We changed into this directory anyway - so it's enough just to send "list".
    // We use '-a' because the application MAY be interested in dot files.
    // The only way to really know would be to have a metadata flag for this...
    // Since some windows ftp server seems not to support the -a argument, we use a fallback here.
    // In fact we have to use -la otherwise -a removes the default -l (e.g. ftp.trolltech.com)
    // Pass KJob::NoError first because we don't want to emit error before we
    // have tried all commands.
    auto result = ftpOpenCommand("list -la", QString(), 'I', KJob::NoError);
    if (!result.success) {
        result = ftpOpenCommand("list", QString(), 'I', KJob::NoError);
    }
    if (!result.success) {
        // Servers running with Turkish locale having problems converting 'i' letter to upper case.
        // So we send correct upper case command as last resort.
        result = ftpOpenCommand("LIST -la", QString(), 'I', ERR_CANNOT_ENTER_DIRECTORY);
    }

    if (!result.success) {
        qCWarning(KIO_FTP) << "Can't open for listing";
        return result;
    }

    qCDebug(KIO_FTP) << "Starting of list was ok";
    return Result::pass();
}

bool FtpInternal::ftpReadDir(FtpEntry &de)
{
    Q_ASSERT(m_data);

    // get a line from the data connection ...
    while (true) {
        while (!m_data->canReadLine() && m_data->waitForReadyRead((q->readTimeout() * 1000))) {}
        QByteArray data = m_data->readLine();
        if (data.size() == 0) {
            break;
        }

        const char *buffer = data.data();
        qCDebug(KIO_FTP) << "dir > " << buffer;

        //Normally the listing looks like
        // -rw-r--r--   1 dfaure   dfaure        102 Nov  9 12:30 log
        // but on Netware servers like ftp://ci-1.ci.pwr.wroc.pl/ it looks like (#76442)
        // d [RWCEAFMS] Admin                     512 Oct 13  2004 PSI

        // we should always get the following 5 fields ...
        const char *p_access, *p_junk, *p_owner, *p_group, *p_size;
        if ((p_access = strtok((char *)buffer, " ")) == nullptr) {
            continue;
        }
        if ((p_junk  = strtok(nullptr, " ")) == nullptr) {
            continue;
        }
        if ((p_owner = strtok(nullptr, " ")) == nullptr) {
            continue;
        }
        if ((p_group = strtok(nullptr, " ")) == nullptr) {
            continue;
        }
        if ((p_size  = strtok(nullptr, " ")) == nullptr) {
            continue;
        }

        qCDebug(KIO_FTP) << "p_access=" << p_access << " p_junk=" << p_junk << " p_owner=" << p_owner << " p_group=" << p_group << " p_size=" << p_size;

        de.access = 0;
        if (qstrlen(p_access) == 1 && p_junk[0] == '[') {     // Netware
            de.access = S_IRWXU | S_IRWXG | S_IRWXO; // unknown -> give all permissions
        }

        const char *p_date_1, *p_date_2, *p_date_3, *p_name;

        // A special hack for "/dev". A listing may look like this:
        // crw-rw-rw-   1 root     root       1,   5 Jun 29  1997 zero
        // So we just ignore the number in front of the ",". Ok, it is a hack :-)
        if (strchr(p_size, ',') != nullptr) {
            qCDebug(KIO_FTP) << "Size contains a ',' -> reading size again (/dev hack)";
            if ((p_size = strtok(nullptr, " ")) == nullptr) {
                continue;
            }
        }

        // This is needed for ftp servers with a directory listing like this (#375610):
        // drwxr-xr-x               folder        0 Mar 15 15:50 directory_name
        if (strcmp(p_junk, "folder") == 0) {
            p_date_1 = p_group;
            p_date_2 = p_size;
            p_size = p_owner;
            p_group = nullptr;
            p_owner = nullptr;
        }
        // Check whether the size we just read was really the size
        // or a month (this happens when the server lists no group)
        // Used to be the case on sunsite.uio.no, but not anymore
        // This is needed for the Netware case, too.
        else if (!isdigit(*p_size)) {
            p_date_1 = p_size;
            p_date_2 = strtok(nullptr, " ");
            p_size = p_group;
            p_group = nullptr;
            qCDebug(KIO_FTP) << "Size didn't have a digit -> size=" << p_size << " date_1=" << p_date_1;
        } else {
            p_date_1 = strtok(nullptr, " ");
            p_date_2 = strtok(nullptr, " ");
            qCDebug(KIO_FTP) << "Size has a digit -> ok. p_date_1=" << p_date_1;
        }

        if (p_date_1 != nullptr &&
                p_date_2 != nullptr &&
                (p_date_3 = strtok(nullptr, " ")) != nullptr &&
                (p_name = strtok(nullptr, "\r\n")) != nullptr) {
            {
                QByteArray tmp(p_name);
                if (p_access[0] == 'l') {
                    int i = tmp.lastIndexOf(" -> ");
                    if (i != -1) {
                        de.link = q->remoteEncoding()->decode(p_name + i + 4);
                        tmp.truncate(i);
                    } else {
                        de.link.clear();
                    }
                } else {
                    de.link.clear();
                }

                if (tmp.startsWith('/')) { // listing on ftp://ftp.gnupg.org/ starts with '/'
                    tmp.remove(0, 1);
                }

                if (tmp.indexOf('/') != -1) {
                    continue;    // Don't trick us!
                }

                de.name     = q->remoteEncoding()->decode(tmp);
            }

            de.type = S_IFREG;
            switch (p_access[0]) {
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

            if (p_access[1] == 'r') {
                de.access |= S_IRUSR;
            }
            if (p_access[2] == 'w') {
                de.access |= S_IWUSR;
            }
            if (p_access[3] == 'x' || p_access[3] == 's') {
                de.access |= S_IXUSR;
            }
            if (p_access[4] == 'r') {
                de.access |= S_IRGRP;
            }
            if (p_access[5] == 'w') {
                de.access |= S_IWGRP;
            }
            if (p_access[6] == 'x' || p_access[6] == 's') {
                de.access |= S_IXGRP;
            }
            if (p_access[7] == 'r') {
                de.access |= S_IROTH;
            }
            if (p_access[8] == 'w') {
                de.access |= S_IWOTH;
            }
            if (p_access[9] == 'x' || p_access[9] == 't') {
                de.access |= S_IXOTH;
            }
            if (p_access[3] == 's' || p_access[3] == 'S') {
                de.access |= S_ISUID;
            }
            if (p_access[6] == 's' || p_access[6] == 'S') {
                de.access |= S_ISGID;
            }
            if (p_access[9] == 't' || p_access[9] == 'T') {
                de.access |= S_ISVTX;
            }

            de.owner    = q->remoteEncoding()->decode(p_owner);
            de.group    = q->remoteEncoding()->decode(p_group);
            de.size     = charToLongLong(p_size);

            // Parsing the date is somewhat tricky
            // Examples : "Oct  6 22:49", "May 13  1999"

            // First get current date - we need the current month and year
            QDate currentDate(QDate::currentDate());
            int currentMonth = currentDate.month();
            int day = currentDate.day();
            int month = currentDate.month();
            int year = currentDate.year();
            int minute = 0;
            int hour = 0;
            // Get day number (always second field)
            if (p_date_2) {
                day = atoi(p_date_2);
            }
            // Get month from first field
            // NOTE : no, we don't want to use KLocale here
            // It seems all FTP servers use the English way
            qCDebug(KIO_FTP) << "Looking for month " << p_date_1;
            static const char s_months[][4] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                                                    };
            for (int c = 0; c < 12; c ++)
                if (!qstrcmp(p_date_1, s_months[c])) {
                    qCDebug(KIO_FTP) << "Found month " << c << " for " << p_date_1;
                    month = c + 1;
                    break;
                }

            // Parse third field
            if (p_date_3 && !strchr(p_date_3, ':')) { // No colon, looks like a year
                year = atoi(p_date_3);
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
                char *semicolon;
                if (p_date_3 && (semicolon = (char *)strchr(p_date_3, ':'))) {
                    *semicolon = '\0';
                    minute = atoi(semicolon + 1);
                    hour = atoi(p_date_3);
                } else {
                    qCWarning(KIO_FTP) << "Can't parse third field " << p_date_3;
                }
            }

            de.date = QDateTime(QDate(year, month, day), QTime(hour, minute));
            qCDebug(KIO_FTP) << de.date;
            return true;
        }
    } // line invalid, loop to get another line
    return false;
}

//===============================================================================
// public: get           download file from server
// helper: ftpGet        called from get() and copy()
//===============================================================================
Result FtpInternal::get(const QUrl &url)
{
    qCDebug(KIO_FTP) << url;
    const Result result = ftpGet(-1, QString(), url, 0);
    ftpCloseCommand();                        // must close command!
    return result;
}

Result FtpInternal::ftpGet(int iCopyFile, const QString &sCopyFile, const QUrl &url, KIO::fileoffset_t llOffset)
{
    auto result = ftpOpenConnection(LoginMode::Implicit);
    if (!result.success) {
        return result;
    }

    // Try to find the size of the file (and check that it exists at
    // the same time). If we get back a 550, "File does not exist"
    // or "not a plain file", check if it is a directory. If it is a
    // directory, return an error; otherwise simply try to retrieve
    // the request...
    if (!ftpSize(url.path(), '?') && (m_iRespCode == 550) &&
            ftpFolder(url.path())) {
        // Ok it's a dir in fact
        qCDebug(KIO_FTP) << "it is a directory in fact";
        return Result::fail(ERR_IS_DIRECTORY);
    }

    QString resumeOffset = q->metaData(QStringLiteral("range-start"));
    if (resumeOffset.isEmpty()) {
        resumeOffset = q->metaData(QStringLiteral("resume")); // old name
    }
    if (!resumeOffset.isEmpty()) {
        llOffset = resumeOffset.toLongLong();
        qCDebug(KIO_FTP) << "got offset from metadata : " << llOffset;
    }

    result = ftpOpenCommand("retr", url.path(), '?', ERR_CANNOT_OPEN_FOR_READING, llOffset);
    if (!result.success) {
        qCWarning(KIO_FTP) << "Can't open for reading";
        return result;
    }

    // Read the size from the response string
    if (m_size == UnknownSize) {
        const char *psz = strrchr(ftpResponse(4), '(');
        if (psz) {
            m_size = charToLongLong(psz + 1);
        }
        if (!m_size) {
            m_size = UnknownSize;
        }
    }

    // Send the MIME type...
    if (iCopyFile == -1) {
        const auto result = ftpSendMimeType(url);
        if (!result.success) {
            return result;
        }
    }

    KIO::filesize_t bytesLeft = 0;
    if (m_size != UnknownSize) {
        bytesLeft = m_size - llOffset;
        q->totalSize(m_size);    // emit the total size...
    }

    qCDebug(KIO_FTP) << "starting with offset=" << llOffset;
    KIO::fileoffset_t processed_size = llOffset;

    QByteArray array;
    char buffer[maximumIpcSize];
    // start with small data chunks in case of a slow data source (modem)
    // - unfortunately this has a negative impact on performance for large
    // - files - so we will increase the block size after a while ...
    int iBlockSize = initialIpcSize;
    int iBufferCur = 0;

    while (m_size == UnknownSize || bytesLeft > 0) {
        // let the buffer size grow if the file is larger 64kByte ...
        if (processed_size - llOffset > 1024 * 64) {
            iBlockSize = maximumIpcSize;
        }

        // read the data and detect EOF or error ...
        if (iBlockSize + iBufferCur > (int)sizeof(buffer)) {
            iBlockSize = sizeof(buffer) - iBufferCur;
        }
        if (m_data->bytesAvailable() == 0) {
            m_data->waitForReadyRead((q->readTimeout() * 1000));
        }
        int n = m_data->read(buffer + iBufferCur, iBlockSize);
        if (n <= 0) {
            // this is how we detect EOF in case of unknown size
            if (m_size == UnknownSize && n == 0) {
                break;
            }
            // unexpected eof. Happens when the daemon gets killed.
            return Result::fail(ERR_CANNOT_READ);
        }
        processed_size += n;

        // collect very small data chunks in buffer before processing ...
        if (m_size != UnknownSize) {
            bytesLeft -= n;
            iBufferCur += n;
            if (iBufferCur < minimumMimeSize && bytesLeft > 0) {
                q->processedSize(processed_size);
                continue;
            }
            n = iBufferCur;
            iBufferCur = 0;
        }

        // write output file or pass to data pump ...
        int writeError = 0;
        if (iCopyFile == -1) {
            array = QByteArray::fromRawData(buffer, n);
            q->data(array);
            array.clear();
        } else if ((writeError = WriteToFile(iCopyFile, buffer, n)) != 0) {
            return Result::fail(writeError, sCopyFile);
        }

        Q_ASSERT(processed_size >= 0);
        q->processedSize(static_cast<KIO::filesize_t>(processed_size));
    }

    qCDebug(KIO_FTP) << "done";
    if (iCopyFile == -1) {       // must signal EOF to data pump ...
        q->data(array);    // array is empty and must be empty!
    }

    q->processedSize(m_size == UnknownSize ? processed_size : m_size);
    return Result::pass();
}

//===============================================================================
// public: put           upload file to server
// helper: ftpPut        called from put() and copy()
//===============================================================================
Result FtpInternal::put(const QUrl &url, int permissions, KIO::JobFlags flags)
{
    qCDebug(KIO_FTP) << url;
    const auto result = ftpPut(-1, url, permissions, flags);
    ftpCloseCommand();                        // must close command!
    return result;
}

Result FtpInternal::ftpPut(int iCopyFile, const QUrl &dest_url,
                           int permissions, KIO::JobFlags flags)
{
    const auto openResult = ftpOpenConnection(LoginMode::Implicit);
    if (!openResult.success) {
        return openResult;
    }

    // Don't use mark partial over anonymous FTP.
    // My incoming dir allows put but not rename...
    bool bMarkPartial;
    if (m_user.isEmpty() || m_user == QLatin1String(FTP_LOGIN)) {
        bMarkPartial = false;
    } else {
        bMarkPartial = q->configValue(QStringLiteral("MarkPartial"), true);
    }

    QString dest_orig = dest_url.path();
    const QString dest_part = dest_orig + QLatin1String(".part");

    if (ftpSize(dest_orig, 'I')) {
        if (m_size == 0) {
            // delete files with zero size
            const QByteArray cmd = "DELE " + q->remoteEncoding()->encode(dest_orig);
            if (!ftpSendCmd(cmd) || (m_iRespType != 2)) {
                return Result::fail(ERR_CANNOT_DELETE_PARTIAL, QString());
            }
        } else if (!(flags & KIO::Overwrite) && !(flags & KIO::Resume)) {
            return Result::fail(ERR_FILE_ALREADY_EXIST, QString());
        } else if (bMarkPartial) {
            // when using mark partial, append .part extension
            const auto result = ftpRename(dest_orig, dest_part, KIO::Overwrite);
            if (!result.success) {
                return Result::fail(ERR_CANNOT_RENAME_PARTIAL, QString());
            }
        }
        // Don't chmod an existing file
        permissions = -1;
    } else if (bMarkPartial && ftpSize(dest_part, 'I')) {
        // file with extension .part exists
        if (m_size == 0) {
            // delete files with zero size
            const QByteArray cmd = "DELE " + q->remoteEncoding()->encode(dest_part);
            if (!ftpSendCmd(cmd) || (m_iRespType != 2)) {
                return Result::fail(ERR_CANNOT_DELETE_PARTIAL, QString());
            }
        } else if (!(flags & KIO::Overwrite) && !(flags & KIO::Resume)) {
            flags |= q->canResume(m_size) ? KIO::Resume : KIO::DefaultFlags;
            if (!(flags & KIO::Resume)) {
                return Result::fail(ERR_FILE_ALREADY_EXIST, QString());
            }
        }
    } else {
        m_size = 0;
    }

    QString dest;

    // if we are using marking of partial downloads -> add .part extension
    if (bMarkPartial) {
        qCDebug(KIO_FTP) << "Adding .part extension to " << dest_orig;
        dest = dest_part;
    } else {
        dest = dest_orig;
    }

    KIO::fileoffset_t offset = 0;

    // set the mode according to offset
    if ((flags & KIO::Resume) && m_size > 0) {
        offset = m_size;
        if (iCopyFile != -1) {
            if (QT_LSEEK(iCopyFile, offset, SEEK_SET) < 0) {
                return Result::fail(ERR_CANNOT_RESUME, QString());
            }
        }
    }

    const auto storResult = ftpOpenCommand("stor", dest, '?', ERR_CANNOT_WRITE, offset);
    if (!storResult.success) {
        return storResult;
    }

    qCDebug(KIO_FTP) << "ftpPut: starting with offset=" << offset;
    KIO::fileoffset_t processed_size = offset;

    QByteArray buffer;
    int result;
    int iBlockSize = initialIpcSize;
    int writeError = 0;
    // Loop until we got 'dataEnd'
    do {
        if (iCopyFile == -1) {
            q->dataReq(); // Request for data
            result = q->readData(buffer);
        } else {
            // let the buffer size grow if the file is larger 64kByte ...
            if (processed_size - offset > 1024 * 64) {
                iBlockSize = maximumIpcSize;
            }
            buffer.resize(iBlockSize);
            result = QT_READ(iCopyFile, buffer.data(), buffer.size());
            if (result < 0) {
                writeError = ERR_CANNOT_READ;
            } else {
                buffer.resize(result);
            }
        }

        if (result > 0) {
            m_data->write(buffer);
            while (m_data->bytesToWrite() && m_data->waitForBytesWritten()) {}
            processed_size += result;
            q->processedSize(processed_size);
        }
    } while (result > 0);

    if (result != 0) { // error
        ftpCloseCommand();               // don't care about errors
        qCDebug(KIO_FTP) << "Error during 'put'. Aborting.";
        if (bMarkPartial) {
            // Remove if smaller than minimum size
            if (ftpSize(dest, 'I') &&
                    (processed_size < q->configValue(QStringLiteral("MinimumKeepSize"), DEFAULT_MINIMUM_KEEP_SIZE))) {
                const QByteArray cmd = "DELE " + q->remoteEncoding()->encode(dest);
                (void) ftpSendCmd(cmd);
            }
        }
        return Result::fail(writeError, dest_url.toString());
    }

    if (!ftpCloseCommand()) {
        return Result::fail(ERR_CANNOT_WRITE);
    }

    // after full download rename the file back to original name
    if (bMarkPartial) {
        qCDebug(KIO_FTP) << "renaming dest (" << dest << ") back to dest_orig (" << dest_orig << ")";
        const auto result = ftpRename(dest, dest_orig, KIO::Overwrite);
        if (!result.success) {
            return Result::fail(ERR_CANNOT_RENAME_PARTIAL);
        }
    }

    // set final permissions
    if (permissions != -1) {
        if (m_user == QLatin1String(FTP_LOGIN))
            qCDebug(KIO_FTP) << "Trying to chmod over anonymous FTP ???";
            // chmod the file we just put
            if (! ftpChmod(dest_orig, permissions)) {
                // To be tested
                //if ( m_user != FTP_LOGIN )
                //    warning( i18n( "Could not change permissions for\n%1" ).arg( dest_orig ) );
            }
    }

    return Result::pass();
}

/** Use the SIZE command to get the file size.
    Warning : the size depends on the transfer mode, hence the second arg. */
bool FtpInternal::ftpSize(const QString &path, char mode)
{
    m_size = UnknownSize;
    if (!ftpDataMode(mode)) {
        return false;
    }

    const QByteArray buf = "SIZE " + q->remoteEncoding()->encode(path);
    if (!ftpSendCmd(buf) || (m_iRespType != 2)) {
        return false;
    }

    // skip leading "213 " (response code)
    QByteArray psz(ftpResponse(4));
    if (psz.isEmpty()) {
        return false;
    }
    bool ok = false;
    m_size = psz.trimmed().toLongLong(&ok);
    if (!ok) {
        m_size = UnknownSize;
    }
    return true;
}

bool FtpInternal::ftpFileExists(const QString &path)
{
    const QByteArray buf = "SIZE " + q->remoteEncoding()->encode(path);
    if (!ftpSendCmd(buf) || (m_iRespType != 2)) {
        return false;
    }

    // skip leading "213 " (response code)
    const char *psz = ftpResponse(4);
    return psz != nullptr;
}

// Today the differences between ASCII and BINARY are limited to
// CR or CR/LF line terminators. Many servers ignore ASCII (like
// win2003 -or- vsftp with default config). In the early days of
// computing, when even text-files had structure, this stuff was
// more important.
// Theoretically "list" could return different results in ASCII
// and BINARY mode. But again, most servers ignore ASCII here.
bool FtpInternal::ftpDataMode(char cMode)
{
    if (cMode == '?') {
        cMode = m_bTextMode ? 'A' : 'I';
    } else if (cMode == 'a') {
        cMode = 'A';
    } else if (cMode != 'A') {
        cMode = 'I';
    }

    qCDebug(KIO_FTP) << "want" << cMode << "has" << m_cDataMode;
    if (m_cDataMode == cMode) {
        return true;
    }

    const QByteArray buf = QByteArrayLiteral("TYPE ") + cMode;
    if (!ftpSendCmd(buf) || (m_iRespType != 2)) {
        return false;
    }
    m_cDataMode = cMode;
    return true;
}

bool FtpInternal::ftpFolder(const QString &path)
{
    QString newPath = path;
    int iLen = newPath.length();
    if (iLen > 1 && newPath[iLen - 1] == QLatin1Char('/')) {
        newPath.chop(1);
    }

    qCDebug(KIO_FTP) << "want" << newPath << "has" << m_currentPath;
    if (m_currentPath == newPath) {
        return true;
    }

    const QByteArray tmp = "cwd " + q->remoteEncoding()->encode(newPath);
    if (!ftpSendCmd(tmp)) {
        return false;    // connection failure
    }
    if (m_iRespType != 2) {
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
Result FtpInternal::copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags)
{
    int iCopyFile = -1;
    bool bSrcLocal = src.isLocalFile();
    bool bDestLocal = dest.isLocalFile();
    QString  sCopyFile;

    Result result = Result::pass();
    if (bSrcLocal && !bDestLocal) {                 // File -> Ftp
        sCopyFile = src.toLocalFile();
        qCDebug(KIO_FTP) << "local file" << sCopyFile << "-> ftp" << dest.path();
        result = ftpCopyPut(iCopyFile, sCopyFile, dest, permissions, flags);
    } else if (!bSrcLocal && bDestLocal) {          // Ftp -> File
        sCopyFile = dest.toLocalFile();
        qCDebug(KIO_FTP) << "ftp" << src.path() << "-> local file" << sCopyFile;
        result = ftpCopyGet(iCopyFile, sCopyFile, src, permissions, flags);
    } else {
        return Result::fail(ERR_UNSUPPORTED_ACTION, QString());
    }

    // perform clean-ups and report error (if any)
    if (iCopyFile != -1) {
        QT_CLOSE(iCopyFile);
    }
    ftpCloseCommand();                        // must close command!

    return result;
}

bool FtpInternal::isSocksProxyScheme(const QString &scheme)
{
    return scheme == QLatin1String("socks") || scheme == QLatin1String("socks5");
}

bool FtpInternal::isSocksProxy() const
{
    return isSocksProxyScheme(m_proxyURL.scheme());
}

Result FtpInternal::ftpCopyPut(int &iCopyFile, const QString &sCopyFile,
                               const QUrl &url, int permissions, KIO::JobFlags flags)
{
    // check if source is ok ...
    QFileInfo info(sCopyFile);
    bool bSrcExists = info.exists();
    if (bSrcExists) {
        if (info.isDir()) {
            return Result::fail(ERR_IS_DIRECTORY);
        }
    } else {
        return Result::fail(ERR_DOES_NOT_EXIST);
    }

    iCopyFile = QT_OPEN(QFile::encodeName(sCopyFile).constData(), O_RDONLY);
    if (iCopyFile == -1) {
        return Result::fail(ERR_CANNOT_OPEN_FOR_READING);
    }

    // delegate the real work (iError gets status) ...
    q->totalSize(info.size());
#ifdef  ENABLE_CAN_RESUME
    return ftpPut(iCopyFile, url, permissions, flags & ~KIO::Resume);
#else
    return ftpPut(iCopyFile, url, permissions, flags | KIO::Resume);
#endif
}

Result FtpInternal::ftpCopyGet(int &iCopyFile, const QString &sCopyFile,
                               const QUrl &url, int permissions, KIO::JobFlags flags)
{
    // check if destination is ok ...
    QFileInfo info(sCopyFile);
    const bool bDestExists = info.exists();
    if (bDestExists) {
        if (info.isDir()) {
            return Result::fail(ERR_IS_DIRECTORY);
        }
        if (!(flags & KIO::Overwrite)) {
            return Result::fail(ERR_FILE_ALREADY_EXIST);
        }
    }

    // do we have a ".part" file?
    const QString sPart = sCopyFile + QLatin1String(".part");
    bool bResume = false;
    QFileInfo sPartInfo(sPart);
    const bool bPartExists = sPartInfo.exists();
    const bool bMarkPartial = q->configValue(QStringLiteral("MarkPartial"), true);
    const QString dest = bMarkPartial ? sPart : sCopyFile;
    if (bMarkPartial && bPartExists && sPartInfo.size() > 0) {
        // must not be a folder! please fix a similar bug in kio_file!!
        if (sPartInfo.isDir()) {
            return Result::fail(ERR_DIR_ALREADY_EXIST);
        }
        //doesn't work for copy? -> design flaw?
#ifdef  ENABLE_CAN_RESUME
        bResume = q->canResume(sPartInfo.size());
#else
        bResume = true;
#endif
    }

    if (bPartExists && !bResume) {                // get rid of an unwanted ".part" file
        QFile::remove(sPart);
    }

    // WABA: Make sure that we keep writing permissions ourselves,
    // otherwise we can be in for a surprise on NFS.
    mode_t initialMode;
    if (permissions >= 0) {
        initialMode = static_cast<mode_t>(permissions | S_IWUSR);
    } else {
        initialMode = 0666;
    }

    // open the output file ...
    KIO::fileoffset_t hCopyOffset = 0;
    if (bResume) {
        iCopyFile = QT_OPEN(QFile::encodeName(sPart).constData(), O_RDWR);  // append if resuming
        hCopyOffset = QT_LSEEK(iCopyFile, 0, SEEK_END);
        if (hCopyOffset < 0) {
            return Result::fail(ERR_CANNOT_RESUME);
        }
        qCDebug(KIO_FTP) << "resuming at " << hCopyOffset;
    } else {
        iCopyFile = QT_OPEN(QFile::encodeName(dest).constData(), O_CREAT | O_TRUNC | O_WRONLY, initialMode);
    }

    if (iCopyFile == -1) {
        qCDebug(KIO_FTP) << "### COULD NOT WRITE " << sCopyFile;
        const int error = (errno == EACCES) ? ERR_WRITE_ACCESS_DENIED
                                            : ERR_CANNOT_OPEN_FOR_WRITING;
        return Result::fail(error);
    }

    // delegate the real work (iError gets status) ...
    auto result = ftpGet(iCopyFile, sCopyFile, url, hCopyOffset);

    if (QT_CLOSE(iCopyFile) == 0 && !result.success) {
        // If closing the file failed but there isn't an error yet, switch
        // into an error!
        result = Result::fail(ERR_CANNOT_WRITE);
    }
    iCopyFile = -1;

    // handle renaming or deletion of a partial file ...
    if (bMarkPartial) {
        if (result.success) {
            // rename ".part" on success
            if (!QFile::rename(sPart, sCopyFile)) {
                // If rename fails, try removing the destination first if it exists.
                if (!bDestExists || !(QFile::remove(sCopyFile) && QFile::rename(sPart, sCopyFile))) {
                    qCDebug(KIO_FTP) << "cannot rename " << sPart << " to " << sCopyFile;
                    result = Result::fail(ERR_CANNOT_RENAME_PARTIAL);
                }
            }
        } else {
            sPartInfo.refresh();
            if (sPartInfo.exists()) { // should a very small ".part" be deleted?
                int size = q->configValue(QStringLiteral("MinimumKeepSize"), DEFAULT_MINIMUM_KEEP_SIZE);
                if (sPartInfo.size() < size) {
                    QFile::remove(sPart);
                }
            }
        }
    }

    if (result.success) {
        const QString mtimeStr = q->metaData(QStringLiteral("modified"));
        if (!mtimeStr.isEmpty()) {
            QDateTime dt = QDateTime::fromString(mtimeStr, Qt::ISODate);
            if (dt.isValid()) {
                qCDebug(KIO_FTP) << "Updating modified timestamp to" << mtimeStr;
                struct utimbuf utbuf;
                info.refresh();
                utbuf.actime = info.lastRead().toSecsSinceEpoch(); // access time, unchanged
                utbuf.modtime = dt.toSecsSinceEpoch(); // modification time
                ::utime(QFile::encodeName(sCopyFile).constData(), &utbuf);
            }
        }
    }

    return result;
}

Result FtpInternal::ftpSendMimeType(const QUrl &url)
{
    const int totalSize = ((m_size == UnknownSize || m_size > 1024) ? 1024 : static_cast<int>(m_size));
    QByteArray buffer(totalSize, '\0');

    while (true) {
        // Wait for content to be available...
        if (m_data->bytesAvailable() == 0 && !m_data->waitForReadyRead((q->readTimeout() * 1000))) {
            return Result::fail(ERR_CANNOT_READ, url.toString());
        }

        const qint64 bytesRead = m_data->peek(buffer.data(), totalSize);

        // If we got a -1, it must be an error so return an error.
        if (bytesRead == -1) {
            return Result::fail(ERR_CANNOT_READ, url.toString());
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
        qCDebug(KIO_FTP) << "Emitting MIME type" << mime.name();
        q->mimeType(mime.name()); // emit the MIME type...
    }

    return Result::pass();
}

void FtpInternal::fixupEntryName(FtpEntry *e)
{
    Q_ASSERT(e);
    if (e->type == S_IFDIR) {
        if (!ftpFolder(e->name)) {
            QString name(e->name.trimmed());
            if (ftpFolder(name)) {
                e->name = name;
                qCDebug(KIO_FTP) << "fixing up directory name from" << e->name << "to" << name;
            } else {
                int index = 0;
                while (e->name.at(index).isSpace()) {
                    index++;
                    name = e->name.mid(index);
                    if (ftpFolder(name)) {
                        qCDebug(KIO_FTP) << "fixing up directory name from" << e->name << "to" << name;
                        e->name = name;
                        break;
                    }
                }
            }
        }
    } else {
        if (!ftpFileExists(e->name)) {
            QString name(e->name.trimmed());
            if (ftpFileExists(name)) {
                e->name = name;
                qCDebug(KIO_FTP) << "fixing up filename from" << e->name << "to" << name;
            } else {
                int index = 0;
                while (e->name.at(index).isSpace()) {
                    index++;
                    name = e->name.mid(index);
                    if (ftpFileExists(name)) {
                        qCDebug(KIO_FTP) << "fixing up filename from" << e->name << "to" << name;
                        e->name = name;
                        break;
                    }
                }
            }
        }
    }
}

ConnectionResult FtpInternal::synchronousConnectToHost(const QString &host, quint16 port)
{
    const QUrl proxyUrl = m_proxyURL;
    QNetworkProxy proxy;
    if (!proxyUrl.isEmpty()) {
        proxy = QNetworkProxy(QNetworkProxy::Socks5Proxy,
                              proxyUrl.host(),
                              static_cast<quint16>(proxyUrl.port(0)),
                              proxyUrl.userName(),
                              proxyUrl.password());
    }

    QTcpSocket *socket = new QSslSocket;
    socket->setProxy(proxy);
    socket->connectToHost(host, port);
    socket->waitForConnected(q->connectTimeout() * 1000);
    const auto socketError = socket->error();
     if (socketError == QAbstractSocket::ProxyAuthenticationRequiredError) {
        AuthInfo info;
        info.url = proxyUrl;
        info.verifyPath = true;    //### whatever

        if (!q->checkCachedAuthentication(info)) {
            info.prompt = i18n("You need to supply a username and a password for "
                               "the proxy server listed below before you are allowed "
                               "to access any sites.");
            info.keepPassword = true;
            info.commentLabel = i18n("Proxy:");
            info.comment = i18n("<b>%1</b>", proxy.hostName());

            const int errorCode = q->openPasswordDialogV2(info, i18n("Proxy Authentication Failed."));
            if (errorCode != KJob::NoError) {
                qCDebug(KIO_FTP) << "user canceled proxy authentication, or communication error." << errorCode;
                return ConnectionResult { socket,
                            Result::fail(errorCode, proxyUrl.toString()) };
            }
        }

        proxy.setUser(info.username);
        proxy.setPassword(info.password);

        delete socket;
        socket = new QSslSocket;
        socket->setProxy(proxy);
        socket->connectToHost(host, port);
        socket->waitForConnected(q->connectTimeout() * 1000);

        if (socket->state() == QAbstractSocket::ConnectedState) {
            // reconnect with credentials was successful -> save data
            q->cacheAuthentication(info);

            m_proxyURL.setUserName(info.username);
            m_proxyURL.setPassword(info.password);
        }
    }

    return ConnectionResult { socket, Result::pass() };
}

//===============================================================================
// Ftp
//===============================================================================

Ftp::Ftp(const QByteArray &pool, const QByteArray &app)
    : SlaveBase(QByteArrayLiteral("ftp"), pool, app)
    , d(new FtpInternal(this))
{
}

Ftp::~Ftp() = default;

void Ftp::setHost(const QString &host, quint16 port, const QString &user, const QString &pass)
{
    d->setHost(host, port, user, pass);
}

void Ftp::openConnection()
{
    const auto result = d->openConnection();
    if (!result.success) {
        error(result.error, result.errorString);
        return;
    }
    opened();
}

void Ftp::closeConnection()
{
    d->closeConnection();
}

void Ftp::stat(const QUrl &url)
{
    finalize(d->stat(url));
}

void Ftp::listDir(const QUrl &url)
{
    finalize(d->listDir(url));
}

void Ftp::mkdir(const QUrl &url, int permissions)
{
    finalize(d->mkdir(url, permissions));
}

void Ftp::rename(const QUrl &src, const QUrl &dst, JobFlags flags)
{
    finalize(d->rename(src, dst, flags));
}

void Ftp::del(const QUrl &url, bool isfile)
{
    finalize(d->del(url, isfile));
}

void Ftp::chmod(const QUrl &url, int permissions)
{
    finalize(d->chmod(url, permissions));
}

void Ftp::get(const QUrl &url)
{
    finalize(d->get(url));
}

void Ftp::put(const QUrl &url, int permissions, JobFlags flags)
{
    finalize(d->put(url, permissions, flags));
}

void Ftp::slave_status()
{
    d->slave_status();
}

void Ftp::copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags)
{
    finalize(d->copy(src, dest, permissions, flags));
}

void Ftp::finalize(const Result &result)
{
    if (!result.success) {
        error(result.error, result.errorString);
        return;
    }
    finished();
}

QDebug operator<<(QDebug dbg, const Result &r)

{
    QDebugStateSaver saver(dbg);
    dbg.nospace() << "Result("
                  << "success=" << r.success
                  << ", err=" << r.error
                  << ", str=" << r.errorString
                  << ')';
    return dbg;
}

// needed for JSON file embedding
#include "ftp.moc"
