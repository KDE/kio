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
#include "worker_p.h" // KIO::Worker::setTestWorkerFactory
#include "workerbase.h"
#include "workerfactory.h"

namespace
{
// What the mock worker answers, and what it saw.
struct FreeSpaceWorkerControl {
    KIO::filesize_t total = 0;
    KIO::filesize_t available = 0;
    bool freeSpaceIsSupported = true;
    KIO::filesize_t bytesWritten = 0;
    bool wrotePastTheCheck = false;
    void reset()
    {
        total = 0;
        available = 0;
        freeSpaceIsSupported = true;
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
        if (!g_worker.freeSpaceIsSupported) {
            return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, url.toDisplayString());
        }

        setMetaData(QStringLiteral("total"), QString::number(g_worker.total));
        setMetaData(QStringLiteral("available"), QString::number(g_worker.available));
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

    // A destination that reports no free space at all is one that has no figure to give, and the
    // file is written rather than the copy being refused before it starts. See bug 431050.
    void copiesToADestinationReportingNoFreeSpace()
    {
        g_worker.total = 1024 * 1024;
        g_worker.available = 0;

        auto *job = KIO::copy(m_sourceFile, destination(), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy finished(job, &KJob::result);
        QVERIFY(finished.wait());

        QVERIFY2(job->error() != KIO::ERR_DISK_FULL, "the copy was refused over a free space figure the destination never gave");
        QCOMPARE(job->error(), KJob::NoError);
        QVERIFY(g_worker.wrotePastTheCheck);
        QCOMPARE(g_worker.bytesWritten, static_cast<KIO::filesize_t>(s_fileContents.size()));
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

    // A destination that cannot answer at all is copied to as well.
    void copiesToADestinationThatCannotReportFreeSpace()
    {
        g_worker.freeSpaceIsSupported = false;

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
