/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "restorejob.h"

#include "kiocoredebug.h"
#include "job_p.h"
#include <kdirnotify.h>

#include <QTimer>

using namespace KIO;

class KIO::RestoreJobPrivate : public KIO::JobPrivate
{
public:
    RestoreJobPrivate(const QList<QUrl> &urls, JobFlags flags)
        : JobPrivate(),
        m_urls(urls),
        m_urlsIterator(m_urls.constBegin()),
        m_progress(0),
        m_flags(flags)
    {
    }
    QList<QUrl> m_urls;
    QList<QUrl>::const_iterator m_urlsIterator;
    int m_progress;
    JobFlags m_flags;

    void slotStart();
    Q_DECLARE_PUBLIC(RestoreJob)

    static inline RestoreJob *newJob(const QList<QUrl> &urls, JobFlags flags)
    {
        RestoreJob *job = new RestoreJob(*new RestoreJobPrivate(urls, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

};

RestoreJob::RestoreJob(RestoreJobPrivate &dd)
    : Job(dd)
{
    QTimer::singleShot(0, this, SLOT(slotStart()));
}

RestoreJob::~RestoreJob()
{
}

void RestoreJobPrivate::slotStart()
{
    Q_Q(RestoreJob);
    if (m_urlsIterator == m_urls.constBegin()) { // first time: emit total
        q->setTotalAmount(KJob::Files, m_urls.count());
    }

    if (m_urlsIterator != m_urls.constEnd()) {
        const QUrl& url = *m_urlsIterator;
        Q_ASSERT(url.scheme() == QLatin1String("trash"));
        QByteArray packedArgs;
        QDataStream stream(&packedArgs, QIODevice::WriteOnly);
        stream << int(3) << url;
        KIO::Job* job = KIO::special(url, packedArgs, m_flags);
        q->addSubjob(job);
        q->setProcessedAmount(KJob::Files, q->processedAmount(KJob::Files) + 1);
    } else {
        org::kde::KDirNotify::emitFilesRemoved(m_urls);
        q->emitResult();
    }

}

void RestoreJob::slotResult(KJob *job)
{
    Q_D(RestoreJob);
    if (job->error()) {
        qCDebug(KIO_CORE) << job->errorString();
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    removeSubjob(job);
    // Move on to next one
    ++d->m_urlsIterator;
    ++d->m_progress;
    emitPercent(d->m_progress, d->m_urls.count());
    d->slotStart();
}


RestoreJob *KIO::restoreFromTrash(const QList<QUrl> &urls, JobFlags flags)
{
    return RestoreJobPrivate::newJob(urls, flags);
}

#include "moc_restorejob.cpp"
