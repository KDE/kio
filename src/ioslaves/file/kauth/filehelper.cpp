/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "filehelper.h"

#include <utime.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "fdsender.h"
#include "../file_p.h"

#ifndef O_PATH
#define O_PATH O_RDONLY
#endif

struct Privilege {
    uid_t uid;
    gid_t gid;
};

static ActionType intToActionType(int action)
{
    switch (action) {
        case 1: return CHMOD;
        case 2: return CHOWN;
        case 3: return DEL;
        case 4: return MKDIR;
        case 5: return OPEN;
        case 6: return OPENDIR;
        case 7: return RENAME;
        case 8: return RMDIR;
        case 9: return SYMLINK;
        case 10: return UTIME;
        default: return UNKNOWN;
    }
}

static bool sendFileDescriptor(int fd, const char *socketPath)
{
    FdSender fdSender(socketPath);
    if (fdSender.isConnected() && fdSender.sendFileDescriptor(fd)) {
        return true;
    }
    return false;
}

static Privilege *getTargetPrivilege(int target_fd)
{
    struct stat buf;
    if (fstat(target_fd, &buf) == -1) {
        return nullptr;
    }
    return new Privilege{buf.st_uid, buf.st_gid};
}

static bool dropPrivilege(Privilege *p)
{
    if (!p) {
        return false;
    }

    uid_t newuid = p->uid;
    gid_t newgid = p->gid;

    //drop ancillary groups first because it requires root privileges.
    if (setgroups(1, &newgid) == -1) {
        return false;
    }
    //change effective gid and uid.
    if (setegid(newgid) == -1 || seteuid(newuid) == -1) {
        return false;
    }

    return true;
}

static void gainPrivilege(Privilege *p)
{
    if (!p) {
        return;
    }

    uid_t olduid = p->uid;
    gid_t oldgid = p->gid;

    seteuid(olduid);
    setegid(oldgid);
    setgroups(1, &oldgid);
}

ActionReply FileHelper::exec(const QVariantMap &args)
{
    ActionReply reply;
    QByteArray data = args[QStringLiteral("arguments")].toByteArray();
    QDataStream in(data);
    int act;
    QVariant arg1, arg2, arg3, arg4;
    in >> act >> arg1 >> arg2 >> arg3 >> arg4; // act=action, arg1=source file, arg$n=dest file, mode, uid, gid, etc.
    ActionType action = intToActionType(act);

    //chown requires privilege (CAP_CHOWN) to change user but the group can be changed without it.
    //It's much simpler to do it in one privileged call.
    if (action == CHOWN) {
        if (lchown(arg1.toByteArray().constData(), arg2.toInt(), arg3.toInt()) == -1) {
            reply.setError(errno);
        }
        return reply;
    }

    QByteArray tempPath1, tempPath2;
    tempPath1 = tempPath2 = arg1.toByteArray();
    const QByteArray parentDir = dirname(tempPath1.data());
    const QByteArray baseName = basename(tempPath2.data());
    int parent_fd = -1, base_fd = -1;

    if ((parent_fd = open(parentDir.data(), O_DIRECTORY | O_PATH | O_NOFOLLOW)) == -1) {
        reply.setError(errno);
        return reply;
    }

    Privilege *origPrivilege = new Privilege{geteuid(), getegid()};
    Privilege *targetPrivilege = nullptr;

    if (action != CHMOD && action != UTIME) {
        targetPrivilege = getTargetPrivilege(parent_fd);
    } else {
        if ((base_fd = openat(parent_fd, baseName.data(), O_NOFOLLOW)) != -1) {
            targetPrivilege = getTargetPrivilege(base_fd);
        } else {
            reply.setError(errno);
        }
    }

    if (dropPrivilege(targetPrivilege)) {
        switch(action) {
            case CHMOD: {
                int mode = arg2.toInt();
                if (fchmod(base_fd, mode) == -1) {
                    reply.setError(errno);
                }
                close(base_fd);
                break;
            }

            case DEL:
            case RMDIR: {
                int flags = 0;
                if (action == RMDIR) {
                    flags |= AT_REMOVEDIR;
                }
                if (unlinkat(parent_fd, baseName.data(), flags) == -1) {
                    reply.setError(errno);
                }
                break;
            }

            case MKDIR: {
                int mode = arg2.toInt();
                if (mkdirat(parent_fd, baseName.data(), mode) == -1) {
                    reply.setError(errno);
                }
                break;
            }

            case OPEN:
            case OPENDIR: {
                int oflags = arg2.toInt();
                int mode = arg3.toInt();
                int extraFlag = O_NOFOLLOW;
                if (action == OPENDIR) {
                    extraFlag |= O_DIRECTORY;
                }
                if (int fd = openat(parent_fd, baseName.data(), oflags | extraFlag, mode) != -1) {
                    gainPrivilege(origPrivilege);
                    if (!sendFileDescriptor(fd, arg4.toByteArray().constData())) {
                        reply.setError(errno);
                    }
                } else {
                    reply.setError(errno);
                }
                break;
            }

            case RENAME: {
                tempPath1 = tempPath2 = arg2.toByteArray();
                const QByteArray newParentDir = dirname(tempPath1.data());
                const QByteArray newBaseName = basename(tempPath2.data());
                int new_parent_fd = open(newParentDir.constData(), O_DIRECTORY | O_PATH | O_NOFOLLOW);
                if (renameat(parent_fd, baseName.data(), new_parent_fd, newBaseName.constData()) == -1) {
                        reply.setError(errno);
                }
                close(new_parent_fd);
                break;
            }

            case SYMLINK: {
                const QByteArray target = arg2.toByteArray();
                if (symlinkat(target.data(), parent_fd, baseName.data()) == -1) {
                    reply.setError(errno);
                }
                break;
            }

            case UTIME: {
                timespec times[2];
                time_t actime = arg2.toULongLong();
                time_t modtime = arg3.toULongLong();
                times[0].tv_sec = actime / 1000;
                times[0].tv_nsec = actime * 1000;
                times[1].tv_sec = modtime / 1000;
                times[1].tv_nsec = modtime * 1000;
                if (futimens(base_fd, times) == -1) {
                    reply.setError(errno);
                }
                close(base_fd);
                break;
            }

            default:
                reply.setError(ENOTSUP);
                break;
        }
        gainPrivilege(origPrivilege);
    } else {
        reply.setError(errno);
    }

    if (origPrivilege) {
        delete origPrivilege;
    }
    if (targetPrivilege) {
        delete targetPrivilege;
    }
    close(parent_fd);
    return reply;
}

KAUTH_HELPER_MAIN("org.kde.kio.file", FileHelper)
