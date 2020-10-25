/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "namefinderjob.h"

#include "kiocoredebug.h"
#include "../pathhelpers_p.h"
#include <KFileUtils>
#include <KIO/StatJob>

#include <QUrl>

class KIO::NameFinderJobPrivate
{
public:
    explicit NameFinderJobPrivate(const QUrl &baseUrl, const QString &name, NameFinderJob *qq)
        : m_baseUrl(baseUrl), m_name(name), m_statJob(nullptr), q(qq)
    {
    }

    QUrl m_baseUrl;
    QString m_name;
    QUrl m_finalUrl;
    KIO::StatJob *m_statJob;
    bool m_firstStat = true;

    KIO::NameFinderJob *const q;

    void statUrl();
    void slotStatResult();
};

KIO::NameFinderJob::NameFinderJob(const QUrl &baseUrl, const QString &name, QObject *parent)
    : KCompositeJob(parent), d(new NameFinderJobPrivate(baseUrl, name, this))
{
}

KIO::NameFinderJob::~NameFinderJob()
{
}

void KIO::NameFinderJob::start()
{
    if (!d->m_baseUrl.isValid() || d->m_baseUrl.scheme().isEmpty()) {
        qCDebug(KIO_CORE) << "Malformed URL" << d->m_baseUrl;
        setError(KIO::ERR_MALFORMED_URL);
        emitResult();
        return;
    }

    d->statUrl();
}

void KIO::NameFinderJobPrivate::statUrl()
{
    m_finalUrl = m_baseUrl;
    m_finalUrl.setPath(concatPaths(m_baseUrl.path(), m_name));

    m_statJob = KIO::statDetails(m_finalUrl, KIO::StatJob::DestinationSide,
                                 KIO::StatNoDetails, // Just checking if it exists
                                 KIO::HideProgressInfo);

    QObject::connect(m_statJob, &KJob::result, q, [this]() { slotStatResult(); });
}

void KIO::NameFinderJobPrivate::slotStatResult()
{
    // m_statJob will resolve the url to the most local one in the first run
    if (m_firstStat) {
        m_finalUrl = m_statJob->mostLocalUrl();
        m_firstStat = false;
    }

    // StripTrailingSlash so that fileName() doesn't return an empty string
    m_finalUrl = m_finalUrl.adjusted(QUrl::StripTrailingSlash);
    m_baseUrl = m_finalUrl.adjusted(QUrl::RemoveFilename);
    m_name = m_finalUrl.fileName();

    if (m_statJob->error()) { // Doesn't exist, we're done
        q->emitResult();
    } else { // Exists, create a new name, then stat again
        m_name = KFileUtils::makeSuggestedName(m_name);
        statUrl();
    }
}

QUrl KIO::NameFinderJob::finalUrl() const
{
    return d->m_finalUrl;
}

QUrl KIO::NameFinderJob::baseUrl() const
{
    return d->m_baseUrl;
}

QString KIO::NameFinderJob::finalName() const
{
    return d->m_name;
}
