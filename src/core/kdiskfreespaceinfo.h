/*
    kdiskfreespaceinfo.h
    SPDX-FileCopyrightText: 2008 Sebastian Trug <trueg@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KDISK_FREE_SPACE_INFO_H_
#define _KDISK_FREE_SPACE_INFO_H_

#include <QSharedDataPointer>
#include <QString>

#include "kiocore_export.h"
#include <kio/global.h>

class KDiskFreeSpaceInfoPrivate;

/**
 * \class KDiskFreeSpaceInfo kdiskfreespaceinfo.h KDiskFreeSpaceInfo
 *
 * \brief Determine the space left on an arbitrary partition.
 *
 * This class determines the free space left on the partition that holds a given
 * path.  This path can be the mount point or any file or directory on the
 * partition.
 *
 * To find how much space is available on the partition containing @p path,
 * simply do the following:
 *
 * \code
 * KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo( path );
 * if( info.isValid() )
 *    doSomething( info.available() );
 * \endcode
 *
 * \author Sebastian Trueg <trueg@kde.org>
 *
 * \since 4.2
 */
class KIOCORE_EXPORT KDiskFreeSpaceInfo
{
public:
    /**
     * Copy constructor
     */
    KDiskFreeSpaceInfo(const KDiskFreeSpaceInfo &);

    /**
     * Destructor
     */
    ~KDiskFreeSpaceInfo();

    /**
     * Assignment operator
     */
    KDiskFreeSpaceInfo &operator=(const KDiskFreeSpaceInfo &);

    /**
     * \return \p true if the available disk space was successfully
     * determined and the values from mountPoint(), size(), available(),
     * and used() are valid. \p false otherwise.
     */
    bool isValid() const;

    /**
     * The mount point of the partition the requested path points to
     *
     * Only valid if isValid() returns \p true.
     */
    QString mountPoint() const;

    /**
     * The total size of the partition mounted at mountPoint()
     *
     * Only valid if isValid() returns \p true.
     *
     * \return Total size of the requested partition in bytes.
     */
    KIO::filesize_t size() const;

    /**
     * The available space in the partition mounted at mountPoint()
     *
     * Only valid if isValid() returns \p true.
     *
     * \return Available space left on the requested partition in bytes.
     */
    KIO::filesize_t available() const;

    /**
     * The used space in the partition mounted at mountPoint()
     *
     * Only valid if isValid() returns \p true.
     *
     * \return Used space on the requested partition in bytes.
     */
    KIO::filesize_t used() const;

    /**
     * Static method used to determine the free disk space.
     *
     * \param path An arbitrary path. The available space will be
     * determined for the partition containing path.
     *
     * Check isValid() to see if the process was successful. Then
     * use mountPoint(), size(), available(), and used() to access
     * the requested values.
     */
    static KDiskFreeSpaceInfo freeSpaceInfo(const QString &path);

private:
    KDiskFreeSpaceInfo();

    QSharedDataPointer<KDiskFreeSpaceInfoPrivate> d;
};

#endif

