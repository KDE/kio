/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2025 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirlistertest.h"

#include "jobuidelegatefactory.h"
#include "kiotesthelper.h"
#include "worker_p.h"
#include "workerbase.h"
#include "workerfactory.h"
#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/jobuidelegateextension.h>
#include <kio/simplejob.h>
#include <kio/udsentry.h>
#include <kprotocolinfo.h>

#ifdef WITH_QTDBUS
#include <KDirNotify>
#endif

#include <KDirWatch>

#include <QDebug>
#include <QTest>

using namespace Qt::StringLiterals;

QTEST_MAIN(KDirListerTest)

GlobalInits::GlobalInits()
{
    // Must be done before the QSignalSpys connect
    qRegisterMetaType<KFileItem>();
    qRegisterMetaType<KFileItemList>();
    qRegisterMetaType<KIO::Job *>();
}

QString KDirListerTest::tempPath() const
{
    return m_tempDir->path() + '/';
}

void KDirListerTest::initTestCase()
{
    // To avoid failing on broken locally defined MIME types
    QStandardPaths::setTestModeEnabled(true);

    m_tempDir.reset(new QTemporaryDir(homeTmpDir()));

    // No message dialogs
    KIO::setDefaultJobUiDelegateExtension(nullptr);
    KIO::setDefaultJobUiDelegateFactory(nullptr);

    m_exitCount = 1;

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs(-120); // 2 minutes ago

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
    createTestFile(tempPath() + "toplevelfile_1");
    createTestFile(tempPath() + "toplevelfile_2");
    createTestFile(tempPath() + "toplevelfile_3");
    createTestDirectory(tempPath() + "subdir");
    createTestDirectory(tempPath() + "subdir/subsubdir");

    qRegisterMetaType<QList<QPair<KFileItem, KFileItem>>>();
}

void KDirListerTest::cleanup()
{
    m_dirLister.clearSpies();
    disconnect(&m_dirLister, nullptr, this, nullptr);
}

void KDirListerTest::testInvalidUrl()
{
    m_dirLister.openUrl(QUrl(":/"), KDirLister::NoFlags);
    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QVERIFY(m_dirLister.spyJobError.wait());
    QCOMPARE(m_dirLister.spyJobError.at(0).at(0).value<KIO::Job *>()->error(), KIO::ERR_MALFORMED_URL);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QVERIFY(m_dirLister.isFinished());
}

void KDirListerTest::testNonListableUrl()
{
    m_dirLister.openUrl(QUrl("data:foo"), KDirLister::NoFlags);
    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QVERIFY(m_dirLister.spyJobError.wait());
    QCOMPARE(m_dirLister.spyJobError.at(0).at(0).value<KIO::Job *>()->error(), KIO::ERR_UNSUPPORTED_ACTION);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QVERIFY(m_dirLister.isFinished());
}

void KDirListerTest::testOpenUrl()
{
    m_items.clear();
    const QString path = tempPath();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    // The call to openUrl itself, emits started
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QVERIFY(!m_dirLister.isFinished());

    // then wait for completed
    qDebug("waiting for completed");
    QTRY_VERIFY(m_dirLister.isFinished());
    QTRY_COMPARE(m_dirLister.spyStarted.count(), 1);
    QTRY_COMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    // qDebug() << m_items;
    // qDebug() << "In dir" << QDir(path).entryList( QDir::AllEntries | QDir::NoDotAndDotDot);
    QCOMPARE(m_items.count(), fileCount());
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, nullptr, this, nullptr);

    const QString fileName = QStringLiteral("toplevelfile_3");
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
        const QString path = tempPath();
        MyDirLister secondDirLister;
        connect(&secondDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

        secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
        QCOMPARE(secondDirLister.spyStarted.count(), 1);
        QCOMPARE(secondDirLister.spyCompleted.count(), 0);
        QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyCanceled.count(), 0);
        QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyClear.count(), 1);

        QCOMPARE(secondDirLister.spyClearDir.count(), 0);

        QCOMPARE(m_items.count(), 0);
        QVERIFY(!secondDirLister.isFinished());

        // then wait for completed
        qDebug("waiting for completed");
        QTRY_COMPARE(secondDirLister.spyStarted.count(), 1);
        QTRY_COMPARE(secondDirLister.spyCompleted.count(), 1);
        QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
        QCOMPARE(secondDirLister.spyCanceled.count(), 0);
        QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
        QCOMPARE(secondDirLister.spyClear.count(), 1);

        QCOMPARE(secondDirLister.spyClearDir.count(), 0);

        QCOMPARE(m_items.count(), 4);
        QVERIFY(secondDirLister.isFinished());
    }

    disconnect(&m_dirLister, nullptr, this, nullptr);
}

// This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
// This test creates 1 file in the temporary directory
void KDirListerTest::testNewItem()
{
    QCOMPARE(m_items.count(), 4);
    const QString path = tempPath();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    qDebug() << "Creating a new file";
    const QString fileName = QStringLiteral("toplevelfile_new");

    createSimpleFile(path + fileName);

    QTRY_COMPARE(m_items.count(), 5);
    QCOMPARE(m_dirLister.spyStarted.count(), 1); // Updates call started
    QCOMPARE(m_dirLister.spyCompleted.count(), 1); // and completed
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
    disconnect(&m_dirLister, nullptr, this, nullptr);
}

// This test assumes testNewItem was run before. So m_dirLister is holding the items already.
// This test creates 100 more files in the temporary directory in reverse order
void KDirListerTest::testNewItems()
{
    QCOMPARE(m_items.count(), 5);
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    const QString path = tempPath();

    qDebug() << "Creating 100 new files";
    for (int i = 50; i > 0; i--) {
        createSimpleFile(path + QString("toplevelfile_new_%1").arg(i));
    }
    QTest::qWait(1000); // Create them with 1s difference
    for (int i = 100; i > 50; i--) {
        createSimpleFile(path + QString("toplevelfile_new_%1").arg(i));
    }

    // choose one of the new created files
    const QString fileName = QStringLiteral("toplevelfile_new_50");

    QTRY_COMPARE(m_items.count(), 105);

    QVERIFY(m_dirLister.spyStarted.count() > 0 && m_dirLister.spyStarted.count() < 3); // Updates call started, probably twice
    QVERIFY(m_dirLister.spyCompleted.count() > 0 && m_dirLister.spyCompleted.count() < 3); // and completed, probably twice
    QVERIFY(m_dirLister.spyCompletedQUrl.count() < 3);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
}

void KDirListerTest::benchFindByUrl()
{
    // We don't want to run benchmarks as part of the normal tests. This test depends on things being set up, which makes moving it to it's own file hard
    QSKIP("Skipped by default");
    // The time used should be in the order of O(100*log2(100))
    const QString path = tempPath();
    QBENCHMARK {
        for (int i = 100; i > 0; i--) {
            KFileItem cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(path + QString("toplevelfile_new_%1").arg(i)));
            QVERIFY(!cachedItem.isNull());
        }
    }
}

void KDirListerTest::testNewItemByCopy()
{
    // This test creates a file using KIO::copyAs, like knewmenu.cpp does.
    // Useful for testing #192185, i.e. whether we catch the kdirwatch event and avoid
    // a KFileItem::refresh().
    const int origItemCount = m_items.count();
    const QString path = tempPath();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    const QString fileName = QStringLiteral("toplevelfile_copy");
    const QUrl itemUrl = QUrl::fromLocalFile(path + fileName);
    KIO::CopyJob *job = KIO::copyAs(QUrl::fromLocalFile(path + "toplevelfile_3"), itemUrl, KIO::HideProgressInfo);
    job->exec();

    // Give time for KDirWatch/KDirNotify to notify us
    QTRY_COMPARE(m_items.count(), origItemCount + 1);

    QCOMPARE(m_dirLister.spyStarted.count(), 1); // Updates call started
    QCOMPARE(m_dirLister.spyCompleted.count(), 1); // and completed
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    // Give some time to KDirWatch
    QTest::qWait(1000);

    KFileItem itemForUrl = KDirLister::cachedItemForUrl(itemUrl);
    QVERIFY(!itemForUrl.isNull());
    QCOMPARE(itemForUrl.url().toString(), itemUrl.toString());
    QCOMPARE(itemForUrl.entry().stringValue(KIO::UDSEntry::UDS_NAME), fileName);
}

