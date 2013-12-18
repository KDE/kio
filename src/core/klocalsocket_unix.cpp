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
#include <config-kiocore.h> // HAVE_STRUCT_SOCKADDR_SA_LEN

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "klocalizedstring.h"

static inline int kSocket(int af, int socketype, int proto)
{
    int ret;
    do {
        ret = ::socket(af, socketype, proto);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

static inline int kBind(int fd, const sockaddr *sa, int len)
{
    int ret;
    do {
        ret = ::bind(fd, sa, len);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

static inline int kConnect(int fd, const sockaddr *sa, int len)
{
    int ret;
    do {
        ret = ::connect(fd, sa, len);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

static inline int kListen(int fd, int backlog)
{
    int ret;
    do {
        ret = ::listen(fd, backlog);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

static inline int kAccept(int fd)
{
    int ret;
    sockaddr sa;
    socklen_t len = sizeof(sa);
    do {
        ret = ::accept(fd, &sa, &len);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

#ifdef socket
#undef socket
#endif

#ifdef bind
#undef bind
#endif

#ifdef listen
#undef listen
#endif

#ifdef connect
#undef connect
#endif

#ifdef accept
#undef accept
#endif

#include <QtCore/qfile.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qvarlengtharray.h>

#include "klocalsocket.h"
#include "klocalsocket_p.h"

#if !defined(AF_UNIX) && defined(AF_LOCAL)
# define AF_UNIX       AF_LOCAL
#endif

class KSockaddrUn
{
    int datalen;
    QVarLengthArray<char, 128> data;
public:
    KSockaddrUn(const QString &path, KLocalSocket::LocalSocketType type);
    bool ok() const { return datalen; }
    int length() const { return datalen; }
    const sockaddr* address()
        { return reinterpret_cast<sockaddr *>(data.data()); }
};

KSockaddrUn::KSockaddrUn(const QString &path, KLocalSocket::LocalSocketType type)
    : datalen(0)
{
    if (path.isEmpty())
        return;

    QString path2(path);
    if (!path.startsWith(QLatin1Char('/')))
        // relative path; put everything in /tmp
        path2.prepend(QLatin1String("/tmp/"));

    QByteArray encodedPath = QFile::encodeName(path2);

    datalen = MIN_SOCKADDR_UN_LEN + encodedPath.length();
    if (type == KLocalSocket::AbstractUnixSocket)
        ++datalen;
    data.resize(datalen);

    sockaddr_un *saddr = reinterpret_cast<sockaddr_un *>(data.data());
    saddr->sun_family = AF_UNIX;
#if HAVE_STRUCT_SOCKADDR_SA_LEN
    saddr->sun_len = datalen;
#endif

    if (type == KLocalSocket::UnixSocket) {
        strcpy(saddr->sun_path, encodedPath.constData());
    } else if (type == KLocalSocket::AbstractUnixSocket) {
        *saddr->sun_path = '\0';
        strcpy(saddr->sun_path + 1, encodedPath.constData());
    } else {
        datalen = 0;            // error
    }
}

static bool setNonBlocking(int fd)
{
    int fdflags = fcntl(fd, F_GETFL, 0);
    if (fdflags == -1)
        return false;		// error

    fdflags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, fdflags) == -1)
        return false;		// error

    return true;
}

void KLocalSocketPrivate::connectToPath(const QString &path, KLocalSocket::LocalSocketType aType,
                                        QAbstractSocket::OpenMode openMode)
{
    if (aType == KLocalSocket::UnixSocket || aType == KLocalSocket::AbstractUnixSocket) {
        // connect to Unix socket
        KSockaddrUn addr(path, aType);
        if (!addr.ok()) {
            emitError(QAbstractSocket::NetworkError, i18n("Specified socket path is invalid"));
            return;
        }

        // create the socket
        int fd = kSocket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) {
            // failed
            emitError(QAbstractSocket::UnsupportedSocketOperationError,
                      i18n("The socket operation is not supported"));
            return;
        }

        // try to connect
        // ### support non-blocking mode!
        if (kConnect(fd, addr.address(), addr.length()) == -1) {
            // failed
            int error = errno;
            ::close(fd);

            switch (error) {
            case ECONNREFUSED:
                emitError(QAbstractSocket::ConnectionRefusedError, i18n("Connection refused"));
                return;

            case EACCES:
            case EPERM:
                emitError(QAbstractSocket::SocketAccessError, i18n("Permission denied"));
                return;

            case ETIMEDOUT:
                emitError(QAbstractSocket::SocketTimeoutError, i18n("Connection timed out"));
                return;

            default:
                emitError(QAbstractSocket::UnknownSocketError, i18n("Unknown error"));
                return;
            }
        }

        // if we got here, we succeeded in connecting
        if (!setNonBlocking(fd)) {
            ::close(fd);
            emitError(QAbstractSocket::UnknownSocketError, i18n("Could not set non-blocking mode"));
            return;
        }

        // all is good
        peerPath = path;
        type = aType;

        // setSocketDescriptor emits stateChanged
        q->setSocketDescriptor(fd, QAbstractSocket::ConnectedState, openMode);
        emit q->connected();
    } else {
        emitError(QAbstractSocket::UnsupportedSocketOperationError,
                  i18n("The socket operation is not supported"));
    }
}

bool KLocalSocketServerPrivate::listen(const QString &path, KLocalSocket::LocalSocketType aType)
{
    qDeleteAll(pendingConnections);
    pendingConnections.clear();

    if (aType == KLocalSocket::UnixSocket || aType == KLocalSocket::AbstractUnixSocket) {
        KSockaddrUn addr(path, aType);
        if (!addr.ok()) {
            emitError(QAbstractSocket::NetworkError, i18n("Specified socket path is invalid"));
            return false;
        }

        // create the socket
        descriptor = kSocket(AF_UNIX, SOCK_STREAM, 0);
        if (descriptor == -1) {
            // failed
            emitError(QAbstractSocket::UnsupportedSocketOperationError,
                      i18n("The socket operation is not supported"));
            return false;
        }

        // try to bind to the address
        localPath = path;
        if (kBind(descriptor, addr.address(), addr.length()) == -1 ||
            kListen(descriptor, 5) == -1) {
            int error = errno;
            close();

            switch (error) {
            case EACCES:
                emitError(QAbstractSocket::SocketAccessError, i18n("Permission denied"));
                return false;

            case EADDRINUSE:
                emitError(QAbstractSocket::AddressInUseError, i18n("Address is already in use"));
                return false;

            case ELOOP:
            case ENAMETOOLONG:
                emitError(QAbstractSocket::NetworkError, i18n("Path cannot be used"));
                return false;

            case ENOENT:
                emitError(QAbstractSocket::HostNotFoundError, i18n("No such file or directory"));
                return false;

            case ENOTDIR:
                emitError(QAbstractSocket::HostNotFoundError, i18n("Not a directory"));
                return false;

            case EROFS:
                emitError(QAbstractSocket::SocketResourceError, i18n("Read-only filesystem"));
                return false;

            default:
                emitError(QAbstractSocket::UnknownSocketError, i18n("Unknown error"));
                return false;
            }
        }

        // if we got here, we succeeded in connecting
        if (!setNonBlocking(descriptor)) {
            close();
            emitError(QAbstractSocket::UnknownSocketError, i18n("Could not set non-blocking mode"));
            return false;
        }

        // done
        state = QAbstractSocket::ListeningState;
        type = aType;
        readNotifier = new QSocketNotifier(descriptor, QSocketNotifier::Read, q);
        readNotifier->setEnabled(maxPendingConnections > 0);
        QObject::connect(readNotifier, SIGNAL(activated(int)),
                         q, SLOT(_k_newConnectionActivity()));
        return true;
    }

    return false;
}

void KLocalSocketServerPrivate::close()
{
    if (descriptor != -1)
        ::close(descriptor);
    descriptor = -1;

    delete readNotifier;
    readNotifier = 0;

    if (type == KLocalSocket::UnixSocket)
        QFile::remove(localPath);
    localPath.clear();
    type = KLocalSocket::UnknownLocalSocketType;

    state = QAbstractSocket::UnconnectedState;
    error = QAbstractSocket::UnknownSocketError;
    errorString.clear();
}

bool KLocalSocketServerPrivate::waitForNewConnection(int msec, bool *timedOut)
{
    timeval tv;
    tv.tv_sec = msec / 1000;
    tv.tv_usec = (msec % 1000) * 1000;

    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(descriptor, &readset);

    while (descriptor != -1) {
        int code = ::select(descriptor + 1, &readset, 0, 0, &tv);
        if (code == -1 && errno == EINTR) {
            // interrupted
            continue;
        } else if (code == -1) {
            // error
            emitError(QAbstractSocket::UnknownSocketError, i18n("Unknown socket error"));
            close();
            return false;
        } else if (code == 0) {
            // timed out
            if (timedOut)
                *timedOut = true;
            return false;
        }

        // we must've got a connection. At least, there's activity.
        if (processSocketActivity()) {
            if (timedOut)
                *timedOut = false;
            return true;
        }
    }
    return false;
}

bool KLocalSocketServerPrivate::processSocketActivity()
{
    // we got a read notification in our socket
    // see if we can accept anything
    int newDescriptor = kAccept(descriptor);
    if (newDescriptor == -1) {
        switch (errno) {
        case EAGAIN:
            // shouldn't have happened, but it's ok
            return false;       // no new socket

        default:
            emitError(QAbstractSocket::UnknownSocketError, i18n("Unknown socket error"));
            // fall through
        }

        close();
        return false;
    }

    q->incomingConnection(newDescriptor);
    readNotifier->setEnabled(pendingConnections.size() < maxPendingConnections);
    return true;
}

void KLocalSocketServerPrivate::_k_newConnectionActivity()
{
    if (descriptor == -1)
        return;

    processSocketActivity();
}
