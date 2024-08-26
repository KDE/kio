/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "scheduler.h"
#include "scheduler_p.h"

#include "job_p.h"
#include "worker_p.h"
#include "workerconfig.h"

#include <kprotocolinfo.h>
#include <kprotocolmanager.h>

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#include <QHash>
#include <QThread>
#include <QThreadStorage>

// Workers may be idle for a certain time (3 minutes) before they are killed.
static const int s_idleWorkerLifetime = 3 * 60;

using namespace KIO;

static inline Worker *jobSWorker(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_worker;
}

static inline void startJob(SimpleJob *job, Worker *worker)
{
    SimpleJobPrivate::get(job)->start(worker);
}

class KIO::SchedulerPrivate
{
public:
    SchedulerPrivate()
        : q(new Scheduler())
    {
    }

    ~SchedulerPrivate()
    {
        removeWorkerOnHold();
        delete q;
        q = nullptr;
        qDeleteAll(m_protocols); // ~ProtoQueue will kill and delete all workers
    }

    SchedulerPrivate(const SchedulerPrivate &) = delete;
    SchedulerPrivate &operator=(const SchedulerPrivate &) = delete;

    Scheduler *q;

    Worker *m_workerOnHold = nullptr;
    QUrl m_urlOnHold;
    bool m_ignoreConfigReparse = false;

    void doJob(SimpleJob *job);
    void cancelJob(SimpleJob *job);
    void jobFinished(KIO::SimpleJob *job, KIO::Worker *worker);
    void putWorkerOnHold(KIO::SimpleJob *job, const QUrl &url);
    void removeWorkerOnHold();
    Worker *heldWorkerForJob(KIO::SimpleJob *job);
    bool isWorkerOnHoldFor(const QUrl &url);
    void updateInternalMetaData(SimpleJob *job);

    MetaData metaDataFor(const QString &protocol, const QUrl &url);
    void setupWorker(KIO::Worker *worker, const QUrl &url, const QString &protocol, bool newWorker, const KIO::MetaData *config = nullptr);

    void slotWorkerDied(KIO::Worker *worker);

#ifdef WITH_QTDBUS
    void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &);
#endif

    ProtoQueue *protoQ(const QString &protocol, const QString &host);

private:
    QHash<QString, ProtoQueue *> m_protocols;
};

static QThreadStorage<SchedulerPrivate *> s_storage;
static SchedulerPrivate *schedulerPrivate()
{
    if (!s_storage.hasLocalData()) {
        s_storage.setLocalData(new SchedulerPrivate);
    }
    return s_storage.localData();
}

Scheduler *Scheduler::self()
{
    return schedulerPrivate()->q;
}

SchedulerPrivate *Scheduler::d_func()
{
    return schedulerPrivate();
}

// static
Scheduler *scheduler()
{
    return schedulerPrivate()->q;
}

////////////////////////////

WorkerManager::WorkerManager()
{
    m_grimTimer.setSingleShot(true);
    connect(&m_grimTimer, &QTimer::timeout, this, &WorkerManager::grimReaper);
}

WorkerManager::~WorkerManager()
{
    grimReaper();
}

void WorkerManager::returnWorker(Worker *worker)
{
    Q_ASSERT(worker);
    worker->setIdle();
    m_idleWorkers.insert(worker->host(), worker);
    scheduleGrimReaper();
}

Worker *WorkerManager::takeWorkerForJob(SimpleJob *job)
{
    Worker *worker = schedulerPrivate()->heldWorkerForJob(job);
    if (worker) {
        return worker;
    }

    QUrl url = SimpleJobPrivate::get(job)->m_url;
    // TODO take port, username and password into account
    QMultiHash<QString, Worker *>::Iterator it = m_idleWorkers.find(url.host());
    if (it == m_idleWorkers.end()) {
        it = m_idleWorkers.begin();
    }
    if (it == m_idleWorkers.end()) {
        return nullptr;
    }
    worker = it.value();
    m_idleWorkers.erase(it);
    return worker;
}

bool WorkerManager::removeWorker(Worker *worker)
{
    // ### performance not so great
    QMultiHash<QString, Worker *>::Iterator it = m_idleWorkers.begin();
    for (; it != m_idleWorkers.end(); ++it) {
        if (it.value() == worker) {
            m_idleWorkers.erase(it);
            return true;
        }
    }
    return false;
}

