/*
    SPDX-FileCopyrightText: 2013 Mark Gaiser <markg85@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "listdirtest.h"
#include <kio/listjob.h>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

QTEST_MAIN(ListDirTest)

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

void ListDirTest::detailsTestCase_data()
{
    QTest::addColumn<KIO::StatDetails>("details");
    QTest::addColumn<QList<KIO::UDSEntry::StandardFieldTypes>>("expectedFields");
    QTest::newRow("StatBasic") << KIO::StatDetails(KIO::StatDetail::StatBasic)
                               << QList<KIO::UDSEntry::StandardFieldTypes>{
                                      KIO::UDSEntry::UDS_NAME,
                                      KIO::UDSEntry::UDS_FILE_TYPE,
                                  };
    QTest::newRow("StatBasicWithInode") << (KIO::StatDetail::StatBasic | KIO::StatDetail::StatInode)
                                        << QList<KIO::UDSEntry::StandardFieldTypes>{
                                               KIO::UDSEntry::UDS_INODE,
                                               KIO::UDSEntry::UDS_NAME,
                                               KIO::UDSEntry::UDS_FILE_TYPE,
                                               KIO::UDSEntry::UDS_SIZE,
                                               KIO::UDSEntry::UDS_ACCESS,
                                               KIO::UDSEntry::UDS_DEVICE_ID,
                                           };
}

void ListDirTest::detailsTestCase()
{
#ifdef Q_OS_WIN
    QSKIP("Setting details is not supported on Windows");
#endif

    QFETCH(KIO::StatDetails, details);
    QFETCH(QList<KIO::UDSEntry::StandardFieldTypes>, expectedFields);

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const QUrl dirUrl = QUrl::fromLocalFile(tempDir.path());

    createEmptyTestFiles(10, tempDir.path());

    KIO::ListJob *job = KIO::listDir(dirUrl, KIO::HideProgressInfo, KIO::ListJob::ListFlag::IncludeHidden);
    job->setDetails(details);
    job->setUiDelegate(nullptr);

    QSignalSpy spy(job, &KIO::ListJob::entries);
    QVERIFY(spy.wait(1000));

    const KIO::UDSEntryList entries = spy.takeFirst().at(1).value<KIO::UDSEntryList>();
    for (const KIO::UDSEntry &entry : entries) {
        QCOMPARE(entry.count(), expectedFields.count());
        for (KIO::UDSEntry::StandardFieldTypes expectedField : expectedFields) {
            QVERIFY(entry.contains(expectedField));
        }
    }
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

#include "moc_listdirtest.cpp"
