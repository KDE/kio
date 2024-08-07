/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef MKDIRJOB_H
#define MKDIRJOB_H

#include "kiocore_export.h"
#include "simplejob.h"

namespace KIO
{
class MkdirJobPrivate;
/*!
 * \class KIO::MkdirJob
 * \inheaderfile KIO/MkdirJob
 * \inmodule KIOCore
 *
 * A KIO job that creates a directory
 * \sa KIO::mkdir()
 */
class KIOCORE_EXPORT MkdirJob : public SimpleJob
{
    Q_OBJECT

public:
    ~MkdirJob() override;

Q_SIGNALS:
    /*!
     * Signals a redirection.
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     *
     * \a job the job that is redirected
     *
     * \a url the new url
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /*!
     * Signals a permanent redirection.
     * The redirection itself is handled internally.
     *
     * \a job the job that is redirected
     *
     * \a fromUrl the original URL
     *
     * \a toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

protected Q_SLOTS:
    void slotFinished() override;

protected:
    KIOCORE_NO_EXPORT explicit MkdirJob(MkdirJobPrivate &dd);

private:
    Q_DECLARE_PRIVATE(MkdirJob)
};

/*!
 * \relates KIO::MkdirJob
 *
 * Creates a single directory.
 *
 * \a url The URL of the directory to create.
 *
 * \a permissions The permissions to set after creating the
 *                    directory (unix-style), -1 for default permissions.
 * Returns a pointer to the job handling the operation.
 */
KIOCORE_EXPORT MkdirJob *mkdir(const QUrl &url, int permissions = -1);

}

#endif /* MKDIRJOB_H */