void WorkerManager::clear()
{
    m_idleWorkers.clear();
}

QList<Worker *> WorkerManager::allWorkers() const
{
    return m_idleWorkers.values();
}

void WorkerManager::scheduleGrimReaper()
{
    if (!m_grimTimer.isActive()) {
        m_grimTimer.start((s_idleWorkerLifetime / 2) * 1000);
    }
}

// private slot
void WorkerManager::grimReaper()
{
    QMultiHash<QString, Worker *>::Iterator it = m_idleWorkers.begin();
    while (it != m_idleWorkers.end()) {
        Worker *worker = it.value();
        if (worker->idleTime() >= s_idleWorkerLifetime) {
            it = m_idleWorkers.erase(it);
            if (worker->job()) {
                // qDebug() << "Idle worker" << worker << "still has job" << worker->job();
            }
            // avoid invoking slotWorkerDied() because its cleanup services are not needed
            worker->kill();
        } else {
            ++it;
        }
    }
    if (!m_idleWorkers.isEmpty()) {
        scheduleGrimReaper();
    }
}

int HostQueue::lowestSerial() const
{
    QMap<int, SimpleJob *>::ConstIterator first = m_queuedJobs.constBegin();
    if (first != m_queuedJobs.constEnd()) {
        return first.key();
    }
    return SerialPicker::maxSerial;
}

void HostQueue::queueJob(SimpleJob *job)
{
    const int serial = SimpleJobPrivate::get(job)->m_schedSerial;
    Q_ASSERT(serial != 0);
    Q_ASSERT(!m_queuedJobs.contains(serial));
    Q_ASSERT(!m_runningJobs.contains(job));
    m_queuedJobs.insert(serial, job);
}

SimpleJob *HostQueue::takeFirstInQueue()
{
    Q_ASSERT(!m_queuedJobs.isEmpty());
    QMap<int, SimpleJob *>::iterator first = m_queuedJobs.begin();
    SimpleJob *job = first.value();
    m_queuedJobs.erase(first);
    m_runningJobs.insert(job);
    return job;
}

bool HostQueue::removeJob(SimpleJob *job)
{
    const int serial = SimpleJobPrivate::get(job)->m_schedSerial;
    if (m_runningJobs.remove(job)) {
        Q_ASSERT(!m_queuedJobs.contains(serial));
        return true;
    }
    if (m_queuedJobs.remove(serial)) {
        return true;
    }
    return false;
}

QList<Worker *> HostQueue::allWorkers() const
{
    QList<Worker *> ret;
    ret.reserve(m_runningJobs.size());
    for (SimpleJob *job : m_runningJobs) {
        Worker *worker = jobSWorker(job);
        Q_ASSERT(worker);
        ret.append(worker);
    }
    return ret;
}

static void ensureNoDuplicates(QMap<int, HostQueue *> *queuesBySerial)
{
    Q_UNUSED(queuesBySerial);
#ifdef SCHEDULER_DEBUG
    // a host queue may *never* be in queuesBySerial twice.
    QSet<HostQueue *> seen;
    auto it = queuesBySerial->cbegin();
    for (; it != queuesBySerial->cend(); ++it) {
        Q_ASSERT(!seen.contains(it.value()));
        seen.insert(it.value());
    }
#endif
}

static void verifyRunningJobsCount(QHash<QString, HostQueue> *queues, int runningJobsCount)
{
    Q_UNUSED(queues);
    Q_UNUSED(runningJobsCount);
#ifdef SCHEDULER_DEBUG
    int realRunningJobsCount = 0;
    auto it = queues->cbegin();
    for (; it != queues->cend(); ++it) {
        realRunningJobsCount += it.value().runningJobsCount();
    }
    Q_ASSERT(realRunningJobsCount == runningJobsCount);

    // ...and of course we may never run the same job twice!
    QSet<SimpleJob *> seenJobs;
    auto it2 = queues->cbegin();
    for (; it2 != queues->cend(); ++it2) {
        for (SimpleJob *job : it2.value().runningJobs()) {
            Q_ASSERT(!seenJobs.contains(job));
            seenJobs.insert(job);
        }
    }
#endif
}

ProtoQueue::ProtoQueue(int maxWorkers, int maxWorkersPerHost)
    : m_maxConnectionsPerHost(maxWorkersPerHost ? maxWorkersPerHost : maxWorkers)
    , m_maxConnectionsTotal(qMax(maxWorkers, maxWorkersPerHost))
    , m_runningJobsCount(0)

