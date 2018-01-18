/***
    Copyright (C) 2017 by Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <QSocketNotifier>
#include <QFile>

#include "sharefd_p.h"
#include "fdreceiver.h"

FdReceiver::FdReceiver(const QString &path, QObject *parent)
          : QObject(parent)
          , m_readNotifier(nullptr)
          , m_socketDes(-1)
          , m_fileDes(-1)
          , m_path(path)
{
    SocketAddress addr(m_path.toStdString());
    if (!addr.address()) {
        std::cerr << "Invalid socket address" << std::endl;
        return;
    }

    m_socketDes = ::socket(AF_LOCAL, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if (m_socketDes == -1) {
        std::cerr << "socket error:" << strerror(errno) << std::endl;
        return;
    }

    if (bind(m_socketDes, addr.address(), addr.length()) != 0 || listen(m_socketDes, 5) != 0) {
        std::cerr << "bind/listen error:" << strerror(errno) << std::endl;
        ::close(m_socketDes);
        m_socketDes = -1;
        return;
    }

    m_readNotifier = new QSocketNotifier(m_socketDes, QSocketNotifier::Read, this);
    connect(m_readNotifier, &QSocketNotifier::activated, this, &FdReceiver::receiveFileDescriptor);
}

FdReceiver::~FdReceiver()
{
    if (m_socketDes >= 0) {
        ::close(m_socketDes);
    }
    ::unlink(QFile::encodeName(m_path).constData());
}

bool FdReceiver::isListening() const
{
    return m_socketDes >= 0 && m_readNotifier;
}

void FdReceiver::receiveFileDescriptor()
{
    int client = ::accept(m_socketDes, NULL, NULL);
    if (client > 0) {
        // Receive fd only if socket owner is root
        bool acceptConnection = false;
#if defined(__linux__)
        ucred cred;
        socklen_t len = sizeof(cred);
        if (getsockopt(client, SOL_SOCKET, SO_PEERCRED, &cred, &len) == 0 && cred.uid == 0) {
            acceptConnection = true;
        }
#elif defined(__FreeBSD__) || defined(__APPLE__)
        uid_t uid;
        gid_t gid;
        if (getpeereid(m_socketDes, &uid, &gid) == 0 && uid == 0) {
            acceptConnection = true;
        }
#else
#error Cannot get socket credentials!
#endif
        if (acceptConnection) {
            FDMessageHeader msg;
            if (::recvmsg(client, msg.message(), 0) == 2) {
                ::memcpy(&m_fileDes, CMSG_DATA(msg.cmsgHeader()), sizeof m_fileDes);
            }
            ::close(client);
        }
    }
}

int FdReceiver::fileDescriptor() const
{
    return m_fileDes;
}
