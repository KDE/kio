/*
 * This file is part of the KDE libraries
 * Copyright (C) 2007 Thiago Macieira <thiago@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KLOCALSOCKET_H
#define KLOCALSOCKET_H

#include <kio/kiocore_export.h>
#include <QtCore/QString>
#include <QtNetwork/QTcpSocket>
#include <QHostAddress>

class KLocalSocketPrivate;
/**
 * @class KLocalSocket
 * @brief KLocalSocket allows one to create and use local (Unix) sockets
 *
 * On some platforms, local sockets are a kind of streaming socket
 * that can be used to transmit and receive data just like Internet
 * (TCP) streaming sockets. The difference is that they remain local
 * to the host running them and cannot be accessed externally. They
 * are also very fast and (in theory) consume less resources than
 * standard TCP sockets.
 *
 * KLocalSocket supports two kinds of local socket types (see
 * KLocalSocket::LocalSocketType):
 * - Unix sockets (UnixSocket): standard Unix sockets whose names are
 *   file paths and obey filesystem restrictions
 * - Abstract Unix sockets (AbstractUnixSocket): similar to Unix
 *   sockets, but they don't exist as entries in the filesystem and,
 *   thus, aren't restricted by its permissions
 *
 * @author Thiago Macieira <thiago@kde.org>
 * @internal DO NOT USE. Only for KIO.
 */
class KIOCORE_EXPORT KLocalSocket : public QTcpSocket // Only exported for the unittest. Header not installed
{
    Q_OBJECT
public:
    /**
     * Defines the local socket type. See KLocalSocket for more
     * information
     */
    enum LocalSocketType {
        UnixSocket,             ///< Unix sockets
        AbstractUnixSocket,     ///< Abstract Unix sockets
        UnknownLocalSocketType = -1
    };

    /**
     * Creates a KLocalSocket object with @p parent as the parent
     * object.
     *
     * @param parent    the parent object
     */
    explicit KLocalSocket(QObject *parent = 0);
    /**
     * Destroys the KLocalSocket object and frees up any resources
     * associated. If the socket is open, it will be closed.
     */
    virtual ~KLocalSocket();

    /**
     * Opens a connection to a listening Unix socket at @p path. Use
     * waitForConnection() to find out if the connection succeeded or
     * not.
     *
     * @param path      the Unix socket to connect to
     * @param mode      the mode to use when opening (see QIODevice::OpenMode)
     */
    void connectToPath(const QString &path, OpenMode mode = ReadWrite);
    /**
     * @overload
     * Opens a connection to a listening local socket at address @p
     * path. Use waitForConnection() to find out if the connection
     * succeeded or not.
     *
     * @param path      the local socket address to connect to
     * @param type      the local socket type to use
     * @param mode      the mode to use when opening (see QIODevice::OpenMode)
     */
    void connectToPath(const QString &path, LocalSocketType type, OpenMode mode = ReadWrite);
    /**
     * Disconnects the socket from its server.
     */
    void disconnectFromPath();

    /**
     * Returns the socket type for this socket, when
     * connected. Returns UnknownLocalSocketType if not
     * connected.
     */
    LocalSocketType localSocketType() const;

    /**
     * Returns the local address of this socket, when
     * connected. Returns QString() if not connected.
     *
     * Most of the time, the socket has no local address.
     */
    QString localPath() const;
    /**
     * Returns the peer address of this socket. That is, the address
     * that this socket connected to (see connectToPath). Returns
     * QString() if not connected.
     */
    QString peerPath() const;

private:
    using QAbstractSocket::connectToHost;
    using QAbstractSocket::disconnectFromHost;

protected Q_SLOTS:
    /// @internal
    void connectToHostImplementation(const QString &hostName, quint16 port, OpenMode mode);
    void disconnectFromHostImplementation();

public:
    virtual void connectToHost(const QHostAddress &address, quint16 port, OpenMode mode = ReadWrite) {
        connectToHostImplementation(address.toString(), port, mode);
    }
    virtual void connectToHost(const QString &hostName, quint16 port, OpenMode mode = ReadWrite, NetworkLayerProtocol protocol = AnyIPProtocol) {
        Q_UNUSED(protocol)
        connectToHostImplementation(hostName, port, mode);
    }
    virtual void disconnectFromHost() {
        disconnectFromHostImplementation();
    }

private:
    Q_DISABLE_COPY(KLocalSocket)
    friend class KLocalSocketPrivate;
    KLocalSocketPrivate * const d;
};

class KLocalSocketServerPrivate;
/**
 * @class KLocalSocketServer
 * @brief KLocalSocketServer allows one to create a listening local
 * socket and accept incoming connections
 *
 * On some platforms, local sockets are a kind of streaming socket
 * that can be used to transmit and receive data just like Internet
 * (TCP) streaming sockets. The difference is that they remain local
 * to the host running them and cannot be accessed externally. They
 * are also very fast and (in theory) consume less resources than
 * standard TCP sockets.
 *
 * KLocalSocketServer allows you to create the listening (i.e.,
 * passive) end of this local socket and accept incoming connections
 * from users of KLocalSocket. It supports the same kind of socket
 * types that KLocalSocket does (see KLocalSocket::LocalSocketType).
 *
 * @author Thiago Macieira <thiago@kde.org>
 */
