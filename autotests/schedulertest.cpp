/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QStandardPaths>
#include <QTest>

#include <atomic>

#include "kio/global.h"
#include "kio/statjob.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// Counts how many workers the scheduler actually creates, so a test can tell a reused worker from a
// freshly spawned one.
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

private:
    std::shared_ptr<TestWorkerFactory> m_factory;
};

QTEST_MAIN(SchedulerTest)

#include "schedulertest.moc"
