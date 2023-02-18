/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef FILE_P_H
#define FILE_P_H

#include <cerrno>

enum ActionType {
    UNKNOWN,
    CHMOD = 1,
    CHOWN,
    DEL,
    MKDIR,
    OPEN,
    OPENDIR,
    RENAME,
    RMDIR,
    SYMLINK,
    UTIME,
    COPY,
};

#endif
