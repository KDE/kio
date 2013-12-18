/* This file is part of the KDE project
   Copyright (C) 2007 David Faure <faure@kde.org>

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

#include "kdirlistertest.h"
#include <qtemporaryfile.h>
#include <kdirlister.h>
#include <qtest.h>

QTEST_MAIN(KDirListerTest)

#include <QDebug>
#include "kiotesthelper.h"
#include <kio/deletejob.h>
#include <kdirwatch.h>
#include <kprotocolinfo.h>
#include <kio/job.h>
#include <kio/copyjob.h>

#define WORKAROUND_BROKEN_INOTIFY 0

void MyDirLister::handleError(KIO::Job* job)
{
    // Currently we don't expect any errors.
    qCritical() << "KDirLister called handleError!" << job << job->error() << job->errorString();
    qFatal("aborting");
}

void KDirListerTest::initTestCase()
{
    m_exitCount = 1;

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs( -120 ); // 2 minutes ago

    // Create test data:
    /*
     * PATH/toplevelfile_1
     * PATH/toplevelfile_2
     * PATH/toplevelfile_3
     * PATH/subdir
     * PATH/subdir/testfile
     * PATH/subdir/subsubdir
     * PATH/subdir/subsubdir/testfile
     */
    const QString path = m_tempDir.path() + '/';
    createTestFile(path+"toplevelfile_1");
    createTestFile(path+"toplevelfile_2");
    createTestFile(path+"toplevelfile_3");
    createTestDirectory(path+"subdir");
    createTestDirectory(path+"subdir/subsubdir");
}

void KDirListerTest::cleanup()
{
    m_dirLister.clearSpies();
    disconnect(&m_dirLister, 0, this, 0);
}

void KDirListerTest::testOpenUrl()
{
    m_items.clear();
    const QString path = m_tempDir.path() + '/';
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    // The call to openUrl itself, emits started
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QVERIFY(!m_dirLister.isFinished());

    // then wait for completed
    qDebug("waiting for completed");
    connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();
    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    //qDebug() << m_items;
    //qDebug() << "In dir" << QDir(path).entryList( QDir::AllEntries | QDir::NoDotAndDotDot);
    QCOMPARE(m_items.count(), fileCount());
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, 0, this, 0);

    const QString fileName = "toplevelfile_3";
    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KFileItem byName = m_dirLister.findByName(fileName);
    QVERIFY(!byName.isNull());
    QCOMPARE(byName.url().toString(), itemUrl.toString());
    QCOMPARE(byName.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
    KFileItem byUrl = m_dirLister.findByUrl(itemUrl);
    QVERIFY(!byUrl.isNull());
    QCOMPARE(byUrl.url().toString(), itemUrl.toString());
    QCOMPARE(byUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);

    KFileItem rootByUrl = m_dirLister.findByUrl(QUrl::fromLocalFile(path));
    QVERIFY(!rootByUrl.isNull());
    QCOMPARE(QString(rootByUrl.url().toLocalFile() + '/'), path);

    m_dirLister.clearSpies(); // for the tests that call testOpenUrl for setup
}

// This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
void KDirListerTest::testOpenUrlFromCache()
{
    // Do the same again, it should behave the same, even with the items in the cache
    testOpenUrl();

    // Get into the case where another dirlister is holding the items
    {
        m_items.clear();
        const QString path = m_tempDir.path() + '/';
        MyDirLister secondDirLister;
        connect(&secondDirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
        secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
        QCOMPARE(secondDirLister.spyStarted.count(), 1);
        QCOMPARE(secondDirLister.spyCompleted.count(), 0);
        QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyCanceled.count(), 0);
        QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyClear.count(), 1);
        QCOMPARE(secondDirLister.spyClearQUrl.count(), 0);
        QCOMPARE(m_items.count(), 0);
        QVERIFY(!secondDirLister.isFinished());

        // then wait for completed
        qDebug("waiting for completed");
        connect(&secondDirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
        enterLoop();
        QCOMPARE(secondDirLister.spyStarted.count(), 1);
        QCOMPARE(secondDirLister.spyCompleted.count(), 1);
        QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
        QCOMPARE(secondDirLister.spyCanceled.count(), 0);
        QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyClear.count(), 1);
        QCOMPARE(secondDirLister.spyClearQUrl.count(), 0);
        QCOMPARE(m_items.count(), 4);
        QVERIFY(secondDirLister.isFinished());
    }

    disconnect(&m_dirLister, 0, this, 0);
}

