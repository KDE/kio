/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "kio/global.h"
#include "kio/listjob.h"
#include "kio/simplejob.h"
#include "kio/statjob.h"
#include "kio/transferjob.h"
#include "kio/udsentry.h"

// Exercises the out-of-process (socket) worker path against the real kio_test worker plugin
// (kiotest protocol), which - unlike the in-process setTestWorkerFactory mock - is spawned as a
// separate kioworker process.
class OutOfProcessWorkerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qputenv("KIOWORKER_ENABLE_TESTMODE", "1"); // the spawned worker enables test mode too
    }

    void statWorksOutOfProcess()
    {
        auto *job = KIO::stat(QUrl(QStringLiteral("kiotest://host/path")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(job->statResult().stringValue(KIO::UDSEntry::UDS_NAME), QStringLiteral("path"));
    }

    void getWorksOutOfProcess()
    {
        auto *job = KIO::get(QUrl(QStringLiteral("kiotest://host/file")), KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QByteArray received;
        connect(job, &KIO::TransferJob::data, this, [&received](KIO::Job *, const QByteArray &d) {
            received += d;
        });
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(received, QByteArray("hello"));
    }

    // A WorkerResult::fail() must propagate as the job's error over the socket path too.
    void failResultBecomesJobError()
    {
        auto *job = KIO::stat(QUrl(QStringLiteral("kiotest://fail/path")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(!job->exec(), "a fail() result must make the job fail");
        QCOMPARE(job->error(), int(KIO::ERR_ACCESS_DENIED));
        QVERIFY2(job->errorText().contains(QStringLiteral("mock denied")), qPrintable(job->errorText()));
    }

    // A large listing must arrive complete and across more than one MSG_LIST_ENTRIES batch
    // (SlaveBase::listEntry batching), and SlaveBase::finished() must synthesize a "." entry when
    // the worker did not list one.
    void listDirDeliversAllEntriesAndSynthesizesDot()
    {
        auto *job = KIO::listDir(QUrl(QStringLiteral("kiotest://many/dir")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        int fileEntries = 0;
        int dotEntries = 0;
        int batches = 0;
        connect(job, &KIO::ListJob::entries, this, [&](KIO::Job *, const KIO::UDSEntryList &list) {
            ++batches;
            for (const KIO::UDSEntry &e : list) {
                const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
                if (name == QLatin1String(".")) {
                    ++dotEntries;
                } else if (name.startsWith(QLatin1String("file"))) {
                    ++fileEntries;
                }
            }
        });
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(fileEntries, 250); // every entry delivered, none dropped at a batch boundary
        QVERIFY2(batches >= 2, "250 entries should span more than one batch");
        QCOMPARE(dotEntries, 1); // synthesized by finished() since the worker did not list "."
    }

    // When the worker lists "." itself, finished() must not add a second one.
    void listDirDoesNotDuplicateListedDot()
    {
        auto *job = KIO::listDir(QUrl(QStringLiteral("kiotest://withdot/dir")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        int dotEntries = 0;
        int fileEntries = 0;
        connect(job, &KIO::ListJob::entries, this, [&](KIO::Job *, const KIO::UDSEntryList &list) {
            for (const KIO::UDSEntry &e : list) {
                const QString name = e.stringValue(KIO::UDSEntry::UDS_NAME);
                if (name == QLatin1String(".")) {
                    ++dotEntries;
                } else if (name.startsWith(QLatin1String("file"))) {
                    ++fileEntries;
                }
            }
        });
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(dotEntries, 1); // the worker's own ".", not duplicated
        QCOMPARE(fileEntries, 3);
    }

    // A worker process that dies mid-job must surface as a clean job error, not a hang.
    void workerDeathMidJobYieldsCleanError()
    {
        auto *job = KIO::get(QUrl(QStringLiteral("kiotest://die/file")), KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QVERIFY2(!job->exec(), "a job whose worker died must fail");
        QCOMPARE(job->error(), int(KIO::ERR_WORKER_DIED));
    }

    // A worker parked with putOnHold() for a URL must be reused by a subsequent GET to the same URL
    // (Scheduler::heldWorkerForJob) and run to completion. Runs last: holding a worker mid-transfer
    // perturbs the pooled worker state, so it must not precede the other cases.
    void heldWorkerIsReusedForMatchingGet()
    {
        auto *job1 = KIO::get(QUrl(QStringLiteral("kiotest://slow/file")), KIO::NoReload, KIO::HideProgressInfo);
        job1->setUiDelegate(nullptr);
        QSignalSpy mimeSpy(job1, &KIO::TransferJob::mimeTypeFound);
        QVERIFY2(mimeSpy.wait(5000), "worker never sent mimetype - job1 was not dispatched");

        // The worker is mid-transfer and assigned to job1. Park it for this URL.
        job1->putOnHold();

        auto *job2 = KIO::get(QUrl(QStringLiteral("kiotest://slow/file")), KIO::NoReload, KIO::HideProgressInfo);
        job2->setUiDelegate(nullptr);
        QByteArray received;
        connect(job2, &KIO::TransferJob::data, this, [&received](KIO::Job *, const QByteArray &d) {
            received += d;
        });
        QSignalSpy resultSpy(job2, &KJob::result);
        QVERIFY2(resultSpy.wait(15000), "the GET reusing the held worker never finished");
        QCOMPARE(job2->error(), 0);
        QVERIFY2(!received.isEmpty(), "the reused worker delivered no data");

        KIO::SimpleJob::removeOnHold();
    }
};

QTEST_MAIN(OutOfProcessWorkerTest)

#include "outofprocessworkertest.moc"
