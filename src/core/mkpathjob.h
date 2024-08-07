/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MKPATHJOB_H
#define MKPATHJOB_H

#include <QUrl>

#include "job_base.h"
#include "kiocore_export.h"

namespace KIO
{
class MkpathJobPrivate;
/*!
 * \class KIO::MkpathJob
 * \inheaderfile KIO/MkpathJob
 * \inmodule KIOCore
 *
 * \brief A KIO job that creates a directory, after creating all parent
 * directories necessary for this.
 *
 * \sa KIO::mkpath()
 * \sa KIO::mkdir()
 * \since 5.4
 */
class KIOCORE_EXPORT MkpathJob : public Job
{
    Q_OBJECT

public:
    ~MkpathJob() override;

Q_SIGNALS:
    /*!
     * Signals that a directory was created.
     */
    void directoryCreated(const QUrl &url);

protected Q_SLOTS:
    void slotResult(KJob *job) override;

protected:
    KIOCORE_NO_EXPORT explicit MkpathJob(MkpathJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(MkpathJob)
};

/*!
 * \relates KIO::MkpathJob
 *
 * Creates a directory, creating parent directories as needed.
 * Unlike KIO::mkdir(), the job will succeed if the directory exists already.
 *
 * \a url The URL of the directory to create.
 *
 * \a baseUrl Optionally, the URL to start from, which is known to exist
 * (e.g. the directory currently listed).
 *
 * \a flags mkpath() supports HideProgressInfo.
 *
 * If \a baseUrl is not an ancestor of \a url, \a baseUrl will be ignored.
 *
 * Returns a pointer to the job handling the operation.
 * \since 5.4
 */
KIOCORE_EXPORT MkpathJob *mkpath(const QUrl &url, const QUrl &baseUrl = QUrl(), JobFlags flags = DefaultFlags);

}

#endif /* MKPATHJOB_H */
