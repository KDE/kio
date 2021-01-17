/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "pastejob.h"
#include "pastejob_p.h"

#include "paste.h"

#include <QMimeData>
#include <QTimer>

#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KUrlMimeData>

using namespace KIO;

extern KIO::Job *pasteMimeDataImpl(const QMimeData *mimeData, const QUrl &destUrl,
                                   const QString &dialogText, QWidget *widget,
                                   bool clipboard);

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
    KIO::Job *job = nullptr;
    if (m_mimeData->hasUrls()) {
        const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls);
        if (!urls.isEmpty()) {
            KIO::CopyJob *copyJob;
            if (move) {
                copyJob = KIO::move(urls, m_destDir, m_flags);
            } else {
                copyJob = KIO::copy(urls, m_destDir, m_flags);
            }
            QObject::connect(copyJob, &KIO::CopyJob::copyingDone,
                             q, [this](KIO::Job* job, const QUrl &src, const QUrl &dest) {
                slotCopyingDone(job, src, dest);
            });

            QObject::connect(copyJob, &KIO::CopyJob::copyingLinkDone,
                             q, [this](KIO::Job* job, const QUrl &from, const QString &target, const QUrl &to) {
                slotCopyingLinkDone(job, from, target, to);
            });

            KIO::FileUndoManager::self()->recordJob(move ? KIO::FileUndoManager::Move : KIO::FileUndoManager::Copy, QList<QUrl>(), m_destDir, copyJob);
            job = copyJob;
        }
    } else {
        const QString dialogText = m_clipboard ? i18n("Filename for clipboard content:") : i18n("Filename for dropped contents:");
        job = pasteMimeDataImpl(m_mimeData, m_destDir, dialogText, KJobWidgets::window(q), m_clipboard);
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
    if (job->error()) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    KIO::SimpleJob *simpleJob = qobject_cast<KIO::SimpleJob*>(job);
    if (simpleJob) {
        Q_EMIT itemCreated(simpleJob->url());
    }

    removeSubjob(job);
    emitResult();
}

PasteJob * KIO::paste(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags)
{
    return PasteJobPrivate::newJob(mimeData, destDir, flags, true /*clipboard*/);
}

#include "moc_pastejob.cpp"
