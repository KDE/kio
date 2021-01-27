/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "deletejob.h"

#include "job.h" // buildErrorString
#include "statjob.h"
#include "listjob.h"
#include "kcoredirlister.h"
#include "scheduler.h"
#include <KDirWatch>
#include "kprotocolmanager.h"
#include <kdirnotify.h>
#include "../pathhelpers_p.h"

#include <KLocalizedString>
#include <kio/jobuidelegatefactory.h>

#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QPointer>
#include <QThread>
#include <QMetaObject>

#include "job_p.h"

extern bool kio_resolve_local_urls; // from copyjob.cpp, abused here to save a symbol.

static bool isHttpProtocol(const QString &protocol)
{
    return (protocol.startsWith(QLatin1String("webdav"), Qt::CaseInsensitive) ||
            protocol.startsWith(QLatin1String("http"), Qt::CaseInsensitive));
}

namespace KIO
{
enum DeleteJobState {
    DELETEJOB_STATE_STATING,
    DELETEJOB_STATE_DELETING_FILES,
    DELETEJOB_STATE_DELETING_DIRS,
};

class DeleteJobIOWorker : public QObject {
    Q_OBJECT

Q_SIGNALS:
    void rmfileResult(bool succeeded, bool isLink);
    void rmddirResult(bool succeeded);

public Q_SLOTS:

    /**
     * Deletes the file @p url points to
     * The file must be a LocalFile
     */
    void rmfile(const QUrl& url, bool isLink) {
        Q_EMIT rmfileResult(QFile::remove(url.toLocalFile()), isLink);
    }

    /**
     * Deletes the directory @p url points to
     * The directory must be a LocalFile
     */
    void rmdir(const QUrl& url) {
        Q_EMIT rmddirResult(QDir().rmdir(url.toLocalFile()));
    }
};

class DeleteJobPrivate: public KIO::JobPrivate
{
public:
    explicit DeleteJobPrivate(const QList<QUrl> &src)
        : state(DELETEJOB_STATE_STATING)
        , m_processedFiles(0)
        , m_processedDirs(0)
        , m_totalFilesDirs(0)
        , m_srcList(src)
        , m_currentStat(m_srcList.begin())
        , m_reportTimer(nullptr)
    {
    }
    DeleteJobState state;
    int m_processedFiles;
    int m_processedDirs;
    int m_totalFilesDirs;
    QUrl m_currentURL;
    QList<QUrl> files;
    QList<QUrl> symlinks;
    QList<QUrl> dirs;
    QList<QUrl> m_srcList;
    QList<QUrl>::iterator m_currentStat;
    QSet<QString> m_parentDirs;
    QTimer *m_reportTimer;
    DeleteJobIOWorker *m_ioworker = nullptr;
    QThread *m_thread = nullptr;

    void statNextSrc();
    DeleteJobIOWorker* worker();
    void currentSourceStated(bool isDir, bool isLink);
    void finishedStatPhase();
    void deleteNextFile();
    void deleteNextDir();
    void restoreDirWatch() const;
    void slotReport();
    void slotStart();
    void slotEntries(KIO::Job *, const KIO::UDSEntryList &list);


    /// Callback of worker rmfile
    void rmFileResult(bool result, bool isLink);
    /// Callback of worker rmdir
    void rmdirResult(bool result);
    void deleteFileUsingJob(const QUrl& url, bool isLink);
    void deleteDirUsingJob(const QUrl& url);

    ~DeleteJobPrivate();

    Q_DECLARE_PUBLIC(DeleteJob)

    static inline DeleteJob *newJob(const QList<QUrl> &src, JobFlags flags)
    {
        DeleteJob *job = new DeleteJob(*new DeleteJobPrivate(src));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = Delete;
        }
        return job;
    }
};

} // namespace KIO

using namespace KIO;