// This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
void KDirListerTest::testNewItems()
{
    QCOMPARE(m_items.count(), 4);
    const QString path = m_tempDir.path() + '/';
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    qDebug() << "Creating new file";
    const QString fileName = "toplevelfile_new";
    createSimpleFile(path + fileName);

    int numTries = 0;
    // Give time for KDirWatch to notify us
    while (m_items.count() == 4) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
    //qDebug() << "numTries=" << numTries;
    QCOMPARE(m_items.count(), 5);

    QCOMPARE(m_dirLister.spyStarted.count(), 1); // Updates call started
    QCOMPARE(m_dirLister.spyCompleted.count(), 1); // and completed
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);

    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
}

void KDirListerTest::testNewItemByCopy()
{
    // This test creates a file using KIO::copyAs, like knewmenu.cpp does.
    // Useful for testing #192185, i.e. whether we catch the kdirwatch event and avoid
    // a KFileItem::refresh().
    const int origItemCount = m_items.count();
    const QString path = m_tempDir.path() + '/';
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    const QString fileName = "toplevelfile_copy";
    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KIO::CopyJob* job = KIO::copyAs(QUrl::fromLocalFile(path+"toplevelfile_3"), itemUrl, KIO::HideProgressInfo);
    job->exec();

    int numTries = 0;
    // Give time for KDirWatch/KDirNotify to notify us
    while (m_items.count() == origItemCount) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
    //qDebug() << "numTries=" << numTries;
    QCOMPARE(m_items.count(), origItemCount+1);

    QCOMPARE(m_dirLister.spyStarted.count(), 1); // Updates call started
    QCOMPARE(m_dirLister.spyCompleted.count(), 1); // and completed
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);

    // Give some time to KDirWatch
    QTest::qWait(1000);

    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
}

void KDirListerTest::testNewItemsInSymlink() // #213799
{
    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    const QString path = m_tempDir.path() + '/';
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QString symPath = tempFile.fileName() + "_link";
    tempFile.close();
    bool symlinkOk = ::symlink(QFile::encodeName(path).constData(), QFile::encodeName(symPath).constData()) == 0;
    QVERIFY(symlinkOk);
    MyDirLister dirLister2;
    m_items2.clear();
    connect(&dirLister2, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems2(KFileItemList)));
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    // The initial listing
    dirLister2.openUrl(QUrl::fromLocalFile(symPath), KDirLister::NoFlags);
    connect(&dirLister2, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();
    QCOMPARE(m_items2.count(), origItemCount);
    QVERIFY(dirLister2.isFinished());

    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    qDebug() << "Creating new file";
    const QString fileName = "toplevelfile_newinlink";
    createSimpleFile(path + fileName);

#if WORKAROUND_BROKEN_INOTIFY
    org::kde::KDirNotify::emitFilesAdded(path);
#endif
    int numTries = 0;
    // Give time for KDirWatch to notify us
    while (m_items2.count() == origItemCount) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
    //qDebug() << "numTries=" << numTries;
    QCOMPARE(m_items2.count(), origItemCount+1);
    QCOMPARE(m_items.count(), origItemCount+1);

    // Now create an item using the symlink-path
    const QString fileName2 = "toplevelfile_newinlink2";
    {
        createSimpleFile(path + fileName2);

        int numTries = 0;
        // Give time for KDirWatch to notify us
        while (m_items2.count() == origItemCount + 1) {
            QVERIFY(++numTries < 10);
            QTest::qWait(200);
        }
        QCOMPARE(m_items2.count(), origItemCount+2);
        QCOMPARE(m_items.count(), origItemCount+2);
    }
    QCOMPARE(fileCount(), m_items.count());

    // Test file deletion
    {
        qDebug() << "Deleting" << (path+fileName);
        QTest::qWait(1000); // for timestamp difference
        QFile::remove(path + fileName);
        while (dirLister2.spyDeleteItem.count() == 0) {
            QVERIFY(++numTries < 10);
            QTest::qWait(200);
        }
        QCOMPARE(dirLister2.spyDeleteItem.count(), 1);
        const KFileItem item = dirLister2.spyDeleteItem[0][0].value<KFileItem>();
        QCOMPARE(item.url().toLocalFile(), QString(symPath + '/' + fileName));
    }

    // TODO: test file update.
    disconnect(&m_dirLister, 0, this, 0);
}