void KDirListerTest::testNewItemByCopyInSubDir() // #440712
{
    // Test that copying a file to a directory whose parent is listed
    // triggers refreshItems for the directory
    const int origItemCount = m_items.count();
    const QString path = tempPath();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    connect(&m_dirLister, &KDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems);
    QSignalSpy refreshItemSpy(this, &KDirListerTest::refreshItemsReceived);

    const QUrl subDirUrl = QUrl::fromLocalFile(path + "subdir");
    KFileItem itemForUrl = KDirLister::cachedItemForUrl(subDirUrl);
    auto origModificationDate = itemForUrl.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME);

    const QString fileName = path + "subdir/toplevelfile_copy";
    const QUrl itemUrl = QUrl::fromLocalFile(fileName);
    KIO::CopyJob *job = KIO::copyAs(QUrl::fromLocalFile(path + "toplevelfile_3"), itemUrl, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    QTRY_COMPARE(m_items.count(), origItemCount);

    // Give some time to KDirNotify
    QVERIFY(refreshItemSpy.wait(100));

    itemForUrl = KDirLister::cachedItemForUrl(subDirUrl);
    QVERIFY(itemForUrl.entry().numberValue(KIO::UDSEntry::UDS_MODIFICATION_TIME) > origModificationDate);

    // clean leftover file
    QVERIFY(QFile(fileName).remove());
    m_refreshedItems.clear();
}

void KDirListerTest::testNewItemsInSymlink() // #213799
{
    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    const QString path = tempPath();
    QTemporaryFile tempFile(homeTmpDir() + QStringLiteral("_normal_file"));
    QVERIFY(tempFile.open());
    const QString symPath = tempFile.fileName() + "_link";
    tempFile.close();
    const bool symlinkOk = KIOPrivate::createSymlink(path, symPath);
    if (!symlinkOk) {
        const QString error =
            QString::fromLatin1("Failed to create symlink '%1' pointing to '%2': %3").arg(symPath, path, QString::fromLocal8Bit(strerror(errno)));
        QVERIFY2(symlinkOk, qPrintable(error));
    }
    MyDirLister dirLister2;
    m_items2.clear();
    m_items.clear();
    connect(&dirLister2, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems2);
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    // The initial listing
    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    dirLister2.openUrl(QUrl::fromLocalFile(symPath));
    QTRY_COMPARE(m_items.count(), m_items2.count());
    QTRY_COMPARE(m_items.count(), origItemCount);
    QTRY_VERIFY(dirLister2.isFinished());

    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    qDebug() << "Creating new file";
    const QString fileName = QStringLiteral("toplevelfile_newinlink");
    createSimpleFile(path + fileName);

    // Give time for KDirWatch to notify us
    QTRY_COMPARE(m_items2.count(), origItemCount + 1);
    QTRY_COMPARE(m_items.count(), origItemCount + 1);

    // Now create an item using the symlink-path
    const QString fileName2 = QStringLiteral("toplevelfile_newinlink2");
    {
        createSimpleFile(path + fileName2);

        // Give time for KDirWatch to notify us
        QTRY_COMPARE(m_items2.count(), origItemCount + 2);
        QTRY_COMPARE(m_items.count(), origItemCount + 2);
    }
    QCOMPARE(fileCount(), m_items.count());

    // Test file deletion
    {
        qDebug() << "Deleting" << (path + fileName);
        QTest::qWait(1000); // for timestamp difference
        QFile::remove(path + fileName);
        QTRY_COMPARE_WITH_TIMEOUT(dirLister2.spyItemsDeleted.count(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(m_dirLister.spyItemsDeleted.count(), 1, 1000);
        const KFileItem item = dirLister2.spyItemsDeleted[0][0].value<KFileItemList>().at(0);
        QCOMPARE(item.url().toLocalFile(), QString(symPath + '/' + fileName));

        dirLister2.spyItemsDeleted.clear();
        m_dirLister.spyItemsDeleted.clear();
    }
    // Test file deletion in symlink dir #469254
    {
        qDebug() << "Deleting" << (symPath + "/" + fileName2);
        QTest::qWait(1000); // for timestamp difference
        QFile::remove(symPath + "/" + fileName2);

        QTRY_COMPARE_WITH_TIMEOUT(m_dirLister.spyItemsDeleted.count(), 1, 1000);
        const KFileItem item = m_dirLister.spyItemsDeleted[0][0].value<KFileItemList>().at(0);
        QCOMPARE(item.url().toLocalFile(), QString(path + fileName2));

        QTRY_COMPARE_WITH_TIMEOUT(dirLister2.spyItemsDeleted.count(), 1, 1000);
        const KFileItem item2 = dirLister2.spyItemsDeleted[0][0].value<KFileItemList>().at(0);
        QCOMPARE(item2.url().toLocalFile(), QString(symPath + '/' + fileName2));
    }
    QFile::remove(symPath);

    m_dirLister.spyItemsDeleted.clear();
    dirLister2.spyItemsDeleted.clear();
}

// This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
// Modifies one of the files to have html content
void KDirListerTest::testRefreshItems()
{
    m_refreshedItems.clear();

    const QString path = tempPath();
    const QString fileName = path + "toplevelfile_1";
    KFileItem cachedItem = m_dirLister.findByUrl(QUrl::fromLocalFile(fileName));
    QVERIFY(!cachedItem.isNull());
    QCOMPARE(cachedItem.mimetype(), QString("application/octet-stream"));

    connect(&m_dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems);

    QFile file(fileName);
    QVERIFY(file.open(QIODevice::Append));
    file.write(QByteArray("<html>"));
    file.close();
    QCOMPARE(QFileInfo(fileName).size(), 11LL /*Hello world*/ + 6 /*<html>*/);

    QTRY_VERIFY(!m_refreshedItems.isEmpty());

    QCOMPARE(m_dirLister.spyStarted.count(), 0); // fast path: no directory listing needed
    QVERIFY(m_dirLister.spyCompleted.count() < 2);
    QVERIFY(m_dirLister.spyCompletedQUrl.count() < 2);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

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
#ifdef WITH_QTDBUS
    // This test assumes testOpenUrl was run before. So m_dirLister is holding the items already.
    m_refreshedItems.clear();
    m_refreshedItems2.clear();

    // The item will be the root item of dirLister2, but also a child item
    // of m_dirLister.
    // In #190535 it would show "." instead of the subdir name, after a refresh...
    const QString path = tempPath() + "subdir";
    MyDirLister dirLister2;
    fillDirLister2(dirLister2, path);

    // Change the subdir by creating a file in it
    waitUntilMTimeChange(path);
    const QString foobar = path + "/.foobar";
    createSimpleFile(foobar);

    connect(&m_dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems);

    // Arguably, the mtime change of "subdir" should lead to a refreshItem of subdir in the root dir.
    // So the next line shouldn't be necessary, if KDirLister did this correctly. This isn't what this test is about though.
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>{QUrl::fromLocalFile(path)});
    QTRY_VERIFY(!m_refreshedItems.isEmpty());

    QCOMPARE(m_dirLister.spyStarted.count(), 0);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 0);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

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

    waitUntilMTimeChange(path);
    const QString directoryFile = path + "/.directory";
    createSimpleFile(directoryFile);

    org::kde::KDirNotify::emitFilesAdded(QUrl::fromLocalFile(path));
    QTest::qWait(200);
    // The order of these two is not deterministic
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>{QUrl::fromLocalFile(directoryFile)});
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>{QUrl::fromLocalFile(path)});
    QTRY_VERIFY(!m_refreshedItems.isEmpty());

    QCOMPARE(m_refreshedItems.count(), 1);
    entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.second.url().toLocalFile(), path);

    m_refreshedItems.clear();
    m_refreshedItems2.clear();

    // Note: this test leaves the .directory file as a side effect.
    // Hidden though, shouldn't matter.
