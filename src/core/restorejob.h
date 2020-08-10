/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
