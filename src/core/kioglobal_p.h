/* This file is part of the KDE libraries
Copyright (C) 2014 Alex Richardson <arichardson.kde@gmail.com>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License version 2 as published by the Free Software Foundation.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#ifndef KIO_KIOGLOBAL_P_H
#define KIO_KIOGLOBAL_P_H

#include <QtGlobal>
#include <qplatformdefs.h>

#ifdef Q_OS_WIN
// windows just sets the mode_t access rights bits to the same value for user+group+other.
// This means using the Linux values here is fine.
#define S_IRUSR 0400
#define S_IRGRP 0040
#define S_IROTH 0004

#define S_IWUSR 0200
#define S_IWGRP 0020
#define S_IWOTH 0002

#define S_IXUSR 0100
#define S_IXGRP 0010
#define S_IXOTH 0001

#define S_IRWXU S_IRUSR | S_IWUSR | S_IXUSR
#define S_IRWXG S_IRGRP | S_IWGRP | S_IXGRP
#define S_IRWXO S_IROTH | S_IWOTH | S_IXOTH

Q_STATIC_ASSERT(S_IRUSR == _S_IREAD && S_IWUSR == _S_IWRITE && S_IXUSR == _S_IEXEC);

// these three will never be set in st_mode
#define S_ISUID 04000 // SUID bit does not exist on windows
#define S_ISGID 02000 // SGID bit does not exist on windows
#define S_ISVTX 01000 // sticky bit does not exist on windows

// Windows does not have S_IFBLK and S_IFSOCK, just use the Linux values, they won't conflict
#define S_IFBLK  0060000
#define S_IFSOCK 0140000

#endif //Q_OS_WIN

namespace KIOPrivate {
    /** @return true if the process with given PID is currently running */
    bool isProcessAlive(qint64 pid);
    /** Send a terminate signal (SIGTERM on UNIX) to the process with given PID. */
    void sendTerminateSignal(qint64 pid);
}

#endif // KIO_KIOGLOBAL_P_H
