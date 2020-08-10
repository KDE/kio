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
/**
 * @class KIO::MkdirJob mkdirjob.h <KIO/MkdirJob>
 *
 * A KIO job that creates a directory
 * @see KIO::mkdir()
 */
class KIOCORE_EXPORT MkdirJob : public SimpleJob
{

    Q_OBJECT

public:
    ~MkdirJob() override;

Q_SIGNALS:
    /**
     * Signals a redirection.
     * Use to update the URL shown to the user.
     * The redirection itself is handled internally.
     * @param job the job that is redirected
     * @param url the new url
     */
    void redirection(KIO::Job *job, const QUrl &url);

    /**
     * Signals a permanent redirection.
     * The redirection itself is handled internally.
     * @param job the job that is redirected
     * @param fromUrl the original URL
     * @param toUrl the new URL
     */
    void permanentRedirection(KIO::Job *job, const QUrl &fromUrl, const QUrl &toUrl);

protected Q_SLOTS:
    void slotFinished() override;

public:
    MkdirJob(MkdirJobPrivate &dd);

private:
    Q_PRIVATE_SLOT(d_func(), void slotRedirection(const QUrl &url))
    Q_DECLARE_PRIVATE(MkdirJob)
};

/**
 * Creates a single directory.
 *
 * @param url The URL of the directory to create.
 * @param permissions The permissions to set after creating the
 *                    directory (unix-style), -1 for default permissions.
 * @return A pointer to the job handling the operation.
 */
KIOCORE_EXPORT MkdirJob *mkdir(const QUrl &url, int permissions = -1);

}

#endif /* MKDIRJOB_H */
