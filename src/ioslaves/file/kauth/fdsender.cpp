/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fdsender.h"

#include "../sharefd_p.h"
#include <errno.h>
#include <string.h>

FdSender::FdSender(const std::string &path)
        : m_socketDes(-1)
{
    const SocketAddress addr(path);
    if (!addr.address()) {
        std::cerr << "Invalid socket address:" << path << std::endl;
        return;
    }

    m_socketDes = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (m_socketDes == -1) {
        std::cerr << "socket error:" << strerror(errno) << std::endl;
        return;
    }

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
