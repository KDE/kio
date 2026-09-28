/*
    SPDX-FileCopyrightText: 2015 Martin Blumenstingl <martin.blumenstingl@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "deletejobtest.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>
#include <QTimer>
#include <QUrl>

#include <kio/deletejob.h>
#include <kio/simplejob.h>

#include <unistd.h>

#if defined(WITH_QTDBUS)
#include <QDBusConnection>

#include <KDirNotify>
#endif

QTEST_MAIN(DeleteJobTest)

static constexpr int KJOB_NO_ERROR = static_cast<int>(KJob::NoError);

void DeleteJobTest::deleteFileTestCase_data() const
{
    QTest::addColumn<QString>("fileName");

    QTest::newRow("latin characters") << "testfile";
    QTest::newRow("german umlauts") << "testger\u00E4t";
    QTest::newRow("chinese characters") << "\u8A66";
}

void DeleteJobTest::deleteFileTestCase()
{
    QFETCH(QString, fileName);

    QTemporaryFile tempFile;
    tempFile.setFileTemplate(fileName + QStringLiteral("XXXXXX"));

    QVERIFY(tempFile.open());
    const QString path = tempFile.fileName();
    tempFile.close();
    QVERIFY(QFile::exists(path));

    /*QBENCHMARK*/ {
        KIO::DeleteJob *job = KIO::del(QUrl::fromLocalFile(path), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);

        QSignalSpy spy(job, &KJob::result);
        QVERIFY(spy.isValid());
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), KJOB_NO_ERROR);
        QVERIFY(!QFile::exists(path));
    }
}

void DeleteJobTest::deleteDirectoryTestCase_data() const
{
    QTest::addColumn<QStringList>("fileNames");

    QTest::newRow("non-empty directory") << QStringList{QStringLiteral("1.txt")};
    QTest::newRow("empty directory") << QStringList();
}

void DeleteJobTest::deleteDirectoryTestCase()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QFETCH(QStringList, fileNames);

    createTestFiles(fileNames, tempDir.path(), 8);

    /*QBENCHMARK*/ {
        KIO::DeleteJob *job = KIO::del(QUrl::fromLocalFile(tempDir.path()), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);

        qulonglong reportedBytes = 0;
        connect(job, &KJob::processedAmountChanged, job, [&](KJob *, KJob::Unit unit, qulonglong amount) {
            if (unit == KJob::Bytes) {
                reportedBytes = amount;
            }
        });

        QSignalSpy spy(job, &KJob::result);
        QVERIFY(spy.isValid());
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), KJOB_NO_ERROR);
        QVERIFY(!QDir(tempDir.path()).exists());
        // What the worker deleted is what it said it deleted, the directory itself besides the files.
        QVERIFY2(reportedBytes >= qulonglong(8 * fileNames.size()),
                 qPrintable(QStringLiteral("reported %1 bytes for %2 files").arg(reportedBytes).arg(fileNames.size())));
    }
}

