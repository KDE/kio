/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>

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
