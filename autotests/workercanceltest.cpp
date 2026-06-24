/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QStandardPaths>
#include <QTest>
#include <QThread>

#include <atomic>

#include "kio/listjob.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// A mock worker whose listDir() polls wasKilled() the way a real readdir loop must, recording
// whether the cancellation was delivered into the running call.
struct ListWorkerControl {
    std::atomic<bool> startedListing{false};
    std::atomic<bool> sawKilled{false};
    std::atomic<bool> completedNormally{false};
    void reset()
    {
        startedListing = false;
        sawKilled = false;
        completedNormally = false;
    }
};
ListWorkerControl g_listWorker;

// A delivered cancellation reaches the worker within milliseconds, so this only bounds how long the
// test waits before treating the run as a failure.
constexpr int s_killTimeoutMs = 1000;
}

class WorkerCancelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void listDirObservesKill()
    {
        // A worker's long listDir() loop must be able to observe cancellation and bail (kio_file's
        // readdir loop and kio_smb's listing both need this). The cancel signal already reaches the
        // worker as wasKilled(). Verify it is delivered into a running listDir() so a polling loop
        // stops instead of running to completion. A worker that does poll then stops promptly.
        g_listWorker.reset();

        class Factory : public KIO::WorkerFactory
        {
        public:
            using KIO::WorkerFactory::WorkerFactory;
            std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
            {
                class ListWorker : public KIO::WorkerBase
                {
                public:
                    ListWorker(const QByteArray &pool, const QByteArray &app)
                        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
                    {
                    }
                    KIO::WorkerResult listDir(const QUrl &) override
                    {
                        g_listWorker.startedListing.store(true);
                        // Stand in for a long readdir loop that polls wasKilled() every iteration.
                        QElapsedTimer elapsed;
                        elapsed.start();
                        while (!wasKilled() && elapsed.elapsed() < s_killTimeoutMs) {
                            QThread::msleep(1);
                        }
                        if (wasKilled()) {
                            g_listWorker.sawKilled.store(true);
                        } else {
                            g_listWorker.completedNormally.store(true);
                        }
                        return KIO::WorkerResult::pass();
                    }
                };
                return std::unique_ptr<KIO::WorkerBase>(new ListWorker(pool, app));
            }
        };
        auto factory = std::make_shared<Factory>();
        KIO::Worker::setTestWorkerFactory(factory);

        auto *job = KIO::listDir(QUrl(QStringLiteral("kio-test://foo")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);

        // Wait until the worker is inside listDir().
        QElapsedTimer waited;
        waited.start();
        while (!g_listWorker.startedListing.load() && waited.elapsed() < s_killTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        QVERIFY2(g_listWorker.startedListing.load(),
                 "timed out waiting for the worker to enter listDir() - the kio-test job was never dispatched to the worker");

        // kill() aborts the worker (sets wasKilled) then joins it. The worker's listDir() must see
        // the cancellation and return.
        job->kill(KJob::EmitResult);

        QVERIFY2(g_listWorker.sawKilled.load(),
                 "the worker ran to its timeout without observing wasKilled() - the kill was not delivered into the running listDir()");
        QVERIFY(!g_listWorker.completedNormally.load());
    }
};

QTEST_MAIN(WorkerCancelTest)

#include "workercanceltest.moc"
