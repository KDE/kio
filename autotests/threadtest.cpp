/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004-2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include <QThreadPool>
#include <QtConcurrentRun>

#include <kio/job.h>
#include "kiotesthelper.h" // homeTmpDir, createTestFile etc.

class KIOThreadTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void concurrentCopying();
    void cleanupTestCase();

private:
    struct FileData;
    bool copyLocalFile(FileData *fileData);
};

void KIOThreadTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
    // Start with a clean base dir
    cleanupTestCase();
    homeTmpDir(); // create it

    QCOMPARE(sizeof(int), sizeof(QAtomicInt));

    Q_UNUSED(createTestDirectory);
}

void KIOThreadTest::cleanupTestCase()
{
    QDir(homeTmpDir()).removeRecursively();
}

struct KIOThreadTest::FileData {
    QString src;
    QString dest;
};

bool KIOThreadTest::copyLocalFile(FileData *fileData)
{
    // to verify the test harness: return QFile::copy(fileData->src, fileData->dest);

    const QUrl u = QUrl::fromLocalFile(fileData->src);
    const QUrl d = QUrl::fromLocalFile(fileData->dest);

    // copy the file with file_copy
    KIO::Job *job = KIO::file_copy(u, d, -1, KIO::HideProgressInfo);
    //qDebug() << job << u << d;
    job->setUiDelegate(nullptr);
    bool ret = job->exec();
    //qDebug() << job << "done";
    return ret;
}

void KIOThreadTest::concurrentCopying()
{
    const int numThreads = 20;
    QVector<FileData> data(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        data[i].src = homeTmpDir() + "file" + QString::number(i);
        data[i].dest = homeTmpDir() + "file" + QString::number(i) + "_copied";
        createTestFile(data[i].src);
    }
    QThreadPool tp;
    tp.setMaxThreadCount(numThreads);
    QVector<QFuture<bool>> futures(numThreads);
    for (int i = 0; i < numThreads; ++i) {
        futures[i] = QtConcurrent::run(&tp, this, &KIOThreadTest::copyLocalFile, &data[i]);
    }
    QVERIFY(tp.waitForDone(60000));

    for (int i = 0; i < numThreads; ++i) {
        QVERIFY(QFile::exists(data[i].dest));
        QVERIFY(futures.at(i).result());
    }
}

QTEST_MAIN(KIOThreadTest)
#include "threadtest.moc"

