/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "statjob.h"

#include "job_p.h"
#include "scheduler.h"
#include "worker_p.h"
#include <KProtocolInfo>
#include <QTimer>
#include <kurlauthorized.h>

using namespace KIO;

class KIO::StatJobPrivate : public SimpleJobPrivate
{
public:
    inline StatJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
        , m_bSource(true)
        , m_details(KIO::StatDefaultDetails)
    {
    }

    UDSEntry m_statResult;
    QUrl m_redirectionURL;
    bool m_bSource;
    KIO::StatDetails m_details;
    void slotStatEntry(const KIO::UDSEntry &entry);
    void slotRedirection(const QUrl &url);

    /**
     * @internal
     * Called by the scheduler when a @p worker gets to
     * work on this job.
     * @param worker the worker that starts working on this job
     */
    void start(Worker *worker) override;

    Q_DECLARE_PUBLIC(StatJob)

    static inline StatJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs, JobFlags flags)
    {
        StatJob *job = new StatJob(*new StatJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            job->setFinishedNotificationHidden();
            KIO::getJobTracker()->registerJob(job);
            emitStating(job, url);
        }
        return job;
    }
};

StatJob::StatJob(StatJobPrivate &dd)
    : SimpleJob(dd)
{
    setTotalAmount(Items, 1);
}

StatJob::~StatJob()
{
}

void StatJob::setSide(StatSide side)
{
    d_func()->m_bSource = side == SourceSide;
}

void StatJob::setDetails(KIO::StatDetails details)
{
    d_func()->m_details = details;
}

const UDSEntry &StatJob::statResult() const
{
    return d_func()->m_statResult;
}

QUrl StatJob::mostLocalUrl() const
{
    const QUrl _url = url();

    if (_url.isLocalFile()) {
        return _url;
    }

    const UDSEntry &udsEntry = d_func()->m_statResult;
    const QString path = udsEntry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);

    if (path.isEmpty()) { // Return url as-is
        return _url;
    }

    const QString protoClass = KProtocolInfo::protocolClass(_url.scheme());
    if (protoClass != QLatin1String(":local")) { // UDS_LOCAL_PATH was set but wrong Class
        qCWarning(KIO_CORE) << "The protocol Class of the url that was being stat'ed" << _url << ", is" << protoClass
                            << ", however UDS_LOCAL_PATH was set; if you use UDS_LOCAL_PATH, the protocolClass"
                               " should be :local, see KProtocolInfo API docs for more details.";
        return _url;
    }

    return QUrl::fromLocalFile(path);
}

void StatJobPrivate::start(Worker *worker)
{
    Q_Q(StatJob);
    m_outgoingMetaData.insert(QStringLiteral("statSide"), m_bSource ? QStringLiteral("source") : QStringLiteral("dest"));
    m_outgoingMetaData.insert(QStringLiteral("details"), QString::number(m_details));

    q->connect(worker, &KIO::WorkerInterface::statEntry, q, [this](const KIO::UDSEntry &entry) {
        slotStatEntry(entry);
    });
    q->connect(worker, &KIO::WorkerInterface::redirection, q, [this](const QUrl &url) {
        slotRedirection(url);
    });

    SimpleJobPrivate::start(worker);
}

void StatJobPrivate::slotStatEntry(const KIO::UDSEntry &entry)
{
    // qCDebug(KIO_CORE);
    m_statResult = entry;
}

// Worker got a redirection request
void StatJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(StatJob);
    // qCDebug(KIO_CORE) << m_url << "->" << url;
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), m_url, url)) {
        qCWarning(KIO_CORE) << "Redirection from" << m_url << "to" << url << "REJECTED!";
        q->setError(ERR_ACCESS_DENIED);
        q->setErrorText(url.toDisplayString());
        return;
    }
    m_redirectionURL = url; // We'll remember that when the job finishes
    // Tell the user that we haven't finished yet
    Q_EMIT q->redirection(q, m_redirectionURL);
}

void StatJob::slotFinished()
{
    Q_D(StatJob);

    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {
        // qCDebug(KIO_CORE) << "StatJob: Redirection to " << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            Q_EMIT permanentRedirection(this, d->m_url, d->m_redirectionURL);
        }

        if (d->m_redirectionHandlingEnabled) {
            d->m_packedArgs.truncate(0);
            QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
            stream << d->m_redirectionURL;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return worker to the scheduler
    SimpleJob::slotFinished();
}

static bool isUrlValid(const QUrl &url)
{
    if (!url.isValid()) {
        qCWarning(KIO_CORE) << "Invalid url:" << url << ", cancelling job.";
        return false;
    }

    if (url.isLocalFile()) {
        qCWarning(KIO_CORE) << "Url" << url << "already represents a local file, cancelling job.";
        return false;
    }

    if (KProtocolInfo::protocolClass(url.scheme()) != QLatin1String(":local")) {
        qCWarning(KIO_CORE) << "Protocol Class of url" << url << ", isn't ':local', cancelling job.";
        return false;
    }

    return true;
}

StatJob *KIO::mostLocalUrl(const QUrl &url, JobFlags flags)
{
    StatJob *job = stat(url, StatJob::SourceSide, KIO::StatDefaultDetails, flags);
    if (!isUrlValid(url)) {
        QTimer::singleShot(0, job, &StatJob::slotFinished);
        Scheduler::cancelJob(job); // deletes the worker if not 0
    }
    return job;
}

StatJob *KIO::stat(const QUrl &url, JobFlags flags)
{
    // Assume SourceSide. Gets are more common than puts.
    return stat(url, StatJob::SourceSide, KIO::StatDefaultDetails, flags);
}

StatJob *KIO::stat(const QUrl &url, KIO::StatJob::StatSide side, KIO::StatDetails details, JobFlags flags)
{
    // qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(side);
    job->setDetails(details);
    return job;
}

#include "moc_statjob.cpp"
