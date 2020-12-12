/*
    SPDX-FileCopyrightText: 2015 Martin Blumenstingl <martin.blumenstingl@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "deletejobtest.h"

#include <QTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QSignalSpy>

#include <kio/deletejob.h>

QTEST_MAIN(DeleteJobTest)

#define KJOB_NO_ERROR static_cast<int>(KJob::NoError)

void DeleteJobTest::initTestCase()
{
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");
}

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
        QVERIFY(!tempFile.exists());
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

    createEmptyTestFiles(fileNames, tempDir.path());

    /*QBENCHMARK*/ {
        KIO::DeleteJob *job = KIO::del(QUrl::fromLocalFile(tempDir.path()), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);

        QSignalSpy spy(job, &KJob::result);
        QVERIFY(spy.isValid());
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), KJOB_NO_ERROR);
        QVERIFY(!QDir(tempDir.path()).exists());
    }
}

void DeleteJobTest::createEmptyTestFiles(const QStringList &fileNames, const QString &path) const
{
    QStringListIterator iterator(fileNames);
    while (iterator.hasNext()) {
        const QString filename = path + QDir::separator() + iterator.next();
        QFile file(filename);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    QCOMPARE(QDir(path).entryList(QDir::Files).count(), fileNames.size());
}