{
    /*qDebug() << "m_maxConnectionsTotal:" << m_maxConnectionsTotal
                 << "m_maxConnectionsPerHost:" << m_maxConnectionsPerHost;*/
    Q_ASSERT(m_maxConnectionsPerHost >= 1);
    Q_ASSERT(maxWorkers >= maxWorkersPerHost);
    m_startJobTimer.setSingleShot(true);
    connect(&m_startJobTimer, &QTimer::timeout, this, &ProtoQueue::startAJob);
}

ProtoQueue::~ProtoQueue()
{
    // Gather list of all workers first
    const QList<Worker *> workers = allWorkers();
    // Clear the idle workers in the manager to avoid dangling pointers
    m_workerManager.clear();
    for (Worker *worker : workers) {
        // kill the worker process and remove the interface in our process
        worker->kill();
    }
}

void ProtoQueue::queueJob(SimpleJob *job)
{
    QString hostname = SimpleJobPrivate::get(job)->m_url.host();
    HostQueue &hq = m_queuesByHostname[hostname];
    const int prevLowestSerial = hq.lowestSerial();
    Q_ASSERT(hq.runningJobsCount() <= m_maxConnectionsPerHost);

    // nevert insert a job twice
    Q_ASSERT(SimpleJobPrivate::get(job)->m_schedSerial == 0);
    SimpleJobPrivate::get(job)->m_schedSerial = m_serialPicker.next();

    const bool wasQueueEmpty = hq.isQueueEmpty();
    hq.queueJob(job);
    // note that HostQueue::queueJob() into an empty queue changes its lowestSerial() too...
    // the queue's lowest serial job may have changed, so update the ordered list of queues.
    // however, we ignore all jobs that would cause more connections to a host than allowed.
    if (prevLowestSerial != hq.lowestSerial()) {
        if (hq.runningJobsCount() < m_maxConnectionsPerHost) {
            // if the connection limit didn't keep the HQ unscheduled it must have been lack of jobs
            if (m_queuesBySerial.remove(prevLowestSerial) == 0) {
                Q_UNUSED(wasQueueEmpty);
                Q_ASSERT(wasQueueEmpty);
            }
            m_queuesBySerial.insert(hq.lowestSerial(), &hq);
        } else {
#ifdef SCHEDULER_DEBUG
            // ### this assertion may fail if the limits were modified at runtime!
            // if the per-host connection limit is already reached the host queue's lowest serial
            // should not be queued.
            Q_ASSERT(!m_queuesBySerial.contains(prevLowestSerial));
#endif
        }
    }
    // just in case; startAJob() will refuse to start a job if it shouldn't.
    m_startJobTimer.start();

    ensureNoDuplicates(&m_queuesBySerial);
}

void ProtoQueue::removeJob(SimpleJob *job)
{
    SimpleJobPrivate *jobPriv = SimpleJobPrivate::get(job);
    HostQueue &hq = m_queuesByHostname[jobPriv->m_url.host()];
    const int prevLowestSerial = hq.lowestSerial();
    const int prevRunningJobs = hq.runningJobsCount();

    Q_ASSERT(hq.runningJobsCount() <= m_maxConnectionsPerHost);

    if (hq.removeJob(job)) {
        if (hq.lowestSerial() != prevLowestSerial) {
            // we have dequeued the not yet running job with the lowest serial
            Q_ASSERT(!jobPriv->m_worker);
            Q_ASSERT(prevRunningJobs == hq.runningJobsCount());
            if (m_queuesBySerial.remove(prevLowestSerial) == 0) {
                // make sure that the queue was not scheduled for a good reason
                Q_ASSERT(hq.runningJobsCount() == m_maxConnectionsPerHost);
            }
        } else {
            if (prevRunningJobs != hq.runningJobsCount()) {
                // we have dequeued a previously running job
                Q_ASSERT(prevRunningJobs - 1 == hq.runningJobsCount());
                m_runningJobsCount--;
                Q_ASSERT(m_runningJobsCount >= 0);
            }
        }
        if (!hq.isQueueEmpty() && hq.runningJobsCount() < m_maxConnectionsPerHost) {
            // this may be a no-op, but it's faster than first checking if it's already in.
            m_queuesBySerial.insert(hq.lowestSerial(), &hq);
        }

        if (hq.isEmpty()) {
            // no queued jobs, no running jobs. this destroys hq from above.
            m_queuesByHostname.remove(jobPriv->m_url.host());
        }

        if (jobPriv->m_worker && jobPriv->m_worker->isAlive()) {
            m_workerManager.returnWorker(jobPriv->m_worker);
        }
        // just in case; startAJob() will refuse to start a job if it shouldn't.
        m_startJobTimer.start();
    }

    ensureNoDuplicates(&m_queuesBySerial);
}

