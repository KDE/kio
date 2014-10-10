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

#include "mkpathjob.h"

#include "job_p.h"

#include "mkdirjob.h"
#include <QTimer>
#include <QDebug>
#include <QFileInfo>

using namespace KIO;

class KIO::MkpathJobPrivate : public KIO::JobPrivate
{
public:
    MkpathJobPrivate(const QUrl &url, const QUrl &baseUrl, JobFlags flags)
        : JobPrivate(),
          m_url(url),
          m_pathComponents(url.path().split('/', QString::SkipEmptyParts)),
          m_pathIterator(),
          m_flags(flags)
    {
        const QStringList basePathComponents = baseUrl.path().split('/', QString::SkipEmptyParts);
        m_url.setPath(QStringLiteral("/"));
        int i = 0;
        for (; i < basePathComponents.count() && i < m_pathComponents.count(); ++i) {
            if (m_pathComponents.at(i) == basePathComponents.at(i)) {
                m_url.setPath(m_url.path() + '/' + m_pathComponents.at(i));
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
                QString testDir = m_url.toLocalFile() + '/' + m_pathComponents.at(i);
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
        m_url.setPath(m_url.path() + '/' + *m_pathIterator);
        KIO::Job* job = KIO::mkdir(m_url);
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

    emit directoryCreated(d->m_url);

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