// This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
void KDirListerTest::testRefreshItems()
{
    m_refreshedItems.clear();

    const QString path = m_tempDir.path() + '/';
    const QString fileName = path+"toplevelfile_1";
    KFileItem cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(fileName));
    QVERIFY(!cachedItem.isNull());
    QCOMPARE(cachedItem.mimetype(), QString("application/octet-stream"));

    connect(&m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
            this, SLOT(slotRefreshItems(QList<QPair<KFileItem,KFileItem> >)));

    QFile file(fileName);
    QVERIFY(file.open(QIODevice::Append));
    file.write(QByteArray("<html>"));
    file.close();
    QCOMPARE(QFileInfo(fileName).size(), 11LL /*Hello world*/ + 6 /*<html>*/);

    waitForRefreshedItems();

    QCOMPARE(m_dirLister.spyStarted.count(), 0); // fast path: no directory listing needed
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_refreshedItems.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), fileName);
    QCOMPARE(entry.first.size(), KIO::filesize_t(11));
    QCOMPARE(entry.first.mimetype(), QString("application/octet-stream"));
    QCOMPARE(entry.second.url().toLocalFile(), fileName);
    QCOMPARE(entry.second.size(), KIO::filesize_t(11 /*Hello world*/ + 6 /*<html>*/));
    QCOMPARE(entry.second.mimetype(), QString("text/html"));

    // Let's see what KDirLister has in cache now
    cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(fileName));
    QCOMPARE(cachedItem.size(), KIO::filesize_t(11 /*Hello world*/ + 6 /*<html>*/));
    m_refreshedItems.clear();
}

// Refresh the root item, plus a hidden file, e.g. changing its icon. #190535
void KDirListerTest::testRefreshRootItem()
{
    // This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
    m_refreshedItems.clear();
    m_refreshedItems2.clear();

    // The item will be the root item of dirLister2, but also a child item
    // of m_dirLister.
    // In #190535 it would show "." instead of the subdir name, after a refresh...
    const QString path = m_tempDir.path() + '/' + "subdir";
    MyDirLister dirLister2;
    fillDirLister2(dirLister2, path);

    connect(&m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
            this, SLOT(slotRefreshItems(QList<QPair<KFileItem,KFileItem> >)));

    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>() << QUrl::fromLocalFile(path));
    waitForRefreshedItems();

    QCOMPARE(m_dirLister.spyStarted.count(), 0);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_refreshedItems.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.first.name(), QString("subdir"));
    QCOMPARE(entry.second.url().toLocalFile(), path);
    QCOMPARE(entry.second.name(), QString("subdir"));

    QCOMPARE(m_refreshedItems2.count(), 1);
    entry = m_refreshedItems2.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.second.url().toLocalFile(), path);
    // item name() doesn't matter here, it's the root item.

    m_refreshedItems.clear();
    m_refreshedItems2.clear();

    const QString directoryFile = path + "/.directory";
    createSimpleFile(directoryFile);

    org::kde::KDirNotify::emitFilesAdded(QUrl::fromLocalFile(path));
    QTest::qWait(200);
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>() << QUrl::fromLocalFile(directoryFile));
    QCOMPARE(m_refreshedItems.count(), 0);

    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>() << QUrl::fromLocalFile(path));
    waitForRefreshedItems();
    QCOMPARE(m_refreshedItems.count(), 1);
    entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.second.url().toLocalFile(), path);

    m_refreshedItems.clear();
    m_refreshedItems2.clear();

    // Note: this test leaves the .directory file as a side effect.
    // Hidden though, shouldn't matter.
}

void KDirListerTest::testDeleteItem()
{
    testOpenUrl(); // ensure m_items is uptodate

    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    const QString path = m_tempDir.path() + '/';
    connect(&m_dirLister, SIGNAL(deleteItem(KFileItem)), this, SLOT(exitLoop()));

    //qDebug() << "Removing " << path+"toplevelfile_1";
    QFile::remove(path+"toplevelfile_1");
    // the remove() doesn't always trigger kdirwatch in stat mode, if this all happens in the same second
    KDirWatch::self()->setDirty(path);
    if (m_dirLister.spyDeleteItem.count() == 0) {
        qDebug("waiting for deleteItem");
        enterLoop();
    }

    QCOMPARE(m_dirLister.spyDeleteItem.count(), 1);
    QCOMPARE(m_dirLister.spyItemsDeleted.count(), 1);

    // OK now kdirlister told us the file was deleted, let's try a re-listing
    m_items.clear();
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    QVERIFY(!m_dirLister.isFinished());
    connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();
    QVERIFY(m_dirLister.isFinished());
    QCOMPARE(m_items.count(), origItemCount-1);

    disconnect(&m_dirLister, 0, this, 0);
    QCOMPARE(fileCount(), m_items.count());
}

