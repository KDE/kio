/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>

#include "batchcopyworker.h"
#include "kio/copyjob.h"
#include "kioglobal_p.h" // S_IRWXU and the rest of the mode bits Windows does not have
#include "kprotocolmanager_p.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// What the mock worker answers, and what it received.
struct BatchWorkerControl {
    bool implementsBatchCopy = true;
    QList<KIO::WorkerResult> outcomes; // one per item of the first batch, reported in order
    int batchError = 0; // what the batch fails with after the outcomes, 0 to pass
    int reportedItems = -1; // how many items of the first batch get an outcome, -1 for all
    QList<int> extraReports; // indices reported as copied after the items of the first batch
    bool waitsForKill = false; // the first batch runs until the job is killed
    std::atomic<bool> startedBatch{false};
    std::atomic<bool> sawKilled{false};
    QHash<QString, int> putErrors; // file name -> what writing it one by one fails with
    qint64 available = -1; // free space of the destination, -1 when it cannot tell
    QList<QList<KIO::BatchCopyItem>> batches;
    QStringList puts; // file names written one by one
    void reset()
    {
        implementsBatchCopy = true;
        outcomes.clear();
        batchError = 0;
        reportedItems = -1;
        extraReports.clear();
        waitsForKill = false;
        startedBatch = false;
        sawKilled = false;
        putErrors.clear();
        available = -1;
        batches.clear();
        puts.clear();
    }
};

constexpr int s_killTimeoutMs = 5000;
BatchWorkerControl g_worker;

// A destination that answers stat and accepts writes one file at a time.
class PlainWorker : public KIO::WorkerBase
{
public:
    PlainWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test-batch"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        // Everything without a file extension stands for a directory, the files to be written do
        // not exist yet.
        if (url.fileName().contains(QLatin1Char('.'))) {
            return KIO::WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
        }

        KIO::UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, url.fileName());
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRWXU);
        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult fileSystemFreeSpace(const QUrl &url) override
    {
        if (g_worker.available < 0) {
            return KIO::WorkerBase::fileSystemFreeSpace(url);
        }
        setMetaData(QStringLiteral("total"), QString::number(1024 * 1024));
        setMetaData(QStringLiteral("available"), QString::number(g_worker.available));
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult put(const QUrl &url, int, KIO::JobFlags) override
    {
        if (const int error = g_worker.putErrors.value(url.fileName())) {
            return KIO::WorkerResult::fail(error, url.fileName());
        }
        for (;;) {
            QByteArray buffer;
            dataReq();
            if (readData(buffer) <= 0) {
                break;
            }
        }
        g_worker.puts.append(url.fileName());
        return KIO::WorkerResult::pass();
    }
};

class BatchWorker : public PlainWorker, public KIO::BatchCopyWorker
{
public:
    using PlainWorker::PlainWorker;

    KIO::WorkerResult batchCopy(const QList<KIO::BatchCopyItem> &items, KIO::JobFlags) override
    {
        g_worker.batches.append(items);
        const bool firstBatch = g_worker.batches.size() == 1;
        if (firstBatch && g_worker.waitsForKill) {
            g_worker.startedBatch = true;
            QElapsedTimer elapsed;
            elapsed.start();
            while (!wasKilled() && elapsed.elapsed() < s_killTimeoutMs) {
                QThread::msleep(5);
            }
            g_worker.sawKilled = wasKilled();
            return KIO::WorkerResult::fail(KIO::ERR_USER_CANCELED, QString());
        }

        const QList<KIO::WorkerResult> outcomes = firstBatch ? g_worker.outcomes : QList<KIO::WorkerResult>();
        const int reported = firstBatch && g_worker.reportedItems >= 0 ? g_worker.reportedItems : items.size();
        for (int i = 0; i < reported; ++i) {
            batchCopyItemFinished(i, i < outcomes.size() ? outcomes.at(i) : KIO::WorkerResult::pass());
            if (g_worker.batchError && i + 1 == outcomes.size()) {
                return KIO::WorkerResult::fail(g_worker.batchError, items.at(i).destination.toDisplayString());
            }
        }
        if (firstBatch) {
            for (const int index : std::as_const(g_worker.extraReports)) {
                batchCopyItemFinished(index, KIO::WorkerResult::pass());
            }
        }
        return KIO::WorkerResult::pass();
    }
};

class Factory : public KIO::WorkerFactory
{
public:
    using KIO::WorkerFactory::WorkerFactory;
    std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
    {
        if (g_worker.implementsBatchCopy) {
            return std::make_unique<BatchWorker>(pool, app);
        }
        return std::make_unique<PlainWorker>(pool, app);
    }
};
}

class CopyJobBatchCopyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        m_factory = std::make_shared<Factory>();
        KIO::Worker::setTestWorkerFactory(m_factory);

        QVERIFY(m_sourceDir.isValid());
        for (int i = 0; i < 3; ++i) {
            const QString path = m_sourceDir.filePath(QStringLiteral("file%1.txt").arg(i));
            QFile source(path);
            QVERIFY(source.open(QIODevice::WriteOnly));
            QVERIFY(source.write("Some bytes to copy over.") > 0);
            m_sources.append(QUrl::fromLocalFile(path));
        }
    }

    void init()
    {
        g_worker.reset();
        KProtocolManagerPrivate::setBatchCopyFromFileForTesting(QStringLiteral("kio-test-batch"));
    }

    void cleanupTestCase()
    {
        KProtocolManagerPrivate::setBatchCopyFromFileForTesting(QString());
    }

    // A run of files goes to the worker as one command, and each file it copied is reported.
    void copiesARunOfFilesInOneCommand()
    {
        const QUrl destination(QStringLiteral("kio-test-batch://host/destination"));
        auto *job = KIO::copy(m_sources, destination, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 1);
        const QList<KIO::BatchCopyItem> items = g_worker.batches.constFirst();
        QCOMPARE(items.size(), 3);
        for (int i = 0; i < 3; ++i) {
            QCOMPARE(items.at(i).source, m_sources.at(i));
            QCOMPARE(items.at(i).destination.path(), QStringLiteral("/destination/file%1.txt").arg(i));
            const QFileInfo source(m_sources.at(i).toLocalFile());
            QCOMPARE(items.at(i).modificationTime.toSecsSinceEpoch(), source.lastModified().toSecsSinceEpoch());
            QCOMPARE(items.at(i).size, KIO::filesize_t(source.size()));
        }
        QVERIFY(g_worker.puts.isEmpty());
        QCOMPARE(done.size(), 3);
        QCOMPARE(job->processedAmount(KJob::Files), 3);
    }

    // A file the batch did not copy is copied on its own afterwards.
    void copiesAFileTheBatchFailedOnItsOwn()
    {
        g_worker.outcomes = {KIO::WorkerResult::pass(),
                             KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, QStringLiteral("file1.txt")),
                             KIO::WorkerResult::pass()};

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 1);
        QCOMPARE(g_worker.puts, QStringList{QStringLiteral("file1.txt")});
        QCOMPARE(done.size(), 3);
        QCOMPARE(job->processedAmount(KJob::Files), 3);
    }

    // A file the batch did not copy does not stop the next batches.
    void keepsBatchingAfterAFileTheBatchFailed()
    {
        QTemporaryDir sourceDir;
        const QList<QUrl> sources = moreThanOneBatch(sourceDir);
        QVERIFY(!sources.isEmpty());
        g_worker.outcomes = {KIO::WorkerResult::pass(), KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, QStringLiteral("many1.txt"))};

        auto *job = KIO::copy(sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 2);
        QCOMPARE(g_worker.batches.at(0).size() + g_worker.batches.at(1).size(), 514);
        QCOMPARE(g_worker.puts, QStringList{QStringLiteral("many1.txt")});
        QCOMPARE(done.size(), 514);
    }

    // The batch only creates new files, so a copy that overwrites goes one file at a time.
    void copiesOneByOneWhenOverwriting()
    {
        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo | KIO::Overwrite);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QVERIFY(g_worker.batches.isEmpty());
        QCOMPARE(g_worker.puts.size(), 3);
    }

    // When the copy of a file the batch did not copy fails as well, that error is the one the job
    // ends with, as on the per-file path.
    void failsWithTheErrorOfTheFileCopiedOnItsOwn_data()
    {
        QTest::addColumn<int>("batchError");
        QTest::addColumn<int>("putError");

        QTest::newRow("conflict") << int(KIO::ERR_FILE_ALREADY_EXIST) << int(KIO::ERR_FILE_ALREADY_EXIST);
        QTest::newRow("error") << int(KIO::ERR_CANNOT_OPEN_FOR_READING) << int(KIO::ERR_CANNOT_WRITE);
    }

    void failsWithTheErrorOfTheFileCopiedOnItsOwn()
    {
        QFETCH(int, batchError);
        QFETCH(int, putError);
        g_worker.outcomes = {KIO::WorkerResult::pass(), KIO::WorkerResult::fail(batchError, QStringLiteral("file1.txt"))};
        g_worker.putErrors.insert(QStringLiteral("file1.txt"), putError);

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY(!job->exec());

        QCOMPARE(job->error(), putError);
        QVERIFY(g_worker.puts.isEmpty());
        QCOMPARE(done.size(), 2);
    }

    // The files the worker gave no outcome for, although the batch passed, are copied on their own.
    void copiesTheFilesWithoutAnOutcomeOnTheirOwn()
    {
        g_worker.reportedItems = 1;

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 1);
        QCOMPARE(g_worker.puts, (QStringList{QStringLiteral("file1.txt"), QStringLiteral("file2.txt")}));
        QCOMPARE(done.size(), 3);
    }

    // An index outside the batch, or a second outcome for a file, reports nothing.
    void ignoresAnIndexOutsideTheBatchAndASecondOutcome()
    {
        g_worker.extraReports = {3, 99, -1, 0};

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QVERIFY(g_worker.puts.isEmpty());
        QCOMPARE(done.size(), 3);
        QCOMPARE(job->processedAmount(KJob::Files), 3);
    }

    // A copy that does not fit in the free space of the destination stops before any batch.
    void sendsNoBatchForACopyThatDoesNotFit()
    {
        // Each source holds 24 bytes.
        g_worker.available = 50;

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());

        QCOMPARE(job->error(), KIO::ERR_DISK_FULL);
        QVERIFY(g_worker.batches.isEmpty());
        QVERIFY(g_worker.puts.isEmpty());
    }

    // A cancel that the worker reports ends the job, without copying the rest one by one.
    void endsTheJobWhenTheWorkerReportsACancel()
    {
        g_worker.outcomes = {KIO::WorkerResult::pass()};
        g_worker.batchError = KIO::ERR_USER_CANCELED;

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY(!job->exec());

        QCOMPARE(job->error(), KIO::ERR_USER_CANCELED);
        QVERIFY(g_worker.puts.isEmpty());
        QCOMPARE(done.size(), 1);
    }

    // Killing the job reaches the worker in the middle of a batch, and nothing is copied after.
    void killingTheJobStopsTheBatch()
    {
        g_worker.waitsForKill = true;

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy result(job, &KJob::result);
        job->start();
        QTRY_VERIFY_WITH_TIMEOUT(g_worker.startedBatch.load(), s_killTimeoutMs);

        job->kill(KJob::EmitResult);
        QCOMPARE(result.size(), 1);
        QCOMPARE(job->error(), KIO::ERR_USER_CANCELED);
        QTRY_VERIFY_WITH_TIMEOUT(g_worker.sawKilled.load(), s_killTimeoutMs);
        QVERIFY(g_worker.puts.isEmpty());
    }

    // After an error that stops a batch, the rest of the job goes one file at a time.
    void stopsBatchingAfterAnErrorThatStoppedABatch()
    {
        QTemporaryDir sourceDir;
        const QList<QUrl> sources = moreThanOneBatch(sourceDir);
        QVERIFY(!sources.isEmpty());
        // The error comes near the end of the first batch, so that few files are left to copy one
        // by one.
        g_worker.outcomes = QList<KIO::WorkerResult>(511, KIO::WorkerResult::pass());
        g_worker.batchError = KIO::ERR_CANNOT_WRITE;

        auto *job = KIO::copy(sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 1);
        QCOMPARE(g_worker.puts, (QStringList{QStringLiteral("many511.txt"), QStringLiteral("many512.txt"), QStringLiteral("many513.txt")}));
        QCOMPARE(done.size(), sources.size());
    }

    // After an error that stops the batch, the files it did not reach are copied one by one.
    void copiesTheRestOneByOneAfterTheBatchStops()
    {
        g_worker.outcomes = {KIO::WorkerResult::pass()};
        g_worker.batchError = KIO::ERR_CANNOT_WRITE;

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.batches.size(), 1);
        QCOMPARE(g_worker.puts, (QStringList{QStringLiteral("file1.txt"), QStringLiteral("file2.txt")}));
        QCOMPARE(done.size(), 3);
    }

    // A worker without KIO::BatchCopyWorker gets every file one by one.
    void copiesOneByOneWithAWorkerWithoutTheCommand()
    {
        // The scheduler keeps one worker per protocol, so a protocol that no other test uses gets a
        // worker created now, without the interface.
        g_worker.implementsBatchCopy = false;
        KProtocolManagerPrivate::setBatchCopyFromFileForTesting(QStringLiteral("kio-test-batch-plain"));

        auto *job = KIO::copy(m_sources, QUrl(QStringLiteral("kio-test-batch-plain://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy done(job, &KIO::CopyJob::copyingDone);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.puts, (QStringList{QStringLiteral("file0.txt"), QStringLiteral("file1.txt"), QStringLiteral("file2.txt")}));
        QCOMPARE(done.size(), 3);
    }

    // A single file is copied on its own.
    void copiesASingleFileOnItsOwn()
    {
        auto *job = KIO::copy(m_sources.constFirst(), QUrl(QStringLiteral("kio-test-batch://host/destination")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QVERIFY(g_worker.batches.isEmpty());
        QCOMPARE(g_worker.puts, QStringList{QStringLiteral("file0.txt")});
    }

private:
    // Empty files, one more batch than the cap of a batch holds. Empty when they cannot be created.
    static QList<QUrl> moreThanOneBatch(const QTemporaryDir &dir)
    {
        QList<QUrl> sources;
        if (!dir.isValid()) {
            return sources;
        }
        for (int i = 0; i < 514; ++i) {
            const QString path = dir.filePath(QStringLiteral("many%1.txt").arg(i));
            QFile source(path);
            if (!source.open(QIODevice::WriteOnly)) {
                return {};
            }
            sources.append(QUrl::fromLocalFile(path));
        }
        return sources;
    }

    std::shared_ptr<Factory> m_factory;
    QTemporaryDir m_sourceDir;
    QList<QUrl> m_sources;
};

QTEST_GUILESS_MAIN(CopyJobBatchCopyTest)

#include "copyjobbatchcopytest.moc"
