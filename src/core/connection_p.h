/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_CONNECTION_P_H
#define KIO_CONNECTION_P_H

#include <QUrl>
#include <QObject>
#include <QString>
#include <QVector>
#include "connectionbackend_p.h"

namespace KIO
{

class ConnectionServer;
class ConnectionPrivate;
/**
 * @private
 *
 * This class provides a simple means for IPC between two applications
 * via a pipe.
 * It handles a queue of commands to be sent which makes it possible to
 * queue data before an actual connection has been established.
 */
class Connection : public QObject
{
    Q_OBJECT

public:
    enum class ReadMode {
        Polled,  ///Any new tasks will be polled
        EventDriven, ///We need to emit signals when we have pending events. Requires a working QEventLoop
    };
    /**
     * Creates a new connection.
     * @see connectToRemote, listenForRemote
     */
    explicit Connection(QObject *parent = nullptr);
    virtual ~Connection();

    /**
     * Connects to the remote address.
     * @param address a local:// or tcp:// URL.
     */
    void connectToRemote(const QUrl &address);

    /// Closes the connection.
    void close();

    QString errorString() const;

    bool isConnected() const;

    /**
    * Checks whether the connection has been initialized.
     * @return true if the initialized
     * @see init()
     */
    bool inited() const;

    /**
    * Sends/queues the given command to be sent.
     * @param cmd the command to set
     * @param arr the bytes to send
     * @return true if successful, false otherwise
     */
    bool send(int cmd, const QByteArray &arr = QByteArray());

    /**
    * Sends the given command immediately.
     * @param _cmd the command to set
     * @param data the bytes to send
     * @return true if successful, false otherwise
     */
    bool sendnow(int _cmd, const QByteArray &data);

    /**
     * Returns true if there are packets to be read immediately,
     * false if waitForIncomingTask must be called before more data
     * is available.
     */
    bool hasTaskAvailable() const;

    /**
     * Waits for one more command to be handled and ready.
     *
     * @param ms   the time to wait in milliseconds
     * @returns true if one command can be read, false if we timed out
     */
    bool waitForIncomingTask(int ms = 30000);

    /**
     * Receive data.
     *
     * @param _cmd the received command will be written here
     * @param data the received data will be written here

     * @return >=0 indicates the received data size upon success
     *         -1  indicates error
     */
    int read(int *_cmd, QByteArray &data);

    /**
     * Don't handle incoming data until resumed.
     */
    void suspend();

    /**
     * Resume handling of incoming data.
     */
    void resume();

    /**
     * Returns status of connection.
    * @return true if suspended, false otherwise
           */
    bool suspended() const;

    void setReadMode(ReadMode mode);

Q_SIGNALS:
    void readyRead();

private:
    Q_PRIVATE_SLOT(d, void dequeue())
    Q_PRIVATE_SLOT(d, void commandReceived(Task))
    Q_PRIVATE_SLOT(d, void disconnected())
    friend class ConnectionPrivate;
    friend class ConnectionServer;
    class ConnectionPrivate *const d;
};

// Separated from Connection only for historical reasons - they are both private now
class ConnectionPrivate
{
public:
    inline ConnectionPrivate()
        : backend(nullptr), q(nullptr), suspended(false), readMode(Connection::ReadMode::EventDriven)
    { }

    void dequeue();
    void commandReceived(const Task &task);
    void disconnected();
    void setBackend(ConnectionBackend *b);

    QVector<Task> outgoingTasks;
    QVector<Task> incomingTasks;
    ConnectionBackend *backend;
    Connection *q;
    bool suspended;
    Connection::ReadMode readMode;
};

class ConnectionServerPrivate;
}

#endif
