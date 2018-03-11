/*
 *  Copyright (C) 2015 Martin Blumenstingl <martin.blumenstingl@googlemail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "deletejobtest.h"

#include <QtTest>
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

        QSignalSpy spy(job, SIGNAL(result(KJob*)));
        QVERIFY(spy.isValid());
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), KJOB_NO_ERROR);
        QVERIFY(!tempFile.exists());
    }
}

void DeleteJobTest::deleteDirectoryTestCase_data() const
{
    QTest::addColumn<QStringList>("fileNames");

    QStringList filesInNonEmptyDirectory = QStringList() << QStringLiteral("1.txt");
    QTest::newRow("non-empty directory") << filesInNonEmptyDirectory;
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

        QSignalSpy spy(job, SIGNAL(result(KJob*)));
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
