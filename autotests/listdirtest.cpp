/*
    SPDX-FileCopyrightText: 2013 Mark Gaiser <markg85@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "listdirtest.h"

#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

QTEST_MAIN(ListDirTest)

void ListDirTest::initTestCase()
{
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
}

void ListDirTest::numFilesTestCase_data()
{
    QTest::addColumn<int>("numOfFiles");
    QTest::newRow("10 files") << 10;
    QTest::newRow("100 files") << 100;
    QTest::newRow("1000 files") << 1000;
}

void ListDirTest::numFilesTestCase()
{
    QFETCH(int, numOfFiles);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    createEmptyTestFiles(numOfFiles, tempDir.path());

    /*QBENCHMARK*/ {
        m_receivedEntryCount = -2; // We start at -2 for . and .. slotResult will just increment this value
        KIO::ListJob *job = KIO::listDir(QUrl::fromLocalFile(tempDir.path()), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        connect(job, &KIO::ListJob::entries, this, &ListDirTest::slotEntries);

        QSignalSpy spy(job, &KJob::result);
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), 0); // no error
    }
    QCOMPARE(m_receivedEntryCount, numOfFiles);
}

void ListDirTest::slotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
{
    m_receivedEntryCount += entries.count();
}

void ListDirTest::createEmptyTestFiles(int numOfFilesToCreate, const QString &path)
{
    for (int i = 0; i < numOfFilesToCreate; i++) {
        const QString filename = path + QDir::separator() + QString::number(i) + ".txt";
        QFile file(filename);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    QCOMPARE(QDir(path).entryList(QDir::Files).count(), numOfFilesToCreate);
}
