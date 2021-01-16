/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 Alex Richardson <arichardson.kde@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KIO_KIOGLOBAL_P_H
#define KIO_KIOGLOBAL_P_H

#include <qplatformdefs.h>
#include "kiocore_export.h"

#include <KUser>

#ifdef Q_OS_WIN
// windows just sets the mode_t access rights bits to the same value for user+group+other.
// This means using the Linux values here is fine.
#ifndef S_IRUSR
#define S_IRUSR 0400
#endif
#ifndef S_IRGRP
#define S_IRGRP 0040
#endif
#ifndef S_IROTH
#define S_IROTH 0004
#endif

#ifndef S_IWUSR
#define S_IWUSR 0200
#endif
#ifndef S_IWGRP
#define S_IWGRP 0020
#endif
#ifndef S_IWOTH
#define S_IWOTH 0002
#endif

#ifndef S_IXUSR
#define S_IXUSR 0100
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001
#endif

#ifndef S_IRWXU
#define S_IRWXU S_IRUSR | S_IWUSR | S_IXUSR
#endif
#ifndef S_IRWXG
#define S_IRWXG S_IRGRP | S_IWGRP | S_IXGRP
#endif
#ifndef S_IRWXO
#define S_IRWXO S_IROTH | S_IWOTH | S_IXOTH
#endif
Q_STATIC_ASSERT(S_IRUSR == _S_IREAD && S_IWUSR == _S_IWRITE && S_IXUSR == _S_IEXEC);

// these three will never be set in st_mode
#ifndef S_ISUID
#define S_ISUID 04000 // SUID bit does not exist on windows
#endif
#ifndef S_ISGID
#define S_ISGID 02000 // SGID bit does not exist on windows
#endif
#ifndef S_ISVTX
#define S_ISVTX 01000 // sticky bit does not exist on windows
#endif

// Windows does not have S_IFBLK and S_IFSOCK, just use the Linux values, they won't conflict
#ifndef S_IFBLK
#define S_IFBLK  0060000
#endif
#ifndef S_IFSOCK
#define S_IFSOCK 0140000
#endif
/** performs a QT_STAT and add QT_STAT_LNK to st_mode if the path is a symlink */
KIOCORE_EXPORT int kio_windows_lstat(const char* path, QT_STATBUF* buffer);

#ifndef QT_LSTAT
#define QT_LSTAT kio_windows_lstat
#endif

#ifndef QT_STAT_LNK
#       define QT_STAT_LNK 0120000
#endif // QT_STAT_LNK

#endif //Q_OS_WIN

namespace KIOPrivate {
    /** @return true if the process with given PID is currently running */
    KIOCORE_EXPORT bool isProcessAlive(qint64 pid);
    /** Send a terminate signal (SIGTERM on UNIX) to the process with given PID. */
    KIOCORE_EXPORT void sendTerminateSignal(qint64 pid);

    enum SymlinkType {
        GuessSymlinkType,
        FileSymlink,
        DirectorySymlink,
    };

    /** Creates a symbolic link at @p destination pointing to @p source
     * Unlike UNIX, Windows needs to know whether the symlink points to a file or a directory
     * when creating the link. This information can be passed in @p type. If @p type is not given
     * the windows code will guess the type based on the source file.
     * @note On Windows this requires the current user to have the SeCreateSymbolicLink privilege which
     * is usually only given to administrators.
     * @return true on success, false on error
     */
    KIOCORE_EXPORT bool createSymlink(const QString &source, const QString &destination, SymlinkType type = GuessSymlinkType);

    /** Changes the ownership of @p file (like chown()) */
    KIOCORE_EXPORT bool changeOwnership(const QString& file, KUserId newOwner, KGroupId newGroup);

    /** Returns an icon name for a standard path,
     * e.g. folder-pictures for any path in QStandardPaths::PicturesLocation */
    QString iconForStandardPath(const QString &localDirectory);
}

#endif // KIO_KIOGLOBAL_P_H
