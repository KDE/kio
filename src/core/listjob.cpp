/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "listjob.h"
#include "job_p.h"
#include "scheduler.h"
#include <kurlauthorized.h>
#include "slave.h"
#include "../pathhelpers_p.h"
#include <QTimer>

#include <QDebug>

using namespace KIO;

class KIO::ListJobPrivate: public KIO::SimpleJobPrivate
{
public:
    ListJobPrivate(const QUrl &url, bool _recursive,
                   const QString &prefix, const QString &displayPrefix,
                   bool _includeHidden)
        : SimpleJobPrivate(url, CMD_LISTDIR, QByteArray()),
          recursive(_recursive), includeHidden(_includeHidden),
          m_prefix(prefix), m_displayPrefix(displayPrefix), m_processedEntries(0)
    {}
    bool recursive;
    bool includeHidden;
    QString m_prefix;
    QString m_displayPrefix;
    unsigned long m_processedEntries;
    QUrl m_redirectionURL;

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) override;

    void slotListEntries(const KIO::UDSEntryList &list);
    void slotRedirection(const QUrl &url);
    void gotEntries(KIO::Job *subjob, const KIO::UDSEntryList &list);
    void slotSubError(ListJob* job, ListJob* subJob);

    Q_DECLARE_PUBLIC(ListJob)

    static inline ListJob *newJob(const QUrl &u, bool _recursive,
                                  const QString &prefix, const QString &displayPrefix,
                                  bool _includeHidden, JobFlags flags = HideProgressInfo)
    {
        ListJob *job = new ListJob(*new ListJobPrivate(u, _recursive, prefix, displayPrefix, _includeHidden));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }
    static inline ListJob *newJobNoUi(const QUrl &u, bool _recursive,
                                      const QString &prefix, const QString &displayPrefix,
                                      bool _includeHidden)
    {
        return new ListJob(*new ListJobPrivate(u, _recursive, prefix, displayPrefix, _includeHidden));
    }
};

ListJob::ListJob(ListJobPrivate &dd)
    : SimpleJob(dd)
{
    Q_D(ListJob);
    // We couldn't set the args when calling the parent constructor,
    // so do it now.
    QDataStream stream(&d->m_packedArgs, QIODevice::WriteOnly);
    stream << d->m_url;
}

ListJob::~ListJob()
{
}

void ListJobPrivate::slotListEntries(const KIO::UDSEntryList &list)
{
    Q_Q(ListJob);
    // Emit progress info (takes care of emit processedSize and percent)
    m_processedEntries += list.count();
    slotProcessedSize(m_processedEntries);

    if (recursive) {
        UDSEntryList::ConstIterator it = list.begin();
        const UDSEntryList::ConstIterator end = list.end();

        for (; it != end; ++it) {

            const UDSEntry &entry = *it;

            QUrl itemURL;
            const QString udsUrl = entry.stringValue(KIO::UDSEntry::UDS_URL);
            QString filename;
            if (!udsUrl.isEmpty()) {
                itemURL = QUrl(udsUrl);
                filename = itemURL.fileName();
            } else { // no URL, use the name
                itemURL = q->url();
                filename = entry.stringValue(KIO::UDSEntry::UDS_NAME);
                Q_ASSERT(!filename.isEmpty()); // we'll recurse forever otherwise :)
                itemURL.setPath(concatPaths(itemURL.path(), filename));
            }

            if (entry.isDir() && !entry.isLink()) {
                Q_ASSERT(!filename.isEmpty());
                QString displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
                if (displayName.isEmpty()) {
                    displayName = filename;
                }
                // skip hidden dirs when listing if requested
                if (filename != QLatin1String("..") && filename != QLatin1String(".") && (includeHidden || filename[0] != QLatin1Char('.'))) {
                    ListJob *job = ListJobPrivate::newJobNoUi(itemURL,
                                   true /*recursive*/,
                                   m_prefix + filename + QLatin1Char('/'),
                                   m_displayPrefix + displayName + QLatin1Char('/'),
                                   includeHidden);
                    Scheduler::setJobPriority(job, 1);
                    QObject::connect(job, &ListJob::entries, q,
                        [this](KIO::Job *job, const KIO::UDSEntryList &list) {gotEntries(job, list);} );
                    QObject::connect(job, &ListJob::subError, q,
                        [this](KIO::ListJob *job, KIO::ListJob *ljob) {slotSubError(job, ljob);} );

                    q->addSubjob(job);
                }
            }
        }
    }

    // Not recursive, or top-level of recursive listing : return now (send . and .. as well)
    // exclusion of hidden files also requires the full sweep, but the case for full-listing
    // a single dir is probably common enough to justify the shortcut
    if (m_prefix.isNull() && includeHidden) {
        Q_EMIT q->entries(q, list);
    } else {
        // cull the unwanted hidden dirs and/or parent dir references from the listing, then emit that
        UDSEntryList newlist;

        UDSEntryList::const_iterator it = list.begin();
        const UDSEntryList::const_iterator end = list.end();
        for (; it != end; ++it) {

            // Modify the name in the UDSEntry
            UDSEntry newone = *it;
            const QString filename = newone.stringValue(KIO::UDSEntry::UDS_NAME);
            QString displayName = newone.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (displayName.isEmpty()) {
                displayName = filename;
            }
            // Avoid returning entries like subdir/. and subdir/.., but include . and .. for
            // the toplevel dir, and skip hidden files/dirs if that was requested
            if ((m_prefix.isNull() || (filename != QLatin1String("..") && filename != QLatin1String(".")))
                    && (includeHidden || (filename[0] != QLatin1Char('.')))) {
                // ## Didn't find a way to use the iterator instead of re-doing a key lookup
                newone.replace(KIO::UDSEntry::UDS_NAME, m_prefix + filename);
                newone.replace(KIO::UDSEntry::UDS_DISPLAY_NAME, m_displayPrefix + displayName);
                newlist.append(newone);
            }
        }

        Q_EMIT q->entries(q, newlist);
    }
}

