/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000, 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "global.h"
#include "directorysizejob.h"
#include "listjob.h"
#include <kio/jobuidelegatefactory.h>
#include <QDebug>
#include <QTimer>

#include "job_p.h"

namespace KIO
{
class DirectorySizeJobPrivate: public KIO::JobPrivate
{
public:
    DirectorySizeJobPrivate()
        : m_totalSize(0L)
        , m_totalFiles(0L)
        , m_totalSubdirs(0L)
        , m_currentItem(0)
    {
    }
    explicit DirectorySizeJobPrivate(const KFileItemList &lstItems)
        : m_totalSize(0L)
        , m_totalFiles(0L)
        , m_totalSubdirs(0L)
        , m_lstItems(lstItems)
        , m_currentItem(0)
    {
    }
    KIO::filesize_t m_totalSize;
    KIO::filesize_t m_totalFiles;
    KIO::filesize_t m_totalSubdirs;
    KFileItemList m_lstItems;
    int m_currentItem;
    QHash<long, QSet<long> > m_visitedInodes; // device -> set of inodes

    void startNextJob(const QUrl &url);
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &);
    void processNextItem();

    Q_DECLARE_PUBLIC(DirectorySizeJob)

    static inline DirectorySizeJob *newJob(const QUrl &directory)
    {
        DirectorySizeJobPrivate *d = new DirectorySizeJobPrivate;
        DirectorySizeJob *job = new DirectorySizeJob(*d);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        d->startNextJob(directory);
        return job;
    }

    static inline DirectorySizeJob *newJob(const KFileItemList &lstItems)
    {
        DirectorySizeJobPrivate *d = new DirectorySizeJobPrivate(lstItems);
        DirectorySizeJob *job = new DirectorySizeJob(*d);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        QTimer::singleShot(0, job, SLOT(processNextItem()));
        return job;
    }
};

} // namespace KIO

using namespace KIO;

DirectorySizeJob::DirectorySizeJob(DirectorySizeJobPrivate &dd)
    : KIO::Job(dd)
{
}

DirectorySizeJob::~DirectorySizeJob()
{
}

KIO::filesize_t DirectorySizeJob::totalSize() const
{
    return d_func()->m_totalSize;
}

KIO::filesize_t DirectorySizeJob::totalFiles() const
{
    return d_func()->m_totalFiles;
}

KIO::filesize_t DirectorySizeJob::totalSubdirs() const
{
    return d_func()->m_totalSubdirs;
}

void DirectorySizeJobPrivate::processNextItem()
{
    Q_Q(DirectorySizeJob);
    while (m_currentItem < m_lstItems.count()) {
        const KFileItem item = m_lstItems[m_currentItem++];
        // qDebug() << item;
        if (!item.isLink()) {
            if (item.isDir()) {
                //qDebug() << "dir -> listing";
                startNextJob(item.url());
                return; // we'll come back later, when this one's finished
            } else {
                m_totalSize += item.size();
                m_totalFiles++;
                //qDebug() << "file -> " << m_totalSize;
            }
        } else {
            m_totalFiles++;
        }
    }
    //qDebug() << "finished";
    q->emitResult();
}

void DirectorySizeJobPrivate::startNextJob(const QUrl &url)
{
    Q_Q(DirectorySizeJob);
    //qDebug() << url;
    KIO::ListJob *listJob = KIO::listRecursive(url, KIO::HideProgressInfo);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
    // TODO KF6: remove legacy details code path
    listJob->addMetaData(QStringLiteral("details"), QStringLiteral("3"));
#endif
    listJob->addMetaData(QStringLiteral("statDetails"),
                         QString::number(KIO::StatBasic | KIO::StatResolveSymlink | KIO::StatInode));
    q->connect(listJob, &KIO::ListJob::entries,
               q, [this](KIO::Job* job, const KIO::UDSEntryList &list) { slotEntries(job, list); });
    q->addSubjob(listJob);
}

void DirectorySizeJobPrivate::slotEntries(KIO::Job *, const KIO::UDSEntryList &list)
{
    KIO::UDSEntryList::ConstIterator it = list.begin();
    const KIO::UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it) {

        const KIO::UDSEntry &entry = *it;

        const long device = entry.numberValue(KIO::UDSEntry::UDS_DEVICE_ID, 0);
        if (device && !entry.isLink()) {
            // Hard-link detection (#67939)
            const long inode = entry.numberValue(KIO::UDSEntry::UDS_INODE, 0);
            QSet<long> &visitedInodes = m_visitedInodes[device];  // find or insert
            if (visitedInodes.contains(inode)) {
                continue;
            }
            visitedInodes.insert(inode);
        }
        const KIO::filesize_t size = entry.numberValue(KIO::UDSEntry::UDS_SIZE, 0);
        const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        if (name == QLatin1Char('.')) {
            m_totalSize += size;
            //qDebug() << "'.': added" << size << "->" << m_totalSize;
        } else if (name != QLatin1String("..")) {
            if (!entry.isLink()) {
                m_totalSize += size;
            }
            if (!entry.isDir()) {
                m_totalFiles++;
            } else {
                m_totalSubdirs++;
            }
            //qDebug() << name << ":" << size << "->" << m_totalSize;
        }
    }
}

void DirectorySizeJob::slotResult(KJob *job)
{
    Q_D(DirectorySizeJob);
    //qDebug() << d->m_totalSize;
    removeSubjob(job);
    if (d->m_currentItem < d->m_lstItems.count()) {
        d->processNextItem();
    } else {
        if (job->error()) {
            setError(job->error());
            setErrorText(job->errorText());
        }
        emitResult();
    }
}

//static
DirectorySizeJob *KIO::directorySize(const QUrl &directory)
{
    return DirectorySizeJobPrivate::newJob(directory); // useless - but consistent with other jobs
}

//static
DirectorySizeJob *KIO::directorySize(const KFileItemList &lstItems)
{
    return DirectorySizeJobPrivate::newJob(lstItems);
}

#include "moc_directorysizejob.cpp"
