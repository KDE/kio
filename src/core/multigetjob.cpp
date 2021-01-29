/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "multigetjob.h"
#include "job_p.h"
#include "scheduler.h"
#include "slave.h"
#include <kurlauthorized.h>

using namespace KIO;

class KIO::MultiGetJobPrivate: public KIO::TransferJobPrivate
{
public:
    explicit MultiGetJobPrivate(const QUrl &url)
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
    typedef std::list<GetRequest> RequestQueue;

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
    void start(Slave *slave) override;

    bool findCurrentEntry();
    void flushQueue(RequestQueue &queue);

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
    d->m_waitQueue.push_back(entry);
}

void MultiGetJobPrivate::flushQueue(RequestQueue &queue)
{
    // Use multi-get
    // Scan all jobs in m_waitQueue
    auto wqIt = m_waitQueue.begin();
    while (wqIt != m_waitQueue.end()) {
        const GetRequest &entry = *wqIt;
        if ((m_url.scheme() == entry.url.scheme()) &&
                (m_url.host() == entry.url.host()) &&
                (m_url.port() == entry.url.port()) &&
                (m_url.userName() == entry.url.userName())) {
            queue.push_back(entry);
            wqIt = m_waitQueue.erase(wqIt);
        } else {
            ++wqIt;
        }
    }
    // Send number of URLs, (URL, metadata)*
    KIO_ARGS << (qint32) queue.size();
    for (const GetRequest &entry : queue) {
        stream << entry.url << entry.metaData;
    }
    m_packedArgs = packedArgs;
    m_command = CMD_MULTI_GET;
    m_outgoingMetaData.clear();
}

void MultiGetJobPrivate::start(Slave *slave)
{
    if (m_waitQueue.empty()) {
        return;
    }
    // Get the first entry from m_waitQueue and add it to m_activeQueue
    const GetRequest entry = m_waitQueue.front();
    m_activeQueue.push_back(entry);
    // then remove it from m_waitQueue
    m_waitQueue.pop_front();

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
        const long id = m_incomingMetaData.value(QStringLiteral("request-id")).toLong();
        for (const GetRequest &entry : m_activeQueue) {
            if (entry.id == id) {
                m_currentEntry = entry;
                return true;
            }
        }

        m_currentEntry.id = 0;
        return false;
    } else {
        if (m_activeQueue.empty()) {
            return false;
        }
        m_currentEntry = m_activeQueue.front();
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
        qCWarning(KIO_CORE) << "Redirection from" << d->m_currentEntry.url << "to" << url << "REJECTED!";
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
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT result(d->m_currentEntry.id);
#endif
        Q_EMIT fileTransferred(d->m_currentEntry.id);
    }
    d->m_redirectionURL = QUrl();
    setError(0);
    d->m_incomingMetaData.clear();
    d->m_activeQueue.remove(d->m_currentEntry);
    if (d->m_activeQueue.empty()) {
        if (d->m_waitQueue.empty()) {
            // All done
            TransferJob::slotFinished();
        } else {
            // return slave to pool
            // fetch new slave for first entry in d->m_waitQueue and call start
            // again.
            d->slaveDone();

            d->m_url = d->m_waitQueue.front().url;
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
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT data(d->m_currentEntry.id, _data);
#endif
        Q_EMIT dataReceived(d->m_currentEntry.id, _data);
    }
}

void MultiGetJob::slotMimetype(const QString &_mimetype)
{
    Q_D(MultiGetJob);
    if (d->b_multiGetActive) {
        MultiGetJobPrivate::RequestQueue newQueue;
        d->flushQueue(newQueue);
        if (!newQueue.empty()) {
            d->m_activeQueue.splice(d->m_activeQueue.cend(), newQueue);
            d->m_slave->send(d->m_command, d->m_packedArgs);
        }
    }
    if (!d->findCurrentEntry()) {
        return;    // Error, unknown request!
    }
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
    Q_EMIT mimetype(d->m_currentEntry.id, _mimetype);
#endif
    Q_EMIT mimeTypeFound(d->m_currentEntry.id, _mimetype);
}

MultiGetJob *KIO::multi_get(long id, const QUrl &url, const MetaData &metaData)
{
    MultiGetJob *job = MultiGetJobPrivate::newJob(url);
    job->get(id, url, metaData);
    return job;
}

#include "moc_multigetjob.cpp"
