/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "scheduler.h"
#include "scheduler_p.h"

#include "sessiondata_p.h"
#include "slaveconfig.h"
#include "slave.h"
#include "connection_p.h"
#include "job_p.h"

#include <kprotocolmanager.h>
#include <kprotocolinfo.h>
//#include <KJobWidgets>

#include <QHash>
#include <QThread>
#include <QThreadStorage>
#include <QDBusConnection>
#include <QDBusMessage>

// Slaves may be idle for a certain time (3 minutes) before they are killed.
static const int s_idleSlaveLifetime = 3 * 60;

using namespace KIO;

static inline Slave *jobSlave(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_slave;
}

static inline int jobCommand(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_command;
}

static inline void startJob(SimpleJob *job, Slave *slave)
{
    SimpleJobPrivate::get(job)->start(slave);
}

// here be uglies
// forward declaration to break cross-dependency of SlaveKeeper and SchedulerPrivate
static void setupSlave(KIO::Slave *slave, const QUrl &url, const QString &protocol,
                       const QStringList &proxyList, bool newSlave, const KIO::MetaData *config = nullptr);
// same reason as above
static Scheduler *scheduler();
static Slave *heldSlaveForJob(SimpleJob *job);

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

void SlaveKeeper::returnSlave(Slave *slave)
{
    Q_ASSERT(slave);
    slave->setIdle();
    m_idleSlaves.insert(slave->host(), slave);
    scheduleGrimReaper();
}