DeleteJob::DeleteJob(DeleteJobPrivate &dd)
    : Job(dd)
{
    d_func()->m_reportTimer = new QTimer(this);
    connect(d_func()->m_reportTimer, &QTimer::timeout, this, [this]() { d_func()->slotReport(); });
    //this will update the report dialog with 5 Hz, I think this is fast enough, aleXXX
    d_func()->m_reportTimer->start(200);

    QTimer::singleShot(0, this, SLOT(slotStart()));
}

DeleteJob::~DeleteJob()
{
}

DeleteJobPrivate::~DeleteJobPrivate()
{
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
    }
}

QList<QUrl> DeleteJob::urls() const
{
    return d_func()->m_srcList;
}

void DeleteJobPrivate::slotStart()
{
    statNextSrc();
}

DeleteJobIOWorker* DeleteJobPrivate::worker()
{
    Q_Q(DeleteJob);

    if (!m_ioworker) {
        m_thread = new QThread();

        m_ioworker = new DeleteJobIOWorker;
        m_ioworker->moveToThread(m_thread);
        QObject::connect(m_thread, &QThread::finished, m_ioworker, &QObject::deleteLater);
        QObject::connect(m_ioworker, &DeleteJobIOWorker::rmfileResult, q, [=](bool result, bool isLink){
            this->rmFileResult(result, isLink);
        });
        QObject::connect(m_ioworker, &DeleteJobIOWorker::rmddirResult, q, [=](bool result){
            this->rmdirResult(result);
        });
        m_thread->start();
    }

    return m_ioworker;
}

void DeleteJobPrivate::slotReport()
{
    Q_Q(DeleteJob);
    Q_EMIT q->deleting(q, m_currentURL);

    // TODO: maybe we could skip everything else when (flags & HideProgressInfo) ?
    JobPrivate::emitDeleting(q, m_currentURL);

    switch (state) {
    case DELETEJOB_STATE_STATING:
        q->setTotalAmount(KJob::Files, files.count());
        q->setTotalAmount(KJob::Directories, dirs.count());
        break;
    case DELETEJOB_STATE_DELETING_DIRS:
        q->setProcessedAmount(KJob::Directories, m_processedDirs);
        q->emitPercent(m_processedFiles + m_processedDirs, m_totalFilesDirs);
        break;
    case DELETEJOB_STATE_DELETING_FILES:
        q->setProcessedAmount(KJob::Files, m_processedFiles);
        q->emitPercent(m_processedFiles, m_totalFilesDirs);
        break;
    }
}

void DeleteJobPrivate::slotEntries(KIO::Job *job, const UDSEntryList &list)
{
    UDSEntryList::ConstIterator it = list.begin();
    const UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it) {
        const UDSEntry &entry = *it;
        const QString displayName = entry.stringValue(KIO::UDSEntry::UDS_NAME);

        Q_ASSERT(!displayName.isEmpty());
        if (displayName != QLatin1String("..") && displayName != QLatin1String(".")) {
            QUrl url;
            const QString urlStr = entry.stringValue(KIO::UDSEntry::UDS_URL);
            if (!urlStr.isEmpty()) {
                url = QUrl(urlStr);
            } else {
                url = static_cast<SimpleJob *>(job)->url(); // assumed to be a dir
                url.setPath(concatPaths(url.path(), displayName));
            }

            //qDebug() << displayName << "(" << url << ")";
            if (entry.isLink()) {
                symlinks.append(url);
            } else if (entry.isDir()) {
                dirs.append(url);
            } else {
                files.append(url);
            }
        }
    }
}

