/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "kio/copyjob.h"
#include "kio/filesystemfreespacejob.h"
#include "kioglobal_p.h" // S_IRWXU and the rest of the mode bits Windows does not have
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// What the mock worker answers, and what it saw.
struct FreeSpaceWorkerControl {
    KIO::filesize_t total = 0;
    KIO::filesize_t available = 0;
    int freeSpaceError = 0; // what the worker fails the request with, 0 to let it answer
    bool answersAFigure = true; // whether that answer carries the sizes
    KIO::filesize_t bytesWritten = 0;
    bool wrotePastTheCheck = false;
    void reset()
    {
        total = 0;
        available = 0;
        freeSpaceError = 0;
        answersAFigure = true;
        bytesWritten = 0;
        wrotePastTheCheck = false;
    }
};
FreeSpaceWorkerControl g_worker;

const QByteArray s_fileContents = QByteArrayLiteral("Some bytes to copy over.");

// A worker for a destination that answers stat, reports whatever free space the test asks it to,
// and accepts writes.
class FreeSpaceWorker : public KIO::WorkerBase
{
public:
    FreeSpaceWorker(const QByteArray &pool, const QByteArray &app)
        : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        // Everything without a file extension stands for a directory, the file to be written does
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
        if (g_worker.freeSpaceError) {
            return KIO::WorkerResult::fail(g_worker.freeSpaceError, url.toDisplayString());
        }

        if (g_worker.answersAFigure) {
            setMetaData(QStringLiteral("total"), QString::number(g_worker.total));
            setMetaData(QStringLiteral("available"), QString::number(g_worker.available));
        }
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult put(const QUrl &, int, KIO::JobFlags) override
    {
        g_worker.wrotePastTheCheck = true;
        for (;;) {
            QByteArray buffer;
            dataReq();
            const int read = readData(buffer);
            if (read <= 0) {
                break;
            }
            g_worker.bytesWritten += buffer.size();
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
        return std::unique_ptr<KIO::WorkerBase>(new FreeSpaceWorker(pool, app));
    }
};
}

class CopyJobFreeSpaceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        m_factory = std::make_shared<Factory>();
        KIO::Worker::setTestWorkerFactory(m_factory);

        QVERIFY(m_sourceDir.isValid());
        m_sourceFile = QUrl::fromLocalFile(m_sourceDir.filePath(QStringLiteral("source.txt")));
        QFile source(m_sourceFile.toLocalFile());
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(s_fileContents), s_fileContents.size());
    }

    void init()
    {
        g_worker.reset();
    }

    // A worker that answers without a figure cannot measure the filesystem, and the sizes stay at
    // the invalid value instead of reading as a filesystem with no room left.
    void reportsAFilesystemItCannotMeasureAsUnsupported()
    {
        g_worker.answersAFigure = false;

        auto *job = KIO::fileSystemFreeSpace(destination());
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QCOMPARE(job->error(), KIO::ERR_UNSUPPORTED_ACTION);
        QCOMPARE(job->size(), static_cast<KIO::filesize_t>(-1));
        QCOMPARE(job->availableSize(), static_cast<KIO::filesize_t>(-1));
    }

    // A destination that fails the request says why, and that reason is what the job reports.
    void keepsTheErrorTheDestinationGave()
    {
        g_worker.freeSpaceError = KIO::ERR_ACCESS_DENIED;

        auto *job = KIO::fileSystemFreeSpace(destination());
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QCOMPARE(job->error(), KIO::ERR_ACCESS_DENIED);
        QCOMPARE(job->availableSize(), static_cast<KIO::filesize_t>(-1));
    }

    // A zero from a worker that did answer is an answer: the filesystem has no room left.
    void refusesACopyToADestinationThatReportsNoRoomLeft()
    {
        g_worker.total = 1024 * 1024;
        g_worker.available = 0;

        auto *job = KIO::copy(m_sourceFile, destination(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QCOMPARE(job->error(), KIO::ERR_DISK_FULL);
        QVERIFY(!g_worker.wrotePastTheCheck);
    }

    // A destination that does report its free space is still taken at its word.
    void refusesACopyThatDoesNotFitInTheReportedFreeSpace()
    {
        g_worker.total = 1024 * 1024;
        g_worker.available = 1;

        auto *job = KIO::copy(m_sourceFile, destination(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QCOMPARE(job->error(), KIO::ERR_DISK_FULL);
        QVERIFY(!g_worker.wrotePastTheCheck);
    }

    // A destination that cannot answer is copied to, and the write is what decides whether the
    // room is there. See bug 431050.
    void copiesToADestinationThatCannotReportFreeSpace()
    {
        g_worker.freeSpaceError = KIO::ERR_UNSUPPORTED_ACTION;

        auto *job = KIO::copy(m_sourceFile, destination(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QCOMPARE(job->error(), KJob::NoError);
        QCOMPARE(g_worker.bytesWritten, static_cast<KIO::filesize_t>(s_fileContents.size()));
    }

private:
    QUrl destination() const
    {
        return QUrl(QStringLiteral("kio-test://host/destination"));
    }

    std::shared_ptr<Factory> m_factory;
    QTemporaryDir m_sourceDir;
    QUrl m_sourceFile;
};

QTEST_MAIN(CopyJobFreeSpaceTest)

#include "copyjobfreespacetest.moc"