#endif
}

void KDirListerTest::testDeleteItem()
{
    testOpenUrl(); // ensure m_items is up-to-date

    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    const QString path = tempPath();

    // qDebug() << "Removing " << path+"toplevelfile_new";
    QFile::remove(path + QString("toplevelfile_new"));
    // the remove() doesn't always trigger kdirwatch in stat mode, if this all happens in the same second
    KDirWatch::self()->setDirty(path);

    // The signal should be emitted once with the deleted file
    QTRY_COMPARE(m_dirLister.spyItemsDeleted.count(), 1);

    // OK now kdirlister told us the file was deleted, let's try a re-listing
    m_items.clear();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    QVERIFY(!m_dirLister.isFinished());

    QTRY_COMPARE(m_items.count(), origItemCount - 1);
    QVERIFY(m_dirLister.isFinished());

    disconnect(&m_dirLister, nullptr, this, nullptr);
    QCOMPARE(fileCount(), m_items.count());
}

void KDirListerTest::testDeleteItems()
{
    testOpenUrl(); // ensure m_items is up-to-date

    const int origItemCount = m_items.count();
    QCOMPARE(fileCount(), origItemCount);
    const QString path = tempPath();

    qDebug() << "Removing 100 files from " << path;
    for (int i = 0; i <= 100; ++i) {
        QFile::remove(path + QString("toplevelfile_new_%1").arg(i));
    }
    // the remove() doesn't always trigger kdirwatch in stat mode, if this all happens in the same second
    KDirWatch::self()->setDirty(path);

    // The signal could be emitted 1 time with all the deleted files or more times
    QTRY_VERIFY(m_dirLister.spyItemsDeleted.count() > 0);

    // OK now kdirlister told us the file was deleted, let's try a re-listing
    m_items.clear();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    QTRY_COMPARE(m_items.count(), origItemCount - 100);
    QVERIFY(m_dirLister.isFinished());

    disconnect(&m_dirLister, nullptr, this, nullptr);
    QCOMPARE(fileCount(), m_items.count());
}

void KDirListerTest::testRenameItem()
{
    m_refreshedItems2.clear();
    const QString dirPath = tempPath();
    connect(&m_dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems2);
    const QString path = dirPath + "toplevelfile_2";
    const QString newPath = dirPath + "toplevelfile_2.renamed.cpp";

    KIO::SimpleJob *job = KIO::rename(QUrl::fromLocalFile(path), QUrl::fromLocalFile(newPath), KIO::HideProgressInfo);
    QVERIFY(job->exec());

    QSignalSpy spyRefreshItems(&m_dirLister, &KCoreDirLister::refreshItems);
    QVERIFY(spyRefreshItems.wait(2000));
    QTRY_COMPARE(m_refreshedItems2.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems2.first();
    QCOMPARE(entry.first.url().toLocalFile(), path);
    QCOMPARE(entry.first.mimetype(), QString("application/octet-stream"));
    QCOMPARE(entry.second.url().toLocalFile(), newPath);
    QCOMPARE(entry.second.mimetype(), QString("text/x-c++src"));
    disconnect(&m_dirLister, nullptr, this, nullptr);

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
    const QString dirPath = tempPath();
    const QString path = dirPath + "toplevelfile_2";
    createTestFile(path);

    KFileItem existingItem;
    while (existingItem.isNull()) {
        QTest::qWait(100);
        existingItem = m_dirLister.findByUrl(QUrl::fromLocalFile(path));
    };
    QCOMPARE(existingItem.url().toLocalFile(), path);

    m_refreshedItems.clear();
    connect(&m_dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems);
    const QString newPath = dirPath + "toplevelfile_2.renamed.cpp";

    KIO::SimpleJob *job = KIO::rename(QUrl::fromLocalFile(newPath), QUrl::fromLocalFile(path), KIO::Overwrite | KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);

    if (m_refreshedItems.isEmpty()) {
        QTRY_VERIFY(!m_refreshedItems.isEmpty()); // could come from KDirWatch or KDirNotify.
    }

    // Check that itemsDeleted was emitted -- preferably BEFORE refreshItems,
    // but we can't easily check that with QSignalSpy...
    QTRY_COMPARE(m_dirLister.spyItemsDeleted.count(), 1);

    QCOMPARE(m_refreshedItems.count(), 1);
    QPair<KFileItem, KFileItem> entry = m_refreshedItems.first();
    QCOMPARE(entry.first.url().toLocalFile(), newPath);
    QCOMPARE(entry.second.url().toLocalFile(), path);
    disconnect(&m_dirLister, nullptr, this, nullptr);

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

    const QString path = tempPath();

    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    connect(&dirLister2, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems2);

    // Before dirLister2 has time to emit the items, let's make m_dirLister move to another dir.
    // This reproduces the use case "clicking on a folder in dolphin iconview, and dirlister2
    // is the one used by the "folder panel". m_dirLister is going to list the subdir,
    // while dirLister2 wants to list the folder that m_dirLister has just left.
    dirLister2.stop(); // like dolphin does, noop.
    dirLister2.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    m_dirLister.openUrl(QUrl::fromLocalFile(path + "subdir"), KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_items.count(), 0);

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 0);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);

    QCOMPARE(dirLister2.spyClearDir.count(), 0);

    QCOMPARE(m_items2.count(), 0);
    QVERIFY(!m_dirLister.isFinished());
    QVERIFY(!dirLister2.isFinished());

    // then wait for completed
    qDebug("waiting for completed");

    // QCOMPARE(m_dirLister.spyStarted.count(), 1); // 2 when subdir is already in cache.
    QTRY_COMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_items.count(), 3);

    QTRY_COMPARE(dirLister2.spyStarted.count(), 1);
    QTRY_COMPARE(dirLister2.spyCompleted.count(), 1);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 1);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);

    QCOMPARE(dirLister2.spyClearDir.count(), 0);

    QCOMPARE(m_items2.count(), origItemCount);
    if (!m_dirLister.isFinished()) { // false when an update is running because subdir is already in cache
        // TODO check why this fails QVERIFY(m_dirLister.spyCanceled.wait(1000));
        QTest::qWait(1000);
    }

    disconnect(&m_dirLister, nullptr, this, nullptr);
    disconnect(&dirLister2, nullptr, this, nullptr);
}

void KDirListerTest::testConcurrentHoldingListing()
{
    // #167851.
    // A dirlister holding the items, and a second dirlister does
    // openUrl(reload) (which triggers updateDirectory())
    // and the first lister immediately does openUrl() (which emits cached items).

    testOpenUrl(); // ensure m_dirLister holds the items.
    const int origItemCount = m_items.count();
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    m_items.clear();
    m_items2.clear();
    const QString path = tempPath();
    MyDirLister dirLister2;
    connect(&dirLister2, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems2);

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
    QTRY_COMPARE(dirLister2.spyStarted.count(), 1);
    QTRY_COMPARE(dirLister2.spyCompleted.count(), 1);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 1);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);

    QCOMPARE(dirLister2.spyClearDir.count(), 0);

    QCOMPARE(m_items2.count(), origItemCount);

    QTRY_COMPARE(m_dirLister.spyStarted.count(), 1);
    QTRY_COMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QVERIFY(dirLister2.isFinished());
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, nullptr, this, nullptr);
    QCOMPARE(m_items.count(), origItemCount);
}