void DeleteJobPrivate::statNextSrc()
{
    Q_Q(DeleteJob);
    //qDebug();
    if (m_currentStat != m_srcList.end()) {
        m_currentURL = (*m_currentStat);

        // if the file system doesn't support deleting, we do not even stat
        if (!KProtocolManager::supportsDeleting(m_currentURL)) {
            QPointer<DeleteJob> that = q;
            ++m_currentStat;
            Q_EMIT q->warning(q, buildErrorString(ERR_CANNOT_DELETE, m_currentURL.toDisplayString()));
            if (that) {
                statNextSrc();
            }
            return;
        }
        // Stat it
        state = DELETEJOB_STATE_STATING;

        // Fast path for KFileItems in directory views
        while (m_currentStat != m_srcList.end()) {
            m_currentURL = (*m_currentStat);
            const KFileItem cachedItem = KCoreDirLister::cachedItemForUrl(m_currentURL);
            if (cachedItem.isNull()) {
                break;
            }
            //qDebug() << "Found cached info about" << m_currentURL << "isDir=" << cachedItem.isDir() << "isLink=" << cachedItem.isLink();
            currentSourceStated(cachedItem.isDir(), cachedItem.isLink());
            ++m_currentStat;
        }

        // Hook for unit test to disable the fast path.
        if (!kio_resolve_local_urls) {

            // Fast path for local files
            // (using a loop, instead of a huge recursion)
            while (m_currentStat != m_srcList.end() && (*m_currentStat).isLocalFile()) {
                m_currentURL = (*m_currentStat);
                QFileInfo fileInfo(m_currentURL.toLocalFile());
                currentSourceStated(fileInfo.isDir(), fileInfo.isSymLink());
                ++m_currentStat;
            }
        }
        if (m_currentStat == m_srcList.end()) {
            // Done, jump to the last else of this method
            statNextSrc();
        } else {
            KIO::SimpleJob *job = KIO::statDetails(m_currentURL, StatJob::SourceSide, KIO::StatBasic, KIO::HideProgressInfo);
            Scheduler::setJobPriority(job, 1);
            //qDebug() << "stat'ing" << m_currentURL;
            q->addSubjob(job);
        }
    } else {
        if (!q->hasSubjobs()) { // don't go there yet if we're still listing some subdirs
            finishedStatPhase();
        }
    }
}

void DeleteJobPrivate::finishedStatPhase()
{
    m_totalFilesDirs = files.count() + symlinks.count() + dirs.count();
    slotReport();
    // Now we know which dirs hold the files we're going to delete.
    // To speed things up and prevent double-notification, we disable KDirWatch
    // on those dirs temporarily (using KDirWatch::self, that's the instance
    // used by e.g. kdirlister).
    const QSet<QString>::const_iterator itEnd = m_parentDirs.constEnd();
    for (QSet<QString>::const_iterator it = m_parentDirs.constBegin(); it != itEnd; ++it) {
        KDirWatch::self()->stopDirScan(*it);
    }
    state = DELETEJOB_STATE_DELETING_FILES;
    deleteNextFile();
}


void DeleteJobPrivate::rmFileResult(bool result, bool isLink)
{
    if (result) {
        m_processedFiles++;

        if (isLink) {
            symlinks.removeFirst();
        } else {
            files.removeFirst();
        }

        deleteNextFile();
    } else {
        // fallback if QFile::remove() failed (we'll use the job's error handling in that case)
        deleteFileUsingJob(m_currentURL, isLink);
    }
}

void DeleteJobPrivate::deleteFileUsingJob(const QUrl &url, bool isLink)
{
    Q_Q(DeleteJob);

    SimpleJob *job;
    if (isHttpProtocol(url.scheme())) {
        job = KIO::http_delete(url, KIO::HideProgressInfo);
    } else {
        job = KIO::file_delete(url, KIO::HideProgressInfo);
        job->setParentJob(q);
    }
    Scheduler::setJobPriority(job, 1);

    if (isLink) {
        symlinks.removeFirst();
    } else {
        files.removeFirst();
    }

    q->addSubjob(job);
}

