/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2019 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KDELIBS_FTP_H
#define KDELIBS_FTP_H

#include <qplatformdefs.h>

#include <QDateTime>

#include <kio/slavebase.h>

class QTcpServer;
class QTcpSocket;
class QNetworkProxy;
class QAuthenticator;

struct FtpEntry {
    QString name;
    QString owner;
    QString group;
    QString link;

    KIO::filesize_t size;
    mode_t type;
    mode_t access;
    QDateTime date;
};

class FtpInternal;

/**
 * Login Mode for ftpOpenConnection
 */
enum class LoginMode {
    Deferred,
    Explicit,
    Implicit,
};

/**
 * Result type for returning error context.
 *
 * This is meant to be returned by functions that do not have a simple
 * error conditions that could be represented by returning a bool, or
 * when the contextual error string can only be correctly constructed
 * inside the function. When using the Result type always mark the
 * function Q_REQUIRED_RESULT to enforce handling of the Result.
 *
 * The Result is forwared all the way to the frontend API where it is
 * turned into an error() or finished() call.
 */
struct Result
{
    bool success;
    int error;
    QString errorString;

    inline static Result fail(int _error = KIO::ERR_UNKNOWN,
                              const QString &_errorString = QString())
    {
        return Result { false, _error, _errorString };
    }

    inline static Result pass()
    {
        return Result { true, 0, QString() };
    }
};

/**
 * Special Result composite for errors during connection.
 */
struct ConnectionResult
{
    QTcpSocket *socket;
    Result result;
};

QDebug operator<<(QDebug dbg, const Result &r);

//===============================================================================
// Ftp
// The API class. This class should not contain *any* FTP logic. It acts
// as a container for FtpInternal to prevent the latter from directly doing
// state manipulation via error/finished/opened etc.
//===============================================================================
class Ftp : public KIO::SlaveBase
{
public:
    Ftp(const QByteArray &pool, const QByteArray &app);
    ~Ftp() override;

    void setHost(const QString &host, quint16 port, const QString &user, const QString &pass) override;

    /**
     * Connects to a ftp server and logs us in
     * m_bLoggedOn is set to true if logging on was successful.
     * It is set to false if the connection becomes closed.
     *
     */
    void openConnection() override;

    /**
     * Closes the connection
     */
    void closeConnection() override;

    void stat(const QUrl &url) override;

    void listDir(const QUrl &url) override;
    void mkdir(const QUrl &url, int permissions) override;
    void rename(const QUrl &src, const QUrl &dst, KIO::JobFlags flags) override;
    void del(const QUrl &url, bool isfile) override;
    void chmod(const QUrl &url, int permissions) override;

    void get(const QUrl &url) override;
    void put(const QUrl &url, int permissions, KIO::JobFlags flags) override;

    void slave_status() override;

    /**
     * Handles the case that one side of the job is a local file
     */
    void copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags) override;

private:
    // WARNING: All members and all logic not confined to one of the public functions
    //   must go into FtpInternal!

    /**
     * Overridden to prevent FtpInternal from easily calling
     * q->opened(). Use a Result return type on error conditions
     * instead. When there was no error Result the
     * connection is considered opened.
     *
     * FtpInternal must not call any state-changing signals!
     */
    void opened()
    {
        SlaveBase::opened();
    }

    /**
     * @see opened()
     */
    void error(int _errid, const QString &_text)
    {
        SlaveBase::error(_errid, _text);
    }

    /**
     * @see opened()
     */
    void finished()
    {
        SlaveBase::finished();
    }

    /**
     * Calls finished() or error() as appropriate
     */
    void finalize(const Result &result);

    QScopedPointer<FtpInternal> d;
};

/**
 * Internal logic class.
 *
 * This class implements strict separation between the API (Ftp) and
 * the logic behind the API (FtpInternal). This class' functions
 * are meant to return Result objects up the call stack to Ftp where
 * they will be turned into command results (e.g. error(),
 * finished(), etc.). This class cannot and must not call these signals
 * directly as it leads to unclear states.
 */
