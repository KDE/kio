/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QThread>
#include <QTimer>

#include "kio/filecopyjob.h"
#include "kiotesthelper.h" // homeTmpDir, createTestFile etc.

class KIOThreadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void asyncConcurrentCopying();
    void copyJobFromThread();
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

QTEST_MAIN(KIOThreadTest)
#include "threadtest.moc"
