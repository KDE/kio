/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QPointer>
#include <QScopeGuard>
#include <QThread>
#include <QTimer>

#include "connectionbackend_p.h" // KIO::ConnectionBackend test hook
#include "kio/filecopyjob.h"
#include "kio/listjob.h"
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
    void killBlockedWorkerOutsideEventLoop();
    void cancelDoesNotFlushUnrelatedDeferredDeletes();
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

    // A worker deref'd from inside an event loop (a job completing or being cancelled
    // mid-dispatch) deletes its WorkerThread asynchronously through QThread::finished ->
    // deleteLater(), rather than joining the thread under a running loop (that join is the
    // bug 468673 deadlock). In a running application the next event-loop iteration reaps
    // those threads; a test process exits right after its last loop, so on some Qt versions
    // the queued DeferredDelete events are never delivered and LeakSanitizer reports the
    // worker threads and their QPluginLoaders as leaked. Force-deliver them here.
    // This drain used to live in ~Worker(), but flushing the global queue from a destructor
    // re-entrantly freed unrelated objects mid-destruction; doing it once from the test
    // teardown is the correct scope.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
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

void KIOThreadTest::killBlockedWorkerOutsideEventLoop()
{
#ifdef Q_OS_WIN
    QSKIP("abortConnection()'s socket shutdown is POSIX-only");
#endif
    // Cancelling a listing at loop level 0 (e.g. a KCoreDirLister destroyed during teardown) makes
    // Worker::deref() join the worker synchronously while it is parked in waitForIncomingTask().
    // The hook makes the peer-side close a no-op, so only WorkerThread::abort() shutting the worker's
    // own fd down can wake it: without the fix this hangs, with it kill() returns at once (bug 468673).
    KIO::ConnectionBackend::setTestBlockSocketClose(true);
    auto closeGuard = qScopeGuard([] {
        KIO::ConnectionBackend::setTestBlockSocketClose(false);
    });

    // A directory with many entries, so the listing worker is mid-list (parked in waitForReadyRead
    // between batches) when we cancel - mirroring a KCoreDirLister torn down during a live listing.
    const QString dir = homeTmpDir() + QLatin1String("listdir/");
    QVERIFY(QDir().mkpath(dir));
    for (int i = 0; i < 5000; ++i) {
        QFile e(dir + QStringLiteral("entry%1").arg(i));
        QVERIFY(e.open(QIODevice::WriteOnly));
    }

    auto *job = KIO::listDir(QUrl::fromLocalFile(dir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    // Run the event loop only until the worker is up and delivering entries, then stop consuming and
    // leave the loop. The worker is now blocked on its socket with no event loop draining it.
    QEventLoop loop;
    bool gotEntries = false;
    connect(job, &KIO::ListJob::entries, this, [&](KIO::Job *, const KIO::UDSEntryList &) {
        gotEntries = true;
        loop.quit();
    });
    QTimer::singleShot(std::chrono::seconds(30), &loop, &QEventLoop::quit); // safety net
    loop.exec();
    QVERIFY(gotEntries);

    // Back at loop level 0: cancelling makes deref() take the synchronous-join path against the
    // blocked worker. Reaching the line after kill() proves the join did not deadlock.
    job->kill();
    QVERIFY(true);
}

void KIOThreadTest::cancelDoesNotFlushUnrelatedDeferredDeletes()
{
    // Regression test: Worker::~Worker() must not flush the global deferred-delete queue
    // (QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete)). ~Worker() runs
    // synchronously when a running job is cancelled, and that often happens deep inside an
    // unrelated object's destruction. Flushing every pending deleteLater() from there
    // re-entrantly deletes objects that are merely scheduled for deletion - or, worse, already
    // being destroyed higher up the stack - which crashed on shutdown (a
    // new-delete-type-mismatch / use-after-free). Assert the observable behaviour: tearing a
    // worker down must not process an unrelated, still-pending deleteLater().

    const QString path = homeTmpDir() + QLatin1String("bigfile_flush");
    const QByteArray bigData(16 * 1024 * 1024, 'a'); // big enough that data() fires mid-transfer
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bigData), qint64(bigData.size()));
    f.close();

    auto *job = KIO::get(QUrl::fromLocalFile(path), KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    QEventLoop loop;
    bool checked = false;
    QPointer<QObject> victim;
    connect(job, &KIO::TransferJob::data, this, [&](KIO::Job *, const QByteArray &) {
        if (checked) {
            return;
        }
        checked = true;
        // Schedule an unrelated object for deletion and immediately, in the same
        // synchronous block (no event-loop iteration in between, so the deleteLater is
        // still pending), cancel the running job. Cancelling derefs and destroys its
        // worker synchronously (Worker::kill() -> deref() -> ~Worker()). Only a
        // re-entrant flush inside ~Worker() could consume the deleteLater here.
        victim = new QObject;
        victim->deleteLater();
        job->kill();
        QVERIFY(victim); // ~Worker() must not have flushed the unrelated deleteLater()
        loop.quit();
    });
    QTimer::singleShot(std::chrono::seconds(30), &loop, &QEventLoop::quit);
    loop.exec();

    QVERIFY(checked);
    QVERIFY(victim); // still alive: the worker teardown did not flush it

    // Sanity: a real deferred-delete flush still works and reclaims the victim.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!victim);
}

QTEST_MAIN(KIOThreadTest)
#include "threadtest.moc"