void KDirListerTest::testRenameItem()
{
    m_refreshedItems2.clear();
    const QString dirPath = m_tempDir.path() + '/';
    connect(&m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
            this, SLOT(slotRefreshItems2(QList<QPair<KFileItem,KFileItem> >)));
    const QString path = dirPath+"toplevelfile_2";
    const QString newPath = dirPath+"toplevelfile_2.renamed.html";

    KIO::SimpleJob* job = KIO::rename(QUrl::fromLocalFile(path), QUrl::fromLocalFile(newPath), KIO::HideProgressInfo);
    QVERIFY(job->exec());

    QSignalSpy spyRefreshItems(&m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)));
    QVERIFY(spyRefreshItems.wait(2000));

    QCOMPARE(m_refreshedItems2.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems2.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.first.mimetype(), QString("application/octet-stream"));
    QCOMPARE(entry.second.url().toLocalFile(), newPath);
    QCOMPARE(entry.second.mimetype(), QString("text/html"));
    disconnect(&m_dirLister, 0, this, 0);

    // Let's see what KDirLister has in cache now
    KFileItem cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(newPath));
    QVERIFY(!cachedItem.isNull());
    QCOMPARE(cachedItem.url().toLocalFile(), newPath);
    KFileItem oldCachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(path));
    QVERIFY(oldCachedItem.isNull());
    m_refreshedItems2.clear();
}

void KDirListerTest::testRenameAndOverwrite() // has to be run after testRenameItem
{
    // Rename toplevelfile_2.renamed.html to toplevelfile_2, overwriting it.
    const QString dirPath = m_tempDir.path() + '/';
    const QString path = dirPath+"toplevelfile_2";
    createTestFile(path);
#if WORKAROUND_BROKEN_INOTIFY
    org::kde::KDirNotify::emitFilesAdded(dirPath);
#endif
    KFileItem existingItem;
    while (existingItem.isNull()) {
        QTest::qWait(100);
        existingItem = m_dirLister.findByUrl(QUrl::fromLocalFile(path));
    };
    QCOMPARE(existingItem.url().toLocalFile(), path);

    m_refreshedItems.clear();
    connect(&m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
            this, SLOT(slotRefreshItems(QList<QPair<KFileItem,KFileItem> >)));
    const QString newPath = dirPath+"toplevelfile_2.renamed.html";

    KIO::SimpleJob* job = KIO::rename(QUrl::fromLocalFile(newPath), QUrl::fromLocalFile(path), KIO::Overwrite | KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);

    if (m_refreshedItems.isEmpty()) {
        waitForRefreshedItems(); // refreshItems could come from KDirWatch or KDirNotify.
    }

    // Check that itemsDeleted was emitted -- preferably BEFORE refreshItems,
    // but we can't easily check that with QSignalSpy...
    QCOMPARE(m_dirLister.spyItemsDeleted.count(), 1);

    QCOMPARE(m_refreshedItems.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), newPath);
    QCOMPARE(entry.second.url().toLocalFile(), path);
    disconnect(&m_dirLister, 0, this, 0);

    // Let's see what KDirLister has in cache now
    KFileItem cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(path));
    QCOMPARE(cachedItem.url().toLocalFile(), path);
    KFileItem oldCachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(newPath));
    QVERIFY(oldCachedItem.isNull());
    m_refreshedItems.clear();
}

void KDirListerTest::testConcurrentListing()
{
    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    m_items.clear();
    m_items2.clear();

    MyDirLister dirLister2;

    const QString path = m_tempDir.path() + '/';

    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    connect(&dirLister2, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems2(KFileItemList)));

    // Before dirLister2 has time to emit the items, let's make m_dirLister move to another dir.
    // This reproduces the use case "clicking on a folder in dolphin iconview, and dirlister2
    // is the one used by the "folder panel". m_dirLister is going to list the subdir,
    // while dirLister2 wants to list the folder that m_dirLister has just left.
    dirLister2.stop(); // like dolphin does, noop.
    dirLister2.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    m_dirLister.openUrl(QUrl::fromLocalFile(path+"subdir"), KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_items.count(), 0);

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 0);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);
    QCOMPARE(dirLister2.spyClearQUrl.count(), 0);
    QCOMPARE(m_items2.count(), 0);
    QVERIFY(!m_dirLister.isFinished());
    QVERIFY(!dirLister2.isFinished());

    // then wait for completed
    qDebug("waiting for completed");
    connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    connect(&dirLister2, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop(2);

    //QCOMPARE(m_dirLister.spyStarted.count(), 1); // 2 when subdir is already in cache.
    QCOMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_items.count(), 3);

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 1);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 1);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);
    QCOMPARE(dirLister2.spyClearQUrl.count(), 0);
    QCOMPARE(m_items2.count(), origItemCount);
    if (!m_dirLister.isFinished()) { // false when an update is running because subdir is already in cache
      // TODO check why this fails QVERIFY(m_dirLister.spyCanceled.wait(1000));
      QTest::qWait(1000);
    }

    disconnect(&m_dirLister, 0, this, 0);
    disconnect(&dirLister2, 0, this, 0);
}