Worker *ProtoQueue::createWorker(const QString &protocol, SimpleJob *job, const QUrl &url)
{
    int error;
    QString errortext;
    Worker *worker = Worker::createWorker(protocol, url, error, errortext);
    if (worker) {
        connect(worker, &Worker::workerDied, scheduler(), [](KIO::Worker *worker) {
            schedulerPrivate()->slotWorkerDied(worker);
        });
    } else {
        qCWarning(KIO_CORE) << "couldn't create worker:" << errortext;
        if (job) {
            job->slotError(error, errortext);
        }
    }
    return worker;
}

bool ProtoQueue::removeWorker(KIO::Worker *worker)
{
    const bool removed = m_workerManager.removeWorker(worker);
    return removed;
}

QList<Worker *> ProtoQueue::allWorkers() const
{
    QList<Worker *> ret(m_workerManager.allWorkers());
    auto it = m_queuesByHostname.cbegin();
    for (; it != m_queuesByHostname.cend(); ++it) {
        ret.append(it.value().allWorkers());
    }

    return ret;
}

// private slot
void ProtoQueue::startAJob()
{
    ensureNoDuplicates(&m_queuesBySerial);
    verifyRunningJobsCount(&m_queuesByHostname, m_runningJobsCount);

#ifdef SCHEDULER_DEBUG
    // qDebug() << "m_runningJobsCount:" << m_runningJobsCount;
    auto it = m_queuesByHostname.cbegin();
    for (; it != m_queuesByHostname.cend(); ++it) {
        const QList<KIO::SimpleJob *> list = it.value().runningJobs();
        for (SimpleJob *job : list) {
            // qDebug() << SimpleJobPrivate::get(job)->m_url;
        }
    }
#endif
    if (m_runningJobsCount >= m_maxConnectionsTotal) {
#ifdef SCHEDULER_DEBUG
        // qDebug() << "not starting any jobs because maxConnectionsTotal has been reached.";
#endif
        return;
    }

    QMap<int, HostQueue *>::iterator first = m_queuesBySerial.begin();
    if (first != m_queuesBySerial.end()) {
        // pick a job and maintain the queue invariant: lower serials first
        HostQueue *hq = first.value();
        const int prevLowestSerial = first.key();
        Q_UNUSED(prevLowestSerial);
        Q_ASSERT(hq->lowestSerial() == prevLowestSerial);
        // the following assertions should hold due to queueJob(), takeFirstInQueue() and
        // removeJob() being correct
        Q_ASSERT(hq->runningJobsCount() < m_maxConnectionsPerHost);
        SimpleJob *startingJob = hq->takeFirstInQueue();
        Q_ASSERT(hq->runningJobsCount() <= m_maxConnectionsPerHost);
        Q_ASSERT(hq->lowestSerial() != prevLowestSerial);

        m_queuesBySerial.erase(first);
        // we've increased hq's runningJobsCount() by calling nexStartingJob()
        // so we need to check again.
        if (!hq->isQueueEmpty() && hq->runningJobsCount() < m_maxConnectionsPerHost) {
            m_queuesBySerial.insert(hq->lowestSerial(), hq);
        }

        // always increase m_runningJobsCount because it's correct if there is a worker and if there
        // is no worker, removeJob() will balance the number again. removeJob() would decrease the
        // number too much otherwise.
        // Note that createWorker() can call slotError() on a job which in turn calls removeJob(),
        // so increase the count here already.
        m_runningJobsCount++;

        bool isNewWorker = false;
        Worker *worker = m_workerManager.takeWorkerForJob(startingJob);
        SimpleJobPrivate *jobPriv = SimpleJobPrivate::get(startingJob);
        if (!worker) {
            isNewWorker = true;
            worker = createWorker(jobPriv->m_protocol, startingJob, jobPriv->m_url);
        }

        if (worker) {
            jobPriv->m_worker = worker;
            schedulerPrivate()->setupWorker(worker, jobPriv->m_url, jobPriv->m_protocol, isNewWorker);
            startJob(startingJob, worker);
        } else {
            // dispose of our records about the job and mark the job as unknown
            // (to prevent crashes later)
            // note that the job's slotError() can have called removeJob() first, so check that
            // it's not a ghost job with null serial already.
            if (jobPriv->m_schedSerial) {
                removeJob(startingJob);
                jobPriv->m_schedSerial = 0;
            }
        }
    } else {
#ifdef SCHEDULER_DEBUG
        // qDebug() << "not starting any jobs because there is no queued job.";
#endif
    }

    if (!m_queuesBySerial.isEmpty()) {
        m_startJobTimer.start();
    }
}