class FtpInternal : public QObject
{
    Q_OBJECT
public:
    explicit FtpInternal(Ftp *qptr);
    ~FtpInternal();

    // ---------------------------------------- API

    void setHost(const QString &host, quint16 port, const QString &user, const QString &pass);

    /**
     * Connects to a ftp server and logs us in
     * m_bLoggedOn is set to true if logging on was successful.
     * It is set to false if the connection becomes closed.
     *
     */
    Q_REQUIRED_RESULT Result openConnection();

    /**
     * Closes the connection
     */
    void closeConnection();

    Q_REQUIRED_RESULT Result stat(const QUrl &url)    ;

    Result listDir(const QUrl &url);
    Q_REQUIRED_RESULT Result mkdir(const QUrl &url, int permissions);
    Q_REQUIRED_RESULT Result rename(const QUrl &src, const QUrl &dst, KIO::JobFlags flags);
    Q_REQUIRED_RESULT Result del(const QUrl &url, bool isfile);
    Q_REQUIRED_RESULT Result chmod(const QUrl &url, int permissions);

    Q_REQUIRED_RESULT Result get(const QUrl &url);
    Q_REQUIRED_RESULT Result put(const QUrl &url, int permissions, KIO::JobFlags flags);
    //virtual void mimetype( const QUrl& url );

    void slave_status();

    /**
     * Handles the case that one side of the job is a local file
     */
    Q_REQUIRED_RESULT Result copy(const QUrl &src, const QUrl &dest, int permissions, KIO::JobFlags flags);

    // ---------------------------------------- END API

    static bool isSocksProxyScheme(const QString &scheme);
    bool isSocksProxy() const;

    /**
     * Connect and login to the FTP server.
     *
     * @param loginMode controls if login info should be sent<br>
     *  loginDeferred  - must not be logged on, no login info is sent<br>
     *  loginExplicit - must not be logged on, login info is sent<br>
     *  loginImplicit - login info is sent if not logged on
     *
     * @return true on success (a login failure would return false).
     */
    Q_REQUIRED_RESULT Result ftpOpenConnection(LoginMode loginMode);

    /**
     * Executes any auto login macro's as specified in a .netrc file.
     */
    void ftpAutoLoginMacro();

    /**
     * Called by openConnection. It logs us in.
     * m_initialPath is set to the current working directory
     * if logging on was successful.
     *
     * @param userChanged if not nullptr, will be set to true if the user name
     *                    was changed during login.
     * @return true on success.
     */
    Q_REQUIRED_RESULT Result ftpLogin(bool *userChanged = nullptr);

    /**
     * ftpSendCmd - send a command (@p cmd) and read response
     *
     * @param maxretries number of time it should retry. Since it recursively
     * calls itself if it can't read the answer (this happens especially after
     * timeouts), we need to limit the recursiveness ;-)
     *
     * return true if any response received, false on error
     */
    Q_REQUIRED_RESULT bool ftpSendCmd(const QByteArray &cmd, int maxretries = 1);

    /**
     * Use the SIZE command to get the file size.
     * @param mode the size depends on the transfer mode, hence this arg.
     * @return true on success
     * Gets the size into m_size.
     */
    bool ftpSize(const QString &path, char mode);

    /**
     * Returns true if the file exists.
     * Implemented using the SIZE command.
     */
    bool ftpFileExists(const QString &path);

    /**
     * Set the current working directory, but only if not yet current
     */
    Q_REQUIRED_RESULT bool ftpFolder(const QString &path);

    /**
     * Runs a command on the ftp server like "list" or "retr". In contrast to
     * ftpSendCmd a data connection is opened. The corresponding socket
     * sData is available for reading/writing on success.
     * The connection must be closed afterwards with ftpCloseCommand.
     *
     * @param mode is 'A' or 'I'. 'A' means ASCII transfer, 'I' means binary transfer.
     * @param errorcode the command-dependent error code to emit on error
     *
     * @return true if the command was accepted by the server.
     */
    Q_REQUIRED_RESULT Result ftpOpenCommand(const char *command, const QString &path, char mode,
                        int errorcode, KIO::fileoffset_t offset = 0);