void KDirListerTest::testConcurrentHoldingListing()
{
    // #167851.
    // A dirlister holding the items, and a second dirlister does
    // openUrl(reload) (which triggers updateDirectory())
    // and the first lister immediately does openUrl() (which emits cached items).

    testOpenUrl(); // ensure m_dirLister holds the items.
    const int origItemCount = m_items.count();
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    m_items.clear();
    m_items2.clear();
    const QString path = m_tempDir.path() + '/';
    MyDirLister dirLister2;
    connect(&dirLister2, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems2(KFileItemList)));

    dirLister2.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload); // will start a list job
    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QCOMPARE(m_items2.count(), 0);

    qDebug("calling m_dirLister.openUrl");
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags); // should emit cached items, and then "join" the running listjob
    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QCOMPARE(m_items2.count(), 0);

    qDebug("waiting for completed");
    connect(&dirLister2, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 1);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 1);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);
    QCOMPARE(dirLister2.spyClearQUrl.count(), 0);
    QCOMPARE(m_items2.count(), origItemCount);

    if (m_dirLister.spyCompleted.isEmpty()) {
        connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
        enterLoop();
    }

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QVERIFY(dirLister2.isFinished());
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, 0, this, 0);
    QCOMPARE(m_items.count(), origItemCount);
}

void KDirListerTest::testConcurrentListingAndStop()
{
    m_items.clear();
    m_items2.clear();

    MyDirLister dirLister2;

    // Use a new tempdir for this test, so that we don't use the cache at all.
    QTemporaryDir tempDir;
    const QString path = tempDir.path() + '/';
    createTestFile(path+"file_1");
    createTestFile(path+"file_2");
    createTestFile(path+"file_3");

    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    connect(&dirLister2, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems2(KFileItemList)));

    // Before m_dirLister has time to emit the items, let's make dirLister2 call stop().
    // This should not stop the list job for m_dirLister (#267709).
    dirLister2.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload);
    m_dirLister.openUrl(QUrl::fromLocalFile(path)/*, KDirLister::Reload*/);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_items.count(), 0);

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 0);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);
    QCOMPARE(dirLister2.spyClearQUrl.count(), 0);
    QCOMPARE(m_items2.count(), 0);
    QVERIFY(!m_dirLister.isFinished());
    QVERIFY(!dirLister2.isFinished());

    dirLister2.stop();

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 0);
    QCOMPARE(dirLister2.spyCanceled.count(), 1);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 1);
    QCOMPARE(dirLister2.spyClear.count(), 1);
    QCOMPARE(dirLister2.spyClearQUrl.count(), 0);
    QCOMPARE(m_items2.count(), 0);

    // then wait for completed
    qDebug("waiting for completed");
    connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();

    QCOMPARE(m_items.count(), 3);
    QCOMPARE(m_items2.count(), 0);

    //QCOMPARE(m_dirLister.spyStarted.count(), 1); // 2 when in cache
    QCOMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);

    disconnect(&m_dirLister, 0, this, 0);
}

void KDirListerTest::testDeleteListerEarly()
{
    // Do the same again, it should behave the same, even with the items in the cache
    testOpenUrl();

    // Start a second lister, it will get a cached items job, but delete it before the job can run
    //qDebug() << "==========================================";
    {
        m_items.clear();
        const QString path = m_tempDir.path() + '/';
        MyDirLister secondDirLister;
        secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
        QVERIFY(!secondDirLister.isFinished());
    }
    //qDebug() << "==========================================";

    // Check if we didn't keep the deleted dirlister in one of our lists.
    // I guess the best way to do that is to just list the same dir again.
    testOpenUrl();
}

void KDirListerTest::testOpenUrlTwice()
{
    // Calling openUrl(reload)+openUrl(normal) before listing even starts.
    const int origItemCount = m_items.count();
    m_items.clear();
    const QString path = m_tempDir.path() + '/';
    MyDirLister secondDirLister;
    connect(&secondDirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload); // will start
    QCOMPARE(secondDirLister.spyStarted.count(), 1);
    QCOMPARE(secondDirLister.spyCompleted.count(), 0);

    qDebug("calling openUrl again");
    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags); // will stop + start

    qDebug("waiting for completed");
    connect(&secondDirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();

    QCOMPARE(secondDirLister.spyStarted.count(), 2);
    QCOMPARE(secondDirLister.spyCompleted.count(), 1);
    QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0); // should not be emitted, see next test
    QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(secondDirLister.spyClear.count(), 2);
    QCOMPARE(secondDirLister.spyClearQUrl.count(), 0);
    if (origItemCount) { // 0 if running this test separately
      QCOMPARE(m_items.count(), origItemCount);
    }
    QVERIFY(secondDirLister.isFinished());
    disconnect(&secondDirLister, 0, this, 0);
}