Scheduler::Scheduler()
{
    setObjectName(QStringLiteral("scheduler"));

#ifdef WITH_QTDBUS
    const QString dbusPath = QStringLiteral("/KIO/Scheduler");
    const QString dbusInterface = QStringLiteral("org.kde.KIO.Scheduler");
    QDBusConnection dbus = QDBusConnection::sessionBus();
    // Not needed, right? We just want to emit two signals.
    // dbus.registerObject(dbusPath, this, QDBusConnection::ExportScriptableSlots |
    //                    QDBusConnection::ExportScriptableSignals);
    dbus.connect(QString(),
                 dbusPath,
                 dbusInterface,
                 QStringLiteral("reparseSlaveConfiguration"),
                 this,
                 SLOT(slotReparseSlaveConfiguration(QString, QDBusMessage)));
#endif
}

Scheduler::~Scheduler()
{
}

void Scheduler::doJob(SimpleJob *job)
{
    schedulerPrivate()->doJob(job);
}

// static
void Scheduler::cancelJob(SimpleJob *job)
{
    schedulerPrivate()->cancelJob(job);
}

void Scheduler::jobFinished(KIO::SimpleJob *job, KIO::Worker *worker)
{
    schedulerPrivate()->jobFinished(job, worker);
}

void Scheduler::putWorkerOnHold(KIO::SimpleJob *job, const QUrl &url)
{
    schedulerPrivate()->putWorkerOnHold(job, url);
}

void Scheduler::removeWorkerOnHold()
{
    schedulerPrivate()->removeWorkerOnHold();
}

bool Scheduler::isWorkerOnHoldFor(const QUrl &url)
{
    return schedulerPrivate()->isWorkerOnHoldFor(url);
}

void Scheduler::updateInternalMetaData(SimpleJob *job)
{
    schedulerPrivate()->updateInternalMetaData(job);
}

void Scheduler::emitReparseSlaveConfiguration()
{
#ifdef WITH_QTDBUS
    // Do it immediately in this process, otherwise we might send a request before reparsing
    // (e.g. when changing useragent in the plugin)
    schedulerPrivate()->slotReparseSlaveConfiguration(QString(), QDBusMessage());
#endif

    schedulerPrivate()->m_ignoreConfigReparse = true;
    Q_EMIT self()->reparseSlaveConfiguration(QString());
}

#ifdef WITH_QTDBUS
void SchedulerPrivate::slotReparseSlaveConfiguration(const QString &proto, const QDBusMessage &)
{
    if (m_ignoreConfigReparse) {
        // qDebug() << "Ignoring signal sent by myself";
        m_ignoreConfigReparse = false;
        return;
    }

    // qDebug() << "proto=" << proto;
    KProtocolManager::reparseConfiguration();
    WorkerConfig::self()->reset();

    QHash<QString, ProtoQueue *>::ConstIterator it = proto.isEmpty() ? m_protocols.constBegin() : m_protocols.constFind(proto);
    QHash<QString, ProtoQueue *>::ConstIterator endIt = m_protocols.constEnd();

    // not found?
    if (it == endIt) {
        return;
    }

    if (!proto.isEmpty()) {
        endIt = it;
        ++endIt;
    }

    for (; it != endIt; ++it) {
        const QList<KIO::Worker *> list = it.value()->allWorkers();
        for (Worker *worker : list) {
            worker->send(CMD_REPARSECONFIGURATION);
            worker->resetHost();
        }
    }
}
#endif

