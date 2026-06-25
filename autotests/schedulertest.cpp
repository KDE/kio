/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

#include <atomic>

#include "kio/global.h"
#include "kio/simplejob.h"
#include "kio/statjob.h"
#include "kio/transferjob.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// Counts how many workers the scheduler actually creates, so a test can tell a reused worker from a
// freshly spawned one. Workers created here run in-process (setTestWorkerFactory spawns a
// WorkerThread), so this exercises the in-process / ThreadConnectionBackend path.
std::atomic<int> g_workersCreated{0};

class TestWorker : public KIO::WorkerBase
{
public:
    TestWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &) override
    {
        KIO::UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("path"));
        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult get(const QUrl &url) override
    {
        mimeType(QStringLiteral("text/plain"));
        if (url.host() == QLatin1String("slow")) {
            // Stream slowly so the test can put the worker on hold mid-transfer before it finishes.
            for (int i = 0; i < 40; ++i) {
                data(QByteArray("chunk"));
                QThread::msleep(25);
            }
        } else {
            data(QByteArray("hello"));
        }
        data(QByteArray()); // EOF
        return KIO::WorkerResult::pass();
    }
};

class TestWorkerFactory : public KIO::WorkerFactory
{
public:
    using KIO::WorkerFactory::WorkerFactory;
    std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        g_workersCreated.fetch_add(1, std::memory_order_relaxed);
        return std::unique_ptr<KIO::WorkerBase>(new TestWorker(pool, app));
    }
};
}

class SchedulerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        m_factory = std::make_shared<TestWorkerFactory>();
        KIO::Worker::setTestWorkerFactory(m_factory);
    }

    // Sequential jobs to the same protocol/host must reuse the idle worker the scheduler keeps
    // pooled, not spawn a fresh one each time (WorkerManager::takeWorkerForJob idle path).
    void idleWorkerIsReusedForSequentialJobs()
    {
        g_workersCreated.store(0, std::memory_order_relaxed);
        for (int i = 0; i < 3; ++i) {
            auto *job = KIO::stat(QUrl(QStringLiteral("kio-test://host/path")), KIO::HideProgressInfo);
            job->setUiDelegate(nullptr);
            QVERIFY2(job->exec(), qPrintable(job->errorString()));
        }
        QCOMPARE(g_workersCreated.load(std::memory_order_relaxed), 1);
    }

    // A worker parked with putOnHold() for a URL must be reused by a subsequent GET to the same URL
    // (Scheduler::heldWorkerForJob), and that reused worker must run to completion. putOnHold()
    // suspends the in-process worker's connection while it is parked, so the scheduler has to resume
    // it on reuse - otherwise the application never drains the worker's replies and the GET hangs.
    void heldWorkerIsReusedForMatchingGet()
    {
        auto *job1 = KIO::get(QUrl(QStringLiteral("kio-test://slow/file")), KIO::NoReload, KIO::HideProgressInfo);
        job1->setUiDelegate(nullptr);
        QSignalSpy mimeSpy(job1, &KIO::TransferJob::mimeTypeFound);
        QVERIFY2(mimeSpy.wait(5000), "worker never sent mimetype - job1 was not dispatched");

        // The in-process worker is mid-transfer and assigned to job1. Park it for this URL.
        job1->putOnHold();

        auto *job2 = KIO::get(QUrl(QStringLiteral("kio-test://slow/file")), KIO::NoReload, KIO::HideProgressInfo);
        job2->setUiDelegate(nullptr);
        QByteArray received;
        connect(job2, &KIO::TransferJob::data, this, [&received](KIO::Job *, const QByteArray &d) {
            received += d;
        });
        QSignalSpy resultSpy(job2, &KJob::result);
        QVERIFY2(resultSpy.wait(15000), "the GET reusing the held in-process worker never finished");
        QCOMPARE(job2->error(), 0);
        QVERIFY2(!received.isEmpty(), "the reused worker delivered no data");

        KIO::SimpleJob::removeOnHold(); // no-op if it was consumed
    }

private:
    std::shared_ptr<TestWorkerFactory> m_factory;
};

QTEST_MAIN(SchedulerTest)

#include "schedulertest.moc"
