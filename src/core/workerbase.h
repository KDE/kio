/*
    SPDX-License-Identifier: LGPL-2.0-or-later
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2019-2022 Harald Sitter <sitter@kde.org>
*/

#ifndef WORKERBASE_H
#define WORKERBASE_H

#include <memory>

#include "slavebase.h" // for KIO::JobFlags

namespace KIO
{
class WorkerBasePrivate;
class WorkerResultPrivate;

/**
 * @brief The result of a worker call
 * When using the Result type always mark the function Q_REQUIRED_RESULT to enforce handling of the Result.
 */
class KIOCORE_EXPORT WorkerResult
{
public:
    /// Use fail() or pass();
    WorkerResult() = delete;
    ~WorkerResult();
    WorkerResult(const WorkerResult &);
    WorkerResult &operator=(const WorkerResult &);
    WorkerResult(WorkerResult &&) noexcept;
    WorkerResult &operator=(WorkerResult &&) noexcept;

    /// Whether or not the result was a success.
    bool success() const;
    /// The error code (or ERR_UNKNOWN) of the result.
    int error() const;
    /// The localized error description if applicable.
    QString errorString() const;

    /// Constructs a failure results.
    Q_REQUIRED_RESULT static WorkerResult fail(int _error = KIO::ERR_UNKNOWN, const QString &_errorString = QString());
    /// Constructs a success result.
    Q_REQUIRED_RESULT static WorkerResult pass();

private:
    KIOCORE_NO_EXPORT explicit WorkerResult(std::unique_ptr<WorkerResultPrivate> &&dptr);
    std::unique_ptr<WorkerResultPrivate> d;
};

/**
 * @class KIO::WorkerBase workerbase.h <KIO/WorkerBase>
 *
 * WorkerBase is the class to use to implement a worker - simply inherit WorkerBase in your worker.
 *
 * A call to foo() results in a call to slotFoo() on the other end.
 *
 * Note that a kioworker doesn't have a Qt event loop. When idle, it's waiting for a command
 * on the socket that connects it to the application. So don't expect a kioworker to react
 * to D-Bus signals for instance. KIOWorkers are short-lived anyway, so any kind of watching
 * or listening for notifications should be done elsewhere, for instance in a kded module
 * (see kio_desktop's desktopnotifier.cpp for an example).
 *
 * If a kioworker needs a Qt event loop within the implementation of one method, e.g. to
 * wait for an asynchronous operation to finish, that is possible, using QEventLoop.
 *
 * @since 5.96
 */
class KIOCORE_EXPORT WorkerBase
{
public:
    WorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket);
    virtual ~WorkerBase();

    /**
     * @internal
     * Terminate the worker by calling the destructor and then ::exit()
     */
    void exit();

    /**
     * @internal
     */
    void dispatchLoop();

    ///////////
    // Message Signals to send to the job
    ///////////

    /**
     * Sends data in the worker to the job (i.e.\ in get).
     *
     * To signal end of data, simply send an empty
     * QByteArray().
     *
     * @param data the data read by the worker
     */
    void data(const QByteArray &data);

    /**
     * Asks for data from the job.
     * @see readData
     */
    void dataReq();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 103)
    /**
     * Call to signal that data from the sub-URL is needed
     * @deprecated Since 5.96, feature no longer exists.
     */
    KIOCORE_DEPRECATED_VERSION_BELATED(5, 103, 5, 96, "Feature no longer exists.")
    void needSubUrlData();