void SchedulerPrivate::doJob(SimpleJob *job)
{
    // qDebug() << job;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    jobPriv->m_protocol = job->url().scheme();

    ProtoQueue *proto = protoQ(jobPriv->m_protocol, job->url().host());
    proto->queueJob(job);
}

void SchedulerPrivate::cancelJob(SimpleJob *job)
{
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    // this method is called all over the place in job.cpp, so just do this check here to avoid
    // much boilerplate in job code.
    if (jobPriv->m_schedSerial == 0) {
        // qDebug() << "Doing nothing because I don't know job" << job;
        return;
    }
    Worker *worker = jobSWorker(job);
    // qDebug() << job << worker;
    jobFinished(job, worker);
    if (worker) {
        ProtoQueue *pq = m_protocols.value(jobPriv->m_protocol);
        if (pq) {
            pq->removeWorker(worker);
        }
        worker->kill(); // don't use worker after this!
    }
}

void SchedulerPrivate::jobFinished(SimpleJob *job, Worker *worker)
{
    // qDebug() << job << worker;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    // make sure that we knew about the job!
    Q_ASSERT(jobPriv->m_schedSerial);

    ProtoQueue *pq = m_protocols.value(jobPriv->m_protocol);
    if (pq) {
        pq->removeJob(job);
    }

    if (worker) {
        // If we have internal meta-data, tell existing KIO workers to reload
        // their configuration.
        if (jobPriv->m_internalMetaData.count()) {
            // qDebug() << "Updating KIO workers with new internal metadata information";
            ProtoQueue *queue = m_protocols.value(worker->protocol());
            if (queue) {
                const QList<Worker *> workers = queue->allWorkers();
                for (auto *runningWorker : workers) {
                    if (worker->host() == runningWorker->host()) {
                        worker->setConfig(metaDataFor(worker->protocol(), job->url()));
                        /*qDebug() << "Updated configuration of" << worker->protocol()
                                     << "KIO worker, pid=" << worker->worker_pid();*/
                    }
                }
            }
        }
        worker->setJob(nullptr);
        worker->disconnect(job);
    }
    jobPriv->m_schedSerial = 0; // this marks the job as unscheduled again
    jobPriv->m_worker = nullptr;
    // Clear the values in the internal metadata container since they have
    // already been taken care of above...
    jobPriv->m_internalMetaData.clear();
}

MetaData SchedulerPrivate::metaDataFor(const QString &protocol, const QUrl &url)
{
    const QString host = url.host();
    MetaData configData = WorkerConfig::self()->configData(protocol, host);

    return configData;
}

void SchedulerPrivate::setupWorker(KIO::Worker *worker, const QUrl &url, const QString &protocol, bool newWorker, const KIO::MetaData *config)
{
    int port = url.port();
    if (port == -1) { // no port is -1 in QUrl, but in kde3 we used 0 and the KIO workers assume that.
        port = 0;
    }
    const QString host = url.host();
    const QString user = url.userName();
    const QString passwd = url.password();

    if (newWorker || worker->host() != host || worker->port() != port || worker->user() != user || worker->passwd() != passwd) {
        MetaData configData = metaDataFor(protocol, url);
        if (config) {
            configData += *config;
        }

        worker->setConfig(configData);
        worker->setProtocol(url.scheme());
        worker->setHost(host, port, user, passwd);
    }
}

void SchedulerPrivate::slotWorkerDied(KIO::Worker *worker)
{
    // qDebug() << worker;
    Q_ASSERT(worker);
    Q_ASSERT(!worker->isAlive());
    ProtoQueue *pq = m_protocols.value(worker->protocol());
    if (pq) {
        if (worker->job()) {
            pq->removeJob(worker->job());
        }
        // in case this was a connected worker...
        pq->removeWorker(worker);
    }
    if (worker == m_workerOnHold) {
        m_workerOnHold = nullptr;
        m_urlOnHold.clear();
    }
    // can't use worker->deref() here because we need to use deleteLater
    worker->aboutToDelete();
    worker->deleteLater();
}

void SchedulerPrivate::putWorkerOnHold(KIO::SimpleJob *job, const QUrl &url)
{
    Worker *worker = jobSWorker(job);
    // qDebug() << job << url << worker;
    worker->disconnect(job);
    // prevent the fake death of the worker from trying to kill the job again;
    // cf. Worker::hold(const QUrl &url) called in SchedulerPrivate::publishWorkerOnHold().
    worker->setJob(nullptr);
    SimpleJobPrivate::get(job)->m_worker = nullptr;

    if (m_workerOnHold) {
        m_workerOnHold->kill();
    }
    m_workerOnHold = worker;
    m_urlOnHold = url;
    m_workerOnHold->suspend();
}

