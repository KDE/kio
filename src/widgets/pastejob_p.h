/* This file is part of the KDE libraries
    Copyright (C) 2014 David Faure <faure@kde.org>

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

#ifndef PASTEJOB_P_H
#define PASTEJOB_P_H

#include <job_p.h>

namespace KIO {
class DropJobPrivate;

class PasteJobPrivate : public KIO::JobPrivate
{
public:
    // Used by KIO::PasteJob (clipboard=true) and KIO::DropJob (clipboard=false)
    PasteJobPrivate(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags, bool clipboard)
        : JobPrivate(),
        m_mimeData(mimeData),
        m_destDir(destDir),
        m_flags(flags),
        m_clipboard(clipboard)
    {
    }

    friend class KIO::DropJobPrivate;

    const QMimeData *m_mimeData;
    QUrl m_destDir;
    JobFlags m_flags;
    bool m_clipboard;

    Q_DECLARE_PUBLIC(PasteJob)

    void slotStart();
    void slotCopyingDone(KIO::Job*, const QUrl &, const QUrl &to) { emit q_func()->itemCreated(to); }
    void slotCopyingLinkDone(KIO::Job*, const QUrl &, const QString &, const QUrl &to) { emit q_func()->itemCreated(to); }

    static inline PasteJob *newJob(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags, bool clipboard)
    {
        PasteJob *job = new PasteJob(*new PasteJobPrivate(mimeData, destDir, flags, clipboard));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }
};

}

#endif