Slave *SlaveKeeper::takeSlaveForJob(SimpleJob *job)
{
    Slave *slave = heldSlaveForJob(job);
    if (slave) {
        return slave;
    }

    QUrl url = SimpleJobPrivate::get(job)->m_url;
    // TODO take port, username and password into account
    QMultiHash<QString, Slave *>::Iterator it = m_idleSlaves.find(url.host());
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

bool SlaveKeeper::removeSlave(Slave *slave)
{
    // ### performance not so great
    QMultiHash<QString, Slave *>::Iterator it = m_idleSlaves.begin();
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

QList<Slave *> SlaveKeeper::allSlaves() const
{
    return m_idleSlaves.values();
}

void SlaveKeeper::scheduleGrimReaper()
{
    if (!m_grimTimer.isActive()) {
        m_grimTimer.start((s_idleSlaveLifetime / 2) * 1000);
    }
}

//private slot
void SlaveKeeper::grimReaper()
{
    QMultiHash<QString, Slave *>::Iterator it = m_idleSlaves.begin();
    while (it != m_idleSlaves.end()) {
        Slave *slave = it.value();
        if (slave->idleTime() >= s_idleSlaveLifetime) {
            it = m_idleSlaves.erase(it);
            if (slave->job()) {
                //qDebug() << "Idle slave" << slave << "still has job" << slave->job();
            }
            slave->kill();
            // avoid invoking slotSlaveDied() because its cleanup services are not needed
            slave->deref();
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

QList<Slave *> HostQueue::allSlaves() const
{
    QList<Slave *> ret;
    ret.reserve(m_runningJobs.size());
    for (SimpleJob *job : m_runningJobs) {
        Slave *slave = jobSlave(job);
        Q_ASSERT(slave);
        ret.append(slave);
    }
    return ret;
}

ConnectedSlaveQueue::ConnectedSlaveQueue()
{
    m_startJobsTimer.setSingleShot(true);
    connect(&m_startJobsTimer, &QTimer::timeout, this, &ConnectedSlaveQueue::startRunnableJobs);
}

bool ConnectedSlaveQueue::queueJob(SimpleJob *job, Slave *slave)
{
    QHash<Slave *, PerSlaveQueue>::Iterator it = m_connectedSlaves.find(slave);
    if (it == m_connectedSlaves.end()) {
        return false;
    }
    SimpleJobPrivate::get(job)->m_slave = slave;

    PerSlaveQueue &jobs = it.value();
    jobs.waitingList.append(job);
    if (!jobs.runningJob) {
        // idle slave now has a job to run
        m_runnableSlaves.insert(slave);
        m_startJobsTimer.start();
    }
    return true;
}

bool ConnectedSlaveQueue::removeJob(SimpleJob *job)
{
    Slave *slave = jobSlave(job);
    Q_ASSERT(slave);
    QHash<Slave *, PerSlaveQueue>::Iterator it = m_connectedSlaves.find(slave);
    if (it == m_connectedSlaves.end()) {
        return false;
    }
    PerSlaveQueue &jobs = it.value();
    if (jobs.runningJob || jobs.waitingList.isEmpty()) {
        // a slave that was busy running a job was not runnable.
        // a slave that has no waiting job(s) was not runnable either.
        Q_ASSERT(!m_runnableSlaves.contains(slave));
    }

    const bool removedRunning = jobs.runningJob == job;
    const bool removedWaiting = jobs.waitingList.removeAll(job) != 0;
    if (removedRunning) {
        jobs.runningJob = nullptr;
        Q_ASSERT(!removedWaiting);
    }
    const bool removedTheJob = removedRunning || removedWaiting;

    if (!slave->isAlive()) {
        removeSlave(slave);
        return removedTheJob;
    }

    if (removedRunning && jobs.waitingList.count()) {
        m_runnableSlaves.insert(slave);
        m_startJobsTimer.start();
    }
    if (removedWaiting && jobs.waitingList.isEmpty()) {
        m_runnableSlaves.remove(slave);
    }
    return removedTheJob;
}

void ConnectedSlaveQueue::addSlave(Slave *slave)
{
    Q_ASSERT(slave);
    if (!m_connectedSlaves.contains(slave)) {
        m_connectedSlaves.insert(slave, PerSlaveQueue());
    }
}

bool ConnectedSlaveQueue::removeSlave(Slave *slave)
{
    QHash<Slave *, PerSlaveQueue>::Iterator it = m_connectedSlaves.find(slave);
    if (it == m_connectedSlaves.end()) {
        return false;
    }
    PerSlaveQueue &jobs = it.value();
    // we need a copy as kill() ends up removing the job from waitingList
    const QList <SimpleJob *> waitingJobs = jobs.waitingList;
    for (SimpleJob *job : waitingJobs) {
        // ### for compatibility with the old scheduler we don't touch the running job, if any.
        // make sure that the job doesn't call back into Scheduler::cancelJob(); this would
        // a) crash and b) be unnecessary because we clean up just fine.
        SimpleJobPrivate::get(job)->m_schedSerial = 0;
        job->kill();
    }
    m_connectedSlaves.erase(it);
    m_runnableSlaves.remove(slave);

    slave->kill();
    return true;
}

// KDE5: only one caller, for doubtful reasons. remove this if possible.
bool ConnectedSlaveQueue::isIdle(Slave *slave)
{
    QHash<Slave *, PerSlaveQueue>::Iterator it = m_connectedSlaves.find(slave);
    if (it == m_connectedSlaves.end()) {
        return false;
    }
    return it.value().runningJob == nullptr;
}

//private slot
void ConnectedSlaveQueue::startRunnableJobs()
{
    QSet<Slave *>::Iterator it = m_runnableSlaves.begin();
    while (it != m_runnableSlaves.end()) {
        Slave *slave = *it;
        if (!slave->isConnected()) {
            // this polling is somewhat inefficient...
            m_startJobsTimer.start();
            ++it;
            continue;
        }
        it = m_runnableSlaves.erase(it);
        PerSlaveQueue &jobs = m_connectedSlaves[slave];
        SimpleJob *job = jobs.waitingList.takeFirst();
        Q_ASSERT(!jobs.runningJob);
        jobs.runningJob = job;

        const QUrl url = job->url();
        // no port is -1 in QUrl, but in kde3 we used 0 and the kioslaves assume that.
        const int port = url.port() == -1 ? 0 : url.port();

        if (slave->host() == QLatin1String("<reset>")) {
            MetaData configData = SlaveConfig::self()->configData(url.scheme(), url.host());
            slave->setConfig(configData);
            slave->setProtocol(url.scheme());
            slave->setHost(url.host(), port, url.userName(), url.password());
        }

        Q_ASSERT(slave->protocol() == url.scheme());
        Q_ASSERT(slave->host() == url.host());
        Q_ASSERT(slave->port() == port);
        startJob(job, slave);
    }
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

ProtoQueue::ProtoQueue(int maxSlaves, int maxSlavesPerHost)
    : m_maxConnectionsPerHost(maxSlavesPerHost ? maxSlavesPerHost : maxSlaves),
      m_maxConnectionsTotal(qMax(maxSlaves, maxSlavesPerHost)),
      m_runningJobsCount(0)

{
    /*qDebug() << "m_maxConnectionsTotal:" << m_maxConnectionsTotal
                 << "m_maxConnectionsPerHost:" << m_maxConnectionsPerHost;*/
    Q_ASSERT(m_maxConnectionsPerHost >= 1);
    Q_ASSERT(maxSlaves >= maxSlavesPerHost);
    m_startJobTimer.setSingleShot(true);
    connect(&m_startJobTimer, &QTimer::timeout, this, &ProtoQueue::startAJob);
}

ProtoQueue::~ProtoQueue()
{
    // Gather list of all slaves first
    const QList<Slave *> slaves = allSlaves();
    // Clear the idle slaves in the keeper to avoid dangling pointers
    m_slaveKeeper.clear();
    for (Slave *slave : slaves) {
        // kill the slave process, then remove the interface in our process
        slave->kill();
        slave->deref();
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
            Q_ASSERT(!jobPriv->m_slave);
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

        if (jobPriv->m_slave && jobPriv->m_slave->isAlive()) {
            m_slaveKeeper.returnSlave(jobPriv->m_slave);
        }
        // just in case; startAJob() will refuse to start a job if it shouldn't.
        m_startJobTimer.start();
    } else {
        // should be a connected slave
        // if the assertion fails the job has probably changed the host part of its URL while
        // running, so we can't find it by hostname. don't do this.
        const bool removed = m_connectedSlaveQueue.removeJob(job);
        Q_UNUSED(removed);
        Q_ASSERT(removed);
    }

    ensureNoDuplicates(&m_queuesBySerial);
}

Slave *ProtoQueue::createSlave(const QString &protocol, SimpleJob *job, const QUrl &url)
{
    int error;
    QString errortext;
    Slave *slave = Slave::createSlave(protocol, url, error, errortext);
    if (slave) {
        scheduler()->connect(slave, SIGNAL(slaveDied(KIO::Slave*)),
                             SLOT(slotSlaveDied(KIO::Slave*)));
        scheduler()->connect(slave, SIGNAL(slaveStatus(qint64,QByteArray,QString,bool)),
                             SLOT(slotSlaveStatus(qint64,QByteArray,QString,bool)));
    } else {
        qCWarning(KIO_CORE) << "couldn't create slave:" << errortext;
        if (job) {
            job->slotError(error, errortext);
        }
    }
    return slave;
}

bool ProtoQueue::removeSlave(KIO::Slave *slave)
{
    const bool removedConnected = m_connectedSlaveQueue.removeSlave(slave);
    const bool removedUnconnected = m_slaveKeeper.removeSlave(slave);
    Q_ASSERT(!(removedConnected && removedUnconnected));
    return removedConnected || removedUnconnected;
}

QList<Slave *> ProtoQueue::allSlaves() const
{
    QList<Slave *> ret(m_slaveKeeper.allSlaves());
    auto it = m_queuesByHostname.cbegin();
    for (; it != m_queuesByHostname.cend(); ++it) {
        ret.append(it.value().allSlaves());
    }
    ret.append(m_connectedSlaveQueue.allSlaves());
    return ret;
}

//private slot
void ProtoQueue::startAJob()
{
    ensureNoDuplicates(&m_queuesBySerial);
    verifyRunningJobsCount(&m_queuesByHostname, m_runningJobsCount);

#ifdef SCHEDULER_DEBUG
    //qDebug() << "m_runningJobsCount:" << m_runningJobsCount;
    auto it = m_queuesByHostname.cbegin();
    for (; it != m_queuesByHostname.cend(); ++it) {
        const QList<KIO::SimpleJob *> list = it.value().runningJobs();
        for (SimpleJob *job : list) {
            //qDebug() << SimpleJobPrivate::get(job)->m_url;
        }
    }
#endif
    if (m_runningJobsCount >= m_maxConnectionsTotal) {
#ifdef SCHEDULER_DEBUG
        //qDebug() << "not starting any jobs because maxConnectionsTotal has been reached.";
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
        Slave *slave = m_slaveKeeper.takeSlaveForJob(startingJob);
        SimpleJobPrivate *jobPriv = SimpleJobPrivate::get(startingJob);
        if (!slave) {
            isNewSlave = true;
            slave = createSlave(jobPriv->m_protocol, startingJob, jobPriv->m_url);
        }

        if (slave) {
            jobPriv->m_slave = slave;
            setupSlave(slave, jobPriv->m_url, jobPriv->m_protocol, jobPriv->m_proxyList, isNewSlave);
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
        //qDebug() << "not starting any jobs because there is no queued job.";
#endif
    }

    if (!m_queuesBySerial.isEmpty()) {
        m_startJobTimer.start();
    }
}

class KIO::SchedulerPrivate
{
public:
    SchedulerPrivate()
        : q(new Scheduler()),
          m_slaveOnHold(nullptr),
          m_checkOnHold(true), // !! Always check with KLauncher for the first request
          m_ignoreConfigReparse(false)
    {
    }

    ~SchedulerPrivate()
    {
        delete q;
        q = nullptr;
        for (ProtoQueue *p : qAsConst(m_protocols)) {
            const QList<KIO::Slave *> list = p->allSlaves();
            for (Slave *slave : list) {
                slave->kill();
            }
        }
        qDeleteAll(m_protocols);
    }

    SchedulerPrivate(const SchedulerPrivate &) = delete;
    SchedulerPrivate &operator=(const SchedulerPrivate &) = delete;

    Scheduler *q;

    Slave *m_slaveOnHold;
    QUrl m_urlOnHold;
    bool m_checkOnHold;
    bool m_ignoreConfigReparse;

    SessionData sessionData;

    void doJob(SimpleJob *job);
#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 5)
    void scheduleJob(SimpleJob *job);
#endif
    void setJobPriority(SimpleJob *job, int priority);
    void cancelJob(SimpleJob *job);
    void jobFinished(KIO::SimpleJob *job, KIO::Slave *slave);
    void putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url);
    void removeSlaveOnHold();
    Slave *getConnectedSlave(const QUrl &url, const KIO::MetaData &metaData);
    bool assignJobToSlave(KIO::Slave *slave, KIO::SimpleJob *job);
    bool disconnectSlave(KIO::Slave *slave);
    void checkSlaveOnHold(bool b);
    void publishSlaveOnHold();
    Slave *heldSlaveForJob(KIO::SimpleJob *job);
    bool isSlaveOnHoldFor(const QUrl &url);
    void updateInternalMetaData(SimpleJob *job);

    MetaData metaDataFor(const QString &protocol, const QStringList &proxyList, const QUrl &url);
    void setupSlave(KIO::Slave *slave, const QUrl &url, const QString &protocol,
                    const QStringList &proxyList, bool newSlave, const KIO::MetaData *config = nullptr);

    void slotSlaveDied(KIO::Slave *slave);
    void slotSlaveStatus(qint64 pid, const QByteArray &protocol,
                         const QString &host, bool connected);

    void slotReparseSlaveConfiguration(const QString &, const QDBusMessage &);
    void slotSlaveOnHoldListChanged();

    void slotSlaveConnected();
    void slotSlaveError(int error, const QString &errorMsg);

    ProtoQueue *protoQ(const QString &protocol, const QString &host)
    {
        ProtoQueue *pq = m_protocols.value(protocol, nullptr);
        if (!pq) {
            //qDebug() << "creating ProtoQueue instance for" << protocol;

            const int maxSlaves = KProtocolInfo::maxSlaves(protocol);
            int maxSlavesPerHost = -1;
            if (!host.isEmpty()) {
                bool ok = false;
                const int value = SlaveConfig::self()->configData(protocol, host, QStringLiteral("MaxConnections")).toInt(&ok);
                if (ok) {
                    maxSlavesPerHost = value;
                }
            }
            if (maxSlavesPerHost == -1) {
                maxSlavesPerHost = KProtocolInfo::maxSlavesPerHost(protocol);
            }
            // Never allow maxSlavesPerHost to exceed maxSlaves.
            pq = new ProtoQueue(maxSlaves, qMin(maxSlaves, maxSlavesPerHost));
            m_protocols.insert(protocol, pq);
        }
        return pq;
    }
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

//static
Scheduler *scheduler()
{
    return schedulerPrivate()->q;
}

//static
Slave *heldSlaveForJob(SimpleJob *job)
{
    return schedulerPrivate()->heldSlaveForJob(job);
}

Scheduler::Scheduler()
{
    setObjectName(QStringLiteral("scheduler"));

    const QString dbusPath = QStringLiteral("/KIO/Scheduler");
    const QString dbusInterface = QStringLiteral("org.kde.KIO.Scheduler");
    QDBusConnection dbus = QDBusConnection::sessionBus();
    // Not needed, right? We just want to emit two signals.
    //dbus.registerObject(dbusPath, this, QDBusConnection::ExportScriptableSlots |
    //                    QDBusConnection::ExportScriptableSignals);
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("reparseSlaveConfiguration"),
                 this, SLOT(slotReparseSlaveConfiguration(QString,QDBusMessage)));
    dbus.connect(QString(), dbusPath, dbusInterface, QStringLiteral("slaveOnHoldListChanged"),
                 this, SLOT(slotSlaveOnHoldListChanged()));
}

Scheduler::~Scheduler()
{
}

void Scheduler::doJob(SimpleJob *job)
{
    schedulerPrivate()->doJob(job);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 5)
void Scheduler::scheduleJob(SimpleJob *job)
{
    schedulerPrivate()->scheduleJob(job);
}
#endif

void Scheduler::setJobPriority(SimpleJob *job, int priority)
{
    schedulerPrivate()->setJobPriority(job, priority);
}

void Scheduler::cancelJob(SimpleJob *job)
{
    schedulerPrivate()->cancelJob(job);
}

void Scheduler::jobFinished(KIO::SimpleJob *job, KIO::Slave *slave)
{
    schedulerPrivate()->jobFinished(job, slave);
}

void Scheduler::putSlaveOnHold(KIO::SimpleJob *job, const QUrl &url)
{
    schedulerPrivate()->putSlaveOnHold(job, url);
}

void Scheduler::removeSlaveOnHold()
{
    schedulerPrivate()->removeSlaveOnHold();
}

void Scheduler::publishSlaveOnHold()
{
    schedulerPrivate()->publishSlaveOnHold();
}

bool Scheduler::isSlaveOnHoldFor(const QUrl &url)
{
    return schedulerPrivate()->isSlaveOnHoldFor(url);
}

void Scheduler::updateInternalMetaData(SimpleJob *job)
{
    schedulerPrivate()->updateInternalMetaData(job);
}

KIO::Slave *Scheduler::getConnectedSlave(const QUrl &url,
        const KIO::MetaData &config)
{
    return schedulerPrivate()->getConnectedSlave(url, config);
}

bool Scheduler::assignJobToSlave(KIO::Slave *slave, KIO::SimpleJob *job)
{
    return schedulerPrivate()->assignJobToSlave(slave, job);
}

bool Scheduler::disconnectSlave(KIO::Slave *slave)
{
    return schedulerPrivate()->disconnectSlave(slave);
}

bool Scheduler::connect(const char *signal, const QObject *receiver,
                        const char *member)
{
    return QObject::connect(self(), signal, receiver, member);
}

bool Scheduler::connect(const QObject *sender, const char *signal,
                        const QObject *receiver, const char *member)
{
    return QObject::connect(sender, signal, receiver, member);
}

bool Scheduler::disconnect(const QObject *sender, const char *signal,
                           const QObject *receiver, const char *member)
{
    return QObject::disconnect(sender, signal, receiver, member);
}

bool Scheduler::connect(const QObject *sender, const char *signal,
                        const char *member)
{
    return QObject::connect(sender, signal, member);
}

void Scheduler::checkSlaveOnHold(bool b)
{
    schedulerPrivate()->checkSlaveOnHold(b);
}

void Scheduler::emitReparseSlaveConfiguration()
{
    // Do it immediately in this process, otherwise we might send a request before reparsing
    // (e.g. when changing useragent in the plugin)
    schedulerPrivate()->slotReparseSlaveConfiguration(QString(), QDBusMessage());

    schedulerPrivate()->m_ignoreConfigReparse = true;
    Q_EMIT self()->reparseSlaveConfiguration(QString());
}

void SchedulerPrivate::slotReparseSlaveConfiguration(const QString &proto, const QDBusMessage &)
{
    if (m_ignoreConfigReparse) {
        //qDebug() << "Ignoring signal sent by myself";
        m_ignoreConfigReparse = false;
        return;
    }

    //qDebug() << "proto=" << proto;
    KProtocolManager::reparseConfiguration();
    SlaveConfig::self()->reset();
    sessionData.reset();
    NetRC::self()->reload();

    QHash<QString, ProtoQueue *>::ConstIterator it = proto.isEmpty() ? m_protocols.constBegin() :
            m_protocols.constFind(proto);
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
        const QList<KIO::Slave *> list = it.value()->allSlaves();
        for (Slave *slave : list) {
            slave->send(CMD_REPARSECONFIGURATION);
            slave->resetHost();
        }
    }
}

void SchedulerPrivate::slotSlaveOnHoldListChanged()
{
    m_checkOnHold = true;
}

static bool mayReturnContent(int cmd, const QString &protocol)
{
    if (cmd == CMD_GET) {
        return true;
    }

    if (cmd == CMD_MULTI_GET) {
        return true;
    }

    if (cmd == CMD_SPECIAL && protocol.startsWith(QLatin1String("http"), Qt::CaseInsensitive)) {
        return true;
    }

    return false;
}

void SchedulerPrivate::doJob(SimpleJob *job)
{
    //qDebug() << job;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    jobPriv->m_proxyList.clear();
    jobPriv->m_protocol = KProtocolManager::slaveProtocol(job->url(), jobPriv->m_proxyList);

    if (mayReturnContent(jobCommand(job), jobPriv->m_protocol)) {
        jobPriv->m_checkOnHold = m_checkOnHold;
        m_checkOnHold = false;
    }

    ProtoQueue *proto = protoQ(jobPriv->m_protocol, job->url().host());
    proto->queueJob(job);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(4, 5)
void SchedulerPrivate::scheduleJob(SimpleJob *job)
{
    //qDebug() << job;
    setJobPriority(job, 1);
}
#endif

void SchedulerPrivate::setJobPriority(SimpleJob *job, int priority)
{
    //qDebug() << job << priority;
    const QString protocol = SimpleJobPrivate::get(job)->m_protocol;
    if (!protocol.isEmpty()) {
        ProtoQueue *proto = protoQ(SimpleJobPrivate::get(job)->m_protocol, job->url().host());
        proto->changeJobPriority(job, priority);
    }
}

void SchedulerPrivate::cancelJob(SimpleJob *job)
{
    // this method is called all over the place in job.cpp, so just do this check here to avoid
    // much boilerplate in job code.
    if (SimpleJobPrivate::get(job)->m_schedSerial == 0) {
        //qDebug() << "Doing nothing because I don't know job" << job;
        return;
    }
    Slave *slave = jobSlave(job);
    //qDebug() << job << slave;
    if (slave) {
        //qDebug() << "Scheduler: killing slave " << slave->slave_pid();
        slave->kill();
    }
    jobFinished(job, slave);
}

void SchedulerPrivate::jobFinished(SimpleJob *job, Slave *slave)
{
    //qDebug() << job << slave;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    // make sure that we knew about the job!
    Q_ASSERT(jobPriv->m_schedSerial);

    ProtoQueue *pq = m_protocols.value(jobPriv->m_protocol);
    if (pq) {
        pq->removeJob(job);
    }

    if (slave) {
        // If we have internal meta-data, tell existing ioslaves to reload
        // their configuration.
        if (jobPriv->m_internalMetaData.count()) {
            //qDebug() << "Updating ioslaves with new internal metadata information";
            ProtoQueue *queue = m_protocols.value(slave->protocol());
            if (queue) {
                QListIterator<Slave *> it(queue->allSlaves());
                while (it.hasNext()) {
                    Slave *runningSlave = it.next();
                    if (slave->host() == runningSlave->host()) {
                        slave->setConfig(metaDataFor(slave->protocol(), jobPriv->m_proxyList, job->url()));
                        /*qDebug() << "Updated configuration of" << slave->protocol()
                                     << "ioslave, pid=" << slave->slave_pid();*/
                    }
                }
            }
        }
        slave->setJob(nullptr);
        slave->disconnect(job);
    }
    jobPriv->m_schedSerial = 0; // this marks the job as unscheduled again
    jobPriv->m_slave = nullptr;
    // Clear the values in the internal metadata container since they have
    // already been taken care of above...
    jobPriv->m_internalMetaData.clear();
}

// static
void setupSlave(KIO::Slave *slave, const QUrl &url, const QString &protocol,
                const QStringList &proxyList, bool newSlave, const KIO::MetaData *config)
{
    schedulerPrivate()->setupSlave(slave, url, protocol, proxyList, newSlave, config);
}

MetaData SchedulerPrivate::metaDataFor(const QString &protocol, const QStringList &proxyList, const QUrl &url)
{
    const QString host = url.host();
    MetaData configData = SlaveConfig::self()->configData(protocol, host);
    sessionData.configDataFor(configData, protocol, host);
    if (proxyList.isEmpty()) {
        configData.remove(QStringLiteral("UseProxy"));
        configData.remove(QStringLiteral("ProxyUrls"));
    } else {
        configData[QStringLiteral("UseProxy")] = proxyList.first();
        configData[QStringLiteral("ProxyUrls")] = proxyList.join(QLatin1Char(','));
    }

    if (configData.contains(QLatin1String("EnableAutoLogin")) &&
            configData.value(QStringLiteral("EnableAutoLogin")).compare(QLatin1String("true"), Qt::CaseInsensitive) == 0) {
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

void SchedulerPrivate::setupSlave(KIO::Slave *slave, const QUrl &url, const QString &protocol,
                                  const QStringList &proxyList, bool newSlave, const KIO::MetaData *config)
{
    int port = url.port();
    if (port == -1) { // no port is -1 in QUrl, but in kde3 we used 0 and the kioslaves assume that.
        port = 0;
    }
    const QString host = url.host();
    const QString user = url.userName();
    const QString passwd = url.password();

    if (newSlave || slave->host() != host || slave->port() != port ||
            slave->user() != user || slave->passwd() != passwd) {

        MetaData configData = metaDataFor(protocol, proxyList, url);
        if (config) {
            configData += *config;
        }

        slave->setConfig(configData);
        slave->setProtocol(url.scheme());
        slave->setHost(host, port, user, passwd);
    }
}

void SchedulerPrivate::slotSlaveStatus(qint64, const QByteArray &, const QString &, bool)
{
}

void SchedulerPrivate::slotSlaveDied(KIO::Slave *slave)
{
    //qDebug() << slave;
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
    Slave *slave = jobSlave(job);
    //qDebug() << job << url << slave;
    slave->disconnect(job);
    // prevent the fake death of the slave from trying to kill the job again;
    // cf. Slave::hold(const QUrl &url) called in SchedulerPrivate::publishSlaveOnHold().
    slave->setJob(nullptr);
    SimpleJobPrivate::get(job)->m_slave = nullptr;

    if (m_slaveOnHold) {
        m_slaveOnHold->kill();
    }
    m_slaveOnHold = slave;
    m_urlOnHold = url;
    m_slaveOnHold->suspend();
}

void SchedulerPrivate::publishSlaveOnHold()
{
    //qDebug() << m_slaveOnHold;
    if (!m_slaveOnHold) {
        return;
    }

    m_slaveOnHold->hold(m_urlOnHold);
    Q_EMIT q->slaveOnHoldListChanged();
}

bool SchedulerPrivate::isSlaveOnHoldFor(const QUrl &url)
{
    if (url.isValid() && m_urlOnHold.isValid() && url == m_urlOnHold) {
        return true;
    }

    return Slave::checkForHeldSlave(url);
}

Slave *SchedulerPrivate::heldSlaveForJob(SimpleJob *job)
{
    Slave *slave = nullptr;
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);

    if (jobPriv->m_checkOnHold) {
        slave = Slave::holdSlave(jobPriv->m_protocol, job->url());
    }

    if (!slave && m_slaveOnHold) {
        // Make sure that the job wants to do a GET or a POST, and with no offset
        const int cmd = jobPriv->m_command;
        bool canJobReuse = (cmd == CMD_GET || cmd == CMD_MULTI_GET);

        if (KIO::TransferJob *tJob = qobject_cast<KIO::TransferJob *>(job)) {
            canJobReuse = (canJobReuse || cmd == CMD_SPECIAL);
            if (canJobReuse) {
                KIO::MetaData outgoing = tJob->outgoingMetaData();
                const QString resume = outgoing.value(QStringLiteral("resume"));
                const QString rangeStart = outgoing.value(QStringLiteral("range-start"));
                //qDebug() << "Resume metadata is" << resume;
                canJobReuse = (resume.isEmpty() || resume == QLatin1Char('0')) && (rangeStart.isEmpty() || rangeStart == QLatin1Char('0'));
            }
        }

        if (job->url() == m_urlOnHold) {
            if (canJobReuse) {
                //qDebug() << "HOLD: Reusing held slave (" << m_slaveOnHold << ")";
                slave = m_slaveOnHold;
            } else {
                //qDebug() << "HOLD: Discarding held slave (" << m_slaveOnHold << ")";
                m_slaveOnHold->kill();
            }
            m_slaveOnHold = nullptr;
            m_urlOnHold.clear();
        }
    } else if (slave) {
        //qDebug() << "HOLD: Reusing klauncher held slave (" << slave << ")";
    }

    return slave;
}

void SchedulerPrivate::removeSlaveOnHold()
{
    //qDebug() << m_slaveOnHold;
    if (m_slaveOnHold) {
        m_slaveOnHold->kill();
    }
    m_slaveOnHold = nullptr;
    m_urlOnHold.clear();
}

Slave *SchedulerPrivate::getConnectedSlave(const QUrl &url, const KIO::MetaData &config)
{
    QStringList proxyList;
    const QString protocol = KProtocolManager::slaveProtocol(url, proxyList);
    ProtoQueue *pq = protoQ(protocol, url.host());

    Slave *slave = pq->createSlave(protocol, /* job */nullptr, url);
    if (slave) {
        setupSlave(slave, url, protocol, proxyList, true, &config);
        pq->m_connectedSlaveQueue.addSlave(slave);

        slave->send(CMD_CONNECT);
        q->connect(slave, SIGNAL(connected()),
                   SLOT(slotSlaveConnected()));
        q->connect(slave, SIGNAL(error(int,QString)),
                   SLOT(slotSlaveError(int,QString)));
    }
    //qDebug() << url << slave;
    return slave;
}

void SchedulerPrivate::slotSlaveConnected()
{
    //qDebug();
    Slave *slave = static_cast<Slave *>(q->sender());
    slave->setConnected(true);
    q->disconnect(slave, SIGNAL(connected()), q, SLOT(slotSlaveConnected()));
    Q_EMIT q->slaveConnected(slave);
}

void SchedulerPrivate::slotSlaveError(int errorNr, const QString &errorMsg)
{
    Slave *slave = static_cast<Slave *>(q->sender());
    //qDebug() << slave << errorNr << errorMsg;
    ProtoQueue *pq = protoQ(slave->protocol(), slave->host());
    if (!slave->isConnected() || pq->m_connectedSlaveQueue.isIdle(slave)) {
        // Only forward to application if slave is idle or still connecting.
        // ### KDE5: can we remove this apparently arbitrary behavior and just always emit SlaveError?
        Q_EMIT q->slaveError(slave, errorNr, errorMsg);
    }
}

bool SchedulerPrivate::assignJobToSlave(KIO::Slave *slave, SimpleJob *job)
{
    //qDebug() << slave << job;
    // KDE5: queueing of jobs can probably be removed, it provides very little benefit
    ProtoQueue *pq = m_protocols.value(slave->protocol());
    if (pq) {
        pq->removeJob(job);
        return pq->m_connectedSlaveQueue.queueJob(job, slave);
    }
    return false;
}

bool SchedulerPrivate::disconnectSlave(KIO::Slave *slave)
{
    //qDebug() << slave;
    ProtoQueue *pq = m_protocols.value(slave->protocol());
    return (pq ? pq->m_connectedSlaveQueue.removeSlave(slave) : false);
}

void SchedulerPrivate::checkSlaveOnHold(bool b)
{
    //qDebug() << b;
    m_checkOnHold = b;
}

void SchedulerPrivate::updateInternalMetaData(SimpleJob *job)
{
    KIO::SimpleJobPrivate *const jobPriv = SimpleJobPrivate::get(job);
    // Preserve all internal meta-data so they can be sent back to the
    // ioslaves as needed...
    const QUrl jobUrl = job->url();
    //qDebug() << job << jobPriv->m_internalMetaData;
    QMapIterator<QString, QString> it(jobPriv->m_internalMetaData);
    while (it.hasNext()) {
        it.next();
        if (it.key().startsWith(QLatin1String("{internal~currenthost}"), Qt::CaseInsensitive)) {
            SlaveConfig::self()->setConfigData(jobUrl.scheme(), jobUrl.host(), it.key().mid(22), it.value());
        } else if (it.key().startsWith(QLatin1String("{internal~allhosts}"), Qt::CaseInsensitive)) {
            SlaveConfig::self()->setConfigData(jobUrl.scheme(), QString(), it.key().mid(19), it.value());
        }
    }
}

#include "moc_scheduler.cpp"
#include "moc_scheduler_p.cpp"
