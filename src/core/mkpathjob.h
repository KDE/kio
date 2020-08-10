/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MKPATHJOB_H
#define MKPATHJOB_H

#include <QUrl>

#include "kiocore_export.h"
#include "job_base.h"

namespace KIO
{

class MkpathJobPrivate;
/**
 * @class KIO::MkpathJob mkpathjob.h <KIO/MkpathJob>
 *
 * A KIO job that creates a directory, after creating all parent
 * directories necessary for this.
 *
 * @see KIO::mkpath(), KIO::mkdir()
 * @since 5.4
 */
class KIOCORE_EXPORT MkpathJob : public Job
{
    Q_OBJECT

public:
    ~MkpathJob() override;

Q_SIGNALS:
    /**
     * Signals that a directory was created.
     */
    void directoryCreated(const QUrl &url);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    MkpathJob(MkpathJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotStart())
    Q_DECLARE_PRIVATE(MkpathJob)
};

/**
 * Creates a directory, creating parent directories as needed.
 * Unlike KIO::mkdir(), the job will succeed if the directory exists already.
 *
 * @param url The URL of the directory to create.
 * @param baseUrl Optionally, the URL to start from, which is known to exist
 * (e.g. the directory currently listed).
 * @param flags mkpath() supports HideProgressInfo.
 *
 * If @p baseUrl is not an ancestor of @p url, @p baseUrl will be ignored.
 *
 * @return A pointer to the job handling the operation.
 * @since 5.4
 */
KIOCORE_EXPORT MkpathJob *mkpath(const QUrl &url, const QUrl &baseUrl = QUrl(), JobFlags flags = DefaultFlags);

}

#endif /* MKPATHJOB_H */