bool SchedulerPrivate::isWorkerOnHoldFor(const QUrl &url)
{
    if (url.isValid() && m_urlOnHold.isValid() && url == m_urlOnHold) {
        return true;
    }

    return false;
}

Worker *SchedulerPrivate::heldWorkerForJob(SimpleJob *job)
{
    Worker *worker = nullptr;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    if (m_workerOnHold) {
        // Make sure that the job wants to do a GET or a POST, and with no offset
        const int cmd = jobPriv->m_command;
        bool canJobReuse = (cmd == CMD_GET);

        if (KIO::TransferJob *tJob = qobject_cast<KIO::TransferJob *>(job)) {
            canJobReuse = (canJobReuse || cmd == CMD_SPECIAL);
            if (canJobReuse) {
                KIO::MetaData outgoing = tJob->outgoingMetaData();
                const QString resume = outgoing.value(QStringLiteral("resume"));
                const QString rangeStart = outgoing.value(QStringLiteral("range-start"));
                // qDebug() << "Resume metadata is" << resume;
                canJobReuse = (resume.isEmpty() || resume == QLatin1Char('0')) && (rangeStart.isEmpty() || rangeStart == QLatin1Char('0'));
            }
        }

        if (job->url() == m_urlOnHold) {
            if (canJobReuse) {
                // qDebug() << "HOLD: Reusing held worker (" << m_workerOnHold << ")";
                worker = m_workerOnHold;
            } else {
                // qDebug() << "HOLD: Discarding held worker (" << m_workerOnHold << ")";
                m_workerOnHold->kill();
            }
            m_workerOnHold = nullptr;
            m_urlOnHold.clear();
        }
    }

    return worker;
}

void SchedulerPrivate::removeWorkerOnHold()
{
    // qDebug() << m_workerOnHold;
    if (m_workerOnHold) {
        m_workerOnHold->kill();
    }
    m_workerOnHold = nullptr;
    m_urlOnHold.clear();
}

ProtoQueue *SchedulerPrivate::protoQ(const QString &protocol, const QString &host)
{
    ProtoQueue *pq = m_protocols.value(protocol, nullptr);
    if (!pq) {
        // qDebug() << "creating ProtoQueue instance for" << protocol;

        const int maxWorkers = KProtocolInfo::maxWorkers(protocol);
        int maxWorkersPerHost = -1;
        if (!host.isEmpty()) {
            bool ok = false;
            const int value = WorkerConfig::self()->configData(protocol, host, QStringLiteral("MaxConnections")).toInt(&ok);
            if (ok) {
                maxWorkersPerHost = value;
            }
        }
        if (maxWorkersPerHost == -1) {
            maxWorkersPerHost = KProtocolInfo::maxWorkersPerHost(protocol);
        }
        // Never allow maxWorkersPerHost to exceed maxWorkers.
        pq = new ProtoQueue(maxWorkers, qMin(maxWorkers, maxWorkersPerHost));
        m_protocols.insert(protocol, pq);
    }
    return pq;
}

void SchedulerPrivate::updateInternalMetaData(SimpleJob *job)
{
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    // Preserve all internal meta-data so they can be sent back to the
    // KIO workers as needed...
    const QUrl jobUrl = job->url();

    const QLatin1String currHostToken("{internal~currenthost}");
    const QLatin1String allHostsToken("{internal~allhosts}");
    // qDebug() << job << jobPriv->m_internalMetaData;
    QMapIterator<QString, QString> it(jobPriv->m_internalMetaData);
    while (it.hasNext()) {
        it.next();
        if (it.key().startsWith(currHostToken, Qt::CaseInsensitive)) {
            WorkerConfig::self()->setConfigData(jobUrl.scheme(), jobUrl.host(), it.key().mid(currHostToken.size()), it.value());
        } else if (it.key().startsWith(allHostsToken, Qt::CaseInsensitive)) {
            WorkerConfig::self()->setConfigData(jobUrl.scheme(), QString(), it.key().mid(allHostsToken.size()), it.value());
        }
    }
}

#include "moc_scheduler.cpp"
#include "moc_scheduler_p.cpp"
