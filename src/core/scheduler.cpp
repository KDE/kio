/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "scheduler.h"
#include "scheduler_p.h"

#include "connection_p.h"
#include "job_p.h"
#include "sessiondata_p.h"
#include "worker_p.h"
#include "workerconfig.h"

#include <kprotocolinfo.h>
#include <kprotocolmanager.h>

#ifndef KIO_ANDROID_STUB
#include <QDBusConnection>
#include <QDBusMessage>
#endif
#include <QHash>
#include <QThread>
#include <QThreadStorage>

// Slaves may be idle for a certain time (3 minutes) before they are killed.
static const int s_idleSlaveLifetime = 3 * 60;

using namespace KIO;

static inline Worker *jobSlave(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_worker;
}

static inline int jobCommand(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_command;
}

static inline void startJob(SimpleJob *job, Worker *slave)
{
    SimpleJobPrivate::get(job)->start(slave);
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
        removeSlaveOnHold();
        delete q;
        q = nullptr;
        qDeleteAll(m_protocols); // ~ProtoQueue will kill and delete all slaves
    }

    SchedulerPrivate(const SchedulerPrivate &) = delete;
    SchedulerPrivate &operator=(const SchedulerPrivate &) = delete;

    Scheduler *q;

    Worker *m_slaveOnHold = nullptr;
    QUrl m_urlOnHold;
    bool m_ignoreConfigReparse = false;

    SessionData sessionData;

    void doJob(SimpleJob *job);
    void setJobPriority(SimpleJob *job, int priority);
    void cancelJob(SimpleJob *job);
    void jobFinished(KIO::SimpleJob *job, KIO::Worker *slave);
    void putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url);
    void removeSlaveOnHold();
    Worker *heldSlaveForJob(KIO::SimpleJob *job);
    bool isSlaveOnHoldFor(const QUrl &url);
    void updateInternalMetaData(SimpleJob *job);

    MetaData metaDataFor(const QString &protocol, const QStringList &proxyList, const QUrl &url);
    void setupSlave(KIO::Worker *slave,
                    const QUrl &url,
                    const QString &protocol,
                    const QStringList &proxyList,
                    bool newSlave,
                    const KIO::MetaData *config = nullptr);

    void slotSlaveDied(KIO::Worker *slave);

#ifndef KIO_ANDROID_STUB
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

int SerialPicker::changedPrioritySerial(int oldSerial, int newPriority) const
{
    Q_ASSERT(newPriority >= -10 && newPriority <= 10);
    newPriority = qBound(-10, newPriority, 10);
    int unbiasedSerial = oldSerial % m_jobsPerPriority;
    return unbiasedSerial + newPriority * m_jobsPerPriority;
}

SlaveKeeper::SlaveKeeper()
{
    m_grimTimer.setSingleShot(true);
    connect(&m_grimTimer, &QTimer::timeout, this, &SlaveKeeper::grimReaper);
}

SlaveKeeper::~SlaveKeeper()
{
    grimReaper();
}

void SlaveKeeper::returnSlave(Worker *slave)
{
    Q_ASSERT(slave);
    slave->setIdle();
    m_idleSlaves.insert(slave->host(), slave);
    scheduleGrimReaper();
}

Worker *SlaveKeeper::takeSlaveForJob(SimpleJob *job)
{
    Worker *slave = schedulerPrivate()->heldSlaveForJob(job);
    if (slave) {
        return slave;
    }

    QUrl url = SimpleJobPrivate::get(job)->m_url;
    // TODO take port, username and password into account
    QMultiHash<QString, Worker *>::Iterator it = m_idleSlaves.find(url.host());
    if (it == m_idleSlaves.end()) {
        it = m_idleSlaves.begin();
    }
    if (it == m_idleSlaves.end()) {
        return nullptr;
    }
    slave = it.value();
    m_idleSlaves.erase(it);
    return slave;
}

bool SlaveKeeper::removeSlave(Worker *slave)
{
    // ### performance not so great
    QMultiHash<QString, Worker *>::Iterator it = m_idleSlaves.begin();
    for (; it != m_idleSlaves.end(); ++it) {
        if (it.value() == slave) {
            m_idleSlaves.erase(it);
            return true;
        }
    }
    return false;
}

void SlaveKeeper::clear()
{
    m_idleSlaves.clear();
}

QList<Worker *> SlaveKeeper::allSlaves() const
{
    return m_idleSlaves.values();
}

void SlaveKeeper::scheduleGrimReaper()
{
    if (!m_grimTimer.isActive()) {
        m_grimTimer.start((s_idleSlaveLifetime / 2) * 1000);
    }
}

