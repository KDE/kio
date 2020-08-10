/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef SHAREFD_P_H
#define SHAREFD_P_H

#include <sys/un.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stddef.h>

// fix SOCK_NONBLOCK for e.g. macOS
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

class SocketAddress
{
    const sockaddr_un addr;

public:
    explicit SocketAddress(const std::string &path)
        : addr(make_address(path))
    {
    }

    int length() const
    {
        return offsetof(struct sockaddr_un, sun_path) + strlen(addr.sun_path) + 1;
    }
    const sockaddr *address() const
    {
        return addr.sun_path[0] ? reinterpret_cast<const sockaddr*>(&addr) : nullptr;
    }

private:
    static sockaddr_un make_address(const std::string& path)
    {
        sockaddr_un a;
        memset(&a, 0, sizeof a);
        a.sun_family = AF_UNIX;
        const size_t pathSize = path.size();
        if (pathSize > 0 && pathSize < sizeof(a.sun_path) - 1) {
            memcpy(a.sun_path, path.c_str(), pathSize + 1);
        }
        return a;
    }
};

class FDMessageHeader
{
    char io_buf[2];
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    iovec io;
    msghdr msg;

public:
    FDMessageHeader()
        : io_buf{0}
        , cmsg_buf{0}
    {
        memset(&io, 0, sizeof io);
        io.iov_base = &io_buf;
        io.iov_len = sizeof io_buf;

        memset(&msg, 0, sizeof msg);
        msg.msg_iov = &io;
        msg.msg_iovlen = 1;
        msg.msg_control = &cmsg_buf;
        msg.msg_controllen = sizeof cmsg_buf;
    }

    msghdr *message()
    {
        return &msg;
    }

    cmsghdr *cmsgHeader()
    {
        return CMSG_FIRSTHDR(&msg);
    }
};

#endif
