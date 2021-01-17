/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mkdirjob.h"
#include "job.h"
#include "job_p.h"
#include <slave.h>
#include <kurlauthorized.h>
#include "kiocoredebug.h"

using namespace KIO;

class KIO::MkdirJobPrivate: public SimpleJobPrivate
{
public:
    MkdirJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
    { }
    QUrl m_redirectionURL;
    void slotRedirection(const QUrl &url);

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) override;

    Q_DECLARE_PUBLIC(MkdirJob)

    static inline MkdirJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs)
    {
        MkdirJob *job = new MkdirJob(*new MkdirJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

MkdirJob::MkdirJob(MkdirJobPrivate &dd)
    : SimpleJob(dd)
{
}

MkdirJob::~MkdirJob()
{
}

void MkdirJobPrivate::start(Slave *slave)
{
    Q_Q(MkdirJob);
    q->connect(slave, &KIO::SlaveInterface::redirection, q, [this](const QUrl &url) { slotRedirection(url); });

    SimpleJobPrivate::start(slave);
}

// Slave got a redirection request
void MkdirJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(MkdirJob);
    //qDebug() << url;
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

void MkdirJob::slotFinished()
{
    Q_D(MkdirJob);

    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {
        //qDebug() << "MkdirJob: Redirection to " << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            Q_EMIT permanentRedirection(this, d->m_url, d->m_redirectionURL);
        }

        if (d->m_redirectionHandlingEnabled) {
            QUrl dummyUrl;
            int permissions;
            QDataStream istream(d->m_packedArgs);
            istream >> dummyUrl >> permissions;

            d->m_packedArgs.truncate(0);
            QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
            stream << d->m_redirectionURL << permissions;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return slave to the scheduler
    SimpleJob::slotFinished();
}

KIO::MkdirJob *KIO::mkdir(const QUrl &url, int permissions)
{
    //qDebug() << "mkdir " << url;
    KIO_ARGS << url << permissions;
    return MkdirJobPrivate::newJob(url, CMD_MKDIR, packedArgs);
}

#include "moc_mkdirjob.cpp"
