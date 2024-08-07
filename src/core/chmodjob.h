// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
/*!
 * \class KIO::ChmodJob
 * \inheaderfile KIO/ChmodJob
 * \inmodule KIOCore
 *
 * \brief This job changes permissions on a list of files or directories,
 * optionally in a recursive manner.
 *
 * \sa KIO::chmod()
 */
class KIOCORE_EXPORT ChmodJob : public KIO::Job
{
    Q_OBJECT
public:
    ~ChmodJob() override;

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit ChmodJob(ChmodJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(ChmodJob)
};

/*!
 * \relates KIO::ChmodJob
 *
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
 * \a lstItems The file items representing several files or directories.
 *
 * \a permissions the permissions we want to set
 *
 * \a mask the bits we are allowed to change.
 * For instance, if mask is 0077, we don't change
 * the "user" bits, only "group" and "others".
 *
 * \a newOwner If non-empty, the new owner for the files
 *
 * \a newGroup If non-empty, the new group for the files
 *
 * \a recursive whether to open directories recursively
 *
 * \a flags We support HideProgressInfo here
 *
 * Returns the job handling the operation.
 */
KIOCORE_EXPORT ChmodJob *chmod(const KFileItemList &lstItems,
                               int permissions,
                               int mask,
                               const QString &newOwner,
                               const QString &newGroup,
                               bool recursive,
                               JobFlags flags = DefaultFlags);

}

#endif
