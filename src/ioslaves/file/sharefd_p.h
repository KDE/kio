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

#include <sys/un.h>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>

// fix SOCK_NONBLOCK for e.g. macOS
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

class SocketAddress
{
    const sockaddr_un addr;

public:
    SocketAddress(const std::string &path)
        : addr(make_address(path))
    {
    }

    int length() const
    {
        return sizeof addr;
    }
    const sockaddr *address() const
    {
        return reinterpret_cast<const sockaddr*>(&addr);
    }

private:
    static sockaddr_un make_address(const std::string& path)
    {
        sockaddr_un a{ AF_UNIX, {0}};
        std::string finalPath = "/tmp/" + path;
#ifdef __linux__
        ::strcpy(&a.sun_path[1], finalPath.c_str());
#else
        ::strcpy(a.sun_path, finalPath.c_str());
        ::unlink(finalPath.c_str());
#endif
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
        , io{io_buf, sizeof io_buf}
        , msg{nullptr, 0,  &io, 1,  &cmsg_buf, sizeof cmsg_buf, 0}
    {
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
