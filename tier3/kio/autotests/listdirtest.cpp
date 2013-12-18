/*
 *  Copyright (C) 2013 Mark Gaiser <markg85@gmail.com>
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

#include "listdirtest.h"

#include <QtTest/QtTest>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QSignalSpy>

QTEST_MAIN( ListDirTest )

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
        KIO::ListJob* job = KIO::listDir(QUrl::fromLocalFile(tempDir.path()), KIO::HideProgressInfo);
        job->setUiDelegate( 0 );
        connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)), this, SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));

        QSignalSpy spy(job, SIGNAL(result(KJob*)));
        QVERIFY(spy.wait(100000));
        QCOMPARE(job->error(), 0); // no error
    }
    QCOMPARE(m_receivedEntryCount, numOfFiles);
}


void ListDirTest::slotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
{
    m_receivedEntryCount += entries.count();
}

void ListDirTest::createEmptyTestFiles(int numOfFilesToCreate, const QString& path)
{
    for(int i = 0; i < numOfFilesToCreate; i++) {
        const QString filename = path + QDir::separator() + QString::number(i) + ".txt";
        QFile file(filename);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    QCOMPARE(QDir(path).entryList(QDir::Files).count(), numOfFilesToCreate);
}