void KDirListerTest::testConcurrentListingAndStop()
{
    m_items.clear();
    m_items2.clear();

    MyDirLister dirLister2;

    // Use a new tempdir for this test, so that we don't use the cache at all.
    QTemporaryDir tempDir(homeTmpDir());
    const QString path = tempDir.path() + '/';
    createTestFile(path + "file_1");
    createTestFile(path + "file_2");
    createTestFile(path + "file_3");

    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    connect(&dirLister2, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems2);

    // Before m_dirLister has time to emit the items, let's make dirLister2 call stop().
    // This should not stop the list job for m_dirLister (#267709).
    dirLister2.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload);
    m_dirLister.openUrl(QUrl::fromLocalFile(path) /*, KDirLister::Reload*/);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_items.count(), 0);

    QCOMPARE(dirLister2.spyStarted.count(), 1);
    QCOMPARE(dirLister2.spyCompleted.count(), 0);
    QCOMPARE(dirLister2.spyCompletedQUrl.count(), 0);
    QCOMPARE(dirLister2.spyCanceled.count(), 0);
    QCOMPARE(dirLister2.spyCanceledQUrl.count(), 0);
    QCOMPARE(dirLister2.spyClear.count(), 1);

    QCOMPARE(dirLister2.spyClearDir.count(), 0);

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

    QCOMPARE(dirLister2.spyClearDir.count(), 0);

    QCOMPARE(m_items2.count(), 0);

    // then wait for completed
    qDebug("waiting for completed");
    QTRY_COMPARE(m_items.count(), 3);
    QTRY_COMPARE(m_items2.count(), 0);
    QTRY_VERIFY(m_dirLister.isFinished());

    // QCOMPARE(m_dirLister.spyStarted.count(), 1); // 2 when in cache
    QCOMPARE(m_dirLister.spyCompleted.count(), 1);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    disconnect(&m_dirLister, nullptr, this, nullptr);
}

void KDirListerTest::testDeleteListerEarly()
{
    // Do the same again, it should behave the same, even with the items in the cache
    testOpenUrl();

    // Start a second lister, it will get a cached items job, but delete it before the job can run
    // qDebug() << "==========================================";
    {
        m_items.clear();
        const QString path = tempPath();
        MyDirLister secondDirLister;
        secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
        QVERIFY(!secondDirLister.isFinished());
    }
    // qDebug() << "==========================================";

    // Check if we didn't keep the deleted dirlister in one of our lists.
    // I guess the best way to do that is to just list the same dir again.
    testOpenUrl();
}

void KDirListerTest::testOpenUrlTwice()
{
    // Calling openUrl(reload)+openUrl(normal) before listing even starts.
    const int origItemCount = m_items.count();
    m_items.clear();
    const QString path = tempPath();
    MyDirLister secondDirLister;
    connect(&secondDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload); // will start
    QCOMPARE(secondDirLister.spyStarted.count(), 1);
    QCOMPARE(secondDirLister.spyCompleted.count(), 0);

    qDebug("calling openUrl again");
    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags); // will stop + start

    qDebug("waiting for completed");
    QTRY_COMPARE(secondDirLister.spyStarted.count(), 2);
    QTRY_COMPARE(secondDirLister.spyCompleted.count(), 1);
    QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0); // should not be emitted, see next test
    QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(secondDirLister.spyClear.count(), 2);

    QCOMPARE(secondDirLister.spyClearDir.count(), 0);

    if (origItemCount) { // 0 if running this test separately
        QCOMPARE(m_items.count(), origItemCount);
    }
    QVERIFY(secondDirLister.isFinished());
    disconnect(&secondDirLister, nullptr, this, nullptr);
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
    const QString path = tempPath() + "newsubdir";
    QDir().mkdir(path);
    MyDirLister secondDirLister;
    connect(&secondDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    secondDirLister.openUrl(QUrl::fromLocalFile(path)); // will start a list job
    QCOMPARE(secondDirLister.spyStarted.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0);
    QCOMPARE(secondDirLister.spyCompleted.count(), 0);

    qDebug("calling openUrl again");
    secondDirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::Keep); // stops and restarts the job

    qDebug("waiting for completed");
    QTRY_COMPARE(secondDirLister.spyStarted.count(), 2);
    QTRY_COMPARE(secondDirLister.spyCompleted.count(), 1);
    QCOMPARE(secondDirLister.spyCompletedQUrl.count(), 1);
    QCOMPARE(secondDirLister.spyCanceled.count(), 0); // should not be emitted, it led to recursion
    QCOMPARE(secondDirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(secondDirLister.spyClear.count(), 1);

    QCOMPARE(secondDirLister.spyClearDir.count(), 1);

    QCOMPARE(m_items.count(), 0);
    QVERIFY(secondDirLister.isFinished());
    disconnect(&secondDirLister, nullptr, this, nullptr);

    QDir().remove(path);
}

void KDirListerTest::testOpenAndStop()
{
    m_items.clear();
    const QString path = QStringLiteral("/"); // better not use a directory that we already listed!
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    qDebug() << "Calling stop!";
    m_dirLister.stop(); // we should also test stop(QUrl::fromLocalFile(path))...

    QCOMPARE(m_dirLister.spyStarted.count(), 1); // The call to openUrl itself, emits started
    QCOMPARE(m_dirLister.spyCompleted.count(), 0); // we had time to stop before the job even started
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 1);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 1);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_items.count(), 0); // we had time to stop before the job even started
    QVERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, nullptr, this, nullptr);
}

