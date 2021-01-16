/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_COMMANDS_P_H
#define KIO_COMMANDS_P_H

#include "kiocore_export.h"

namespace KIO
{
/**
 * @internal
 * Commands that can be invoked by a job.
 */
enum Command {
    CMD_HOST = '0', // 48
    CMD_CONNECT = '1', // 49
    CMD_DISCONNECT = '2', // 50
    CMD_SLAVE_STATUS = '3', // 51
    CMD_SLAVE_CONNECT = '4', // 52
    CMD_SLAVE_HOLD = '5', // 53
    CMD_NONE = 'A', // 65
    CMD_TESTDIR = 'B', // 66   TODO KF6 REMOVE
    CMD_GET = 'C', // 67
    CMD_PUT = 'D', // 68
    CMD_STAT = 'E', // 69
    CMD_MIMETYPE = 'F', // 70
    CMD_LISTDIR = 'G', // 71
    CMD_MKDIR = 'H', // 72
    CMD_RENAME = 'I', // 73
    CMD_COPY = 'J', // 74
    CMD_DEL = 'K', // 75
    CMD_CHMOD = 'L', // 76
    CMD_SPECIAL = 'M', // 77
    CMD_SETMODIFICATIONTIME = 'N', // 78
    CMD_REPARSECONFIGURATION = 'O', // 79
    CMD_META_DATA = 'P', // 80
    CMD_SYMLINK = 'Q', // 81
    CMD_SUBURL = 'R', // 82  Inform the slave about the url it is streaming on.
    CMD_MESSAGEBOXANSWER = 'S', // 83
    CMD_RESUMEANSWER = 'T', // 84
    CMD_CONFIG = 'U', // 85
    CMD_MULTI_GET = 'V', // 86
    CMD_SETLINKDEST = 'W', // 87
    CMD_OPEN = 'X', // 88
    CMD_CHOWN = 'Y', // 89
    CMD_READ = 'Z', // 90
    CMD_WRITE = 91,
    CMD_SEEK = 92,
    CMD_CLOSE = 93,
    CMD_HOST_INFO = 94,
    CMD_FILESYSTEMFREESPACE = 95,
    CMD_TRUNCATE = 96,
                    // Add new ones here once a release is done, to avoid breaking binary compatibility.
                    // Note that protocol-specific commands shouldn't be added here, but should use special.
};

} // namespace

#endif

