/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "statjob.h"

#include "job_p.h"
#include "slave.h"
#include "scheduler.h"
#include <KProtocolInfo>
#include <kurlauthorized.h>
#include <QTimer>

using namespace KIO;

class KIO::StatJobPrivate: public SimpleJobPrivate
{
public:
    inline StatJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs), m_bSource(true), m_details(KIO::StatDefaultDetails)
    {}

    UDSEntry m_statResult;
    QUrl m_redirectionURL;
    bool m_bSource;
    KIO::StatDetails m_details;
    void slotStatEntry(const KIO::UDSEntry &entry);
    void slotRedirection(const QUrl &url);

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) override;

    Q_DECLARE_PUBLIC(StatJob)

    static inline StatJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs,
                                  JobFlags flags)
    {
        StatJob *job = new StatJob(*new StatJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
            emitStating(job, url);
        }
        return job;
    }
};

StatJob::StatJob(StatJobPrivate &dd)
    : SimpleJob(dd)
{
}

StatJob::~StatJob()
{
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 0)
void StatJob::setSide(bool source)
{
    d_func()->m_bSource = source;
}
#endif

void StatJob::setSide(StatSide side)
{
    d_func()->m_bSource = side == SourceSide;
}

void StatJob::setDetails(KIO::StatDetails details)
{
    d_func()->m_details = details;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
void StatJob::setDetails(short int details)
{
    // for backward compatibility
    d_func()->m_details = detailsToStatDetails(details);
}

void StatJob::setDetails(KIO::StatDetail detail)
{
    d_func()->m_details = detail;
}

KIO::StatDetails KIO::detailsToStatDetails(int details)
{
    KIO::StatDetails detailsFlag = KIO::StatBasic;
    if (details > 0) {
        detailsFlag |= KIO::StatUser | KIO::StatTime;
    }
    if (details > 1) {
         detailsFlag |= KIO::StatResolveSymlink | KIO::StatAcl;
    }
    if (details > 2) {
        detailsFlag |= KIO::StatInode;
    }
    return detailsFlag;
}
#endif

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

void StatJobPrivate::start(Slave *slave)
{
    Q_Q(StatJob);
    m_outgoingMetaData.insert(QStringLiteral("statSide"), m_bSource ? QStringLiteral("source") : QStringLiteral("dest"));
    m_outgoingMetaData.insert(QStringLiteral("statDetails"), QString::number(m_details));

    q->connect(slave, &KIO::SlaveInterface::statEntry, q, [this](const KIO::UDSEntry &entry) { slotStatEntry(entry); });
    q->connect(slave, &KIO::SlaveInterface::redirection, q, [this](const QUrl &url) { slotRedirection(url); });

    SimpleJobPrivate::start(slave);
}

void StatJobPrivate::slotStatEntry(const KIO::UDSEntry &entry)
{
    //qCDebug(KIO_CORE);
    m_statResult = entry;
}

// Slave got a redirection request
void StatJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(StatJob);
    //qCDebug(KIO_CORE) << m_url << "->" << url;
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
        //qCDebug(KIO_CORE) << "StatJob: Redirection to " << m_redirectionURL;
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

    // Return slave to the scheduler
    SimpleJob::slotFinished();
}

void StatJob::slotMetaData(const KIO::MetaData &_metaData)
{
    Q_D(StatJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

StatJob *KIO::stat(const QUrl &url, JobFlags flags)
{
    // Assume sideIsSource. Gets are more common than puts.
    return statDetails(url, StatJob::SourceSide, KIO::StatDefaultDetails, flags);
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
    StatJob *job = statDetails(url, StatJob::SourceSide, KIO::StatDefaultDetails, flags);
    if (!isUrlValid(url)) {
        QTimer::singleShot(0, job, &StatJob::slotFinished);
        Scheduler::cancelJob(job); // deletes the slave if not 0
    }
    return job;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 0)
StatJob *KIO::stat(const QUrl &url, bool sideIsSource, short int details, JobFlags flags)
{
    //qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(sideIsSource ? StatJob::SourceSide : StatJob::DestinationSide);
    job->setDetails(details);
    return job;
}
#endif

StatJob *KIO::statDetails(const QUrl &url, KIO::StatJob::StatSide side, KIO::StatDetails details, JobFlags flags)
{
    // TODO KF6: rename to stat
    //qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(side);
    job->setDetails(details);
    return job;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
StatJob *KIO::stat(const QUrl &url, KIO::StatJob::StatSide side, short int details, JobFlags flags)
{
    //qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(side);
    job->setDetails(details);
    return job;
}
#endif

#include "moc_statjob.cpp"
