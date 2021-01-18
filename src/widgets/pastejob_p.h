/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
    void slotCopyingDone(KIO::Job*, const QUrl &, const QUrl &to) { Q_EMIT q_func()->itemCreated(to); }
    void slotCopyingLinkDone(KIO::Job*, const QUrl &, const QString &, const QUrl &to) { Q_EMIT q_func()->itemCreated(to); }

    static inline PasteJob *newJob(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags, bool clipboard)
    {
        PasteJob *job = new PasteJob(*new PasteJobPrivate(mimeData, destDir, flags, clipboard));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        // Note: never KIO::getJobTracker()->registerJob here
        // The progress information comes from the underlying job (so we don't have to forward it here).
        return job;
    }
};

}

#endif
