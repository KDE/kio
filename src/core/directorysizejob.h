/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000, 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DIRECTORYSIZEJOB_H
#define DIRECTORYSIZEJOB_H

#include "kiocore_export.h"
#include "job_base.h"
#include <kfileitem.h>

namespace KIO
{

class DirectorySizeJobPrivate;
/**
 * @class KIO::DirectorySizeJob directorysizejob.h <KIO/DirectorySizeJob>
 *
 * Computes a directory size (similar to "du", but doesn't give the same results
 * since we simply sum up the dir and file sizes, whereas du speaks disk blocks)
 *
 * Usage: see KIO::directorySize.
 */
class KIOCORE_EXPORT DirectorySizeJob : public KIO::Job
{
    Q_OBJECT

public:
    ~DirectorySizeJob() override;

public:
    /**
     * @return the size we found
     */
    KIO::filesize_t totalSize() const;

    /**
     * @return the total number of files (counting symlinks to files, sockets
     * and character devices as files) in this directory and all sub-directories
     */
    KIO::filesize_t totalFiles() const;

    /**
     * @return the total number of sub-directories found (not including the
     * directory the search started from and treating symlinks to directories
     * as directories)
     */
    KIO::filesize_t totalSubdirs() const;

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    DirectorySizeJob(DirectorySizeJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotEntries(KIO::Job *, const KIO::UDSEntryList &))
    Q_PRIVATE_SLOT(d_func(), void processNextItem())
    Q_DECLARE_PRIVATE(DirectorySizeJob)
};

/**
 * Computes a directory size (by doing a recursive listing).
 * Connect to the result signal (this is the preferred solution to avoid blocking the GUI),
 * or use exec() for a synchronous (blocking) calculation.
 *
 * This one lists a single directory.
 */
KIOCORE_EXPORT DirectorySizeJob *directorySize(const QUrl &directory);

/**
 * Computes a directory size (by doing a recursive listing).
 * Connect to the result signal (this is the preferred solution to avoid blocking the GUI),
 * or use exec() for a synchronous (blocking) calculation.
 *
 * This one lists the items from @p lstItems.
 * The reason we asks for items instead of just urls, is so that
 * we directly know if the item is a file or a directory,
 * and in case of a file, we already have its size.
 */
KIOCORE_EXPORT DirectorySizeJob *directorySize(const KFileItemList &lstItems);

}

#endif
