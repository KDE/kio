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

#include "statjob.h"

#include "job_p.h"
#include "slave.h"
#include "scheduler.h"
#include <kurlauthorized.h>
#include <QTimer>

using namespace KIO;

class KIO::StatJobPrivate: public SimpleJobPrivate
{
public:
    inline StatJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs), m_bSource(true), m_details(2)
    {}

    UDSEntry m_statResult;
    QUrl m_redirectionURL;
    bool m_bSource;
    short int m_details;
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

void StatJob::setSide(bool source)
{
    d_func()->m_bSource = source;
}

void StatJob::setSide(StatSide side)
{
    d_func()->m_bSource = side == SourceSide;
}

void StatJob::setDetails(short int details)
{
    d_func()->m_details = details;
}

const UDSEntry &StatJob::statResult() const
{
    return d_func()->m_statResult;
}

QUrl StatJob::mostLocalUrl() const
{
    if (!url().isLocalFile()) {
        const UDSEntry &udsEntry = d_func()->m_statResult;
        const QString path = udsEntry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
        if (!path.isEmpty()) {
            return QUrl::fromLocalFile(path);
        }
    }
    return url();
}

void StatJobPrivate::start(Slave *slave)
{
    Q_Q(StatJob);
    m_outgoingMetaData.insert(QStringLiteral("statSide"), m_bSource ? QStringLiteral("source") : QStringLiteral("dest"));
    m_outgoingMetaData.insert(QStringLiteral("details"), QString::number(m_details));

    q->connect(slave, SIGNAL(statEntry(KIO::UDSEntry)),
               SLOT(slotStatEntry(KIO::UDSEntry)));
    q->connect(slave, SIGNAL(redirection(QUrl)),
               SLOT(slotRedirection(QUrl)));

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
    emit q->redirection(q, m_redirectionURL);
}

void StatJob::slotFinished()
{
    Q_D(StatJob);

    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {
        //qCDebug(KIO_CORE) << "StatJob: Redirection to " << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);
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
    return stat(url, StatJob::SourceSide, 2, flags);
}

StatJob *KIO::mostLocalUrl(const QUrl &url, JobFlags flags)
{
    StatJob *job = stat(url, StatJob::SourceSide, 2, flags);
    if (url.isLocalFile()) {
        QTimer::singleShot(0, job, &StatJob::slotFinished);
        Scheduler::cancelJob(job); // deletes the slave if not 0
    }
    return job;
}

StatJob *KIO::stat(const QUrl &url, bool sideIsSource, short int details, JobFlags flags)
{
    //qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(sideIsSource ? StatJob::SourceSide : StatJob::DestinationSide);
    job->setDetails(details);
    return job;
}

StatJob *KIO::stat(const QUrl &url, KIO::StatJob::StatSide side, short int details, JobFlags flags)
{
    //qCDebug(KIO_CORE) << "stat" << url;
    KIO_ARGS << url;
    StatJob *job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide(side);
    job->setDetails(details);
    return job;
}

#include "moc_statjob.cpp"
