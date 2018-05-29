/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2014 David Faure <faure@kde.org>

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
