/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000, 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DIRECTORYSIZEJOB_H
#define DIRECTORYSIZEJOB_H

#include "job_base.h"
#include "kiocore_export.h"
#include <kfileitem.h>

namespace KIO
{
class DirectorySizeJobPrivate;
/*!
 * \class KIO::DirectorySizeJob
 * \inheaderfile KIO/DirectorySizeJob
 * \inmodule KIOCore
 *
 * \brief Computes a directory size.
 *
 * Similar to "du", but doesn't give the same results
 * since we simply sum up the dir and file sizes, whereas du speaks disk blocks.
 *
 * \sa KIO::directorySize.
 */
class KIOCORE_EXPORT DirectorySizeJob : public KIO::Job
{
    Q_OBJECT

public:
    ~DirectorySizeJob() override;

public:
    /*!
     * Returns how many bytes of data the files we found hold. Directories do not add to it: the
     * room their list of entries needs is not data anybody put there, and it varies wildly between
     * filesystems, which made the same folder report a different size once copied. See
     * totalSizeOnDisk() for the space everything takes up.
     */
    KIO::filesize_t totalSize() const;

    /*!
     * Returns the space the files and directories we found take up on the storage they live on,
     * which is what the filesystem has allocated to them rather than how many bytes of data they
     * hold. Empty when the protocol has no way to tell, which is anything but local files. Zero is
     * an answer in its own right: a file that is entirely sparse holds data but occupies nothing.
     *
     * \since 6.30
     */
    std::optional<KIO::filesize_t> totalSizeOnDisk() const;

    /*!
     * Returns the total number of files (counting symlinks to files, sockets
     * and character devices as files) in this directory and all sub-directories
     */
    KIO::filesize_t totalFiles() const;

    /*!
     * Returns the total number of sub-directories found (not including the
     * directory the search started from and treating symlinks to directories
     * as directories)
     */
    KIO::filesize_t totalSubdirs() const;

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit DirectorySizeJob(DirectorySizeJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(DirectorySizeJob)
};

/*!
 * \relates KIO::DirectorySizeJob
 *
 * Computes a directory size (by doing a recursive listing).
 * Connect to the result signal (this is the preferred solution to avoid blocking the GUI),
 * or use exec() for a synchronous (blocking) calculation.
 *
 * This one lists a single directory.
 */
KIOCORE_EXPORT DirectorySizeJob *directorySize(const QUrl &directory);

/*!
 * \relates KIO::DirectorySizeJob
 *
 * Computes a directory size (by doing a recursive listing).
 * Connect to the result signal (this is the preferred solution to avoid blocking the GUI),
 * or use exec() for a synchronous (blocking) calculation.
 *
 * This one lists the items from \a lstItems.
 * The reason we asks for items instead of just urls, is so that
 * we directly know if the item is a file or a directory,
 * and in case of a file, we already have its size.
 */
KIOCORE_EXPORT DirectorySizeJob *directorySize(const KFileItemList &lstItems);

}

#endif
