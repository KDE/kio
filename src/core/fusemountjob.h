/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KIO_FUSEMOUNTJOB_H
#define KIO_FUSEMOUNTJOB_H

#include "kiocore_export.h"

#include <KJob>

#include <QUrl>

#include <memory>

namespace KIO
{
class FuseMountJobPrivate;
/*!
 * \class KIO::FuseMountJob
 * \inheaderfile KIO/FuseMountJob
 * \inmodule KIOCore
 *
 * \brief FuseMountJob makes a URL reachable through a local path using kio-fuse.
 *
 * This is useful for handing files from any KIO location to code that only
 * understands local paths, e.g. a library that expects a file name.
 *
 * The job asks the kio-fuse service to mount the URL and returns the local
 * path where it is reachable. The service is started on demand if it is
 * installed. A local file URL is returned as its path without mounting.
 *
 * If kio-fuse is not available, the job fails with KIO::ERR_UNSUPPORTED_ACTION,
 * which lets callers fall back to another approach, e.g. copying the file.
 * This is also the case when KIO was built without D-Bus support.
 *
 * \code
 *    auto *job = new KIO::FuseMountJob(url, this);
 *    connect(job, &KJob::result, this, [job]() {
 *        if (!job->error()) {
 *            const QString path = job->localPath();
 *            ...
 *        }
 *    });
 *    job->start();
 * \endcode
 *
 * \since 6.33
 */
class KIOCORE_EXPORT FuseMountJob : public KJob
{
    Q_OBJECT

public:
    /*!
     * Creates a FuseMountJob for \a url.
     */
    explicit FuseMountJob(const QUrl &url, QObject *parent = nullptr);

    ~FuseMountJob() override;

    /*!
     * Sets how long to wait for kio-fuse, in milliseconds, before failing
     * with KIO::ERR_SERVER_TIMEOUT.
     *
     * By default there is no timeout, since mounting can involve asking the
     * user for a password. Kill the job to stop waiting.
     *
     * Must be called before start().
     */
    void setTimeout(int msecs);

    /*!
     * Starts the job.
     */
    void start() override;

    /*!
     * Returns the URL the job was created for.
     */
    QUrl url() const;

    /*!
     * Returns the local path where url() is reachable.
     *
     * Call this in a slot connected to the result() signal, and only after
     * making sure no error happened.
     */
    QString localPath() const;

    QString errorString() const override;

protected:
    bool doKill() override;

private:
    friend class FuseMountJobPrivate;
    std::unique_ptr<FuseMountJobPrivate> d;
};

} // namespace KIO

#endif
