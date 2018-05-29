/* This file is part of the KDE libraries
    Copyright 2014  David Faure <faure@kde.org>

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

#ifndef KIO_RESTOREJOB_H
#define KIO_RESTOREJOB_H

#include <QObject>
#include <QUrl>

#include "kiocore_export.h"
#include "job_base.h"

namespace KIO
{

class RestoreJobPrivate;
/**
 * @class KIO::RestoreJob restorejob.h <KIO/RestoreJob>
 *
 * RestoreJob is used to restore files from the trash.
 * Don't create the job directly, but use KIO::restoreFromTrash().
 *
 * @see KIO::trash()
 * @see KIO::copy()
 * @since 5.2
 */
class KIOCORE_EXPORT RestoreJob : public Job
{
    Q_OBJECT

public:
    ~RestoreJob() override;

    /**
     * Returns the list of trash URLs to restore.
     */
    QList<QUrl> trashUrls() const;

Q_SIGNALS:

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    RestoreJob(RestoreJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotStart())

    Q_DECLARE_PRIVATE(RestoreJob)
};

/**
 * Restore a set of trashed files or directories.
 * @since 5.2
 *
 * @param urls the trash:/ URLs to restore. The trash implementation
 * will know where the files came from and will restore them to their
 * original location.
 *
 * @param flags restoreFromTrash() supports HideProgressInfo.
 *
 * @return the job handling the operation
 */
KIOCORE_EXPORT RestoreJob *restoreFromTrash(const QList<QUrl> &urls, JobFlags flags = DefaultFlags);

}

#endif