// A bug in the decAutoUpdate/incAutoUpdate logic made KDirLister stop watching a directory for changes,
// and never watch it again when opening it from the cache.
void KDirListerTest::testBug211472()
{
    m_items.clear();

    QTemporaryDir newDir(homeTmpDir());
    const QString path = newDir.path() + "/newsubdir/";
    QDir().mkdir(path);
    MyDirLister dirLister;
    connect(&dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    dirLister.openUrl(QUrl::fromLocalFile(path));
    QVERIFY(dirLister.spyCompleted.wait(1000));
    QVERIFY(dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // This block is required to trigger bug 211472.

    // Go 'up' to the parent of 'newsubdir'.
    dirLister.openUrl(QUrl::fromLocalFile(newDir.path()));
    QVERIFY(dirLister.spyCompleted.wait(1000));
    QTRY_VERIFY(dirLister.isFinished());
    QTRY_VERIFY(!m_items.isEmpty());
    m_items.clear();

    // Create a file in 'newsubdir' while we are listing its parent dir.
    createTestFile(path + "newFile-1");
    // At this point, newsubdir is not used, so it's moved to the cache.
    // This happens in checkUpdate, called when receiving a notification for the cached dir,
    // this is why this unittest needs to create a test file in the subdir.

    // wait a second and ensure the list is still empty afterwards
    QTest::qWait(1000);
    QTRY_VERIFY(m_items.isEmpty());

    // Return to 'newsubdir'. It will be emitted from the cache, then an update will happen.
    dirLister.openUrl(QUrl::fromLocalFile(path));
    // Check that completed is emitted twice
    QVERIFY(dirLister.spyCompleted.wait(1000));
    QVERIFY(dirLister.spyCompleted.wait(1000));
    QTRY_VERIFY(dirLister.isFinished());
    QTRY_COMPARE(m_items.count(), 1);
    m_items.clear();

    // Now try to create a second file in 'newsubdir' and verify that the
    // dir lister notices it.
    QTest::qWait(1000); // We need a 1s timestamp difference on the dir, otherwise FAM won't notice anything.

    createTestFile(path + "newFile-2");
    QTRY_COMPARE(m_items.count(), 1);

    newDir.remove();
    QSignalSpy spyClear(&dirLister, qOverload<>(&KCoreDirLister::clear));
    QVERIFY(spyClear.wait(1000));
}

void KDirListerTest::testRenameCurrentDir() // #294445
{
#ifdef WITH_QTDBUS
    m_items.clear();

    const QString path = tempPath() + "newsubdir-1";
    QVERIFY(QDir().mkdir(path));
    MyDirLister secondDirLister;
    connect(&secondDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    secondDirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&secondDirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(secondDirLister.isFinished());
    QVERIFY(m_items.empty());
    QCOMPARE(secondDirLister.rootItem().url().toLocalFile(), path);

    const QString newPath = tempPath() + "newsubdir-2";
    QVERIFY(QDir().rename(path, newPath));
    org::kde::KDirNotify::emitFileRenamed(QUrl::fromLocalFile(path), QUrl::fromLocalFile(newPath));
    QSignalSpy spyRedirection(&secondDirLister, &KCoreDirLister::redirection);
    QVERIFY(spyRedirection.wait(1000));

    // Check that the URL of the root item got updated
    QCOMPARE(secondDirLister.rootItem().url().toLocalFile(), newPath);

    disconnect(&secondDirLister, nullptr, this, nullptr);
    QDir().rmdir(newPath);
#endif
}

void KDirListerTest::slotOpenUrlOnRename(const QUrl &newUrl)
{
    QVERIFY(m_dirLister.openUrl(newUrl));
}

// This tests for a crash if you connect redirects to openUrl, due
// to internal data being inconsistently exposed.
// Matches usage in gwenview.
void KDirListerTest::testRenameCurrentDirOpenUrl()
{
#ifdef WITH_QTDBUS
    m_items.clear();
    const QString path = tempPath() + "newsubdir-1/";
    QVERIFY(QDir().mkdir(path));
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);

    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&m_dirLister, qOverload<>(&KCoreDirLister::completed));
    // Wait for the signal completed to be emitted
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());

    const QString newPath = tempPath() + "newsubdir-2";
    QVERIFY(QDir().rename(path, newPath));

    org::kde::KDirNotify::emitFileRenamed(QUrl::fromLocalFile(path), QUrl::fromLocalFile(newPath));

    // Connect the redirection to openURL, so that on a rename the new location is opened.
    // This matches usage in gwenview, and crashes
    connect(&m_dirLister, &KCoreDirLister::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        slotOpenUrlOnRename(newUrl);
    });

    QTRY_VERIFY(m_dirLister.isFinished());
    disconnect(&m_dirLister, nullptr, this, nullptr);
    QDir().rmdir(newPath);
#endif
}

void KDirListerTest::testRedirection()
{
    m_items.clear();
    const QUrl url(QStringLiteral("file://somemachine/"));

    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("smb"))) {
        QSKIP("smb not installed");
    }

    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    // The call to openUrl itself, emits started
    m_dirLister.openUrl(url, KDirLister::NoFlags);

    QCOMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0);
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QCOMPARE(m_dirLister.spyRedirection.count(), 0);
    QCOMPARE(m_items.count(), 0);
    QVERIFY(!m_dirLister.isFinished());

    // then wait for the redirection signal
    qDebug("waiting for redirection");
    QTRY_COMPARE(m_dirLister.spyStarted.count(), 1);
    QCOMPARE(m_dirLister.spyCompleted.count(), 0); // we stopped before the listing.
    QCOMPARE(m_dirLister.spyCompletedQUrl.count(), 0);
    QCOMPARE(m_dirLister.spyCanceled.count(), 0);
    QCOMPARE(m_dirLister.spyCanceledQUrl.count(), 0);
    QTRY_COMPARE(m_dirLister.spyClear.count(), 2); // redirection cleared a second time (just in case...)

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    QTRY_COMPARE(m_dirLister.spyRedirection.count(), 1);
    QVERIFY(m_items.isEmpty());
    QVERIFY(!m_dirLister.isFinished());

    m_dirLister.stop(url);
    QVERIFY(!m_dirLister.isFinished());
    disconnect(&m_dirLister, nullptr, this, nullptr);
}

