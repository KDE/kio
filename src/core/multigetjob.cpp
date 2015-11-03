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

#include "multigetjob.h"
#include "job_p.h"
#include "scheduler.h"
#include "slave.h"
#include <kurlauthorized.h>
#include <QLinkedList>
#include <QDebug>

using namespace KIO;

class KIO::MultiGetJobPrivate: public KIO::TransferJobPrivate
{
public:
    MultiGetJobPrivate(const QUrl &url)
        : TransferJobPrivate(url, 0, QByteArray(), QByteArray()),
          m_currentEntry(0, QUrl(), MetaData())
    {}
    struct GetRequest {
        GetRequest(long _id, const QUrl &_url, const MetaData &_metaData)
            : id(_id), url(_url), metaData(_metaData) { }
        long id;
        QUrl url;
        MetaData metaData;

        inline bool operator==(const GetRequest &req) const
        {
            return req.id == id;
        }
    };
    typedef QLinkedList<GetRequest> RequestQueue;

    RequestQueue m_waitQueue;
    RequestQueue m_activeQueue;
    GetRequest m_currentEntry;
    bool b_multiGetActive;

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) Q_DECL_OVERRIDE;

    bool findCurrentEntry();
    void flushQueue(QLinkedList<GetRequest> &queue);

    Q_DECLARE_PUBLIC(MultiGetJob)

    static inline MultiGetJob *newJob(const QUrl &url)
    {
        MultiGetJob *job = new MultiGetJob(*new MultiGetJobPrivate(url));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

MultiGetJob::MultiGetJob(MultiGetJobPrivate &dd)
    : TransferJob(dd)
{
}

MultiGetJob::~MultiGetJob()
{
}

void MultiGetJob::get(long id, const QUrl &url, const MetaData &metaData)
{
    Q_D(MultiGetJob);
    MultiGetJobPrivate::GetRequest entry(id, url, metaData);
    entry.metaData[QStringLiteral("request-id")] = QString::number(id);
    d->m_waitQueue.append(entry);
}

void MultiGetJobPrivate::flushQueue(RequestQueue &queue)
{
    // Use multi-get
    // Scan all jobs in m_waitQueue
    RequestQueue::iterator wqit = m_waitQueue.begin();
    const RequestQueue::iterator wqend = m_waitQueue.end();
    while (wqit != wqend) {
        const GetRequest &entry = *wqit;
        if ((m_url.scheme() == entry.url.scheme()) &&
                (m_url.host() == entry.url.host()) &&
                (m_url.port() == entry.url.port()) &&
                (m_url.userName() == entry.url.userName())) {
            queue.append(entry);
            wqit = m_waitQueue.erase(wqit);
        } else {
            ++wqit;
        }
    }
    // Send number of URLs, (URL, metadata)*
    KIO_ARGS << (qint32) queue.count();
    RequestQueue::const_iterator qit = queue.begin();
    const RequestQueue::const_iterator qend = queue.end();
    for (; qit != qend; ++qit) {
        stream << (*qit).url << (*qit).metaData;
    }
    m_packedArgs = packedArgs;
    m_command = CMD_MULTI_GET;
    m_outgoingMetaData.clear();
}

void MultiGetJobPrivate::start(Slave *slave)
{
    // Add first job from m_waitQueue and add it to m_activeQueue
    GetRequest entry = m_waitQueue.takeFirst();
    m_activeQueue.append(entry);

    m_url = entry.url;

    if (!entry.url.scheme().startsWith(QLatin1String("http"))) {
        // Use normal get
        KIO_ARGS << entry.url;
        m_packedArgs = packedArgs;
        m_outgoingMetaData = entry.metaData;
        m_command = CMD_GET;
        b_multiGetActive = false;
    } else {
        flushQueue(m_activeQueue);
        b_multiGetActive = true;
    }

    TransferJobPrivate::start(slave); // Anything else to do??
}

bool MultiGetJobPrivate::findCurrentEntry()
{
    if (b_multiGetActive) {
        long id = m_incomingMetaData[QStringLiteral("request-id")].toLong();
        RequestQueue::const_iterator qit = m_activeQueue.begin();
        const RequestQueue::const_iterator qend = m_activeQueue.end();
        for (; qit != qend; ++qit) {
            if ((*qit).id == id) {
                m_currentEntry = *qit;
                return true;
            }
        }
        m_currentEntry.id = 0;
        return false;
    } else {
        if (m_activeQueue.isEmpty()) {
            return false;
        }
        m_currentEntry = m_activeQueue.first();
        return true;
    }
}

void MultiGetJob::slotRedirection(const QUrl &url)
{
    Q_D(MultiGetJob);
    if (!d->findCurrentEntry()) {
        return;    // Error
    }
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), d->m_url, url)) {
        qWarning() << "Redirection from" << d->m_currentEntry.url << "to" << url << "REJECTED!";
        return;
    }
    d->m_redirectionURL = url;
    get(d->m_currentEntry.id, d->m_redirectionURL, d->m_currentEntry.metaData); // Try again
}

void MultiGetJob::slotFinished()
{
    Q_D(MultiGetJob);
    if (!d->findCurrentEntry()) {
        return;
    }
    if (d->m_redirectionURL.isEmpty()) {
        // No redirection, tell the world that we are finished.
        emit result(d->m_currentEntry.id);
    }
    d->m_redirectionURL = QUrl();
    setError(0);
    d->m_incomingMetaData.clear();
    d->m_activeQueue.removeAll(d->m_currentEntry);
    if (d->m_activeQueue.count() == 0) {
        if (d->m_waitQueue.count() == 0) {
            // All done
            TransferJob::slotFinished();
        } else {
            // return slave to pool
            // fetch new slave for first entry in d->m_waitQueue and call start
            // again.
            d->slaveDone();

            d->m_url = d->m_waitQueue.first().url;
            if ((d->m_extraFlags & JobPrivate::EF_KillCalled) == 0) {
                Scheduler::doJob(this);
            }
        }
    }
}

void MultiGetJob::slotData(const QByteArray &_data)
{
    Q_D(MultiGetJob);
    if (d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error()) {
        emit data(d->m_currentEntry.id, _data);
    }
}

void MultiGetJob::slotMimetype(const QString &_mimetype)
{
    Q_D(MultiGetJob);
    if (d->b_multiGetActive) {
        MultiGetJobPrivate::RequestQueue newQueue;
        d->flushQueue(newQueue);
        if (!newQueue.isEmpty()) {
            d->m_activeQueue += newQueue;
            d->m_slave->send(d->m_command, d->m_packedArgs);
        }
    }
    if (!d->findCurrentEntry()) {
        return;    // Error, unknown request!
    }
    emit mimetype(d->m_currentEntry.id, _mimetype);
}

MultiGetJob *KIO::multi_get(long id, const QUrl &url, const MetaData &metaData)
{
    MultiGetJob *job = MultiGetJobPrivate::newJob(url);
    job->get(id, url, metaData);
    return job;
}

#include "moc_multigetjob.cpp"