#endif

    /**
     * Used to report the status of the worker.
     * @param host the worker is currently connected to. (Should be
     *        empty if not connected)
     * @param connected Whether an actual network connection exists.
     **/
    void workerStatus(const QString &host, bool connected);

    /**
     * Call this from stat() to express details about an object, the
     * UDSEntry customarily contains the atoms describing file name, size,
     * MIME type, etc.
     * @param _entry The UDSEntry containing all of the object attributes.
     */
    void statEntry(const UDSEntry &_entry);

    /**
     * Call this in listDir, each time you have a bunch of entries
     * to report.
     * @param _entry The UDSEntry containing all of the object attributes.
     */
    void listEntries(const UDSEntryList &_entry);

    /**
     * Call this at the beginning of put(), to give the size of the existing
     * partial file, if there is one. The @p offset argument notifies the
     * other job (the one that gets the data) about the offset to use.
     * In this case, the boolean returns whether we can indeed resume or not
     * (we can't if the protocol doing the get() doesn't support setting an offset)
     */
    bool canResume(KIO::filesize_t offset);

    /**
     * Call this at the beginning of get(), if the "range-start" metadata was set
     * and returning byte ranges is implemented by this protocol.
     */
    void canResume();

    ///////////
    // Info Signals to send to the job
    ///////////

    /**
     * Call this in get and copy, to give the total size
     * of the file.
     */
    void totalSize(KIO::filesize_t _bytes);
    /**
     * Call this during get and copy, once in a while,
     * to give some info about the current state.
     * Don't emit it in listDir, listEntries speaks for itself.
     */
    void processedSize(KIO::filesize_t _bytes);

    void position(KIO::filesize_t _pos);

    void written(KIO::filesize_t _bytes);

    void truncated(KIO::filesize_t _length);

    /**
     * Call this in get and copy, to give the current transfer
     * speed, but only if it can't be calculated out of the size you
     * passed to processedSize (in most cases you don't want to call it)
     */
    void speed(unsigned long _bytes_per_second);

    /**
     * Call this to signal a redirection.
     * The job will take care of going to that url.
     */
    void redirection(const QUrl &_url);

    /**
     * Tell that we will only get an error page here.
     * This means: the data you'll get isn't the data you requested,
     * but an error page (usually HTML) that describes an error.
     */
    void errorPage();

    /**
     * Call this in mimetype() and in get(), when you know the MIME type.
     * See mimetype() about other ways to implement it.
     */
    void mimeType(const QString &_type);

    /**
     * Call to signal a warning, to be displayed in a dialog box.
     */
    void warning(const QString &msg);

    /**
     * Call to signal a message, to be displayed if the application wants to,
     * for instance in a status bar. Usual examples are "connecting to host xyz", etc.
     */
    void infoMessage(const QString &msg);

    /**
     * Type of message box. Should be kept in sync with KMessageBox::DialogType.
     */
    enum MessageBoxType {
        QuestionTwoActions = 1, ///< @since 5.100
        WarningTwoActions = 2, ///< @since 5.100
        WarningContinueCancel = 3,
        WarningTwoActionsCancel = 4, ///< @since 5.100
        Information = 5,
        SSLMessageBox = 6,
        // In KMessageBox::DialogType; Sorry = 7, Error = 8, QuestionTwoActionsCancel = 9
        WarningContinueCancelDetailed = 10,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
        QuestionYesNo ///< @deprecated Since 5.100, use QuestionTwoActions.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use QuestionTwoActions.") = QuestionTwoActions,
        WarningYesNo ///< @deprecated Since 5.100, use WarningTwoActions.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use WarningTwoActions.") = WarningTwoActions,
        WarningYesNoCancel ///< @deprecated Since 5.100, use WarningTwoActionsCancel.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use WarningTwoActionsCancel.") = WarningTwoActionsCancel,
#endif
    };

    /**
     * Button codes. Should be kept in sync with KMessageBox::ButtonCode
     */
    enum ButtonCode {
        Ok = 1,
        Cancel = 2,
        PrimaryAction = 3, ///< @since 5.100
        SecondaryAction = 4, ///< @since 5.100
        Continue = 5,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
        Yes ///< @deprecated Since 5.100, use PrimaryAction.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use PrimaryAction.") = PrimaryAction,
        No ///< @deprecated Since 5.100, use SecondaryAction.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use SecondaryAction.") = SecondaryAction,
#endif
    };

    /**
     * Call this to show a message box from the worker
     * @param type type of message box
     * @param text Message string. May contain newlines.
     * @param title Message box title.
     * @param primaryActionText the text for the first button.
     *                          Ignored for @p type Information & SSLMessageBox.
     *                          The (deprecated since 5.100) default is i18n("&Yes").
     * @param secondaryActionText the text for the second button.
     *                            Ignored for @p type WarningContinueCancel, WarningContinueCancelDetailed,
     *                            Information & SSLMessageBox.
     *                            The (deprecated since 5.100) default is i18n("&No").
     * @return a button code, as defined in ButtonCode, or 0 on communication error.
     */
    int messageBox(MessageBoxType type,
                   const QString &text,
                   const QString &title = QString(),
                   const QString &primaryActionText = QString(),
                   const QString &secondaryActionText = QString());

    /**
     * Call this to show a message box from the worker
     * @param text Message string. May contain newlines.
     * @param type type of message box
     * @param title Message box title.
     * @param primaryActionText the text for the first button.
     *                          Ignored for @p type Information & SSLMessageBox.
     *                          The (deprecated since 5.100) default is i18n("&Yes").
     * @param secondaryActionText the text for the second button.
     *                            Ignored for @p type WarningContinueCancel, WarningContinueCancelDetailed,
     *                            Information & SSLMessageBox.
     *                            The (deprecated since 5.100) default is i18n("&No").
     * @param dontAskAgainName the name used to store result from 'Do not ask again' checkbox.
     * @return a button code, as defined in ButtonCode, or 0 on communication error.
     */
    int messageBox(const QString &text,
                   MessageBoxType type,
                   const QString &title = QString(),
                   const QString &primaryActionText = QString(),
                   const QString &secondaryActionText = QString(),
                   const QString &dontAskAgainName = QString());

    /**
     * Sets meta-data to be send to the application before the first
     * data() or finished() signal.
     */
    void setMetaData(const QString &key, const QString &value);

    /**
     * Queries for the existence of a certain config/meta-data entry
     * send by the application to the worker.
     */
    bool hasMetaData(const QString &key) const;

    /**
     * Queries for config/meta-data send by the application to the worker.
     */
    QString metaData(const QString &key) const;

    /**
     * @internal for ForwardingSlaveBase
     * Contains all metadata (but no config) sent by the application to the worker.
     */
    MetaData allMetaData() const;

    /**
     * Returns a map to query config/meta-data information from.
     *
     * The application provides the worker with all configuration information
     * relevant for the current protocol and host.
     *
     * Use configValue() as shortcut.
     */
    QMap<QString, QVariant> mapConfig() const;

    /**
     * Returns a bool from the config/meta-data information.
     */
    bool configValue(const QString &key, bool defaultValue) const;

    /**
     * Returns an int from the config/meta-data information.
     */
    int configValue(const QString &key, int defaultValue) const;

    /**
     * Returns a QString from the config/meta-data information.
     */
    QString configValue(const QString &key, const QString &defaultValue = QString()) const;

    /**
     * Returns a configuration object to query config/meta-data information
     * from.
     *
     * The application provides the worker with all configuration information
     * relevant for the current protocol and host.
     *
     * @note Since 5.64 prefer to use mapConfig() or one of the configValue(...) overloads.
     * @todo Find replacements for the other current usages of this method.
     */
    KConfigGroup *config();
    // KF6: perhaps rename mapConfig() to config() when removing this

    /**
     * Returns an object that can translate remote filenames into proper
     * Unicode forms. This encoding can be set by the user.
     */
    KRemoteEncoding *remoteEncoding();

    ///////////
    // Commands sent by the job, the worker has to
    // override what it wants to implement
    ///////////

    /**
     * @brief Application connected to the worker.
     *
     * Called when an application has connected to the worker. Mostly only useful
     * when you want to e.g. send metadata to the application once it connects.
     */
    virtual void appConnectionMade();

    /**
     * Set the host
     *
     * Called directly by createWorker and not via the interface.
     *
     * This method is called whenever a change in host, port or user occurs.
     */
    virtual void setHost(const QString &host, quint16 port, const QString &user, const QString &pass);

    /**
     * Opens the connection (forced).
     * When this function gets called the worker is operating in
     * connection-oriented mode.
     * When a connection gets lost while the worker operates in
     * connection oriented mode, the worker should report
     * ERR_CONNECTION_BROKEN instead of reconnecting. The user is
     * expected to disconnect the worker in the error handler.
     */
    Q_REQUIRED_RESULT virtual WorkerResult openConnection();

    /**
     * Closes the connection (forced).
     * Called when the application disconnects the worker to close
     * any open network connections.
     *
     * When the worker was operating in connection-oriented mode,
     * it should reset itself to connectionless (default) mode.
     */
    virtual void closeConnection();

    /**
     * get, aka read.
     * @param url the full url for this request. Host, port and user of the URL
     *        can be assumed to be the same as in the last setHost() call.
     *
     * The worker should first "emit" the MIME type by calling mimeType(),
     * and then "emit" the data using the data() method.
     *
     * The reason why we need get() to emit the MIME type is:
     * when pasting a URL in krunner, or konqueror's location bar,
     * we have to find out what is the MIME type of that URL.
     * Rather than doing it with a call to mimetype(), then the app or part
     * would have to do a second request to the same server, this is done
     * like this: get() is called, and when it emits the MIME type, the job
     * is put on hold and the right app or part is launched. When that app
     * or part calls get(), the worker is magically reused, and the download
     * can now happen. All with a single call to get() in the worker.
     * This mechanism is also described in KIO::get().
     */
    Q_REQUIRED_RESULT virtual WorkerResult get(const QUrl &url);

    /**
     * open.
     * @param url the full url for this request. Host, port and user of the URL
     *        can be assumed to be the same as in the last setHost() call.
     * @param mode see \ref QIODevice::OpenMode
     */
    Q_REQUIRED_RESULT virtual WorkerResult open(const QUrl &url, QIODevice::OpenMode mode);

    /**
     * read.
     * @param size the requested amount of data to read
     * @see KIO::FileJob::read()
     */
    Q_REQUIRED_RESULT virtual WorkerResult read(KIO::filesize_t size);
    /**
     * write.
     * @param data the data to write
     * @see KIO::FileJob::write()
     */
    Q_REQUIRED_RESULT virtual WorkerResult write(const QByteArray &data);
    /**
     * seek.
     * @param offset the requested amount of data to read
     * @see KIO::FileJob::read()
     */
    Q_REQUIRED_RESULT virtual WorkerResult seek(KIO::filesize_t offset);
    /**
     * truncate
     * @param size size to truncate the file to
     * @see KIO::FileJob::truncate()
     */
    Q_REQUIRED_RESULT virtual WorkerResult truncate(KIO::filesize_t size);
    /**
     * close.
     * @see KIO::FileJob::close()
     */
    Q_REQUIRED_RESULT virtual WorkerResult close();

    /**
     * put, i.e.\ write data into a file.
     *
     * @param url where to write the file
     * @param permissions may be -1. In this case no special permission mode is set.
     * @param flags We support Overwrite here. Hopefully, we're going to
     * support Resume in the future, too.
     * If the file indeed already exists, the worker should NOT apply the
     * permissions change to it.
     * The support for resuming using .part files is done by calling canResume().
     *
     * IMPORTANT: Use the "modified" metadata in order to set the modification time of the file.
     *
     * @see canResume()
     */
    Q_REQUIRED_RESULT virtual WorkerResult put(const QUrl &url, int permissions, JobFlags flags);

    /**
     * Finds all details for one file or directory.
     * The information returned is the same as what listDir returns,
     * but only for one file or directory.
     * Call statEntry() after creating the appropriate UDSEntry for this
     * url.
     *
     * You can use the "details" metadata to optimize this method to only
     * do as much work as needed by the application.
     * By default details is 2 (all details wanted, including modification time, size, etc.),
     * details==1 is used when deleting: we don't need all the information if it takes
     * too much time, no need to follow symlinks etc.
     * details==0 is used for very simple probing: we'll only get the answer
     * "it's a file or a directory (or a symlink), or it doesn't exist".
     */
    Q_REQUIRED_RESULT virtual WorkerResult stat(const QUrl &url);

    /**
     * Finds MIME type for one file or directory.
     *
     * This method should either emit 'mimeType' or it
     * should send a block of data big enough to be able
     * to determine the MIME type.
     *
     * If the worker doesn't reimplement it, a get will
     * be issued, i.e.\ the whole file will be downloaded before
     * determining the MIME type on it - this is obviously not a
     * good thing in most cases.
     */
    Q_REQUIRED_RESULT virtual WorkerResult mimetype(const QUrl &url);

    /**
     * Lists the contents of @p url.
     * The worker should emit ERR_CANNOT_ENTER_DIRECTORY if it doesn't exist,
     * if we don't have enough permissions.
     * You should not list files if the path in @p url is empty, but redirect
     * to a non-empty path instead.
     */
    Q_REQUIRED_RESULT virtual WorkerResult listDir(const QUrl &url);

    /**
     * Create a directory
     * @param url path to the directory to create
     * @param permissions the permissions to set after creating the directory
     * (-1 if no permissions to be set)
     * The worker emits ERR_CANNOT_MKDIR if failure.
     */
    Q_REQUIRED_RESULT virtual WorkerResult mkdir(const QUrl &url, int permissions);

    /**
     * Rename @p oldname into @p newname.
     * If the worker returns an error ERR_UNSUPPORTED_ACTION, the job will
     * ask for copy + del instead.
     *
     * Important: the worker must implement the logic "if the destination already
     * exists, error ERR_DIR_ALREADY_EXIST or ERR_FILE_ALREADY_EXIST".
     * For performance reasons no stat is done in the destination before hand,
     * the worker must do it.
     *
     * By default, rename() is only called when renaming (moving) from
     * yourproto://host/path to yourproto://host/otherpath.
     *
     * If you set renameFromFile=true then rename() will also be called when
     * moving a file from file:///path to yourproto://host/otherpath.
     * Otherwise such a move would have to be done the slow way (copy+delete).
     * See KProtocolManager::canRenameFromFile() for more details.
     *
     * If you set renameToFile=true then rename() will also be called when
     * moving a file from yourproto: to file:.
     * See KProtocolManager::canRenameToFile() for more details.
     *
     * @param src where to move the file from
     * @param dest where to move the file to
     * @param flags We support Overwrite here
     */
    Q_REQUIRED_RESULT virtual WorkerResult rename(const QUrl &src, const QUrl &dest, JobFlags flags);

    /**
     * Creates a symbolic link named @p dest, pointing to @p target, which
     * may be a relative or an absolute path.
     * @param target The string that will become the "target" of the link (can be relative)
     * @param dest The symlink to create.
     * @param flags We support Overwrite here
     */
    Q_REQUIRED_RESULT virtual WorkerResult symlink(const QString &target, const QUrl &dest, JobFlags flags);

    /**
     * Change permissions on @p url.
     * The worker emits ERR_DOES_NOT_EXIST or ERR_CANNOT_CHMOD
     */
    Q_REQUIRED_RESULT virtual WorkerResult chmod(const QUrl &url, int permissions);

    /**
     * Change ownership of @p url.
     * The worker emits ERR_DOES_NOT_EXIST or ERR_CANNOT_CHOWN
     */
    Q_REQUIRED_RESULT virtual WorkerResult chown(const QUrl &url, const QString &owner, const QString &group);

    /**
     * Sets the modification time for @url.
     * For instance this is what CopyJob uses to set mtime on dirs at the end of a copy.
     * It could also be used to set the mtime on any file, in theory.
     * The usual implementation on unix is to call utime(path, &myutimbuf).
     * The worker emits ERR_DOES_NOT_EXIST or ERR_CANNOT_SETTIME
     */
    Q_REQUIRED_RESULT virtual WorkerResult setModificationTime(const QUrl &url, const QDateTime &mtime);

    /**
     * Copy @p src into @p dest.
     *
     * By default, copy() is only called when copying a file from
     * yourproto://host/path to yourproto://host/otherpath.
     *
     * If you set copyFromFile=true then copy() will also be called when
     * moving a file from file:///path to yourproto://host/otherpath.
     * Otherwise such a copy would have to be done the slow way (get+put).
     * See also KProtocolManager::canCopyFromFile().
     *
     * If you set copyToFile=true then copy() will also be called when
     * moving a file from yourproto: to file:.
     * See also KProtocolManager::canCopyToFile().
     *
     * If the worker returns an error ERR_UNSUPPORTED_ACTION, the job will
     * ask for get + put instead.
     *
     * If the worker returns an error ERR_FILE_ALREADY_EXIST, the job will
     * ask for a different destination filename.
     *
     * @param src where to copy the file from (decoded)
     * @param dest where to copy the file to (decoded)
     * @param permissions may be -1. In this case no special permission mode is set,
     *        and the owner and group permissions are not preserved.
     * @param flags We support Overwrite here
     *
     * Don't forget to set the modification time of @p dest to be the modification time of @p src.
     */
    Q_REQUIRED_RESULT virtual WorkerResult copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags);

    /**
     * Delete a file or directory.
     * @param url file/directory to delete
     * @param isfile if true, a file should be deleted.
     *               if false, a directory should be deleted.
     *
     * By default, del() on a directory should FAIL if the directory is not empty.
     * However, if metadata("recurse") == "true", then the worker can do a recursive deletion.
     * This behavior is only invoked if the worker specifies deleteRecursive=true in its protocol file.
     */
    Q_REQUIRED_RESULT virtual WorkerResult del(const QUrl &url, bool isfile);

    /**
     * Used for any command that is specific to this worker (protocol).
     * Examples are : HTTP POST, mount and unmount (kio_file)
     *
     * @param data packed data; the meaning is completely dependent on the
     *        worker, but usually starts with an int for the command number.
     * Document your worker's commands, at least in its header file.
     */
    Q_REQUIRED_RESULT virtual WorkerResult special(const QByteArray &data);

    /**
     * Used for multiple get. Currently only used for HTTP pipelining
     * support.
     *
     * @param data packed data; Contains number of URLs to fetch, and for
     * each URL the URL itself and its associated MetaData.
     */
    Q_REQUIRED_RESULT virtual WorkerResult multiGet(const QByteArray &data);

    /**
     * Get a filesystem's total and available space.
     *
     * @param url Url to the filesystem
     */
    Q_REQUIRED_RESULT virtual WorkerResult fileSystemFreeSpace(const QUrl &url);

    /**
     * Called to get the status of the worker. Worker should respond
     * by calling workerStatus(...)
     */
    virtual void worker_status();

    /**
     * Called by the scheduler to tell the worker that the configuration
     * changed (i.e.\ proxy settings) .
     */
    virtual void reparseConfiguration();

    /**
     * @return timeout value for connecting to remote host.
     */
    int connectTimeout();

    /**
     * @return timeout value for connecting to proxy in secs.
     */
    int proxyConnectTimeout();

    /**
     * @return timeout value for read from first data from
     * remote host in seconds.
     */
    int responseTimeout();

    /**
     * @return timeout value for read from subsequent data from
     * remote host in secs.
     */
    int readTimeout();

    /**
     * This function sets a timeout of @p timeout seconds and calls
     * special(data) when the timeout occurs as if it was called by the
     * application.
     *
     * A timeout can only occur when the worker is waiting for a command
     * from the application.
     *
     * Specifying a negative timeout cancels a pending timeout.
     *
     * Only one timeout at a time is supported, setting a timeout
     * cancels any pending timeout.
     */
    void setTimeoutSpecialCommand(int timeout, const QByteArray &data = QByteArray());

    /**
     * Read data sent by the job, after a dataReq
     *
     * @param buffer buffer where data is stored
     * @return 0 on end of data,
     *         > 0 bytes read
     *         < 0 error
     **/
    int readData(QByteArray &buffer);

    /**
     * It collects entries and emits them via listEntries
     * when enough of them are there or a certain time
     * frame exceeded (to make sure the app gets some
     * items in time but not too many items one by one
     * as this will cause a drastic performance penalty).
     * @param entry The UDSEntry containing all of the object attributes.
     */
    void listEntry(const UDSEntry &entry);

    /**
     * internal function to connect a worker to/ disconnect from
     * either the worker pool or the application
     */
    void connectWorker(const QString &path);
    void disconnectWorker();

    /**
     * Prompt the user for Authorization info (login & password).
     *
     * Use this function to request authorization information from
     * the end user. You can also pass an error message which explains
     * why a previous authorization attempt failed. Here is a very
     * simple example:
     *
     * \code
     * KIO::AuthInfo authInfo;
     * int errorCode = openPasswordDialogV2(authInfo);
     * if (!errorCode) {
     *    qDebug() << QLatin1String("User: ") << authInfo.username;
     *    qDebug() << QLatin1String("Password: not displayed here!");
     * } else {
     *    error(errorCode, QString());
     * }
     * \endcode
     *
     * You can also preset some values like the username, caption or
     * comment as follows:
     *
     * \code
     * KIO::AuthInfo authInfo;
     * authInfo.caption = i18n("Acme Password Dialog");
     * authInfo.username = "Wile E. Coyote";
     * QString errorMsg = i18n("You entered an incorrect password.");
     * int errorCode = openPasswordDialogV2(authInfo, errorMsg);
     * [...]
     * \endcode
     *
     * \note You should consider using checkCachedAuthentication() to
     * see if the password is available in kpasswdserver before calling
     * this function.
     *
     * \note A call to this function can fail and return @p false,
     * if the password server could not be started for whatever reason.
     *
     * \note This function does not store the password information
     * automatically (and has not since kdelibs 4.7). If you want to
     * store the password information in a persistent storage like
     * KWallet, then you MUST call @ref cacheAuthentication.
     *
     * @see checkCachedAuthentication
     * @param info  See AuthInfo.
     * @param errorMsg Error message to show
     * @return a KIO error code: NoError (0), KIO::USER_CANCELED, or other error codes.
     */
    int openPasswordDialog(KIO::AuthInfo &info, const QString &errorMsg = QString());

    /**
     * Checks for cached authentication based on parameters
     * given by @p info.
     *
     * Use this function to check if any cached password exists
     * for the URL given by @p info.  If @p AuthInfo::realmValue
     * and/or @p AuthInfo::verifyPath flag is specified, then
     * they will also be factored in determining the presence
     * of a cached password.  Note that @p Auth::url is a required
     * parameter when attempting to check for cached authorization
     * info. Here is a simple example:
     *
     * \code
     * AuthInfo info;
     * info.url = QUrl("https://www.foobar.org/foo/bar");
     * info.username = "somename";
     * info.verifyPath = true;
     * if ( !checkCachedAuthentication( info ) )
     * {
     *    int errorCode = openPasswordDialogV2(info);
     *     ....
     * }
     * \endcode
     *
     * @param       info See AuthInfo.
     * @return      @p true if cached Authorization is found, false otherwise.
     */
    bool checkCachedAuthentication(AuthInfo &info);

    /**
     * Caches @p info in a persistent storage like KWallet.
     *
     * Note that calling openPasswordDialogV2 does not store passwords
     * automatically for you (and has not since kdelibs 4.7).
     *
     * Here is a simple example of how to use cacheAuthentication:
     *
     * \code
     * AuthInfo info;
     * info.url = QUrl("https://www.foobar.org/foo/bar");
     * info.username = "somename";
     * info.verifyPath = true;
     * if ( !checkCachedAuthentication( info ) ) {
     *    int errorCode = openPasswordDialogV2(info);
     *    if (!errorCode) {
     *        if (info.keepPassword)  {  // user asked password be save/remembered
     *             cacheAuthentication(info);
     *        }
     *    }
     * }
     * \endcode
     *
     * @param info See AuthInfo.
     * @return @p true if @p info was successfully cached.
     */
    bool cacheAuthentication(const AuthInfo &info);

    /**
     * Wait for an answer to our request, until we get @p expected1 or @p expected2
     * @return the result from readData, as well as the cmd in *pCmd if set, and the data in @p data
     */
    int waitForAnswer(int expected1, int expected2, QByteArray &data, int *pCmd = nullptr);

    /**
     * Internal function to transmit meta data to the application.
     * m_outgoingMetaData will be cleared; this means that if the worker is for
     * example put on hold and picked up by a different KIO::Job later the new
     * job will not see the metadata sent before.
     * See kio/DESIGN.krun for an overview of the state
     * progression of a job/worker.
     * @warning calling this method may seriously interfere with the operation
     * of KIO which relies on the presence of some metadata at some points in time.
     * You should not use it if you are not familiar with KIO and not before
     * the worker is connected to the last job before returning to idle state.
     */
    void sendMetaData();

    /**
     * Internal function to transmit meta data to the application.
     * Like sendMetaData() but m_outgoingMetaData will not be cleared.
     * This method is mainly useful in code that runs before the worker is connected
     * to its final job.
     */
    void sendAndKeepMetaData();

    /** If your ioworker was killed by a signal, wasKilled() returns true.
     Check it regularly in lengthy functions (e.g. in get();) and return
     as fast as possible from this function if wasKilled() returns true.
     This will ensure that your worker destructor will be called correctly.
     */
    bool wasKilled() const;

    /** Internally used
     * @internal
     */
    void lookupHost(const QString &host);

    /** Internally used
     * @internal
     */
    int waitForHostInfo(QHostInfo &info);

    /**
     * Checks with job if privilege operation is allowed.
     * @return privilege operation status.
     * @see PrivilegeOperationStatus
     */
    PrivilegeOperationStatus requestPrivilegeOperation(const QString &operationDetails);

    /**
     * Adds @p action to the list of PolicyKit actions which the
     * worker is authorized to perform.
     *
     * @param action the PolicyKit action
     */
    void addTemporaryAuthorization(const QString &action);

    /**
     * @brief Set the Incoming Meta Data
     * This is only really useful if your worker wants to overwrite the
     * metadata for consumption in other worker functions; this overwrites
     * existing metadata set by the client!
     *
     * @param metaData metadata to set
     * @since 5.99
     */
    void setIncomingMetaData(const KIO::MetaData &metaData);

private:
    std::unique_ptr<WorkerBasePrivate> d;
    Q_DISABLE_COPY_MOVE(WorkerBase)
    friend class WorkerSlaveBaseBridge;
    friend class WorkerThread;
};

} // namespace KIO

#endif