void KDirListerTest::testListEmptyDirFromCache() // #278431
{
    m_items.clear();

    QTemporaryDir newDir(homeTmpDir());
    const QUrl url = QUrl::fromLocalFile(newDir.path());

    // List and watch an empty dir
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    m_dirLister.openUrl(url);
    QSignalSpy spyCompleted(&m_dirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // List it with two more dirlisters (one will create a cached items job, the second should also benefit from it)
    MyDirLister secondDirLister;
    connect(&secondDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    secondDirLister.openUrl(url);
    MyDirLister thirdDirLister;
    connect(&thirdDirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    thirdDirLister.openUrl(url);

    // The point of this test is that (with DEBUG_CACHE enabled) it used to assert here
    // with "HUH? Lister KDirLister(0x7ffd1f044260) is supposed to be listing, but has no job!"
    // due to the if (!itemU->lstItems.isEmpty()) check which is now removed.

    QVERIFY(!secondDirLister.isFinished()); // we didn't go to the event loop yet
    QSignalSpy spySecondCompleted(&secondDirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spySecondCompleted.wait(1000));
    if (!thirdDirLister.isFinished()) {
        QSignalSpy spyThirdCompleted(&thirdDirLister, qOverload<>(&KCoreDirLister::completed));
        QVERIFY(spyThirdCompleted.wait(1000));
    }
}

void KDirListerTest::testWatchingAfterCopyJob() // #331582
{
    m_items.clear();

    QTemporaryDir newDir(homeTmpDir());
    const QString path = newDir.path() + '/';

    // List and watch an empty dir
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&m_dirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait(1000));
    QVERIFY(m_dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // Create three subfolders.
    QVERIFY(QDir().mkdir(path + "New Folder"));
    QVERIFY(QDir().mkdir(path + "New Folder 1"));
    QVERIFY(QDir().mkdir(path + "New Folder 2"));

    QVERIFY(spyCompleted.wait(1000));
    QTRY_VERIFY(m_dirLister.isFinished());
    QTRY_COMPARE(m_items.count(), 3);

    // Create a new file and verify that the dir lister notices it.
    m_items.clear();
    createTestFile(path + QLatin1Char('a'));
    QVERIFY(spyCompleted.wait(1000));
    QTRY_VERIFY(m_dirLister.isFinished());
    QTRY_COMPARE(m_items.count(), 1);

    // Rename one of the subfolders.
    const QString oldPath = path + "New Folder 1";
    const QString newPath = path + "New Folder 1a";

    // NOTE: The following two lines are required to trigger the bug!
    KIO::Job *job = KIO::moveAs(QUrl::fromLocalFile(oldPath), QUrl::fromLocalFile(newPath), KIO::HideProgressInfo);
    job->exec();

    // Now try to create a second new file and verify that the
    // dir lister notices it.
    m_items.clear();
    createTestFile(path + QLatin1Char('b'));

    // This should end up in "KCoreDirListerCache::slotFileDirty"
    QTRY_COMPARE(m_items.count(), 1);

    newDir.remove();
    QSignalSpy clearSpy(&m_dirLister, qOverload<>(&KCoreDirLister::clear));
    QVERIFY(clearSpy.wait(1000));
}

void KDirListerTest::testRemoveWatchedDirectory()
{
    m_items.clear();

    QTemporaryDir newDir(homeTmpDir());
    const QString path = newDir.path() + '/';

    // List and watch an empty dir
    connect(&m_dirLister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    m_dirLister.openUrl(QUrl::fromLocalFile(path));
    QSignalSpy spyCompleted(&m_dirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait(1000));
    QTRY_VERIFY(m_dirLister.isFinished());
    QTRY_VERIFY(m_items.isEmpty());

    // Create a subfolder.
    const QString subDirPath = path + "abc";
    QVERIFY(QDir().mkdir(subDirPath));

    QVERIFY(spyCompleted.wait(1000));
    QTRY_VERIFY(m_dirLister.isFinished());
    QTRY_COMPARE(m_items.count(), 1);
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
    QTemporaryDir tempDir(homeTmpDir());

    const QString path = tempDir.path() + '/';
    const QString subdir = path + QLatin1String("subdir");
    QVERIFY(QDir().mkdir(subdir));

    // ensure initial permissions are different to the ones we set below
    const mode_t initPermissions = (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP);
    QVERIFY(KIO::chmod(QUrl::fromLocalFile(subdir), initPermissions)->exec());

    MyDirLister mylister;
    mylister.openUrl(QUrl::fromLocalFile(tempDir.path()));
    QSignalSpy spyCompleted(&mylister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait(1000));

    KFileItemList list = mylister.items();
    QVERIFY(mylister.isFinished());
    QCOMPARE(list.count(), 1);
    QCOMPARE(mylister.rootItem().url().toLocalFile(), tempDir.path());

    const mode_t permissions = (S_IRUSR | S_IWUSR | S_IXUSR);
    KIO::SimpleJob *job = KIO::chmod(list.first().url(), permissions);
    QVERIFY(job->exec());

    QSignalSpy spyRefreshItems(&mylister, &KCoreDirLister::refreshItems);
    QVERIFY(spyRefreshItems.wait(2000));

    list = mylister.items();
    QCOMPARE(list.first().permissions(), permissions);
    QVERIFY(QDir().rmdir(subdir));
}

void KDirListerTest::slotNewItems(const KFileItemList &lst)
{
    m_items += lst;
}

void KDirListerTest::slotNewItems2(const KFileItemList &lst)
{
    m_items2 += lst;
}

void KDirListerTest::slotRefreshItems(const QList<QPair<KFileItem, KFileItem>> &lst)
{
    m_refreshedItems += lst;
    Q_EMIT refreshItemsReceived();
}

void KDirListerTest::slotRefreshItems2(const QList<QPair<KFileItem, KFileItem>> &lst)
{
    m_refreshedItems2 += lst;
}

void KDirListerTest::testCopyAfterListingAndMove() // #353195
{
    const QString dirA = tempPath() + "a";
    QVERIFY(QDir().mkdir(dirA));
    const QString dirB = tempPath() + "b";
    QVERIFY(QDir().mkdir(dirB));

    // ensure m_dirLister holds the items.
    m_dirLister.openUrl(QUrl::fromLocalFile(tempPath()), KDirLister::NoFlags);
    QSignalSpy spyCompleted(&m_dirLister, qOverload<>(&KCoreDirLister::completed));
    QVERIFY(spyCompleted.wait());

    // Move b into a
    KIO::Job *moveJob = KIO::move(QUrl::fromLocalFile(dirB), QUrl::fromLocalFile(dirA));
    moveJob->setUiDelegate(nullptr);
    QVERIFY(moveJob->exec());
    QVERIFY(QFileInfo(tempPath() + "a/b").isDir());

    // Give some time to processPendingUpdates
    QTest::qWait(1000);

    // Copy folder a elsewhere
    const QString dest = tempPath() + "subdir";
    KIO::Job *copyJob = KIO::copy(QUrl::fromLocalFile(dirA), QUrl::fromLocalFile(dest));
    copyJob->setUiDelegate(nullptr);
    QVERIFY(copyJob->exec());
    QVERIFY(QFileInfo(tempPath() + "subdir/a/b").isDir());
}

void KDirListerTest::testRenameDirectory() // #401552
{
    // Create the directory structure to reproduce the bug in a reliable way
    const QString dirW = tempPath() + "w";
    QVERIFY(QDir().mkdir(dirW));
    const QString dirW1 = tempPath() + "w/Files";
    QVERIFY(QDir().mkdir(dirW1));
    const QString dirW2 = tempPath() + "w/Files/Files";
    QVERIFY(QDir().mkdir(dirW2));
    // Place some empty files in each directory
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW + QString("t_%1").arg(i));
    }
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW + QString("z_%1").arg(i));
    }
    // Place some empty files with prefix Files in w. Note that / is missing.
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW1 + QString("t_%1").arg(i));
    }
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW1 + QString("z_%1").arg(i));
    }
    // Place some empty files with prefix Files in w/Files. Note that / is missing.
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW2 + QString("t_%1").arg(i));
    }
    for (int i = 0; i < 50; i++) {
        createSimpleFile(dirW2 + QString("z_%1").arg(i));
    }
    // Listen to the w directory
    m_dirLister.openUrl(QUrl::fromLocalFile(dirW), KDirLister::NoFlags);

    // Try to reproduce the bug #401552 renaming the w directory several times if needed
    const QStringList dirs = {dirW + "___", dirW + QLatin1Char('_'), dirW + "______", dirW + "_c", dirW + "___", dirW + "_________"};

    QString currDir = dirW;
    KIO::SimpleJob *job = nullptr;
    // Connect the redirection to openURL, so that on a rename the new location is opened.
    connect(&m_dirLister, &KCoreDirLister::redirection, this, [this](const QUrl &, const QUrl &newUrl) {
        slotOpenUrlOnRename(newUrl);
    });

    for (const auto &newDir : dirs) {
        // Wait for the listener to get all files
        QTRY_VERIFY(m_dirLister.isFinished());
        // Do the rename
        job = KIO::rename(QUrl::fromLocalFile(currDir), QUrl::fromLocalFile(newDir), KIO::HideProgressInfo);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QTest::qWait(500); // Without the delay the crash doesn't happen
        currDir = newDir;
    }

    // cleanup
    const auto delJob = KIO::del(QUrl::fromLocalFile(dirs.last()));
    QVERIFY(delJob->exec());

    disconnect(&m_dirLister, nullptr, this, nullptr);
}

void KDirListerTest::testRequestMimeType()
{
    // Use a new tempdir and lister instance for this test, so that we don't use any cache at all.
    QTemporaryDir tempDir(homeTmpDir());
    QString path = tempDir.path() + QLatin1Char('/');

    createTestFile(path + "/file_1");
    createTestFile(path + "/file_2.txt");
    createTestFile(path + "/file_3.cpp");
    createTestFile(path + "/file_3.md");

    MyDirLister lister;
    // Explicitly set requestMimeTypeWhileListing to false so we know what state
    // it is in.
    lister.setRequestMimeTypeWhileListing(false);
    lister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);

    QTRY_VERIFY(lister.isFinished());

    auto items = lister.items();
    for (auto item : std::as_const(items)) {
        QVERIFY(!item.isMimeTypeKnown());
    }

    // Verify that the mime types are what we expect them to be
    QCOMPARE(items[0].mimetype(), QStringLiteral("application/octet-stream"));
    QCOMPARE(items[1].mimetype(), QStringLiteral("text/plain"));
    QCOMPARE(items[2].mimetype(), QStringLiteral("text/x-c++src"));
    QCOMPARE(items[3].mimetype(), QStringLiteral("text/markdown"));

    lister.setRequestMimeTypeWhileListing(true);
    lister.openUrl(QUrl::fromLocalFile(path), KDirLister::Reload);

    QTRY_VERIFY(lister.isFinished());

    // If requestMimeTypeWhileListing is on, we should know the mime type of
    // items when they have been listed.
    items = lister.items();
    for (auto item : std::as_const(items)) {
        QVERIFY(item.isMimeTypeKnown());
    }

    // Verify that the mime types are what we expect them to be
    QCOMPARE(items[0].mimetype(), QStringLiteral("application/octet-stream"));
    QCOMPARE(items[1].mimetype(), QStringLiteral("text/plain"));
    QCOMPARE(items[2].mimetype(), QStringLiteral("text/x-c++src"));
    QCOMPARE(items[3].mimetype(), QStringLiteral("text/markdown"));
}

