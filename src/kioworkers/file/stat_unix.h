/*
    SPDX-FileCopyrightText: 2024 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef STAT_UNIX_H
#define STAT_UNIX_H

#include "kioglobal_p.h"
#include "qplatformdefs.h"

#include <config-stat-unix.h>
#include <kio/statjob.h>

#if HAVE_STATX
#include <sys/stat.h>
#include <sys/sysmacros.h> // for makedev()
#endif

#ifdef Q_OS_WIN
// QT_LSTAT on Windows
#include "kioglobal_p.h"
#endif

#if HAVE_STATX
// statx syscall is available
inline int LSTAT(const char *path, struct statx *buff, KIO::StatDetails details)
{
    uint32_t mask = 0;
    if (details & KIO::StatBasic) {
        // filename, access, type, size, linkdest
        mask |= STATX_SIZE | STATX_TYPE;
    }
    if (details & KIO::StatUser) {
        // uid, gid
        mask |= STATX_UID | STATX_GID;
    }
    if (details & KIO::StatTime) {
        // atime, mtime, btime
        mask |= STATX_ATIME | STATX_MTIME | STATX_BTIME;
    }
    if (details & KIO::StatInode) {
        // dev, inode
        mask |= STATX_INO;
    }
    return statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, mask, buff);
}
inline int STAT(const char *path, struct statx *buff, const KIO::StatDetails &details)
{
    uint32_t mask = 0;
    // KIO::StatAcl needs type
    if (details & (KIO::StatBasic | KIO::StatAcl | KIO::StatResolveSymlink)) {
        // filename, access, type
        mask |= STATX_TYPE;
    }
    if (details & (KIO::StatBasic | KIO::StatResolveSymlink)) {
        // size, linkdest
        mask |= STATX_SIZE;
    }
    if (details & KIO::StatUser) {
        // uid, gid
        mask |= STATX_UID | STATX_GID;
    }
    if (details & KIO::StatTime) {
        // atime, mtime, btime
        mask |= STATX_ATIME | STATX_MTIME | STATX_BTIME;
    }
    // KIO::Inode is ignored as when STAT is called, the entry inode field has already been filled
    return statx(AT_FDCWD, path, AT_STATX_SYNC_AS_STAT, mask, buff);
}
inline static uint16_t stat_mode(const struct statx &buf)
{
    return buf.stx_mode;
}
inline static dev_t stat_dev(const struct statx &buf)
{
    return makedev(buf.stx_dev_major, buf.stx_dev_minor);
}
inline static uint64_t stat_ino(const struct statx &buf)
{
    return buf.stx_ino;
}
inline static size_t stat_size(const struct statx &buf)
{
    return buf.stx_size;
}
inline static uint32_t stat_uid(const struct statx &buf)
{
    return buf.stx_uid;
}
inline static uint32_t stat_gid(const struct statx &buf)
{
    return buf.stx_gid;
}
inline static int64_t stat_atime(const struct statx &buf)
{
    return buf.stx_atime.tv_sec;
}
inline static int64_t stat_mtime(const struct statx &buf)
{
    return buf.stx_mtime.tv_sec;
}
#else
// regular stat struct
inline int LSTAT(const char *path, QT_STATBUF *buff, KIO::StatDetails details)
{
    Q_UNUSED(details)
    return QT_LSTAT(path, buff);
}
inline int STAT(const char *path, QT_STATBUF *buff, KIO::StatDetails details)
{
    Q_UNUSED(details)
    return QT_STAT(path, buff);
}
inline static mode_t stat_mode(const QT_STATBUF &buf)
{
    return buf.st_mode;
}
inline static dev_t stat_dev(const QT_STATBUF &buf)
{
    return buf.st_dev;
}
inline static ino_t stat_ino(const QT_STATBUF &buf)
{
    return buf.st_ino;
}
inline static off_t stat_size(const QT_STATBUF &buf)
{
    return buf.st_size;
}
inline static uint32_t stat_uid(const QT_STATBUF &buf)
{
    return buf.st_uid;
}
inline static uint32_t stat_gid(const QT_STATBUF &buf)
{
    return buf.st_gid;
}
inline static time_t stat_atime(const QT_STATBUF &buf)
{
    return buf.st_atime;
}
inline static time_t stat_mtime(const QT_STATBUF &buf)
{
    return buf.st_mtime;
}
#endif

#endif // STAT_UNIX_H
