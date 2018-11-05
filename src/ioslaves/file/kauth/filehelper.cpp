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

#include "filehelper.h"

#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "fdsender.h"
#include "../file_p.h"

static bool sendFileDescriptor(int fd, const char *socketPath)
{
    FdSender fdSender(socketPath);
    if (fdSender.isConnected() && fdSender.sendFileDescriptor(fd)) {
        return true;
    }
    return false;
}

ActionReply FileHelper::exec(const QVariantMap &args)
{
    ActionReply reply;
    QByteArray data = args["arguments"].toByteArray();
    QDataStream in(data);
    int action;
    QVariant arg1, arg2, arg3, arg4;
    in >> action >> arg1 >> arg2 >> arg3 >> arg4;

    // the path of an existing or a new file/dir upon which the method will operate
    const QByteArray path = arg1.toByteArray();

    switch(action) {
    case CHMOD: {
        int mode = arg2.toInt();
        if (chmod(path.data(), mode) == 0) {
            return reply;
        }
        break;
    }
    case CHOWN: {
        int uid = arg2.toInt();
        int gid = arg3.toInt();
        if (chown(path.data(), uid, gid) == 0) {
            return reply;
        }
        break;
    }
    case DEL: {
        if (unlink(path.data()) == 0) {
            return reply;
        }
        break;
    }
    case MKDIR: {
        if (mkdir(path.data(), 0777) == 0) {
            return reply;
        }
        break;
    }
    case OPEN: {
        int oflags = arg2.toInt();
        int mode = arg3.toInt();
        int fd = open(path.data(), oflags, mode);
        bool success = (fd != -1) && sendFileDescriptor(fd, arg4.toByteArray().constData());
        close(fd);
        if (success) {
            return reply;
        }
        break;
    }
    case OPENDIR: {
        DIR *dp = opendir(path.data());
        bool success = false;
        if (dp) {
            int fd = dirfd(dp);
            success = (fd != -1) && sendFileDescriptor(fd, arg4.toByteArray().constData());
            closedir(dp);
            if (success) {
                return reply;
            }
        }
        break;
    }
    case RENAME: {
        const QByteArray newName = arg2.toByteArray();
        if (rename(path.data(), newName.data()) == 0) {
            return reply;
        }
        break;
    }
    case RMDIR: {
        if (rmdir(path.data()) == 0) {
            return reply;
        }
        break;
    }
    case SYMLINK: {
        const QByteArray target = arg2.toByteArray();
        if (symlink(target.data(), path.data()) == 0) {
            return reply;
        }
        break;
    }
    case UTIME: {
        utimbuf ut;
        ut.actime = arg2.toULongLong();
        ut.modtime = arg3.toULongLong();
        if (utime(path.data(), &ut) == 0) {
            return reply;
        }
        break;
    }
    };

    reply.setError(errno ? errno : -1);
    return reply;
}

KAUTH_HELPER_MAIN("org.kde.kio.file", FileHelper)