class KIOCORE_EXPORT KLocalSocketServer : public QObject
{
    Q_OBJECT
public:
    /**
     * Creates a KLocalSocketServer object with @p parent as the
     * parent object. The object is created without binding to any
     * address.
     *
     * @param parent     the parent object
     */
    explicit KLocalSocketServer(QObject *parent = 0);
    /**
     * Destroys the KLocalSocketServer object and frees up any
     * resource associated. If the socket is still listening, it's
     * closed (see close()).
     *
     * The sockets that were accepted using this KLocalSocketServer
     * object are not affected and will remain open. However, note
     * that nextPendingConnection() returns objects that have this
     * KLocalSocketServer as parents, so the QObject destruction will
     * delete any objects that were not reparented.
     */
    virtual ~KLocalSocketServer();

    /**
     * Binds this socket to the address @p path and starts listening
     * there.
     *
     * If @p type is KLocalSocket::UnixSocket, @p path is
     * treated as a Unix filesystem path and the calling user must
     * have permission to create the named directory entry (that is,
     * the user must have write permission to the parent directory,
     * etc.)
     *
     * If @p type is KLocalSocket::AbstractUnixSocket, @p path is
     * just a name that can be anything. It'll be converted to an
     * 8-bit identifier just as if it were a file path, but
     * filesystem restrictions do not apply.
     *
     * This function returns true if it succeeded in binding the
     * socket to @p path and placing it in listen mode. It returns
     * false otherwise.
     *
     * @param path     the path to listen on
     * @param type     the local socket type
     * @returns true on success, false otherwise
     */
    bool listen(const QString &path, KLocalSocket::LocalSocketType type = KLocalSocket::UnixSocket);

    /**
     * Closes the socket. No further connections will be accepted,
     * but connections that were already pending can still be
     * retrieved with nextPendingConnection().
     *
     * Connections that were accepted and are already open will not
     * be affected.
     */
    void close();

    /**
     * Returns true if the socket is listening, false otherwise.
     */
    bool isListening() const;

    /**
     * Sets the maximum number of connections that KLocalSocketServer
     * will accept on your behalf and keep queued, ready to be
     * retrieved with nextPendingConnection(). If you set @p
     * numConnections to 0, hasPendingConnections() will always
     * return false. You can still use waitForNewConnection(),
     * though.
     *
     * @param numConnections   the number of connections to accept
     *                         and keep queued.
     */
    void setMaxPendingConnections(int numConnections);
    /**
     * Returns the value set with setMaxPendingConnections().
     */
    int maxPendingConnections() const;

    /**
     * Returns the socket type that this socket is listening on. If it
     * is not listening, returns QAbstractSocket::UnknownLocalSocketType.
     */
    KLocalSocket::LocalSocketType localSocketType() const;
    /**
     * Returns the address of this socket if it is listening on, or
     * QString() if it is not listening.
     */
    QString localPath() const;

    /**
     * Suspends the execution of the calling thread for at most @p
     * msec milliseconds and wait for a new socket connection to be
     * accepted (whichever comes first). If no new socket connection
     * is received within @p msec milliseconds, consider this a
     * time-out and set the boolean pointed by @p timedOut to false
     * (if it's not 0).
     *
     * If @p msec is 0, this call will not block, but will simply poll
     * the system to check if a new connection has been received in
     * the background.
     *
     * Use @p msec value of -1 to block indefinitely.
     *
     * @param msec      the time in milliseconds to block at most (-1
     *                  to block forever)
     * @param timedOut  points to a boolean that will be set to true
     *                  if a timeout did occur
     * @returns true if a new connection has been accepted or false if
     * an error occurred or if the operation timed out.
     */
    bool waitForNewConnection(int msec = 0, bool *timedOut = 0);

    /**
     * Returns true if a new socket can be received with
     * nextPendingConnection().
     */
    virtual bool hasPendingConnections() const;
    /**
     * Returns a new socket if one is available or 0 if none is.
     *
     * Note that the objects returned by this function will have the
     * current KLocalSocketServer object as its parent. You may want
     * to reparent the accepted objects if you intend them to outlive
     * the current object.
     */
    virtual KLocalSocket *nextPendingConnection();

    /**
     * If an error occurred, return the error code.
     */
    QAbstractSocket::SocketError serverError() const;
    /**
     * If an error occurred, return the error message.
     */
    QString errorString() const;

protected:
    /// @internal
    virtual void incomingConnection(int handle);

Q_SIGNALS:
    /**
     * The newConnection() signal is emitted whenever a new connection
     * is ready and has been accepted. Whenever it is emitted, calling
     * nextPendingConnection() will return a valid object at least
     * once.
     */
    void newConnection();

private:
    Q_PRIVATE_SLOT(d, void _k_newConnectionActivity())
    Q_DISABLE_COPY(KLocalSocketServer)
    friend class KLocalSocketServerPrivate;
    KLocalSocketServerPrivate * const d;
};

#endif
