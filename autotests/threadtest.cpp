/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QScopeGuard>
#include <QThread>
#include <QTimer>

#include "kio/filecopyjob.h"
#include "kio/transferjob.h"
#include "kiotesthelper.h" // homeTmpDir, createTestFile etc.
#include "workerthread_p.h" // KIO::WorkerThread test hooks

class KIOThreadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void asyncConcurrentCopying();
    void copyJobFromThread();
    void cancelJobWhileWorkerIsBlocked();
    void cleanupTestCase();

private:
    static bool copyLocalFile(const QString &src, const QString &dest);
};

void KIOThreadTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    cleanupTestCase();
    homeTmpDir();
}

void KIOThreadTest::cleanupTestCase()
{
    QDir(homeTmpDir()).removeRecursively();
}

bool KIOThreadTest::copyLocalFile(const QString &src, const QString &dest)
{
    const QUrl u = QUrl::fromLocalFile(src);
    const QUrl d = QUrl::fromLocalFile(dest);
    std::unique_ptr<KIO::Job> job(KIO::file_copy(u, d, -1, KIO::HideProgressInfo));
    job->setUiDelegate(nullptr);
    return job->exec();
}

void KIOThreadTest::asyncConcurrentCopying()
{
    const int numFiles = 10;
    QList<QString> srcs;
    QList<QString> dests;
    srcs.reserve(numFiles);
    dests.reserve(numFiles);
    for (int i = 0; i < numFiles; ++i) {
        srcs << homeTmpDir() + QLatin1String("file") + QString::number(i);
        dests << homeTmpDir() + QLatin1String("file") + QString::number(i) + QLatin1String("_copy");
        createTestFile(srcs.last());
    }

    // All jobs are queued before the event loop runs so the scheduler
    // dispatches them concurrently from a single SchedulerPrivate instance.
    int completedJobs = 0;
    QEventLoop loop;
    for (int i = 0; i < numFiles; ++i) {
        auto *job = KIO::file_copy(QUrl::fromLocalFile(srcs.at(i)), QUrl::fromLocalFile(dests.at(i)), -1, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        connect(job, &KJob::result, this, [&completedJobs, numFiles, &loop](KJob *j) {
            QVERIFY(!j->error());
            if (++completedJobs == numFiles) {
                loop.quit();
            }
        });
    }
    QTimer::singleShot(std::chrono::seconds(60), &loop, &QEventLoop::quit);
    loop.exec();

    QCOMPARE(completedJobs, numFiles);
    for (const QString &dest : std::as_const(dests)) {
        QVERIFY(QFile::exists(dest));
    }
}

void KIOThreadTest::copyJobFromThread()
{
    const QString src = homeTmpDir() + QLatin1String("src_thread");
    const QString dest = homeTmpDir() + QLatin1String("dst_thread");
    createTestFile(src);

    // One thread: Q_PLUGIN_INSTANCE is not thread-safe for concurrent first access.
    bool jobSucceeded = false;
    auto *thread = QThread::create([&src, &dest, &jobSucceeded]() {
        jobSucceeded = copyLocalFile(src, dest);
    });
    thread->start();
    QVERIFY(thread->wait(30000));
    delete thread;

    QVERIFY(jobSucceeded);
    QVERIFY(QFile::exists(dest));
}

void KIOThreadTest::cancelJobWhileWorkerIsBlocked()
{
    // Regression test for the Worker::deref() deadlock (bug 468673). Cancelling a running job
    // derefs its in-process worker, and a worker thread that is mid-transfer can only finish
    // once the main thread keeps running its event loop (a real worker blocks in
    // waitForBytesWritten() until the main thread drains its socket). If deref() joins that
    // thread synchronously while we are inside the event loop dispatching the worker's data,
    // the main thread blocks on the join instead of running its event loop, and both threads
    // wedge. This used to surface only downstream (Dolphin's concurrent directory-size
    // listings); reproduce it here. WorkerThread's BUILD_TESTING exit gate makes the worker's
    // "needs the main thread to make progress" dependency deterministic: without the fix this
    // test deadlocks and is killed by the harness timeout; with it the loop drains cleanly.

    KIO::WorkerThread::setTestExitGateEnabled(true);
    auto gateGuard = qScopeGuard([] {
        // Make sure no worker (including idle ones torn down at exit) can stay gated.
        KIO::WorkerThread::setTestExitGateEnabled(false);
        KIO::WorkerThread::releaseTestExitGate();
    });

    const QString path = homeTmpDir() + QLatin1String("bigfile");
    const QByteArray bigData(16 * 1024 * 1024, 'a'); // big enough that data() fires mid-transfer
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bigData), qint64(bigData.size()));
    f.close();

    auto *job = KIO::get(QUrl::fromLocalFile(path), KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    QEventLoop loop;
    bool cancelled = false;
    connect(job, &KIO::TransferJob::data, this, [&](KIO::Job *, const QByteArray &) {
        if (cancelled) {
            return;
        }
        cancelled = true;
        // Cancel from inside the data dispatch (loopLevel > 0): this derefs the gated worker.
        // A synchronous join here would block the main thread before it can release the gate.
        job->kill();
        // Release the gate and finish - but only from the event loop. With the deadlock bug,
        // control never returns here, so the worker stays gated and the test hangs.
        QTimer::singleShot(0, this, [] {
            KIO::WorkerThread::releaseTestExitGate();
        });
        QTimer::singleShot(100, &loop, &QEventLoop::quit);
    });
    // Absolute safety net; with the fix the 100ms timer above quits the loop far sooner.
    QTimer::singleShot(std::chrono::seconds(30), &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(cancelled);
}

QTEST_MAIN(KIOThreadTest)
#include "threadtest.moc"
