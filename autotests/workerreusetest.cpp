/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

#include <atomic>

#include "kio/global.h"
#include "kio/simplejob.h"
#include "kio/transferjob.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
struct WorkerControl {
    std::atomic<int> createCount{0};
    std::atomic<bool> startedTransfer{false};
    std::atomic<bool> blockTransfer{false};
    std::atomic<bool> dieInTransfer{false};
    void reset()
    {
        createCount = 0;
        startedTransfer = false;
        blockTransfer = false;
        dieInTransfer = false;
    }
};
WorkerControl g_control;

// Bounds the blocking get() so a stuck test fails instead of hanging.
constexpr int s_timeoutMs = 1000;

// A factory whose worker serves get(): immediately, blocking (so the job can be held mid-flight),
// or dropping its connection to mimic a worker that died. createCount counts how many distinct
// workers the scheduler spawned - it only asks the factory when it cannot reuse one.
class TestFactory : public KIO::WorkerFactory
{
public:
    using KIO::WorkerFactory::WorkerFactory;
    std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        g_control.createCount.fetch_add(1);

        class TestWorker : public KIO::WorkerBase
        {
        public:
            TestWorker(const QByteArray &pool, const QByteArray &app)
                : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
            {
            }
            KIO::WorkerResult get(const QUrl &) override
            {
                g_control.startedTransfer.store(true);
                if (g_control.dieInTransfer.load()) {
                    // Drop the connection mid-transfer to mimic a worker that died unexpectedly.
                    // Unlike Worker::kill(), the app-side Worker still thinks the worker is alive,
                    // so the broken connection is reported as a death (ERR_WORKER_DIED, handled by
                    // SchedulerPrivate::slotWorkerDied()).
                    disconnectWorker();
                    return KIO::WorkerResult::pass();
                }
                QElapsedTimer elapsed;
                elapsed.start();
                while (g_control.blockTransfer.load() && !wasKilled() && elapsed.elapsed() < s_timeoutMs) {
                    QThread::msleep(1);
                }
                mimeType(QStringLiteral("application/octet-stream"));
                data(QByteArray("hello"));
                data(QByteArray()); // empty data marks the end
                return KIO::WorkerResult::pass();
            }
        };
        return std::unique_ptr<KIO::WorkerBase>(new TestWorker(pool, app));
    }
};

KIO::TransferJob *startGet(const QUrl &url)
{
    auto *job = KIO::get(url, KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    return job;
}

// Pump the event loop until predicate() is true or we time out.
template<typename Predicate>
bool waitUntil(Predicate predicate)
{
    QElapsedTimer waited;
    waited.start();
    while (!predicate() && waited.elapsed() < s_timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    return predicate();
}
}

class WorkerReuseTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void heldWorkerReusedForGet()
    {
        // A worker can be put on hold to be reused by the next job for the same URL
        // (Scheduler::putWorkerOnHold / heldWorkerForJob). Hold the worker of a running job, then
        // verify a following get() to the same URL reuses it instead of spawning a new one.
        g_control.reset();
        auto factory = std::make_shared<TestFactory>();
        KIO::Worker::setTestWorkerFactory(factory);

        const QUrl url(QStringLiteral("kio-test-hold://host/file"));

        // First job blocks inside get() so it still owns its worker when we hold it.
        g_control.blockTransfer.store(true);
        auto *first = startGet(url);
        QVERIFY2(waitUntil([] {
                     return g_control.startedTransfer.load();
                 }),
                 "first job never reached the worker");
        QCOMPARE(g_control.createCount.load(), 1);

        // Put the worker on hold for this URL; this detaches and quietly kills the first job.
        first->putOnHold();

        // Let the (now jobless) get() return so the worker is back in its dispatch loop, held.
        g_control.blockTransfer.store(false);

        // A get() to the held URL must reuse the held worker: no new worker is created.
        QVERIFY2(startGet(url)->exec(), "reusing the held worker failed");
        QCOMPARE(g_control.createCount.load(), 1);
    }

    void removeWorkerOnHoldDiscardsIt()
    {
        // removeOnHold() discards a held worker (killing it). A following get() can then no longer
        // reuse it and must spawn a fresh worker.
        g_control.reset();
        auto factory = std::make_shared<TestFactory>();
        KIO::Worker::setTestWorkerFactory(factory);

        const QUrl url(QStringLiteral("kio-test-drop://host/file"));

        g_control.blockTransfer.store(true);
        auto *first = startGet(url);
        QVERIFY(waitUntil([] {
            return g_control.startedTransfer.load();
        }));

        first->putOnHold();
        KIO::SimpleJob::removeOnHold(); // kills the held worker
        g_control.blockTransfer.store(false);

        QVERIFY(startGet(url)->exec());
        QCOMPARE(g_control.createCount.load(), 2);
    }

    void dyingWorkerIsCleanedUp()
    {
        // When a worker dies while serving a job, the scheduler (slotWorkerDied) tears it down and
        // the job fails with ERR_WORKER_DIED.
        g_control.reset();
        auto factory = std::make_shared<TestFactory>();
        KIO::Worker::setTestWorkerFactory(factory);

        g_control.dieInTransfer.store(true);
        auto *job = startGet(QUrl(QStringLiteral("kio-test-die://host/file")));

        QVERIFY(!job->exec());
        QCOMPARE(job->error(), int(KIO::ERR_WORKER_DIED));
    }

    void idleWorkerIsReaped()
    {
        // An idle worker is killed by the grim reaper once it has been idle for longer than the
        // idle timeout (WorkerManager::grimReaper). Force the timeout to 0 so the next idle worker
        // is reaped right away, and verify a following job has to spawn a new worker.
        qputenv("KIO_WORKER_IDLE_TIMEOUT", "0");
        auto restoreTimeout = qScopeGuard([] {
            qunsetenv("KIO_WORKER_IDLE_TIMEOUT");
        });

        g_control.reset();
        auto factory = std::make_shared<TestFactory>();
        KIO::Worker::setTestWorkerFactory(factory);

        const QUrl url(QStringLiteral("kio-test-reap://host/file"));

        // Run a job to completion so its worker goes idle and the reaper is scheduled.
        QVERIFY(startGet(url)->exec());
        QCOMPARE(g_control.createCount.load(), 1);

        // Let the (zero-delay) reaper fire and kill the idle worker.
        QTest::qWait(5);

        // The idle worker is gone, so the next job must create a new one.
        QVERIFY(startGet(url)->exec());
        QCOMPARE(g_control.createCount.load(), 2);
    }
};

QTEST_MAIN(WorkerReuseTest)

#include "workerreusetest.moc"