void KDirListerTest::testMimeFilter_data()
{
    QTest::addColumn<QStringList>("files");
    QTest::addColumn<QStringList>("mimeTypes");
    QTest::addColumn<QStringList>("filteredFiles");

    const QStringList files = {"bla.txt", "main.cpp", "main.c", "image.jpeg"};

    QTest::newRow("single_file_exact_mimetype") << files << QStringList{"text/x-c++src"} << QStringList{"main.cpp"};
    QTest::newRow("inherited_mimetype") << files << QStringList{"text/plain"} << QStringList{"bla.txt", "main.cpp", "main.c"};
    QTest::newRow("no_match") << files << QStringList{"audio/flac"} << QStringList{};
}

void KDirListerTest::testMimeFilter()
{
    // Use a new tempdir and lister instance for this test, so that we don't use any cache at all.
    QTemporaryDir tempDir(homeTmpDir());
    QString path = tempDir.path() + '/';

    QFETCH(QStringList, files);
    QFETCH(QStringList, mimeTypes);
    QFETCH(QStringList, filteredFiles);

    for (const QString &fileName : files) {
        createTestFile(path + fileName);
    }

    MyDirLister lister;
    lister.setMimeFilter(mimeTypes);
    lister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);

    QVERIFY(lister.spyCompleted.wait(1000));

    QCOMPARE(lister.items().size(), filteredFiles.size());

    const auto items = lister.items();
    for (const auto &item : items) {
        QVERIFY(filteredFiles.indexOf(item.name()) != -1);
    }
}

void KDirListerTest::testDeleteCurrentDir()
{
    // ensure m_dirLister holds the items.
    m_dirLister.openUrl(QUrl::fromLocalFile(tempPath()), KDirLister::NoFlags);

    QVERIFY(m_dirLister.spyCompleted.wait(1000));

    m_dirLister.clearSpies();
    KIO::DeleteJob *job = KIO::del(QUrl::fromLocalFile(tempPath()), KIO::HideProgressInfo);
    bool ok = job->exec();
    QVERIFY(ok);
    QTRY_COMPARE(m_dirLister.spyClear.count(), 1);

    QCOMPARE(m_dirLister.spyClearDir.count(), 0);

    // there can be duplicated delete events
    QVERIFY(m_dirLister.spyItemsDeleted.count() >= 1 && m_dirLister.spyItemsDeleted.count() <= 2);
    const QUrl currentDirUrl = QUrl::fromLocalFile(tempPath()).adjusted(QUrl::StripTrailingSlash);
    for (const auto &deletedItem : m_dirLister.spyItemsDeleted) {
        QCOMPARE(currentDirUrl, deletedItem.at(0).value<KFileItemList>().at(0).url());
    }
}

void KDirListerTest::testForgetDir()
{
    QTemporaryDir tempDir(homeTmpDir());
    QString path = tempDir.path();
    createTestFile(path + "/file_1");

    m_dirLister.openUrl(QUrl::fromLocalFile(path), KDirLister::Keep);
    QVERIFY(m_dirLister.spyCompleted.wait());

    m_dirLister.forgetDirs(QUrl::fromLocalFile(path));

    QSignalSpy addedSpy(&m_dirLister, &MyDirLister::itemsAdded);
    createTestFile(path + "/file_2");
    QVERIFY(!addedSpy.wait(1000)); // to allow for KDirWatch's internal 500ms timer
}

int KDirListerTest::fileCount() const
{
    return QDir(tempPath()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count();
}

void KDirListerTest::createSimpleFile(const QString &fileName)
{
    QFile file(fileName);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray("foo"));
    file.close();
}

void KDirListerTest::fillDirLister2(MyDirLister &lister, const QString &path)
{
    m_items2.clear();
    connect(&lister, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems2);
    connect(&m_dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems2);
    lister.openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    QTRY_VERIFY(lister.isFinished());
}

void KDirListerTest::waitUntilMTimeChange(const QString &path)
{
    // Wait until the current second is more than the file's mtime
    // otherwise this change will go unnoticed

    QFileInfo fi(path);
    QVERIFY(fi.exists());
    const QDateTime mtime = fi.lastModified();
    waitUntilAfter(mtime);
}

void KDirListerTest::waitUntilAfter(const QDateTime &ctime)
{
    int totalWait = 0;
    QDateTime now;
    Q_FOREVER {
        now = QDateTime::currentDateTime();
        if (now.toSecsSinceEpoch() == ctime.toSecsSinceEpoch()) { // truncate milliseconds
            totalWait += 50;
            QTest::qWait(50);
        } else {
            QVERIFY(now > ctime); // can't go back in time ;)
            QTest::qWait(50); // be safe
            break;
        }
    }
    // if (totalWait > 0)
    qDebug() << "Waited" << totalWait << "ms so that now" << now.toString(Qt::ISODate) << "is >" << ctime.toString(Qt::ISODate);
}

// A bug in the decAutoUpdate/incAutoUpdate logic made KDirLister stop watching a directory for changes,
// and stop watching a directory because a separate lister left a directory open in another lister
void KDirListerTest::testBug386763()
{
    QTemporaryDir newDir(homeTmpDir());
    const QString path = newDir.path() + "/newsubdir/";
    const QString otherpath = newDir.path() + "/othersubdir/";

    QDir().mkdir(path);
    MyDirLister dirLister;
    dirLister.openUrl(QUrl::fromLocalFile(path));

    // second lister opening same dir
    MyDirLister dirLister2;
    dirLister2.openUrl(QUrl::fromLocalFile(path));
    QCOMPARE(dirLister2.spyCompleted.count(), 0);

    connect(&dirLister2, &KCoreDirLister::newItems, this, &KDirListerTest::slotNewItems);
    QVERIFY(dirLister.spyCompleted.wait(500));
    QVERIFY(dirLister.isFinished());
    QVERIFY(m_items.isEmpty());

    // first lister opening another dir
    dirLister.openUrl(QUrl::fromLocalFile(otherpath));

    // Create a file in 'newsubdir' while still opened in dirLister2
    // bug was that the watch on ’newsubdir’ when dirLister left this dir
    // eventhough dirLister2 is still listing it
    QCOMPARE(dirLister2.spyCompleted.count(), 1);
    createTestFile(path + "newFile-1");

    QTRY_COMPARE(m_items.count(), 1);
    QVERIFY(KDirWatch::self()->contains(path));

    dirLister2.openUrl(QUrl::fromLocalFile(otherpath));
    // checks we still watch the old path when second lister leaves it as it should be now in cache
    QVERIFY(KDirWatch::self()->contains(path));

    newDir.remove();
}

void KDirListerTest::testCacheEviction()
{
    QTemporaryDir newDir(homeTmpDir());

    MyDirLister dirLister;
    dirLister.openUrl(QUrl::fromLocalFile(newDir.path()));
    QVERIFY(dirLister.spyCompleted.wait(500));
    QVERIFY(dirLister.isFinished());
    QVERIFY(KDirWatch::self()->contains(newDir.path()));

    for (int i = 0; i < 12; i++) {
        const QString newDirPath = newDir.path() + QString("dir_%1").arg(i);
        QVERIFY(QDir().mkdir(newDirPath));

        dirLister.openUrl(QUrl::fromLocalFile(newDirPath));
        QVERIFY(dirLister.spyCompleted.wait(500));
        QVERIFY(dirLister.isFinished());
        QVERIFY(KDirWatch::self()->contains(newDirPath));
    }

    // watches were removed as the dirItem were evicted from cache
    QVERIFY(!KDirWatch::self()->contains(newDir.path()));
    QVERIFY(!KDirWatch::self()->contains(newDir.path() + QString("dir_0")));
    QVERIFY(KDirWatch::self()->contains(newDir.path() + QString("dir_1")));
}