void KDirListerTest::testOpenUrlTwiceWithKeep()
{
    // Calling openUrl(reload)+openUrl(keep) on a new dir,
    // before listing even starts (#177387)
    // Well, in 177387 the second openUrl call was made from within slotCanceled
    // called by the first openUrl
    // (slotLoadingFinished -> setCurrentItem -> expandToUrl -> listDir),
    // which messed things up in kdirlister (unexpected reentrancy).
    m_items.clear();
    const QString path = m_tempDir.path() + "/newsubdir";
    QDir().mkdir(path);
    MyDirLister secondDirLister;
    connect(&secondDirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    secondDirLister.openUrl(QUrl::fromLocalFile(path)); // will start a list job
    QCOMPARE(secondDirLister.spyStarted.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0);
    QCOMPARE(secondDirLister.spyCompleted.count(), 0);

    qDebug("calling openUrl again");
    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::Keep); // stops and restarts the job

    qDebug("waiting for completed");
    connect(&secondDirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();

    QCOMPARE(secondDirLister.spyStarted.count(), 2);
    QCOMPARE(secondDirLister.spyCompleted.count(), 1);
    QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0); // should not be emitted, it led to recursion
    QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(secondDirLister.spyClear.count(), 1);
    QCOMPARE(secondDirLister.spyClearQUrl.count(), 1);
    QCOMPARE(m_items.count(), 0);
    QVERIFY(secondDirLister.isFinished());
    disconnect(&secondDirLister, 0, this, 0);

    QDir().remove(path);
}

void KDirListerTest::testOpenAndStop()
{
    m_items.clear();
    const QString path = "/"; // better not use a directory that we already listed!
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    qDebug() << "Calling stop!";
    m_dirLister.stop(); // we should also test stop(QUrl::fromLocalFile(path))...

    QCOMPARE(m_dirLister.spyStarted.count(), 1); // The call to openUrl itself, emits started
    QCOMPARE(m_dirLister.spyCompleted.count(), 0); // we had time to stop before the job even started
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 1);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_items.count(), 0); // we had time to stop before the job even started
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, 0, this, 0);
}

// A bug in the decAutoUpdate/incAutoUpdate logic made KDirLister stop watching a directory for changes,
// and never watch it again when opening it from the cache.
void KDirListerTest::testBug211472()
{
    m_items.clear();

    QTemporaryDir newDir;
    const QString path = newDir.path() + "/newsubdir/";
    QDir().mkdir(path);
    MyDirLister dirLister;
    connect(&dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&dirLister, SIGNAL(completed()));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    if (true) {
        // This block is required to trigger bug 211472.

        // Go 'up' to the parent of 'newsubdir'.
        dirLister.openUrl(QUrl::fromLocalFile(newDir.path()));
        QVERIFY(spyCompleted.wait(1000));
        QVERIFY(dirLister.isFinished());
        QVERIFY(!m_items.isEmpty());
        m_items.clear();

        // Create a file in 'newsubdir' while we are listing its parent dir.
        createTestFile(path + "newFile-1");
        // At this point, newsubdir is not used, so it's moved to the cache.
        // This happens in checkUpdate, called when receiving a notification for the cached dir,
        // this is why this unittest needs to create a test file in the subdir.
        QTest::qWait(1000);
        QVERIFY(m_items.isEmpty());

        // Return to 'newsubdir'. It will be emitted from the cache, then an update will happen.
        dirLister.openUrl(QUrl::fromLocalFile(path));
        QVERIFY(spyCompleted.wait(1000));
        QVERIFY(spyCompleted.wait(1000));
        QVERIFY(dirLister.isFinished());
        QCOMPARE(m_items.count(), 1);
        m_items.clear();
    }

    // Now try to create a second file in 'newsubdir' and verify that the
    // dir lister notices it.
    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    createTestFile(path + "newFile-2");

    int numTries = 0;
    // Give time for KDirWatch to notify us
    while (m_items.isEmpty()) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
    QCOMPARE(m_items.count(), 1);

    newDir.remove();
    QSignalSpy spyClear(&dirLister, SIGNAL(clear()));
    QVERIFY(spyClear.wait(1000));
}

