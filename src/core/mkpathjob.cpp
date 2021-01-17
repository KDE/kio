/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mkpathjob.h"
#include "job_p.h"
#include "mkdirjob.h"
#include "../pathhelpers_p.h"

#include <QTimer>
#include <QFileInfo>

using namespace KIO;

class KIO::MkpathJobPrivate : public KIO::JobPrivate
{
public:
    MkpathJobPrivate(const QUrl &url, const QUrl &baseUrl, JobFlags flags)
        : JobPrivate(),
          m_url(url),
          m_pathComponents(url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts)),
          m_pathIterator(),
          m_flags(flags)
    {
        const QStringList basePathComponents = baseUrl.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);

#ifdef Q_OS_WIN
        const QString startPath;
#else
        const QString startPath = QLatin1String("/");
#endif
        m_url.setPath(startPath);
        int i = 0;
        for (; i < basePathComponents.count() && i < m_pathComponents.count(); ++i) {
            const QString pathComponent = m_pathComponents.at(i);
            if (pathComponent == basePathComponents.at(i)) {
                m_url.setPath(concatPaths(m_url.path(), pathComponent));
            } else {
                break;
            }
        }
        if (i > 0) {
            m_pathComponents.erase(m_pathComponents.begin(), m_pathComponents.begin() + i);
        }

        // fast path for local files using QFileInfo::isDir
        if (m_url.isLocalFile()) {
            i = 0;
            for (; i < m_pathComponents.count(); ++i) {
                const QString localFile = m_url.toLocalFile();
                QString testDir;
                if (localFile == startPath) {
                    testDir = localFile + m_pathComponents.at(i);
                } else {
                    testDir = localFile + QLatin1Char('/') + m_pathComponents.at(i);
                }
                if (QFileInfo(testDir).isDir()) {
                    m_url.setPath(testDir);
                } else {
                    break;
                }
            }
            if (i > 0) {
                m_pathComponents.erase(m_pathComponents.begin(), m_pathComponents.begin() + i);
            }
        }

        m_pathIterator = m_pathComponents.constBegin();
    }

    QUrl m_url;
    QUrl m_baseUrl;
    QStringList m_pathComponents;
    QStringList::const_iterator m_pathIterator;
    const JobFlags m_flags;
    Q_DECLARE_PUBLIC(MkpathJob)

    void slotStart();

    static inline MkpathJob *newJob(const QUrl &url, const QUrl &baseUrl, JobFlags flags)
    {
        MkpathJob *job = new MkpathJob(*new MkpathJobPrivate(url, baseUrl, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = MkDir;
        }
        return job;
    }

};

MkpathJob::MkpathJob(MkpathJobPrivate &dd)
    : Job(dd)
{
    QTimer::singleShot(0, this, SLOT(slotStart()));
}

MkpathJob::~MkpathJob()
{
}

void MkpathJobPrivate::slotStart()
{
    Q_Q(MkpathJob);

    if (m_pathIterator == m_pathComponents.constBegin()) { // first time: emit total
        q->setTotalAmount(KJob::Directories, m_pathComponents.count());
    }

    if (m_pathIterator != m_pathComponents.constEnd()) {
        m_url.setPath(concatPaths(m_url.path(), *m_pathIterator));
        KIO::Job* job = KIO::mkdir(m_url);
        job->setParentJob(q);
        q->addSubjob(job);
        q->setProcessedAmount(KJob::Directories, q->processedAmount(KJob::Directories) + 1);
    } else {
        q->emitResult();
    }
}

void MkpathJob::slotResult(KJob *job)
{
    Q_D(MkpathJob);
    if (job->error() && job->error() != KIO::ERR_DIR_ALREADY_EXIST) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    removeSubjob(job);

    Q_EMIT directoryCreated(d->m_url);

    // Move on to next one
    ++d->m_pathIterator;
    emitPercent(d->m_pathIterator - d->m_pathComponents.constBegin(), d->m_pathComponents.count());
    d->slotStart();
}

MkpathJob * KIO::mkpath(const QUrl &url, const QUrl &baseUrl, KIO::JobFlags flags)
{
    return MkpathJobPrivate::newJob(url, baseUrl, flags);
}

#include "moc_mkpathjob.cpp"
