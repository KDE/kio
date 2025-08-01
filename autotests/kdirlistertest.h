/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2025 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KDIRLISTERTEST_H
#define KDIRLISTERTEST_H

#include <QDate>
#include <QEventLoop>
#include <QObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <kdirlister.h>
#include <kio/workerbase.h>
#include <memory>

Q_DECLARE_METATYPE(KFileItemList)

class GlobalInits
{
public:
    GlobalInits();
};

class MyDirLister : public KDirLister, GlobalInits
{
public:
    MyDirLister()
        : spyStarted(this, &KCoreDirLister::started)
        , spyItemsDeleted(this, &KCoreDirLister::itemsDeleted)
        , spyClear(this, qOverload<>(&KCoreDirLister::clear))
        , spyClearDir(this, &KCoreDirLister::clearDir)
        , spyCompleted(this, qOverload<>(&KCoreDirLister::completed))
        , spyCanceled(this, qOverload<>(&KCoreDirLister::canceled))
        , spyCompletedQUrl(this, &KCoreDirLister::listingDirCompleted)
        , spyCanceledQUrl(this, &KCoreDirLister::listingDirCanceled)
        , spyRedirection(this, &KCoreDirLister::redirection)
        , spyJobError(this, &KCoreDirLister::jobError)
    {
    }

    void clearSpies()
    {
        spyStarted.clear();
        spyClear.clear();
        spyCompleted.clear();
        spyCompletedQUrl.clear();
        spyCanceled.clear();
        spyCanceledQUrl.clear();
        spyRedirection.clear();
        spyItemsDeleted.clear();
        spyJobError.clear();
    }

    QSignalSpy spyStarted;
    QSignalSpy spyItemsDeleted;
    QSignalSpy spyClear;
    QSignalSpy spyClearDir;
    QSignalSpy spyCompleted;
    QSignalSpy spyCanceled;
    QSignalSpy spyCompletedQUrl;
    QSignalSpy spyCanceledQUrl;
    QSignalSpy spyRedirection;
    QSignalSpy spyJobError;
};

class KDirListerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testInvalidUrl();
    void testNonListableUrl();
    void testOpenUrl();
    void testOpenUrlFromCache();
    void testNewItem();
    void testNewItems();
    void benchFindByUrl();
    void testNewItemByCopy();
    void testNewItemByCopyInSubDir();
    void testNewItemsInSymlink();
    void testRefreshItems();
    void testRefreshRootItem();
    void testDeleteItem();
    void testDeleteItems();
    void testRenameItem();
    void testRenameAndOverwrite();
    void testConcurrentListing();
    void testConcurrentHoldingListing();
    void testConcurrentListingAndStop();
    void testDeleteListerEarly();
    void testOpenUrlTwice();
    void testOpenUrlTwiceWithKeep();
    void testOpenAndStop();
    void testBug211472();
    void testRenameCurrentDir();
    void testRenameCurrentDirOpenUrl();
    void testRedirection();
    void testListEmptyDirFromCache();
    void testWatchingAfterCopyJob();
    void testRemoveWatchedDirectory();
    void testDirPermissionChange();
    void testCopyAfterListingAndMove(); // #353195
    void testRenameDirectory(); // #401552
    void testRequestMimeType();
    void testMimeFilter_data();
    void testMimeFilter();
    void testBug386763();
    void testCacheEviction();
    void testUnreadableParentDirectory();
    void testPathWithSquareBrackets();
    void testSFTPRedirect();
    void testDeleteCurrentDir(); // must be just before last!
    void testForgetDir(); // must be last!

protected Q_SLOTS: // 'more private than private slots' - i.e. not seen by qtestlib
    void slotNewItems(const KFileItemList &);
    void slotNewItems2(const KFileItemList &);
    void slotRefreshItems(const QList<QPair<KFileItem, KFileItem>> &);
    void slotRefreshItems2(const QList<QPair<KFileItem, KFileItem>> &);
    void slotOpenUrlOnRename(const QUrl &);

Q_SIGNALS:
    void refreshItemsReceived();

private:
    int fileCount() const;
    QString tempPath() const;
    void createSimpleFile(const QString &fileName);
    void fillDirLister2(MyDirLister &lister, const QString &path);
    void waitUntilMTimeChange(const QString &path);
    void waitUntilAfter(const QDateTime &ctime);

private:
    int m_exitCount;
    QEventLoop m_eventLoop;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    MyDirLister m_dirLister;
    KFileItemList m_items;
    KFileItemList m_items2;
    QList<QPair<KFileItem, KFileItem>> m_refreshedItems, m_refreshedItems2;
};

#endif