void KDirListerTest::testRenameCurrentDir() // #294445
{
    m_items.clear();

    const QString path = m_tempDir.path() + "/newsubdir-1";
    QVERIFY(QDir().mkdir(path));
    MyDirLister secondDirLister;
    connect(&secondDirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));

    secondDirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&secondDirLister, SIGNAL(completed()));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(secondDirLister.isFinished());
    QVERIFY(m_items.empty());
    QCOMPARE(secondDirLister.rootItem().url().toLocalFile(), path);

    const QString newPath = m_tempDir.path() + "/newsubdir-2";
    QVERIFY(QDir().rename(path, newPath));
    org::kde::KDirNotify::emitFileRenamed(QUrl::fromLocalFile(path), QUrl::fromLocalFile(newPath));
    QSignalSpy spyRedirection(&secondDirLister, SIGNAL(redirection(QUrl,QUrl)));
    QVERIFY(spyRedirection.wait(1000));

    // Check that the URL of the root item got updated
    QCOMPARE(secondDirLister.rootItem().url().toLocalFile(), newPath);

    disconnect(&secondDirLister, 0, this, 0);
    QDir().remove(newPath);
}

void KDirListerTest::testRedirection()
{
    m_items.clear();
    const QUrl url("file://somemachine/");

    if (!KProtocolInfo::isKnownProtocol("smb"))
        QSKIP("smb not installed");

    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    // The call to openUrl itself, emits started
    m_dirLister.openUrl(url, KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QVERIFY(!m_dirLister.isFinished());

    // then wait for the redirection signal
    qDebug("waiting for redirection");
    connect(&m_dirLister, SIGNAL(redirection(QUrl,QUrl)), this, SLOT(exitLoop()));
    enterLoop();
    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0); // we stopped before the listing.
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 2); // redirection cleared a second time (just in case...)
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyRedirection.count(), 1);
    QVERIFY(m_items.isEmpty());
    QVERIFY(!m_dirLister.isFinished());

    m_dirLister.stop(url);
    QVERIFY(!m_dirLister.isFinished());
    disconnect(&m_dirLister, 0, this, 0);
}

void KDirListerTest::testWatchingAfterCopyJob() // #331582
{
    m_items.clear();

    QTemporaryDir newDir;
    const QString path = newDir.path() + '/';

    // List and watch an empty dir
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&m_dirLister, SIGNAL(completed()));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // Create three subfolders.
    QVERIFY(QDir().mkdir(path + "New Folder"));
    QVERIFY(QDir().mkdir(path + "New Folder 1"));
    QVERIFY(QDir().mkdir(path + "New Folder 2"));

    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QCOMPARE(m_items.count(), 3);

    // Create a new file and verify that the dir lister notices it.
    m_items.clear();
    createTestFile(path + "a");
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QCOMPARE(m_items.count(), 1);

    // Rename one of the subfolders.
    const QString oldPath = path + "New Folder 1";
    const QString newPath = path + "New Folder 1a";

    // NOTE: The following two lines are required to trigger the bug!
    KIO::Job* job = KIO::moveAs(QUrl::fromLocalFile(oldPath), QUrl::fromLocalFile(newPath), KIO::HideProgressInfo);
    job->exec();

    // Now try to create a second new file in and verify that the
    // dir lister notices it.
    m_items.clear();
    createTestFile(path + "b");

    int numTries = 0;
    // Give time for KDirWatch to notify us
    // This should end up in "KCoreDirListerCache::slotFileDirty"
    while (m_items.isEmpty()) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
    QCOMPARE(m_items.count(), 1);

    newDir.remove();
    QSignalSpy clearSpy(&m_dirLister, SIGNAL(clear()));
    QVERIFY(clearSpy.wait(1000));
}

void KDirListerTest::testRemoveWatchedDirectory()
{
    m_items.clear();

    QTemporaryDir newDir;
    const QString path = newDir.path() + '/';

    // List and watch an empty dir
    connect(&m_dirLister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems(KFileItemList)));
    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&m_dirLister, SIGNAL(completed()));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // Create a subfolder.
    const QString subDirPath = path + "abc";
    QVERIFY(QDir().mkdir(subDirPath));

    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QCOMPARE(m_items.count(), 1);
    const KFileItem item = m_items.at(0);

    // Watch the subfolder for changes, independently.
    // This is what triggers the bug.
    // (Technically, this could become a KDirWatch unittest, but if one day we use QFSW, good to have the tests here)
    KDirWatch watcher;
    watcher.addDir(subDirPath);

    // Remove the subfolder.
    m_items.clear();
    QVERIFY(QDir().rmdir(path + "abc"));

    // This should trigger an update.
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QCOMPARE(m_items.count(), 0);
    QCOMPARE(m_dirLister.spyItemsDeleted.count(), 1);
    const KFileItem deletedItem = m_dirLister.spyItemsDeleted.at(0).at(0).value<KFileItemList>().at(0);
    QCOMPARE(item, deletedItem);
}

