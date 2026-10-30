/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDataStream>
#include <QDateTime>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

#include "batchcopy_p.h"
#include "batchcopyjob_p.h"
#include "batchcopyworker.h"
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// What the mock worker answers, and what it received.
struct BatchWorkerControl {
    bool implementsBatchCopy = true;
    QList<KIO::WorkerResult> outcomes; // one per item, reported in order
    int batchError = 0; // what the batch fails with after the outcomes, 0 to pass
    QList<KIO::BatchCopyItem> received;
    KIO::JobFlags receivedFlags;
    void reset()
    {
        implementsBatchCopy = true;
        outcomes.clear();
        batchError = 0;
        received.clear();
        receivedFlags = {};
    }
};
BatchWorkerControl g_worker;

class PlainWorker : public KIO::WorkerBase
{
public:
    PlainWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
    {
    }
};

class BatchWorker : public KIO::WorkerBase, public KIO::BatchCopyWorker
{
public:
    BatchWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
    {
    }

    KIO::WorkerResult batchCopy(const QList<KIO::BatchCopyItem> &items, KIO::JobFlags flags) override
    {
        g_worker.received = items;
        g_worker.receivedFlags = flags;
        for (int i = 0; i < g_worker.outcomes.size(); ++i) {
            batchCopyItemFinished(i, g_worker.outcomes.at(i));
        }
        if (g_worker.batchError) {
            return KIO::WorkerResult::fail(g_worker.batchError, QStringLiteral("destination"));
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

QList<KIO::BatchCopyItem> threeItems()
{
    QList<KIO::BatchCopyItem> items;
    for (int i = 0; i < 3; ++i) {
        const QString name = QStringLiteral("file%1.txt").arg(i);
        items.append({QUrl::fromLocalFile(QStringLiteral("/source/") + name),
                      QUrl(QStringLiteral("kio-test://host/destination/") + name),
                      0644,
                      QDateTime::fromSecsSinceEpoch(1700000000 + i, QTimeZone::UTC),
                      KIO::filesize_t(1000 + i)});
    }
    return items;
}
}

class BatchCopyWorkerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        m_factory = std::make_shared<Factory>();
        KIO::Worker::setTestWorkerFactory(m_factory);
    }

    void init()
    {
        g_worker.reset();
    }

    // Each outcome the worker reports reaches the job with its index, and the worker gets the items
    // the job sent, without the flags that only concern the job.
    void reportsTheOutcomeOfEachItem()
    {
        g_worker.outcomes = {KIO::WorkerResult::pass(),
                             KIO::WorkerResult::fail(KIO::ERR_FILE_ALREADY_EXIST, QStringLiteral("file1.txt")),
                             KIO::WorkerResult::fail(KIO::ERR_CANNOT_OPEN_FOR_READING, QStringLiteral("file2.txt"))};

        auto *job = KIO::batchCopy(QUrl(QStringLiteral("kio-test://host/destination")), threeItems(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy items(job, &KIO::BatchCopyJob::itemFinished);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QCOMPARE(g_worker.received, threeItems());
        QCOMPARE(g_worker.receivedFlags, KIO::JobFlags{});
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.at(0), (QList<QVariant>{0, 0, QString()}));
        QCOMPARE(items.at(1), (QList<QVariant>{1, int(KIO::ERR_FILE_ALREADY_EXIST), QStringLiteral("file1.txt")}));
        QCOMPARE(items.at(2), (QList<QVariant>{2, int(KIO::ERR_CANNOT_OPEN_FOR_READING), QStringLiteral("file2.txt")}));
    }

    // An error that stops the batch fails the job, and the items reported before it still arrive.
    void anErrorThatStopsTheBatchFailsTheJob()
    {
        g_worker.outcomes = {KIO::WorkerResult::pass()};
        g_worker.batchError = KIO::ERR_DISK_FULL;

        auto *job = KIO::batchCopy(QUrl(QStringLiteral("kio-test://host/destination")), threeItems(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy items(job, &KIO::BatchCopyJob::itemFinished);
        QVERIFY(!job->exec());

        QCOMPARE(job->error(), KIO::ERR_DISK_FULL);
        QCOMPARE(items.size(), 1);
    }

    // A worker without KIO::BatchCopyWorker fails the command as unsupported, so the job can copy
    // the files one by one.
    void aWorkerWithoutTheInterfaceIsUnsupported()
    {
        // The scheduler keeps one worker per protocol, so a protocol that no other test uses gets a
        // worker created now, without the interface.
        g_worker.implementsBatchCopy = false;

        auto *job = KIO::batchCopy(QUrl(QStringLiteral("kio-test-plain://host/destination")), threeItems(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY(!job->exec());

        QCOMPARE(job->error(), KIO::ERR_UNSUPPORTED_ACTION);
    }

    // A request with more items than the cap, or cut short, reads as corrupt instead of as a
    // shorter list.
    void aMalformedRequestReadsAsCorrupt()
    {
        QByteArray tooMany;
        {
            QDataStream stream(&tooMany, QIODevice::WriteOnly);
            stream << qint32(0) << qint32(KIO::s_maxBatchCopyItems + 1);
        }
        KIO::BatchCopyRequest request;
        QDataStream readTooMany(tooMany);
        readTooMany >> request;
        QVERIFY(readTooMany.status() != QDataStream::Ok);

        QByteArray full;
        {
            QDataStream stream(&full, QIODevice::WriteOnly);
            stream << KIO::BatchCopyRequest{threeItems(), KIO::DefaultFlags};
        }
        QDataStream readTruncated(full.left(full.size() - 2));
        readTruncated >> request;
        QVERIFY(readTruncated.status() != QDataStream::Ok);
        QVERIFY(request.items.isEmpty());
    }

private:
    std::shared_ptr<Factory> m_factory;
};

QTEST_GUILESS_MAIN(BatchCopyWorkerTest)

#include "batchcopyworkertest.moc"
