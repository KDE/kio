/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "fdreceiver.h"

#include <QSocketNotifier>
#include <errno.h>

#include "sharefd_p.h"

FdReceiver::FdReceiver(const std::string &path, QObject *parent)
          : QObject(parent)
          , m_readNotifier(nullptr)
          , m_path(path)
          , m_socketDes(-1)
          , m_fileDes(-1)
{
    const SocketAddress addr(m_path);
    if (!addr.address()) {
        std::cerr << "Invalid socket address:" << m_path << std::endl;
        return;
    }

    m_socketDes = ::socket(AF_LOCAL, SOCK_STREAM|SOCK_NONBLOCK, 0);
    if (m_socketDes == -1) {
        std::cerr << "socket error:" << strerror(errno) << std::endl;
        return;
    }

    ::unlink(m_path.c_str());
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
    ::unlink(m_path.c_str());
}

bool FdReceiver::isListening() const
{
    return m_socketDes >= 0 && m_readNotifier;
}

void FdReceiver::receiveFileDescriptor()
{
    int client = ::accept(m_socketDes, nullptr, nullptr);
    if (client > 0) {
        FDMessageHeader msg;
        if (::recvmsg(client, msg.message(), 0) == 2) {
            ::memcpy(&m_fileDes, CMSG_DATA(msg.cmsgHeader()), sizeof m_fileDes);
        }
        ::close(client);
    }
}

int FdReceiver::fileDescriptor() const
{
    return m_fileDes;
}
