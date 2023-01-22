/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009, 2010 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef SCHEDULER_P_H
#define SCHEDULER_P_H

#include "kiocore_export.h"

#include <QSet>
#include <QTimer>
// #define SCHEDULER_DEBUG

namespace KIO
{
// The slave keeper manages the list of idle slaves that can be reused
class SlaveKeeper : public QObject
{
    Q_OBJECT
public:
    SlaveKeeper();
    ~SlaveKeeper() override;
    void returnSlave(KIO::Slave *slave);
    // pick suitable slave for job and return it, return null if no slave found.
    // the slave is removed from the keeper.
    KIO::Slave *takeSlaveForJob(KIO::SimpleJob *job);
    // remove slave from keeper
    bool removeSlave(KIO::Slave *slave);
    // remove all slaves from keeper
    void clear();
    QList<KIO::Slave *> allSlaves() const;

private:
    void scheduleGrimReaper();

private Q_SLOTS:
    void grimReaper();

private:
    QMultiHash<QString, KIO::Slave *> m_idleSlaves;
    QTimer m_grimTimer;
};

class HostQueue
{
public:
    int lowestSerial() const;

    bool isQueueEmpty() const
    {
        return m_queuedJobs.isEmpty();
    }
    bool isEmpty() const
    {
        return m_queuedJobs.isEmpty() && m_runningJobs.isEmpty();
    }
    int runningJobsCount() const
    {
        return m_runningJobs.count();
    }
#ifdef SCHEDULER_DEBUG
    QList<KIO::SimpleJob *> runningJobs() const
    {
        return QList<KIO::SimpleJob *>(m_runningJobs.cbegin(), m_runningJobs.cend());
    }
#endif
    bool isJobRunning(KIO::SimpleJob *job) const
    {
        return m_runningJobs.contains(job);
    }

    void queueJob(KIO::SimpleJob *job);
    KIO::SimpleJob *takeFirstInQueue();
    bool removeJob(KIO::SimpleJob *job);

    QList<KIO::Slave *> allSlaves() const;

private:
    QMap<int, KIO::SimpleJob *> m_queuedJobs;
    QSet<KIO::SimpleJob *> m_runningJobs;
};

class SchedulerPrivate;

class SerialPicker
{
public:
    // note that serial number zero is the default value from job_p.h and invalid!

    int next()
    {
        if (m_offset >= m_jobsPerPriority) {
            m_offset = 1;
        }
        return m_offset++;
    }

    int changedPrioritySerial(int oldSerial, int newPriority) const;

private:
    static const uint m_jobsPerPriority = 100000000;
    uint m_offset = 1;

public:
    static const int maxSerial = m_jobsPerPriority * 20;
};

class ProtoQueue : public QObject
{
    Q_OBJECT
public:
    ProtoQueue(int maxWorkers, int maxWorkersPerHost);
    ~ProtoQueue() override;

    void queueJob(KIO::SimpleJob *job);
    void changeJobPriority(KIO::SimpleJob *job, int newPriority);
    void removeJob(KIO::SimpleJob *job);
    KIO::Slave *createSlave(const QString &protocol, KIO::SimpleJob *job, const QUrl &url);
    bool removeSlave(KIO::Slave *slave);
    QList<KIO::Slave *> allSlaves() const;

private Q_SLOTS:
    // start max one (non-connected) job and return
    void startAJob();

private:
    SerialPicker m_serialPicker;
    QTimer m_startJobTimer;
    QMap<int, HostQueue *> m_queuesBySerial;
    QHash<QString, HostQueue> m_queuesByHostname;
    SlaveKeeper m_slaveKeeper;
    int m_maxConnectionsPerHost;
    int m_maxConnectionsTotal;
    int m_runningJobsCount;
};

} // namespace KIO

#endif // SCHEDULER_P_H