    /**
     * The counterpart to openCommand.
     * Closes data sockets and then reads line sent by server at
     * end of command.
     * @return false on error (line doesn't start with '2')
     */
    bool ftpCloseCommand();

    /**
     * Send "TYPE I" or "TYPE A" only if required, see m_cDataMode.
     *
     * Use 'A' to select ASCII and 'I' to select BINARY mode.  If
     * cMode is '?' the m_bTextMode flag is used to choose a mode.
     */
    bool ftpDataMode(char cMode);

    //void ftpAbortTransfer();

    /**
     * Used by ftpOpenCommand, return 0 on success or an error code
     */
    int ftpOpenDataConnection();

    /**
     * closes a data connection, see ftpOpenDataConnection()
     */
    void ftpCloseDataConnection();

    /**
     * Helper for ftpOpenDataConnection
     */
    int ftpOpenPASVDataConnection();
    /**
     * Helper for ftpOpenDataConnection
     */
    int ftpOpenEPSVDataConnection();
    /**
     * Helper for ftpOpenDataConnection
     */
    int ftpOpenPortDataConnection();

    bool ftpChmod(const QString &path, int permissions);

    // used by listDir
    Q_REQUIRED_RESULT Result ftpOpenDir(const QString &path);
    /**
      * Called to parse directory listings, call this until it returns false
      */
    bool ftpReadDir(FtpEntry &ftpEnt);

    /**
      * Helper to fill an UDSEntry
      */
    void ftpCreateUDSEntry(const QString &filename, const FtpEntry &ftpEnt, KIO::UDSEntry &entry, bool isDir);

    void ftpShortStatAnswer(const QString &filename, bool isDir);

    Q_REQUIRED_RESULT Result ftpStatAnswerNotFound(const QString &path, const QString &filename);

    /**
     * This is the internal implementation of rename() - set put().
     *
     * @return true on success.
     */
    Q_REQUIRED_RESULT Result ftpRename(const QString &src, const QString &dst, KIO::JobFlags flags);

    /**
     * Called by openConnection. It opens the control connection to the ftp server.
     *
     * @return true on success.
     */
    Q_REQUIRED_RESULT Result ftpOpenControlConnection();
    Q_REQUIRED_RESULT Result ftpOpenControlConnection(const QString &host, int port);

    /**
     * closes the socket holding the control connection (see ftpOpenControlConnection)
     */
    void ftpCloseControlConnection();

    /**
     * read a response from the server (a trailing CR gets stripped)
     * @param iOffset -1 to read a new line from the server<br>
     *                 0 to return the whole response string
     *                >0 to return the response with iOffset chars skipped
     * @return the response message with iOffset chars skipped (or "" if iOffset points
     *         behind the available data)
     */
    const char *ftpResponse(int iOffset);

    /**
     * This is the internal implementation of get() - see copy().
     *
     * IMPORTANT: the caller should call ftpCloseCommand() on return.
     * The function does not call error(), the caller should do this.
     *
     * @param iError      set to an ERR_xxxx code on error
     * @param iCopyFile   -1 -or- handle of a local destination file
     * @param hCopyOffset local file only: non-zero for resume
     * @return 0 for success, -1 for server error, -2 for client error
     */
    Q_REQUIRED_RESULT Result ftpGet(int iCopyFile, const QString &sCopyFile, const QUrl &url, KIO::fileoffset_t hCopyOffset);

    /**
     * This is the internal implementation of put() - see copy().
     *
     * IMPORTANT: the caller should call ftpCloseCommand() on return.
     * The function does not call error(), the caller should do this.
     *
     * @param iError      set to an ERR_xxxx code on error
     * @param iCopyFile   -1 -or- handle of a local source file
     * @return 0 for success, -1 for server error, -2 for client error
     */
    Q_REQUIRED_RESULT Result ftpPut(int iCopyFile, const QUrl &url, int permissions, KIO::JobFlags flags);