void DeleteJobPrivate::deleteNextFile()
{
    //qDebug();

    // if there is something else to delete
    // the loop is run using callbacks slotResult and rmFileResult
    if (!files.isEmpty() || !symlinks.isEmpty()) {

        // Take first file to delete out of list
        QList<QUrl>::iterator it = files.begin();
        const bool isLink = (it == files.end()); // No more files
        if (isLink) {
            it = symlinks.begin(); // Pick up a symlink to delete
        }
        m_currentURL = (*it);

        // If local file, try do it directly
        if (m_currentURL.isLocalFile()) {
            // separate thread will do the work
            QMetaObject::invokeMethod(worker(), "rmfile", Qt::QueuedConnection, Q_ARG(QUrl, m_currentURL), Q_ARG(bool, isLink));
        } else {
            // if remote, use a job
            deleteFileUsingJob(m_currentURL, isLink);
        }
        return;
    }

    state = DELETEJOB_STATE_DELETING_DIRS;
    deleteNextDir();
}

void DeleteJobPrivate::rmdirResult(bool result)
{
    if (result) {
        m_processedDirs++;
        dirs.removeLast();
        deleteNextDir();
    } else {
        // fallback
        deleteDirUsingJob(m_currentURL);
    }
}

void DeleteJobPrivate::deleteDirUsingJob(const QUrl &url)
{
    Q_Q(DeleteJob);

    // Call rmdir - works for kioslaves with canDeleteRecursive too,
    // CMD_DEL will trigger the recursive deletion in the slave.
    SimpleJob *job = KIO::rmdir(url);
    job->setParentJob(q);
    job->addMetaData(QStringLiteral("recurse"), QStringLiteral("true"));
    Scheduler::setJobPriority(job, 1);
    dirs.removeLast();
    q->addSubjob(job);
}

void DeleteJobPrivate::deleteNextDir()
{
    Q_Q(DeleteJob);

    if (!dirs.isEmpty()) { // some dirs to delete ?

        // the loop is run using callbacks slotResult and rmdirResult
        // Take first dir to delete out of list - last ones first !
        QList<QUrl>::iterator it = --dirs.end();
        m_currentURL = (*it);
        // If local dir, try to rmdir it directly
        if (m_currentURL.isLocalFile()) {
            // delete it on separate worker thread
            QMetaObject::invokeMethod(worker(), "rmdir", Qt::QueuedConnection, Q_ARG(QUrl, m_currentURL));
        } else {
            deleteDirUsingJob(m_currentURL);
        }
        return;
    }

    // Re-enable watching on the dirs that held the deleted files
    restoreDirWatch();

    // Finished - tell the world
    if (!m_srcList.isEmpty()) {
        //qDebug() << "KDirNotify'ing FilesRemoved" << m_srcList;
        org::kde::KDirNotify::emitFilesRemoved(m_srcList);
    }
    if (m_reportTimer != nullptr) {
        m_reportTimer->stop();
    }
    // display final numbers
    q->setProcessedAmount(KJob::Directories, m_processedDirs);
    q->setProcessedAmount(KJob::Files, m_processedFiles);
    q->emitPercent(m_processedFiles + m_processedDirs, m_totalFilesDirs);

    q->emitResult();
}

void DeleteJobPrivate::restoreDirWatch() const
{
    const auto itEnd = m_parentDirs.constEnd();
    for (auto it = m_parentDirs.constBegin(); it != itEnd; ++it) {
        KDirWatch::self()->restartDirScan(*it);
    }
}