void KDirListerTest::testUnreadableParentDirectory()
{
#ifdef WITH_QTDBUS
    QTemporaryDir newDir(homeTmpDir());
    MyDirLister dirLister;

    const QString hiddenPath = newDir.path() + QString("/hidden");
    const QString visiblePath = hiddenPath + QString("/visible");

    KDirWatch::self()->addDir(newDir.path(), KDirWatch::WatchSubDirs);

    // Create hidden folder and add it to lister
    QVERIFY(QDir().mkdir(hiddenPath));
    // Set folder to u-r to mimic `chown root:root hidden`
    const mode_t badperms = (S_IWUSR | S_IXUSR | S_IXGRP);
    QVERIFY(KIO::chmod(QUrl::fromLocalFile(hiddenPath), badperms)->exec());
    // Set the permissions normal to allow test to clean up at end of scope
    auto cleanup = qScopeGuard([hiddenPath] {
        const mode_t clearperms = (S_IWUSR | S_IXUSR | S_IRUSR | S_IRGRP | S_IWGRP | S_IXGRP);
        QVERIFY(KIO::chmod(QUrl::fromLocalFile(hiddenPath), clearperms)->exec());
    });
    QVERIFY(dirLister.openUrl(QUrl::fromLocalFile(hiddenPath), MyDirLister::Keep));
    // This should fail since we cant read the folder, so check for jobError
    QVERIFY(dirLister.spyJobError.wait(500));
    QVERIFY(dirLister.isFinished());
    QVERIFY(KDirWatch::self()->contains(hiddenPath));

    // Create visible folder and add it to lister
    QVERIFY(QDir().mkdir(visiblePath));
    QVERIFY(dirLister.openUrl(QUrl::fromLocalFile(visiblePath), MyDirLister::Keep));
    QVERIFY(dirLister.spyCompleted.wait(500));
    QVERIFY(dirLister.isFinished());
    QVERIFY(KDirWatch::self()->contains(visiblePath));

    // Wait until the time changes so the cache will have to be updated
    waitUntilMTimeChange(hiddenPath);
    const QString hiddenFile = hiddenPath + "/aaaa";
    createSimpleFile(hiddenFile);

    // Add a file to have difference between old and new cache data
    waitUntilMTimeChange(visiblePath);
    const QString visibleFile = visiblePath + "/bbbb";
    createSimpleFile(visibleFile);
    // Make sure we emit files changed so the cache will be re-read
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>{QUrl::fromLocalFile(visiblePath), QUrl::fromLocalFile(hiddenPath)});
#endif
}

void KDirListerTest::testPathWithSquareBrackets()
{
#if QT_VERSION == QT_VERSION_CHECK(6, 8, 3) || QT_VERSION == QT_VERSION_CHECK(6, 9, 0)
    QSKIP("This test is expected to fail on Qt 6.8.3 / 6.9.0");
#endif
    QTemporaryDir newDir(homeTmpDir());
    QFile file(newDir.filePath("[test].txt"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.close();
    MyDirLister dirLister;
    dirLister.openUrl(QUrl::fromLocalFile(newDir.path()));

    QVERIFY(dirLister.spyCompleted.wait(500));
    QVERIFY(dirLister.isFinished());

    m_refreshedItems.clear();
    connect(&dirLister, &KCoreDirLister::refreshItems, this, &KDirListerTest::slotRefreshItems);
    QSignalSpy spyRefreshItems(&dirLister, &KCoreDirLister::refreshItems);

    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write(QByteArray("foo"));
    file.close();
    QVERIFY(spyRefreshItems.wait(1000));
    QCOMPARE(m_refreshedItems.count(), 1);
    QCOMPARE(m_refreshedItems.at(0).first.url(), QUrl::fromLocalFile(file.fileName()));
}

void KDirListerTest::testSFTPRedirect()
{
    // This mock worker is needed to emulate very specific redirection case.
    class Factory : public KIO::WorkerFactory
    {
    public:
        using KIO::WorkerFactory::WorkerFactory;
        std::unique_ptr<KIO::WorkerBase> createWorker(const QByteArray &pool, const QByteArray &app) override
        {
            class RedirectWorker : public KIO::WorkerBase
            {
            public:
                RedirectWorker(const QByteArray &pool, const QByteArray &app)
                    : WorkerBase(QByteArrayLiteral("kio-test"), pool, app)
                {
                }

                // This emulates the behavior sftp:// protocol does, when connecting to
                // sftp://user@host -> it redirects to sftp://user@host/home/user
                Q_REQUIRED_RESULT KIO::WorkerResult listDir(const QUrl &url) override
                {
                    if (url.toString() == u"kio-test://foo@bar"_s) {
                        QUrl redir(url);
                        redir.setPath(u"/home/foo"_s);
                        redirection(redir);

                        // It is important to return a pass() here, otherwise the DirItem will not be marked complete and
                        // consequently isn't inserted into the cache.
                        return KIO::WorkerResult::pass();
                    }

                    if (url.toString() == u"kio-test://foo@bar/home/foo"_s) {
                        // Create fake entries
                        auto fakeEntry = [](QString name, int size) {
                            KIO::UDSEntry entry;
                            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, size);
                            entry.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("user1"));
                            entry.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("group1"));
                            entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
                            entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, 123456);
                            entry.fastInsert(KIO::UDSEntry::UDS_CREATION_TIME, 12345);
                            entry.fastInsert(KIO::UDSEntry::UDS_DEVICE_ID, 2);
                            entry.fastInsert(KIO::UDSEntry::UDS_INODE, 56);
                            return entry;
                        };

                        listEntry(fakeEntry(QStringLiteral("filename1.json"), 10));
                        listEntry(fakeEntry(QStringLiteral("filename2.txt"), 1000));
                        listEntry(fakeEntry(QStringLiteral("."), 1));

                        return KIO::WorkerResult::pass();
                    }

                    return KIO::WorkerResult::fail(KIO::ERR_UNSUPPORTED_ACTION, QStringLiteral("Unsupported URL: %1").arg(url.toString()));
                }
            };

            return std::unique_ptr<KIO::WorkerBase>(new RedirectWorker(pool, app));
        }
    };
    auto factory = std::make_shared<Factory>();
    KIO::Worker::setTestWorkerFactory(factory);
    QUrl testUrl(u"kio-test://foo@bar"_s);
    MyDirLister dirLister;
    // Mimic what dolphin does: upon redirection we open the redirected url. This is needless but exercises specific code paths.
    connect(&dirLister, &KCoreDirLister::redirection, this, [&dirLister](const QUrl &oldUrl, const QUrl &newUrl) {
        Q_UNUSED(oldUrl);
        dirLister.openUrl(newUrl);
    });
    dirLister.openUrl(testUrl);
    QVERIFY(dirLister.spyCompleted.wait(500));
    // Make sure we have the items listed properly on the first time.
    QVERIFY(dirLister.items().count() == 2);

    // This should not crash!
    dirLister.openUrl(testUrl);
    QVERIFY(dirLister.spyCompleted.wait(500));
    // This should not list any items: We have already done it in the previous iteration.
    // If this lists items, the view (for example in Dolphin) will have the items duplicated.
    QVERIFY(dirLister.items().count() == 0);
}

#include "moc_kdirlistertest.cpp"