    /**
     * helper called from copy() to implement FILE -> FTP transfers
     *
     * @param iError      set to an ERR_xxxx code on error
     * @param iCopyFile   [out] handle of a local source file
     * @param sCopyFile   path of the local source file
     * @return 0 for success, -1 for server error, -2 for client error
     */
    Q_REQUIRED_RESULT Result ftpCopyPut(int &iCopyFile, const QString &sCopyFile, const QUrl &url, int permissions, KIO::JobFlags flags);

    /**
     * helper called from copy() to implement FTP -> FILE transfers
     *
     * @param iError      set to an ERR_xxxx code on error
     * @param iCopyFile   [out] handle of a local source file
     * @param sCopyFile   path of the local destination file
     * @return 0 for success, -1 for server error, -2 for client error
     */
    Q_REQUIRED_RESULT Result ftpCopyGet(int &iCopyFile, const QString &sCopyFile, const QUrl &url, int permissions, KIO::JobFlags flags);

    /**
     * Sends the MIME type of the content to retrieved.
     *
     * @param iError      set to an ERR_xxxx code on error
     * @return 0 for success, -1 for server error, -2 for client error
     */
    Q_REQUIRED_RESULT Result ftpSendMimeType(const QUrl &url);

    /**
     * Fixes up an entry name so that extraneous whitespaces do not cause
     * problems. See bug# 88575 and bug# 300988.
     */
    void fixupEntryName(FtpEntry *ftpEnt);

    /**
     * Calls @ref statEntry.
     */
    bool maybeEmitStatEntry(FtpEntry &ftpEnt, const QString &filename, bool isDir);

    /**
     * Setup the connection to the server.
     */
    Q_REQUIRED_RESULT ConnectionResult synchronousConnectToHost(const QString &host, quint16 port);

private: // data members
    Ftp *const q;

    QString m_host;
    int m_port = 0;
    QString m_user;
    QString m_pass;
    /**
     * Where we end up after connecting
     */
    QString m_initialPath;
    QUrl m_proxyURL;
    QStringList m_proxyUrls;

    /**
      * the current working directory - see ftpFolder
      */
    QString m_currentPath;

    /**
     * the status returned by the FTP protocol, set in ftpResponse()
     */
    int m_iRespCode = 0;

    /**
     * the status/100 returned by the FTP protocol, set in ftpResponse()
     */
    int m_iRespType = 0;

    /**
     * This flag is maintained by ftpDataMode() and contains I or A after
     * ftpDataMode() has successfully set the mode.
     */
    char m_cDataMode;

    /**
     * true if logged on (m_control should also be non-nullptr)
     */
    bool m_bLoggedOn;

    /**
     * true if a "textmode" metadata key was found by ftpLogin(). This
     * switches the ftp data transfer mode from binary to ASCII.
     */
    bool m_bTextMode;

    /**
     * true if a data stream is open, used in closeConnection().
     *
     * When the user cancels a get or put command the Ftp dtor will be called,
     * which in turn calls closeConnection(). The later would try to send QUIT
     * which won't work until timeout. ftpOpenCommand sets the m_bBusy flag so
     * that the sockets will be closed immediately - the server should be
     * capable of handling this and return an error code on thru the control
     * connection. The m_bBusy gets cleared by the ftpCloseCommand() routine.
     */
    bool m_bBusy;

    bool m_bPasv;

    KIO::filesize_t m_size;
    static const KIO::filesize_t UnknownSize;

    enum {
        epsvUnknown = 0x01,
        epsvAllUnknown = 0x02,
        eprtUnknown = 0x04,
        epsvAllSent = 0x10,
        pasvUnknown = 0x20,
        chmodUnknown = 0x100,
    };
    int m_extControl;

    /**
     * control connection socket, only set if openControl() succeeded
     */
    QTcpSocket  *m_control = nullptr;
    QByteArray m_lastControlLine;

    /**
     * data connection socket
     */
    QTcpSocket  *m_data = nullptr;

    /**
     * active mode server socket
     */
    QTcpServer *m_server = nullptr;
};


#endif // KDELIBS_FTP_H

