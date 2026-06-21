/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QElapsedTimer>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWaitCondition>

#include "kio/filecopyjob.h"
#include "kio/listjob.h"
#include "kio/transferjob.h"
#include "kiotesthelper.h" // homeTmpDir, createTestFile etc.
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

#include <atomic>

namespace
{
// Shared control for the mock worker used by cancelDuringSlowWorkerDoesNotBlock(): its listDir()
// parks on this condition, standing in for a slow syscall that Connection::close() cannot interrupt.
struct SlowWorkerControl {
    QMutex mutex;
    QWaitCondition cond;
    std::atomic<bool> inListDir{false};
    bool release = false;
};
SlowWorkerControl g_slowWorker;
}

class KIOThreadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void asyncConcurrentCopying();
    void copyJobFromThread();
    void closeJoinsBlockedWorkerWithoutEventLoop();
    void cancelDuringSlowWorkerDoesNotBlock();
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

void KIOThreadTest::closeJoinsBlockedWorkerWithoutEventLoop()
{
    // At loop level 0 (no running event loop, e.g. a KCoreDirLister torn down during teardown),
    // Worker::deref() joins the in-process worker thread synchronously. Connection::close() wakes
    // the worker from its send back-pressure wait, so the join completes without the event loop
    // having to drain it. The socket-backed worker deadlocked here (bug 468673): it blocked in
    // waitForBytesWritten() for the main thread to drain the socket while the join blocked the main
    // thread. Reaching the line after kill() proves the synchronous join did not deadlock.

    const QString path = homeTmpDir() + QLatin1String("bigfile");
    const QByteArray bigData(16 * 1024 * 1024, 'a'); // large enough to back-pressure the worker before we consume
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QCOMPARE(f.write(bigData), qint64(bigData.size()));
    f.close();

    auto *job = KIO::get(QUrl::fromLocalFile(path), KIO::NoReload, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    // Run the loop only until the worker starts delivering, then return to loop level 0 with the
    // worker still mid-transfer (back-pressured, with no event loop draining it).
    QEventLoop loop;
    bool gotData = false;
    connect(job, &KIO::TransferJob::data, this, [&](KIO::Job *, const QByteArray &) {
        gotData = true;
        loop.quit();
    });
    QTimer::singleShot(std::chrono::seconds(30), &loop, &QEventLoop::quit); // safety net
    loop.exec();
    QVERIFY(gotData);

    // Back at loop level 0: kill() takes the synchronous-join path against the blocked worker.
    job->kill();
    QVERIFY(true); // reached => the synchronous join did not deadlock
}

void KIOThreadTest::cancelDuringSlowWorkerDoesNotBlock()
{
    // Regression test: cancelling a job from inside a running event loop (loopLevel > 0) must not
    // freeze the calling thread when the in-process worker is mid-operation between channel
    // checkpoints. Here the mock worker parks in listDir() on a condition that close() cannot wake
    // (a stand-in for a slow stat()/read()). deref() must reap the thread asynchronously; the old
    // unconditional synchronous join would block in wait() until the operation returned - which in
    // this test only happens after kill(), so the buggy path deadlocks and is killed by the timeout.
    g_slowWorker.inListDir.store(false);
    {
        QMutexLocker lock(&g_slowWorker.mutex);
        g_slowWorker.release = false;
    }

    class Factory : public KIO::WorkerFactory
    {
    public:
        using KIO::WorkerFactory::WorkerFactory;
        std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
        {
            class SlowWorker : public KIO::WorkerBase
            {
            public:
                SlowWorker(const QByteArray &pool, const QByteArray &app)
                    : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
                {
                }
                KIO::WorkerResult listDir(const QUrl &) override
                {
                    g_slowWorker.inListDir.store(true);
                    QMutexLocker lock(&g_slowWorker.mutex);
                    while (!g_slowWorker.release) {
                        g_slowWorker.cond.wait(&g_slowWorker.mutex);
                    }
                    return KIO::WorkerResult::pass();
                }
            };
            return std::unique_ptr<KIO::WorkerBase>(new SlowWorker(pool, app));
        }
    };
    auto factory = std::make_shared<Factory>();
    KIO::Worker::setTestWorkerFactory(factory);

    auto *job = KIO::listDir(QUrl(QStringLiteral("kio-test://foo")), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    QEventLoop loop;
    bool killReturned = false;
    QTimer::singleShot(0, this, [&]() {
        // Inside loop.exec(): loopLevel > 0. Wait until the worker is parked in listDir().
        QElapsedTimer waited;
        waited.start();
        while (!g_slowWorker.inListDir.load() && waited.elapsed() < 10000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        }
        // close() cannot wake the parked worker. With the fix kill() returns at once. With the
        // synchronous-join bug it blocks until the worker is released below - which never runs,
        // because control never returns from kill() - so the test deadlocks and times out.
        job->kill(KJob::EmitResult);
        killReturned = true;
        {
            QMutexLocker lock(&g_slowWorker.mutex);
            g_slowWorker.release = true;
        }
        g_slowWorker.cond.wakeAll();
        loop.quit();
    });
    QTimer::singleShot(std::chrono::seconds(30), &loop, &QEventLoop::quit); // safety net
    loop.exec();

    QVERIFY(g_slowWorker.inListDir.load()); // the worker really did reach (and park in) listDir()
    QVERIFY(killReturned);

    // The worker thread is reaped asynchronously (QThread::finished -> deleteLater). Now that it is
    // released, spin the loop so it finishes and is deleted before teardown (no false leak report).
    QTest::qWait(300);
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