void DeleteJobPrivate::currentSourceStated(bool isDir, bool isLink)
{
    Q_Q(DeleteJob);
    const QUrl url = (*m_currentStat);
    if (isDir && !isLink) {
        // Add toplevel dir in list of dirs
        dirs.append(url);
        if (url.isLocalFile()) {
            // We are about to delete this dir, no need to watch it
            // Maybe we should ask kdirwatch to remove all watches recursively?
            // But then there would be no feedback (things disappearing progressively) during huge deletions
            KDirWatch::self()->stopDirScan(url.adjusted(QUrl::StripTrailingSlash).toLocalFile());
        }
        if (!KProtocolManager::canDeleteRecursive(url)) {
            //qDebug() << url << "is a directory, let's list it";
            ListJob *newjob = KIO::listRecursive(url, KIO::HideProgressInfo);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 69)
            // TODO KF6: remove legacy details code path
            newjob->addMetaData(QStringLiteral("details"), QStringLiteral("0"));
#endif
            newjob->addMetaData(QStringLiteral("statDetails"), QString::number(KIO::StatBasic));
            newjob->setUnrestricted(true); // No KIOSK restrictions
            Scheduler::setJobPriority(newjob, 1);
            QObject::connect(newjob, &KIO::ListJob::entries,
                             q, [this](KIO::Job* job, const KIO::UDSEntryList &list) { slotEntries(job, list); });
            q->addSubjob(newjob);
            // Note that this listing job will happen in parallel with other stat jobs.
        }
    } else {
        if (isLink) {
            //qDebug() << "Target is a symlink";
            symlinks.append(url);
        } else {
            //qDebug() << "Target is a file";
            files.append(url);
        }
    }
    if (url.isLocalFile()) {
        const QString parentDir = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).path();
        m_parentDirs.insert(parentDir);
    }
}

void DeleteJob::slotResult(KJob *job)
{
    Q_D(DeleteJob);
    switch (d->state) {
    case DELETEJOB_STATE_STATING:
        removeSubjob(job);

        // Was this a stat job or a list job? We do both in parallel.
        if (StatJob *statJob = qobject_cast<StatJob *>(job)) {
            // Was there an error while stating ?
            if (job->error()) {
                // Probably : doesn't exist
                Job::slotResult(job); // will set the error and emit result(this)
                d->restoreDirWatch();
                return;
            }

            const UDSEntry &entry = statJob->statResult();
            // Is it a file or a dir ?
            const bool isLink = entry.isLink();
            const bool isDir = entry.isDir();
            d->currentSourceStated(isDir, isLink);

            ++d->m_currentStat;
            d->statNextSrc();
        } else {
            if (job->error()) {
                // Try deleting nonetheless, it may be empty (and non-listable)
            }
            if (!hasSubjobs()) {
                d->finishedStatPhase();
            }
        }
        break;
    case DELETEJOB_STATE_DELETING_FILES:
        // Propagate the subjob's metadata (a SimpleJob) to the real DeleteJob
        // FIXME: setMetaData() in the KIO API only allows access to outgoing metadata,
        // but we need to alter the incoming one
        d->m_incomingMetaData = dynamic_cast<KIO::Job *>(job)->metaData();

        if (job->error()) {
            Job::slotResult(job);   // will set the error and emit result(this)
            d->restoreDirWatch();
            return;
        }
        removeSubjob(job);
        Q_ASSERT(!hasSubjobs());
        d->m_processedFiles++;

        d->deleteNextFile();
        break;
    case DELETEJOB_STATE_DELETING_DIRS:
        if (job->error()) {
            Job::slotResult(job);   // will set the error and emit result(this)
            d->restoreDirWatch();
            return;
        }
        removeSubjob(job);
        Q_ASSERT(!hasSubjobs());
        d->m_processedDirs++;
        //emit processedAmount( this, KJob::Directories, d->m_processedDirs );
        //emitPercent( d->m_processedFiles + d->m_processedDirs, d->m_totalFilesDirs );

        d->deleteNextDir();
        break;
    default:
        Q_ASSERT(0);
    }
}

DeleteJob *KIO::del(const QUrl &src, JobFlags flags)
{
    QList<QUrl> srcList;
    srcList.append(src);
    DeleteJob *job = DeleteJobPrivate::newJob(srcList, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::RemoveContent);
    }
    return job;
}

DeleteJob *KIO::del(const QList<QUrl> &src, JobFlags flags)
{
    DeleteJob *job = DeleteJobPrivate::newJob(src, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::RemoveContent);
    }
    return job;
}

#include "deletejob.moc"
#include "moc_deletejob.cpp"