void KDirListerTest::testDirPermissionChange()
{
    QTemporaryDir tempDir;

    const QString path = tempDir.path() + '/';
    const QString subdir = path + QLatin1String("subdir");
    QVERIFY(QDir().mkdir(subdir));

    MyDirLister mylister;
    mylister.openUrl(QUrl::fromLocalFile(tempDir.path()));
    QSignalSpy spyCompleted(&mylister, SIGNAL(completed()));
    QVERIFY(spyCompleted.wait(1000));

    KFileItemList list = mylister.items();
    QVERIFY(mylister.isFinished());
    QCOMPARE(list.count(), 1);
    QCOMPARE(mylister.rootItem().url().toLocalFile(), tempDir.path());

    const mode_t permissions = (S_IRUSR | S_IWUSR | S_IXUSR);
    KIO::SimpleJob* job = KIO::chmod(list.first().url(), permissions);
    QVERIFY(job->exec());

    QSignalSpy spyRefreshItems(&mylister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)));
    QVERIFY(spyRefreshItems.wait(2000));

    list = mylister.items();
    QCOMPARE(list.first().permissions(), permissions);
    QVERIFY(QDir().rmdir(subdir));
}

void KDirListerTest::enterLoop(int exitCount)
{
    //qDebug("enterLoop");
    m_exitCount = exitCount;
    m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
}

void KDirListerTest::exitLoop()
{
    //qDebug("exitLoop");
    --m_exitCount;
    if (m_exitCount <= 0) {
        m_eventLoop.quit();
    }
}

void KDirListerTest::slotNewItems(const KFileItemList& lst)
{
    m_items += lst;
}

void KDirListerTest::slotNewItems2(const KFileItemList& lst)
{
    m_items2 += lst;
}

void KDirListerTest::slotRefreshItems(const QList<QPair<KFileItem, KFileItem> > & lst)
{
    m_refreshedItems += lst;
    emit refreshItemsReceived();
}

void KDirListerTest::slotRefreshItems2(const QList<QPair<KFileItem, KFileItem> > & lst)
{
    m_refreshedItems2 += lst;
}

void KDirListerTest::testDeleteCurrentDir()
{
    // ensure m_dirLister holds the items.
    m_dirLister.openUrl(QUrl::fromLocalFile(path()), KDirLister::NoFlags);
    connect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();
    disconnect(&m_dirLister, SIGNAL(completed()), this, SLOT(exitLoop()));

    m_dirLister.clearSpies();
    connect(&m_dirLister, SIGNAL(clear()), &m_eventLoop, SLOT(quit()));
    KIO::DeleteJob* job = KIO::del(QUrl::fromLocalFile(path()), KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);
    enterLoop();
    QCOMPARE(m_dirLister.spyClear.count(), 1);
    QCOMPARE(m_dirLister.spyClearQUrl.count(), 0);
    QList<QUrl> deletedUrls;
    for (int i = 0; i < m_dirLister.spyItemsDeleted.count(); ++i)
        deletedUrls += m_dirLister.spyItemsDeleted[i][0].value<KFileItemList>().urlList();
    //qDebug() << deletedUrls;
    QUrl currentDirUrl = QUrl::fromLocalFile(path()).adjusted(QUrl::StripTrailingSlash);
    // Sometimes I get ("current/subdir", "current") here, but that seems ok.
    QVERIFY(deletedUrls.contains(currentDirUrl));
}

int KDirListerTest::fileCount() const
{
    return QDir(path()).entryList( QDir::AllEntries | QDir::NoDotAndDotDot).count();
}

void KDirListerTest::waitForRefreshedItems()
{
    int numTries = 0;
    // Give time for KDirWatch to notify us
    while (m_refreshedItems.isEmpty()) {
        QVERIFY(++numTries < 10);
        QTest::qWait(200);
    }
}

void KDirListerTest::createSimpleFile(const QString& fileName)
{
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray("foo"));
    file.close();
}

void KDirListerTest::fillDirLister2(MyDirLister& lister, const QString& path)
{
    m_items2.clear();
    connect(&lister, SIGNAL(newItems(KFileItemList)), this, SLOT(slotNewItems2(KFileItemList)));
    connect(&lister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
            this, SLOT(slotRefreshItems2(QList<QPair<KFileItem,KFileItem> >)));
    lister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    connect(&lister, SIGNAL(completed()), this, SLOT(exitLoop()));
    enterLoop();
    QVERIFY(lister.isFinished());
}

#include "moc_kdirlistertest.cpp"
