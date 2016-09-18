/* This file is part of the KDE project
   Copyright (C) 2004-2014 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <qtest.h>

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
    QStandardPaths::enableTestMode(true);

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
    job->setUiDelegate(0);
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

