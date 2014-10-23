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

#include "pastejob.h"

#include "job_p.h"

#include "paste.h"

#include <QDebug>
#include <QFileInfo>
#include <QMimeData>
#include <QTimer>

#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KJobWidgets>
#include <KUrlMimeData>

using namespace KIO;

extern KIO::Job *pasteMimeDataImpl(const QMimeData *mimeData, const QUrl &destUrl,
                                   const QString &dialogText, QWidget *widget,
                                   bool clipboard);

class KIO::PasteJobPrivate : public KIO::JobPrivate
{
public:
    PasteJobPrivate(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags, bool clipboard)
        : JobPrivate(),
        m_mimeData(mimeData),
        m_destDir(destDir),
        m_flags(flags),
        m_clipboard(clipboard)
    {
    }

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

PasteJob::PasteJob(PasteJobPrivate &dd)
    : Job(dd)
{
    QTimer::singleShot(0, this, SLOT(slotStart()));
}

PasteJob::~PasteJob()
{
}

void PasteJobPrivate::slotStart()
{
    Q_Q(PasteJob);
    const bool move = KIO::isClipboardDataCut(m_mimeData);
    KIO::Job *job = 0;
    if (m_mimeData->hasUrls()) {
        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls);
        if (!urls.isEmpty()) {
            KIO::CopyJob *copyJob;
            if (move) {
                copyJob = KIO::move(urls, m_destDir, m_flags);
            } else {
                copyJob = KIO::copy(urls, m_destDir, m_flags);
            }
            QObject::connect(copyJob, SIGNAL(copyingDone(KIO::Job*, QUrl, QUrl, QDateTime, bool, bool)), q, SLOT(slotCopyingDone(KIO::Job*, QUrl, QUrl)));
            QObject::connect(copyJob, SIGNAL(copyingLinkDone(KIO::Job*, QUrl, QString, QUrl)), q, SLOT(slotCopyingLinkDone(KIO::Job*, QUrl, QString, QUrl)));
            KIO::FileUndoManager::self()->recordJob(move ? KIO::FileUndoManager::Move : KIO::FileUndoManager::Copy, QList<QUrl>(), m_destDir, copyJob);
            job = copyJob;
        }
    } else {
        job = pasteMimeDataImpl(m_mimeData, m_destDir, QString(), KJobWidgets::window(q), m_clipboard);
        if (KIO::SimpleJob* simpleJob = qobject_cast<KIO::SimpleJob *>(job)) {
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Put, QList<QUrl>(), simpleJob->url(), job);
        }
    }
    if (job) {
        q->addSubjob(job);
    } else {
        q->setError(KIO::ERR_NO_CONTENT);
        q->emitResult();
    }
}

void PasteJob::slotResult(KJob *job)
{
    Q_D(PasteJob);
    if (job->error()) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    KIO::SimpleJob *simpleJob = qobject_cast<KIO::SimpleJob*>(job);
    if (simpleJob) {
        emit itemCreated(simpleJob->url());
    }

    removeSubjob(job);
    emitResult();
}

PasteJob * KIO::paste(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags)
{
    return PasteJobPrivate::newJob(mimeData, destDir, flags, true);
}

PasteJob * KIO::drop(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags)
{
    return PasteJobPrivate::newJob(mimeData, destDir, flags, false);
}

#include "moc_pastejob.cpp"
