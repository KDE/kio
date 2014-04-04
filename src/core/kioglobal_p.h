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
#include "kiocore_export.h"

#include <qplatformdefs.h>
#include <KUser>

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

/** performs a QT_STAT and add QT_STAT_LNK to st_mode if the path is a symlink */
KIOCORE_EXPORT int kio_windows_lstat(const char* path, QT_STATBUF* buffer);

#ifndef QT_LSTAT
#define QT_LSTAT kio_windows_lstat
#endif

#endif //Q_OS_WIN

namespace KIOPrivate {
    /** @return true if the process with given PID is currently running */
    KIOCORE_EXPORT bool isProcessAlive(qint64 pid);
    /** Send a terminate signal (SIGTERM on UNIX) to the process with given PID. */
    KIOCORE_EXPORT void sendTerminateSignal(qint64 pid);

    enum SymlinkType {
        GuessSymlinkType,
        FileSymlink,
        DirectorySymlink
    };

    /** Creates a symbolic link at @p destination pointing to @p source
     * Unlink UNIX, Windows needs to know whether the symlink points to a file or a directory
     * when creating the link. This information can be passed in @p type. If @p type is not given
     * the windows code will guess the type based on the source file.
     * @note On Windows this requires the current user to have the SeCreateSymbolicLink privilege which
     * is usually only given to administrators.
     * @return true on success, false on error
     */
    KIOCORE_EXPORT bool createSymlink(const QString &source, const QString &destination, SymlinkType type = GuessSymlinkType);

    /** Changes the ownership of @p file (like chown()) */
    KIOCORE_EXPORT bool changeOwnership(const QString& file, KUserId newOwner, KGroupId newGroup);
}

#endif // KIO_KIOGLOBAL_P_H
