// -*- c++ -*-
/* This file is part of the KDE libraries
    Copyright (C) 2000 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KIO_CHMODJOB_H
#define KIO_CHMODJOB_H

#include "global.h"
#include "job_base.h"
#include "kiocore_export.h"
#include <kfileitem.h>

namespace KIO
{

class ChmodJobPrivate;
/**
 * @class KIO::ChmodJob chmodjob.h <KIO/ChmodJob>
 *
 * This job changes permissions on a list of files or directories,
 * optionally in a recursive manner.
 * @see KIO::chmod()
 */
class KIOCORE_EXPORT ChmodJob : public KIO::Job
{
    Q_OBJECT
public:
    virtual ~ChmodJob();

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    ChmodJob(ChmodJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void _k_slotEntries(KIO::Job *, const KIO::UDSEntryList &))
    Q_PRIVATE_SLOT(d_func(), void _k_processList())
    Q_PRIVATE_SLOT(d_func(), void _k_chmodNextFile())
    Q_DECLARE_PRIVATE(ChmodJob)
};

/**
 * Creates a job that changes permissions/ownership on several files or directories,
 * optionally recursively.
 * This version of chmod uses a KFileItemList so that it directly knows
 * what to do with the items. TODO: a version that takes a QList<QUrl>,
 * and a general job that stats each url and returns a KFileItemList.
 *
 * Note that change of ownership is only supported for local files.
 *
 * Inside directories, the "x" bits will only be changed for files that had
 * at least one "x" bit before, and for directories.
 * This emulates the behavior of chmod +X.
 *
 * @param lstItems The file items representing several files or directories.
 * @param permissions the permissions we want to set
 * @param mask the bits we are allowed to change.
 * For instance, if mask is 0077, we don't change
 * the "user" bits, only "group" and "others".
 * @param newOwner If non-empty, the new owner for the files
 * @param newGroup If non-empty, the new group for the files
 * @param recursive whether to open directories recursively
 * @param flags We support HideProgressInfo here
 * @return The job handling the operation.
 */
KIOCORE_EXPORT ChmodJob *chmod(const KFileItemList &lstItems, int permissions, int mask,
                               const QString &newOwner, const QString &newGroup,
                               bool recursive, JobFlags flags = DefaultFlags);

}

#endif
