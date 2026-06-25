/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include <atomic>
#include <sys/stat.h>

#include "kio/filecopyjob.h"
#include "kio/filejob.h"
#include "kio/global.h"
#include "kio/mimetypejob.h"
#include "kio/mkdirjob.h"
#include "kio/simplejob.h"
#include "kio/statjob.h"
#include "kio/udsentry.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// Records which operations the worker received, so a test can confirm that a move whose worker has
// no rename() fell back to copy()+del().
std::atomic<bool> g_copyCalled{false};
std::atomic<bool> g_delCalled{false};

// A single mock worker for the kio-test protocol. Its behaviour is keyed off the request URL host
// rather than off which test created it, because the scheduler pools and reuses idle workers across
// jobs (a worker created for one test serves the next): a per-test factory would hand later tests a
// stale worker from an earlier one. With behaviour driven by the URL, worker reuse is harmless.
class TestWorker : public KIO::WorkerBase
{
public:
    TestWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        if (url.host() == QLatin1String("fail")) {
            return KIO::WorkerResult::fail(KIO::ERR_ACCESS_DENIED, QStringLiteral("mock denied"));
        }
        KIO::UDSEntry entry;
        entry.reserve(2);
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("path"));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult open(const QUrl &, QIODevice::OpenMode) override
    {
        totalSize(4);
        position(0);
        return KIO::WorkerResult::pass(); // bridge calls opened(), not finished()
    }

    KIO::WorkerResult read(KIO::filesize_t size) override
    {
        data(QByteArray("data", int(qMin<KIO::filesize_t>(size, 4))));
        return KIO::WorkerResult::pass(); // maybeError(): must not finish the job
    }

    KIO::WorkerResult close() override
    {
        return KIO::WorkerResult::pass(); // finalize(): finishes the job
    }

    KIO::WorkerResult get(const QUrl &) override
    {
        mimeType(QStringLiteral("text/plain"));
        data(QByteArray("hello"));
        data(QByteArray()); // EOF
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult copy(const QUrl &, const QUrl &, int, KIO::JobFlags) override
    {
        g_copyCalled.store(true, std::memory_order_relaxed);
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult del(const QUrl &, bool) override
    {
        g_delCalled.store(true, std::memory_order_relaxed);
        return KIO::WorkerResult::pass();
    }

    // mkdir(), symlink(), rename() and put() are intentionally NOT overridden: they exercise the
    // WorkerBase defaults, which return ERR_UNSUPPORTED_ACTION.
};

class TestWorkerFactory : public KIO::WorkerFactory
{
public:
    using KIO::WorkerFactory::WorkerFactory;
    std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        return std::unique_ptr<KIO::WorkerBase>(new TestWorker(pool, app));
    }
};
}

class WorkerBaseTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Kept alive for the whole test run: the worker machinery holds only a weak_ptr.
        m_factory = std::make_shared<TestWorkerFactory>();
        KIO::Worker::setTestWorkerFactory(m_factory);
    }

    // A WorkerResult::fail(code, text) returned from a worker virtual must surface as the job's
    // error()/errorText(), and finalize the command exactly once. The WorkerSlaveBaseBridge turns a
    // failed result into error() and a successful one into finished(); nothing else in the suite
    // pins this core error-propagation contract.
    void failResultBecomesJobError()
    {
        auto *job = KIO::stat(QUrl(QStringLiteral("kio-test://fail/path")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy resultSpy(job, &KJob::result);

        QVERIFY2(!job->exec(), "a fail() result must make the job fail");
        QCOMPARE(job->error(), int(KIO::ERR_ACCESS_DENIED));
        QVERIFY2(job->errorText().contains(QStringLiteral("mock denied")), qPrintable(job->errorText()));
        QCOMPARE(resultSpy.count(), 1); // finalized exactly once
    }

    // A WorkerResult::pass() must finish the job successfully, exactly once, and the data the worker
    // produced before returning (here a stat entry) must reach the job.
    void successResultFinishesJobOnce()
    {
        auto *job = KIO::stat(QUrl(QStringLiteral("kio-test://ok/path")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy resultSpy(job, &KJob::result);

        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(job->error(), 0);
        QCOMPARE(resultSpy.count(), 1);
        QCOMPARE(job->statResult().stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("path"));
    }

    // The bridge finalizes commands differently: open() reports opened() and read() uses
    // maybeError() (no finish on success), while close() finalizes with finished(). A read()
    // returning pass() inside an open session must therefore NOT finish the job - only close() does,
    // and exactly once.
    void readWithinOpenSessionDoesNotFinishUntilClose()
    {
        auto *job = KIO::open(QUrl(QStringLiteral("kio-test://file/x")), QIODevice::ReadOnly);
        job->setUiDelegate(nullptr);
        QSignalSpy resultSpy(job, &KJob::result);
        QSignalSpy openSpy(job, &KIO::FileJob::open);
        QSignalSpy dataSpy(job, &KIO::FileJob::data);

        QVERIFY2(openSpy.wait(5000), "the worker never reported the file as opened");
        QCOMPARE(resultSpy.count(), 0);

        job->read(4);
        QVERIFY2(dataSpy.wait(5000), "read() produced no data");
        QCOMPARE(resultSpy.count(), 0); // read() must not finalize the job

        job->close();
        QVERIFY2(resultSpy.wait(5000), "close() never finished the job");
        QCOMPARE(resultSpy.count(), 1); // close() finalizes exactly once
        QCOMPARE(job->error(), 0);
    }

    // A WorkerBase virtual the worker does not implement must fail with ERR_UNSUPPORTED_ACTION.
    // CopyJob/FileCopyJob fallbacks (rename -> copy+del, copy -> get+put) depend on exactly this
    // code, and a worker that "forgot" an operation must report failure rather than silent success.
    void unimplementedOperationsReturnUnsupportedAction()
    {
        auto *mkdirJob = KIO::mkdir(QUrl(QStringLiteral("kio-test://ok/newdir")));
        mkdirJob->setUiDelegate(nullptr);
        QVERIFY2(!mkdirJob->exec(), "the mkdir default must fail");
        QCOMPARE(mkdirJob->error(), int(KIO::ERR_UNSUPPORTED_ACTION));

        auto *symlinkJob = KIO::symlink(QStringLiteral("target"), QUrl(QStringLiteral("kio-test://ok/link")), KIO::HideProgressInfo);
        symlinkJob->setUiDelegate(nullptr);
        QVERIFY2(!symlinkJob->exec(), "the symlink default must fail");
        QCOMPARE(symlinkJob->error(), int(KIO::ERR_UNSUPPORTED_ACTION));
    }

    // A move whose worker does not implement rename() (default ERR_UNSUPPORTED_ACTION) must fall
    // back to copy()+del() and still succeed.
    void renameFallsBackToCopyAndDel()
    {
        g_copyCalled.store(false, std::memory_order_relaxed);
        g_delCalled.store(false, std::memory_order_relaxed);

        auto *job = KIO::file_move(QUrl(QStringLiteral("kio-test://ok/src")),
                                   QUrl(QStringLiteral("kio-test://ok/dst")),
                                   -1,
                                   KIO::HideProgressInfo | KIO::Overwrite);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QVERIFY2(g_copyCalled.load(std::memory_order_relaxed), "rename should have fallen back to copy()");
        QVERIFY2(g_delCalled.load(std::memory_order_relaxed), "the move should have deleted the source after copying");
    }

    // WorkerBase::mimetype() is the one default that delegates to get() instead of failing. A worker
    // implementing only get() must still answer a mimetype request.
    void mimetypeFallsBackToGet()
    {
        auto *job = KIO::mimetype(QUrl(QStringLiteral("kio-test://ok/file")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(job->mimetype(), QStringLiteral("text/plain"));
    }

private:
    std::shared_ptr<TestWorkerFactory> m_factory;
};

QTEST_MAIN(WorkerBaseTest)

#include "workerbasetest.moc"
