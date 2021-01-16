/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __kio_slaveinterface_h
#define __kio_slaveinterface_h

#include <qplatformdefs.h>

#include <QObject>
#include <QHostInfo>

#include <kio/global.h>
#include <kio/udsentry.h>
#include <kio/authinfo.h>

class QUrl;

namespace KIO
{

class Connection;
// better there is one ...
class SlaveInterfacePrivate;

// Definition of enum Command has been moved to global.h

/**
 * Identifiers for KIO informational messages.
 */
enum Info {
    INF_TOTAL_SIZE = 10,
    INF_PROCESSED_SIZE = 11,
    INF_SPEED,
    INF_REDIRECTION = 20,
    INF_MIME_TYPE = 21,
    INF_ERROR_PAGE = 22,
    INF_WARNING = 23,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(3, 0)
    INF_GETTING_FILE, ///< @deprecated Since 3.0
#else
    INF_GETTING_FILE_DEPRECATED_DO_NOT_USE,
#endif
    INF_UNUSED = 25, ///< now unused
    INF_INFOMESSAGE,
    INF_META_DATA,
    INF_NETWORK_STATUS,
    INF_MESSAGEBOX,
    INF_POSITION,
    INF_TRUNCATED,
    // add new ones here once a release is done, to avoid breaking binary compatibility
};

/**
 * Identifiers for KIO data messages.
 */
enum Message {
    MSG_DATA = 100,
    MSG_DATA_REQ,
    MSG_ERROR,
    MSG_CONNECTED,
    MSG_FINISHED,
    MSG_STAT_ENTRY, // 105
    MSG_LIST_ENTRIES,
    MSG_RENAMED, ///< unused
    MSG_RESUME,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 45)
    MSG_SLAVE_STATUS, ///< @deprecated Since 5.45, use MSG_SLAVE_STATUS_V2
#else
    MSG_SLAVE_STATUS_DEPRECATED_DO_NOT_USE,
#endif
    MSG_SLAVE_ACK, // 110
    MSG_NET_REQUEST,
    MSG_NET_DROP,
    MSG_NEED_SUBURL_DATA,
    MSG_CANRESUME,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(3, 1)
    MSG_AUTH_KEY, ///< @deprecated Since 3.1
    MSG_DEL_AUTH_KEY, ///< @deprecated Since 3.1
#else
    MSG_AUTH_KEY_DEPRECATED_DO_NOT_USE,
    MSG_DEL_AUTH_KEY_DEPRECATED_DO_NOT_USE,
#endif
    MSG_OPENED,
    MSG_WRITTEN,
    MSG_HOST_INFO_REQ,
    MSG_PRIVILEGE_EXEC,
    MSG_SLAVE_STATUS_V2,
    // add new ones here once a release is done, to avoid breaking binary compatibility
};

/**
 * @class KIO::SlaveInterface slaveinterface.h <KIO/SlaveInterface>
 *
 * There are two classes that specifies the protocol between application
 * ( KIO::Job) and kioslave. SlaveInterface is the class to use on the application
 * end, SlaveBase is the one to use on the slave end.
 *
 * A call to foo() results in a call to slotFoo() on the other end.
 */
class KIOCORE_EXPORT SlaveInterface : public QObject
{
    Q_OBJECT

protected:
    SlaveInterface(SlaveInterfacePrivate &dd, QObject *parent = nullptr);
public:

    virtual ~SlaveInterface();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    // TODO KF6: remove these methods, Connection isn't an exported class
    KIOCORE_DEPRECATED_VERSION(5, 0, "Do not use")
    void setConnection(Connection *connection);
    KIOCORE_DEPRECATED_VERSION(5, 0, "Do not use")
    Connection *connection() const;
#endif

    // Send our answer to the MSG_RESUME (canResume) request
    // (to tell the "put" job whether to resume or not)
    void sendResumeAnswer(bool resume);

    /**
     * Sends our answer for the INF_MESSAGEBOX request.
     *
     * @since 4.11
     */
    void sendMessageBoxAnswer(int result);

    void setOffset(KIO::filesize_t offset);
    KIO::filesize_t offset() const;

Q_SIGNALS:
    ///////////
    // Messages sent by the slave
    ///////////

    void data(const QByteArray &);
    void dataReq();
    void error(int, const QString &);
    void connected();
    void finished();
    void slaveStatus(qint64, const QByteArray &, const QString &, bool);
    void listEntries(const KIO::UDSEntryList &);
    void statEntry(const KIO::UDSEntry &);
    void needSubUrlData();

    void canResume(KIO::filesize_t);

    void open();
    void written(KIO::filesize_t);
    void close();

    void privilegeOperationRequested();

    ///////////
    // Info sent by the slave
    //////////
    void metaData(const KIO::MetaData &);
    void totalSize(KIO::filesize_t);
    void processedSize(KIO::filesize_t);
    void redirection(const QUrl &);
    void position(KIO::filesize_t);
    void truncated(KIO::filesize_t);

    void speed(unsigned long);
    void errorPage();
    void mimeType(const QString &);
    void warning(const QString &);
    void infoMessage(const QString &);
    //void connectFinished(); //it does not get emitted anywhere

protected:
    /////////////////
    // Dispatching
    ////////////////

    virtual bool dispatch();
    virtual bool dispatch(int _cmd, const QByteArray &data);

    void messageBox(int type, const QString &text, const QString &caption,
                    const QString &buttonYes, const QString &buttonNo);

    void messageBox(int type, const QString &text, const QString &caption,
                    const QString &buttonYes, const QString &buttonNo,
                    const QString &dontAskAgainName);

    // I need to identify the slaves
    void requestNetwork(const QString &, const QString &);
    void dropNetwork(const QString &, const QString &);

protected Q_SLOTS:
    void calcSpeed();
protected:
    SlaveInterfacePrivate *const d_ptr;
    Q_DECLARE_PRIVATE(SlaveInterface)
private:
    Q_PRIVATE_SLOT(d_func(), void slotHostInfo(QHostInfo))
};

}

#endif