void DeleteJobTest::deletePartialFailureNotifiesRemovals()
{
#if !defined(WITH_QTDBUS)
    QSKIP("Removal notifications are delivered over D-Bus, which is unavailable in this build");
#else
    if (geteuid() == 0) {
        QSKIP("Root ignores the permission bits this test relies on");
    }

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString base = tempDir.path();

    // A file that can be deleted.
    const QString deletable = base + QLatin1String("/possible.txt");
    QFile deletableFile(deletable);
    QVERIFY(deletableFile.open(QIODevice::WriteOnly));
    deletableFile.close();

    // An empty directory whose parent denies write access, so removing it fails
    // with a permission error. The failure happens in the directory phase, which
    // runs after all files have already been deleted, so possible.txt is gone by
    // the time the job errors out regardless of the order entries were stated in.
    const QString locked = base + QLatin1String("/locked");
    const QString undeletableDir = locked + QLatin1String("/sub");
    QVERIFY(QDir().mkpath(undeletableDir));
    QVERIFY(QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::ExeOwner));

    org::kde::KDirNotify notifyListener(QString(), QString(), QDBusConnection::sessionBus(), this);
    QSignalSpy removedSpy(&notifyListener, &org::kde::KDirNotify::FilesRemoved);
    QVERIFY(removedSpy.isValid());

    const QList<QUrl> urls{QUrl::fromLocalFile(deletable), QUrl::fromLocalFile(undeletableDir)};
    KIO::DeleteJob *job = KIO::del(urls, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    QSignalSpy resultSpy(job, &KJob::result);
    QVERIFY(resultSpy.wait(100000));

    // The job fails overall, but the file was really removed from storage while
    // the protected directory was left in place.
    QVERIFY(job->error() != KJOB_NO_ERROR);
    QVERIFY(!QFile::exists(deletable));
    QVERIFY(QDir(undeletableDir).exists());

    // The removal of the file is announced, so dir listers stop showing it.
    QVERIFY(removedSpy.count() > 0 || removedSpy.wait(5000));
    bool deletableReported = false;
    for (const auto &emission : std::as_const(removedSpy)) {
        const QStringList removed = emission.at(0).toStringList();
        for (const QString &url : removed) {
            if (QUrl(url) == QUrl::fromLocalFile(deletable)) {
                deletableReported = true;
            }
        }
    }
    QVERIFY(deletableReported);

    // Let QTemporaryDir clean up.
    QFile::setPermissions(locked, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
#endif
}

void DeleteJobTest::createTestFiles(const QStringList &fileNames, const QString &path, qint64 bytesEach) const
{
    QStringListIterator iterator(fileNames);
    while (iterator.hasNext()) {
        const QString filename = path + QDir::separator() + iterator.next();
        QFile file(filename);
        QVERIFY(file.open(QIODevice::WriteOnly));
        if (bytesEach > 0) {
            QCOMPARE(file.write(QByteArray(bytesEach, 'x')), bytesEach);
        }
    }

    QCOMPARE(QDir(path).entryList(QDir::Files).count(), fileNames.size());
}

static int countFiles(const QString &dir)
{
    int n = 0;
    QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}

void DeleteJobTest::killedRecursiveDeletionStopsEarly()
{
    // A cancelled recursive deletion must stop promptly instead of emptying the whole tree.
    // kio_file's deleteRecursive() now polls wasKilled(). Without that poll the worker deletes
    // everything before noticing the kill. rmdir() + the "recurse" metadata sends the deletion
    // straight to the worker (KIO::del would enumerate the tree first, so a cancel there would not
    // exercise deleteRecursive()). Build a tree large enough that the deletion is still running
    // when we cancel it.
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QString root = tempDir.path() + QStringLiteral("/tree");
    QVERIFY(QDir().mkpath(root));

    constexpr int subdirectories = 50;
    int created = 0;
    for (int d = 0; d < subdirectories; ++d) {
        const QString sub = root + QStringLiteral("/sub%1").arg(d);
        QVERIFY(QDir().mkpath(sub));
        for (int f = 0; f < 300; ++f) {
            QFile file(sub + QStringLiteral("/f%1").arg(f));
            QVERIFY(file.open(QIODevice::WriteOnly));
            // A byte apiece, so that the worker has something to count as it removes them.
            QCOMPARE(file.write("x", 1), 1);
            ++created;
        }
    }

    // The deletion reports the bytes it frees as it goes, and SlaveBase lets a report through only
    // once 100 ms have passed since the last one. This worker served the tests before, so the
    // window is given time to clear before the job starts or there is nothing to count.
    QTest::qWait(150);

    KIO::SimpleJob *job = KIO::rmdir(QUrl::fromLocalFile(root));
    job->setUiDelegate(nullptr);
    job->addMetaData(QStringLiteral("recurse"), QStringLiteral("true"));

    int byteReports = 0;
    connect(job, &KJob::processedAmountChanged, job, [&](KJob *, KJob::Unit unit, qulonglong amount) {
        if (unit == KJob::Bytes && amount > 0) {
            ++byteReports;
        }
    });

    // A subdirectory goes once it is emptied, so one missing says the deletion is under way and a
    // kill now lands inside deleteRecursive(). The tree is watched rather than the progress the
    // worker reports, which SlaveBase holds back within 100 ms of the last one it let through.
    bool killed = false;
    QTimer watcher;
    connect(&watcher, &QTimer::timeout, job, [&]() {
        if (!killed && QDir(root).entryList(QDir::Dirs | QDir::NoDotAndDotDot).count() < subdirectories) {
            killed = true;
            job->kill(KJob::EmitResult);
        }
    });
    watcher.start(1);

    QSignalSpy spy(job, &KJob::result);
    // kill(EmitResult) makes the job report almost immediately. A timeout here means the deletion
    // never started (so it was never cancelled) or the job did not finish at all.
    QVERIFY(spy.wait(10000));
    QVERIFY2(byteReports > 0, "the deletion freed bytes without reporting any");
    QVERIFY2(killed, qPrintable(QStringLiteral("no progress reported, so nothing was cancelled. Job error %1: %2").arg(job->error()).arg(job->errorString())));

    // The job reports at once, but the worker runs on until deleteRecursive() returns. Wait for
    // the count to settle, so that what is checked is where the worker stopped.
    int remaining = countFiles(root);
    int previous = -1;
    QElapsedTimer settle;
    settle.start();
    while (remaining != previous && settle.elapsed() < 30000) {
        previous = remaining;
        QTest::qWait(200);
        remaining = countFiles(root);
    }

    QVERIFY2(remaining > 0, qPrintable(QStringLiteral("expected files to remain after cancel, %1 of %2 left").arg(remaining).arg(created)));
}

#include "moc_deletejobtest.cpp"
