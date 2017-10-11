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

#include <unistd.h>
#include <sys/socket.h>

#include "../sharefd_p.h"
#include "fdsender.h"

FdSender::FdSender(const std::string &path)
        : m_socketDes(-1)
{
    m_socketDes = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_socketDes == -1) {
        std::cerr << "socket error:" << strerror(errno) << std::endl;
        return;
    }

    SocketAddress addr(path);
    if (::connect(m_socketDes, addr.address(), addr.length()) != 0) {
        std::cerr << "connection error:" << strerror(errno) << std::endl;
        ::close(m_socketDes);
        m_socketDes = -1;
        return;
    }
}

FdSender::~FdSender()
{
    if (m_socketDes >= 0) {
        ::close(m_socketDes);
    }
}

bool FdSender::sendFileDescriptor(int fd)
{
    FDMessageHeader msg;
    cmsghdr *cmsg = msg.cmsgHeader();
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_level = SOL_SOCKET;
    memcpy(CMSG_DATA(cmsg), &fd, sizeof fd);
    bool success = sendmsg(m_socketDes, msg.message(), 0) == 2;
    ::close(m_socketDes);
    return success;
}

bool FdSender::isConnected() const
{
    return m_socketDes >= 0;
}
