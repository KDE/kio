/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "batchcopy_p.h"
#include "batchcopyjob_p.h"
#include "job_p.h"
#include "worker_p.h"

#include <QDataStream>

using namespace KIO;

class KIO::BatchCopyJobPrivate : public SimpleJobPrivate
{
public:
    BatchCopyJobPrivate(const QUrl &url, const TaskPayload &packedArgs)
        : SimpleJobPrivate(url, CMD_BATCH_COPY, packedArgs)
    {
    }

    void start(Worker *worker) override;

    static BatchCopyJob *newJob(const QUrl &url, const TaskPayload &packedArgs)
    {
        return new BatchCopyJob(*new BatchCopyJobPrivate(url, packedArgs));
    }

    Q_DECLARE_PUBLIC(BatchCopyJob)
};

BatchCopyJob::BatchCopyJob(BatchCopyJobPrivate &dd)
    : SimpleJob(dd)
{
}

BatchCopyJob::~BatchCopyJob() = default;

void BatchCopyJobPrivate::start(Worker *worker)
{
    Q_Q(BatchCopyJob);
    q->connect(worker, &WorkerInterface::batchCopyItemFinished, q, &BatchCopyJob::itemFinished);
    SimpleJobPrivate::start(worker);
}

BatchCopyJob *KIO::batchCopy(const QUrl &workerUrl, const QList<BatchCopyItem> &items, JobFlags flags)
{
    QByteArray packedArgs;
    QDataStream stream(&packedArgs, QIODevice::WriteOnly);
    // HideProgressInfo only concerns the job tracker, not the worker.
    stream << BatchCopyRequest{items, flags & ~JobFlags(HideProgressInfo)};
    auto *job = BatchCopyJobPrivate::newJob(workerUrl, packedArgs);
    if (!(flags & HideProgressInfo)) {
        KIO::getJobTracker()->registerJob(job);
    }
    return job;
}

#include "moc_batchcopyjob_p.cpp"
