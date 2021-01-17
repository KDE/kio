/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "mimetypejob.h"
#include "job_p.h"

using namespace KIO;

class KIO::MimetypeJobPrivate: public KIO::TransferJobPrivate
{
public:
    MimetypeJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : TransferJobPrivate(url, command, packedArgs, QByteArray())
    {}

    Q_DECLARE_PUBLIC(MimetypeJob)

    static inline MimetypeJob *newJob(const QUrl &url, int command, const QByteArray &packedArgs,
                                      JobFlags flags)
    {
        MimetypeJob *job = new MimetypeJob(*new MimetypeJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
            emitStating(job, url);
        }
        return job;
    }
};

MimetypeJob::MimetypeJob(MimetypeJobPrivate &dd)
    : TransferJob(dd)
{
}

MimetypeJob::~MimetypeJob()
{
}

void MimetypeJob::slotFinished()
{
    Q_D(MimetypeJob);
    //qDebug();
    if (error() == KIO::ERR_IS_DIRECTORY) {
        // It is in fact a directory. This happens when HTTP redirects to FTP.
        // Due to the "protocol doesn't support listing" code in KRun, we
        // assumed it was a file.
        //qDebug() << "It is in fact a directory!";
        d->m_mimetype = QStringLiteral("inode/directory");
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 78)
        Q_EMIT TransferJob::mimetype(this, d->m_mimetype);
#endif
        Q_EMIT TransferJob::mimeTypeFound(this, d->m_mimetype);
        setError(0);
    }

    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() && !error()) {
        //qDebug() << "Redirection to " << m_redirectionURL;
        if (queryMetaData(QStringLiteral("permanent-redirect")) == QLatin1String("true")) {
            Q_EMIT permanentRedirection(this, d->m_url, d->m_redirectionURL);
        }

        if (d->m_redirectionHandlingEnabled) {
            d->staticData.truncate(0);
            d->m_internalSuspended = false;
            d->m_packedArgs.truncate(0);
            QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
            stream << d->m_redirectionURL;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return slave to the scheduler
    TransferJob::slotFinished();
}

MimetypeJob *KIO::mimetype(const QUrl &url, JobFlags flags)
{
    KIO_ARGS << url;
    return MimetypeJobPrivate::newJob(url, CMD_MIMETYPE, packedArgs, flags);
}

#include "moc_mimetypejob.cpp"