void ListJobPrivate::gotEntries(KIO::Job *, const KIO::UDSEntryList &list)
{
    // Forward entries received by subjob - faking we received them ourselves
    Q_Q(ListJob);
    Q_EMIT q->entries(q, list);
}

void ListJobPrivate::slotSubError(KIO::ListJob* /*job*/, KIO::ListJob* subJob)
{
    Q_Q(ListJob);
    Q_EMIT q->subError(q, subJob); // Let the signal of subError go up
}

void ListJob::slotResult(KJob *job)
{
    Q_D(ListJob);
    if (job->error()) {
        // If we can't list a subdir, the result is still ok
        // This is why we override KCompositeJob::slotResult - to not set
        // an error on parent job.
        // Let's emit a signal about this though
        Q_EMIT subError(this, static_cast<KIO::ListJob *>(job));
    }
    removeSubjob(job);
    if (!hasSubjobs() && !d->m_slave) { // if the main directory listing is still running, it will emit result in SimpleJob::slotFinished()
        emitResult();
    }
}

void ListJobPrivate::slotRedirection(const QUrl &url)
{
    Q_Q(ListJob);
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("redirect"), m_url, url)) {
        qCWarning(KIO_CORE) << "Redirection from" << m_url << "to" << url << "REJECTED!";
        return;
    }
    m_redirectionURL = url; // We'll remember that when the job finishes
    Q_EMIT q->redirection(q, m_redirectionURL);
}

void ListJob::slotFinished()
{
    Q_D(ListJob);

    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() && !error()) {

        //qDebug() << "Redirection to " << d->m_redirectionURL;
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

void ListJob::slotMetaData(const KIO::MetaData &_metaData)
{
    Q_D(ListJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

ListJob *KIO::listDir(const QUrl &url, JobFlags flags, bool includeHidden)
{
    return ListJobPrivate::newJob(url, false, QString(), QString(), includeHidden, flags);
}

ListJob *KIO::listRecursive(const QUrl &url, JobFlags flags, bool includeHidden)
{
    return ListJobPrivate::newJob(url, true, QString(), QString(), includeHidden, flags);
}

void ListJob::setUnrestricted(bool unrestricted)
{
    Q_D(ListJob);
    if (unrestricted) {
        d->m_extraFlags |= JobPrivate::EF_ListJobUnrestricted;
    } else {
        d->m_extraFlags &= ~JobPrivate::EF_ListJobUnrestricted;
    }
}

void ListJobPrivate::start(Slave *slave)
{
    Q_Q(ListJob);
    if (!KUrlAuthorized::authorizeUrlAction(QStringLiteral("list"), m_url, m_url) &&
            !(m_extraFlags & EF_ListJobUnrestricted)) {
        q->setError(ERR_ACCESS_DENIED);
        q->setErrorText(m_url.toDisplayString());
        QTimer::singleShot(0, q, &ListJob::slotFinished);
        return;
    }
    QObject::connect(slave, &Slave::listEntries, q,
        [this](const KIO::UDSEntryList &list){ slotListEntries(list);} );

    QObject::connect(slave, &Slave::totalSize, q,
        [this](KIO::filesize_t size){ slotTotalSize(size);} );

    QObject::connect(slave, &Slave::redirection, q,
        [this](const QUrl &url){ slotRedirection(url);} );

    SimpleJobPrivate::start(slave);
}

const QUrl &ListJob::redirectionUrl() const
{
    return d_func()->m_redirectionURL;
}

#include "moc_listjob.cpp"