// private slot
void SlaveKeeper::grimReaper()
{
    QMultiHash<QString, Worker *>::Iterator it = m_idleSlaves.begin();
    while (it != m_idleSlaves.end()) {
        Worker *slave = it.value();
        if (slave->idleTime() >= s_idleSlaveLifetime) {
            it = m_idleSlaves.erase(it);
            if (slave->job()) {
                // qDebug() << "Idle slave" << slave << "still has job" << slave->job();
            }
            // avoid invoking slotSlaveDied() because its cleanup services are not needed
            slave->kill();
        } else {
            ++it;
        }
    }
    if (!m_idleSlaves.isEmpty()) {
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

QList<Worker *> HostQueue::allSlaves() const
{
    QList<Worker *> ret;
    ret.reserve(m_runningJobs.size());
    for (SimpleJob *job : m_runningJobs) {
        Worker *slave = jobSlave(job);
        Q_ASSERT(slave);
        ret.append(slave);
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
    // Gather list of all slaves first
    const QList<Worker *> slaves = allSlaves();
    // Clear the idle slaves in the keeper to avoid dangling pointers
    m_slaveKeeper.clear();
    for (Worker *slave : slaves) {
        // kill the slave process and remove the interface in our process
        slave->kill();
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

void ProtoQueue::changeJobPriority(SimpleJob *job, int newPrio)
{
    SimpleJobPrivate *jobPriv = SimpleJobPrivate::get(job);
    QHash<QString, HostQueue>::Iterator it = m_queuesByHostname.find(jobPriv->m_url.host());
    if (it == m_queuesByHostname.end()) {
        return;
    }
    HostQueue &hq = it.value();
    const int prevLowestSerial = hq.lowestSerial();
    if (hq.isJobRunning(job) || !hq.removeJob(job)) {
        return;
    }
    jobPriv->m_schedSerial = m_serialPicker.changedPrioritySerial(jobPriv->m_schedSerial, newPrio);
    hq.queueJob(job);
    const bool needReinsert = hq.lowestSerial() != prevLowestSerial;
    // the host queue might be absent from m_queuesBySerial because the connections per host limit
    // for that host has been reached.
    if (needReinsert && m_queuesBySerial.remove(prevLowestSerial)) {
        m_queuesBySerial.insert(hq.lowestSerial(), &hq);
    }
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
            m_slaveKeeper.returnSlave(jobPriv->m_worker);
        }
        // just in case; startAJob() will refuse to start a job if it shouldn't.
        m_startJobTimer.start();
    }

    ensureNoDuplicates(&m_queuesBySerial);
}

Worker *ProtoQueue::createSlave(const QString &protocol, SimpleJob *job, const QUrl &url)
{
    int error;
    QString errortext;
    Worker *slave = Worker::createWorker(protocol, url, error, errortext);
    if (slave) {
        connect(slave, &Worker::workerDied, scheduler(), [](KIO::Worker *slave) {
            schedulerPrivate()->slotSlaveDied(slave);
        });
    } else {
        qCWarning(KIO_CORE) << "couldn't create worker:" << errortext;
        if (job) {
            job->slotError(error, errortext);
        }
    }
    return slave;
}

bool ProtoQueue::removeSlave(KIO::Worker *slave)
{
    const bool removed = m_slaveKeeper.removeSlave(slave);
    return removed;
}

QList<Worker *> ProtoQueue::allSlaves() const
{
    QList<Worker *> ret(m_slaveKeeper.allSlaves());
    auto it = m_queuesByHostname.cbegin();
    for (; it != m_queuesByHostname.cend(); ++it) {
        ret.append(it.value().allSlaves());
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

        // always increase m_runningJobsCount because it's correct if there is a slave and if there
        // is no slave, removeJob() will balance the number again. removeJob() would decrease the
        // number too much otherwise.
        // Note that createSlave() can call slotError() on a job which in turn calls removeJob(),
        // so increase the count here already.
        m_runningJobsCount++;

        bool isNewSlave = false;
        Worker *slave = m_slaveKeeper.takeSlaveForJob(startingJob);
        SimpleJobPrivate *jobPriv = SimpleJobPrivate::get(startingJob);
        if (!slave) {
            isNewSlave = true;
            slave = createSlave(jobPriv->m_protocol, startingJob, jobPriv->m_url);
        }

        if (slave) {
            jobPriv->m_worker = slave;
            schedulerPrivate()->setupSlave(slave, jobPriv->m_url, jobPriv->m_protocol, jobPriv->m_proxyList, isNewSlave);
            startJob(startingJob, slave);
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

#ifndef KIO_ANDROID_STUB
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
void Scheduler::setSimpleJobPriority(SimpleJob *job, int priority)
{
    schedulerPrivate()->setJobPriority(job, priority);
}

void Scheduler::cancelJob(SimpleJob *job)
{
    schedulerPrivate()->cancelJob(job);
}

void Scheduler::jobFinished(KIO::SimpleJob *job, KIO::Worker *slave)
{
    schedulerPrivate()->jobFinished(job, slave);
}

void Scheduler::putWorkerOnHold(KIO::SimpleJob *job, const QUrl &url)
{
    schedulerPrivate()->putSlaveOnHold(job, url);
}

void Scheduler::removeWorkerOnHold()
{
    schedulerPrivate()->removeSlaveOnHold();
}

bool Scheduler::isWorkerOnHoldFor(const QUrl &url)
{
    return schedulerPrivate()->isSlaveOnHoldFor(url);
}

void Scheduler::updateInternalMetaData(SimpleJob *job)
{
    schedulerPrivate()->updateInternalMetaData(job);
}

void Scheduler::emitReparseSlaveConfiguration()
{
#ifndef KIO_ANDROID_STUB
    // Do it immediately in this process, otherwise we might send a request before reparsing
    // (e.g. when changing useragent in the plugin)
    schedulerPrivate()->slotReparseSlaveConfiguration(QString(), QDBusMessage());
#endif

    schedulerPrivate()->m_ignoreConfigReparse = true;
    Q_EMIT self()->reparseSlaveConfiguration(QString());
}

#ifndef KIO_ANDROID_STUB
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
    sessionData.reset();
    NetRC::self()->reload();

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
        const QList<KIO::Worker *> list = it.value()->allSlaves();
        for (Worker *slave : list) {
            slave->send(CMD_REPARSECONFIGURATION);
            slave->resetHost();
        }
    }
}
#endif

void SchedulerPrivate::doJob(SimpleJob *job)
{
    // qDebug() << job;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    jobPriv->m_proxyList.clear();
    jobPriv->m_protocol = KProtocolManager::workerProtocol(job->url(), jobPriv->m_proxyList);

    ProtoQueue *proto = protoQ(jobPriv->m_protocol, job->url().host());
    proto->queueJob(job);
}

void SchedulerPrivate::setJobPriority(SimpleJob *job, int priority)
{
    // qDebug() << job << priority;
    const QString protocol = SimpleJobPrivate::get(job)->m_protocol;
    if (!protocol.isEmpty()) {
        ProtoQueue *proto = protoQ(protocol, job->url().host());
        proto->changeJobPriority(job, priority);
    }
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
    Worker *slave = jobSlave(job);
    // qDebug() << job << slave;
    jobFinished(job, slave);
    if (slave) {
        ProtoQueue *pq = m_protocols.value(jobPriv->m_protocol);
        if (pq) {
            pq->removeSlave(slave);
        }
        slave->kill(); // don't use slave after this!
    }
}

void SchedulerPrivate::jobFinished(SimpleJob *job, Worker *slave)
{
    // qDebug() << job << slave;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    // make sure that we knew about the job!
    Q_ASSERT(jobPriv->m_schedSerial);

    ProtoQueue *pq = m_protocols.value(jobPriv->m_protocol);
    if (pq) {
        pq->removeJob(job);
    }

    if (slave) {
        // If we have internal meta-data, tell existing KIO workers to reload
        // their configuration.
        if (jobPriv->m_internalMetaData.count()) {
            // qDebug() << "Updating KIO workers with new internal metadata information";
            ProtoQueue *queue = m_protocols.value(slave->protocol());
            if (queue) {
                const QList<Worker *> slaves = queue->allSlaves();
                for (auto *runningSlave : slaves) {
                    if (slave->host() == runningSlave->host()) {
                        slave->setConfig(metaDataFor(slave->protocol(), jobPriv->m_proxyList, job->url()));
                        /*qDebug() << "Updated configuration of" << slave->protocol()
                                     << "KIO worker, pid=" << slave->slave_pid();*/
                    }
                }
            }
        }
        slave->setJob(nullptr);
        slave->disconnect(job);
    }
    jobPriv->m_schedSerial = 0; // this marks the job as unscheduled again
    jobPriv->m_worker = nullptr;
    // Clear the values in the internal metadata container since they have
    // already been taken care of above...
    jobPriv->m_internalMetaData.clear();
}

MetaData SchedulerPrivate::metaDataFor(const QString &protocol, const QStringList &proxyList, const QUrl &url)
{
    const QString host = url.host();
    MetaData configData = WorkerConfig::self()->configData(protocol, host);
    sessionData.configDataFor(configData, protocol, host);
    if (proxyList.isEmpty()) {
        configData.remove(QStringLiteral("UseProxy"));
        configData.remove(QStringLiteral("ProxyUrls"));
    } else {
        configData[QStringLiteral("UseProxy")] = proxyList.first();
        configData[QStringLiteral("ProxyUrls")] = proxyList.join(QLatin1Char(','));
    }

    if (configData.contains(QLatin1String("EnableAutoLogin"))
        && configData.value(QStringLiteral("EnableAutoLogin")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
        NetRC::AutoLogin l;
        l.login = url.userName();
        bool usern = (protocol == QLatin1String("ftp"));
        if (NetRC::self()->lookup(url, l, usern)) {
            configData[QStringLiteral("autoLoginUser")] = l.login;
            configData[QStringLiteral("autoLoginPass")] = l.password;
            if (usern) {
                QString macdef;
                QMap<QString, QStringList>::ConstIterator it = l.macdef.constBegin();
                for (; it != l.macdef.constEnd(); ++it) {
                    macdef += it.key() + QLatin1Char('\\') + it.value().join(QLatin1Char('\\')) + QLatin1Char('\n');
                }
                configData[QStringLiteral("autoLoginMacro")] = macdef;
            }
        }
    }

    return configData;
}

void SchedulerPrivate::setupSlave(KIO::Worker *slave,
                                  const QUrl &url,
                                  const QString &protocol,
                                  const QStringList &proxyList,
                                  bool newSlave,
                                  const KIO::MetaData *config)
{
    int port = url.port();
    if (port == -1) { // no port is -1 in QUrl, but in kde3 we used 0 and the KIO workers assume that.
        port = 0;
    }
    const QString host = url.host();
    const QString user = url.userName();
    const QString passwd = url.password();

    if (newSlave || slave->host() != host || slave->port() != port || slave->user() != user || slave->passwd() != passwd) {
        MetaData configData = metaDataFor(protocol, proxyList, url);
        if (config) {
            configData += *config;
        }

        slave->setConfig(configData);
        slave->setProtocol(url.scheme());
        slave->setHost(host, port, user, passwd);
    }
}

void SchedulerPrivate::slotSlaveDied(KIO::Worker *slave)
{
    // qDebug() << slave;
    Q_ASSERT(slave);
    Q_ASSERT(!slave->isAlive());
    ProtoQueue *pq = m_protocols.value(slave->protocol());
    if (pq) {
        if (slave->job()) {
            pq->removeJob(slave->job());
        }
        // in case this was a connected slave...
        pq->removeSlave(slave);
    }
    if (slave == m_slaveOnHold) {
        m_slaveOnHold = nullptr;
        m_urlOnHold.clear();
    }
    // can't use slave->deref() here because we need to use deleteLater
    slave->aboutToDelete();
    slave->deleteLater();
}

void SchedulerPrivate::putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url)
{
    Worker *slave = jobSlave(job);
    // qDebug() << job << url << slave;
    slave->disconnect(job);
    // prevent the fake death of the slave from trying to kill the job again;
    // cf. Worker::hold(const QUrl &url) called in SchedulerPrivate::publishSlaveOnHold().
    slave->setJob(nullptr);
    SimpleJobPrivate::get(job)->m_worker = nullptr;

    if (m_slaveOnHold) {
        m_slaveOnHold->kill();
    }
    m_slaveOnHold = slave;
    m_urlOnHold = url;
    m_slaveOnHold->suspend();
}

bool SchedulerPrivate::isSlaveOnHoldFor(const QUrl &url)
{
    if (url.isValid() && m_urlOnHold.isValid() && url == m_urlOnHold) {
        return true;
    }

    return false;
}

Worker *SchedulerPrivate::heldSlaveForJob(SimpleJob *job)
{
    Worker *slave = nullptr;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    if (m_slaveOnHold) {
        // Make sure that the job wants to do a GET or a POST, and with no offset
        const int cmd = jobPriv->m_command;
        bool canJobReuse = (cmd == CMD_GET || cmd == CMD_MULTI_GET);

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
                // qDebug() << "HOLD: Reusing held slave (" << m_slaveOnHold << ")";
                slave = m_slaveOnHold;
            } else {
                // qDebug() << "HOLD: Discarding held slave (" << m_slaveOnHold << ")";
                m_slaveOnHold->kill();
            }
            m_slaveOnHold = nullptr;
            m_urlOnHold.clear();
        }
    }

    return slave;
}

void SchedulerPrivate::removeSlaveOnHold()
{
    // qDebug() << m_slaveOnHold;
    if (m_slaveOnHold) {
        m_slaveOnHold->kill();
    }
    m_slaveOnHold = nullptr;
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
