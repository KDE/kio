/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KDIRLISTERTEST_H
#define KDIRLISTERTEST_H

#include <QSignalSpy>
#include <QObject>
#include <QTemporaryDir>
#include <QDate>
#include <kdirlister.h>
#include <QEventLoop>

Q_DECLARE_METATYPE(KFileItemList)

class GlobalInits
{
public:
    GlobalInits()
    {
        // Must be done before the QSignalSpys connect
        qRegisterMetaType<KFileItem>();
        qRegisterMetaType<KFileItemList>();
    }
};

class MyDirLister : public KDirLister, GlobalInits
{
public:
    MyDirLister()
        : spyStarted(this, &KCoreDirLister::started),
          spyClear(this, QOverload<>::of(&KCoreDirLister::clear)),
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
          spyClearQUrl(this, QOverload<const QUrl &>::of(&KCoreDirLister::clear)),
#endif
          spyClearDir(this, &KCoreDirLister::clearDir),
          spyCompleted(this, QOverload<>::of(&KCoreDirLister::completed)),
          spyCanceled(this, QOverload<>::of(&KCoreDirLister::canceled)),
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
          spyCompletedQUrl(this, QOverload<const QUrl &>::of(&KCoreDirLister::completed)),
          spyCanceledQUrl(this, QOverload<const QUrl &>::of(&KCoreDirLister::canceled)),
#else
          spyCompletedQUrl(this, &KCoreDirLister::listingDirCompleted),
          spyCanceledQUrl(this, &KCoreDirLister::listingDirCanceled),
#endif
          spyRedirection(this, QOverload<const QUrl &>::of(&KCoreDirLister::redirection)),
          spyItemsDeleted(this, &KCoreDirLister::itemsDeleted)
    {}

    void clearSpies()
    {
        spyStarted.clear();
        spyClear.clear();
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        spyClearQUrl.clear();
#endif
        spyCompleted.clear();
        spyCompletedQUrl.clear();
        spyCanceled.clear();
        spyCanceledQUrl.clear();
        spyRedirection.clear();
        spyItemsDeleted.clear();
    }

    QSignalSpy spyStarted;
    QSignalSpy spyClear;
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
    QSignalSpy spyClearQUrl;
#endif
    QSignalSpy spyClearDir;
    QSignalSpy spyCompleted;
    QSignalSpy spyCanceled;
    QSignalSpy spyCompletedQUrl;
    QSignalSpy spyCanceledQUrl;
    QSignalSpy spyRedirection;
    QSignalSpy spyItemsDeleted;
protected:
    void handleError(KIO::Job *job) override;
};

class KDirListerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testOpenUrl();
    void testOpenUrlFromCache();
    void testNewItem();
    void testNewItems();
    void benchFindByUrl();
    void testNewItemByCopy();
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
    void testDeleteCurrentDir(); // must be last!

protected Q_SLOTS: // 'more private than private slots' - i.e. not seen by qtestlib
    void slotNewItems(const KFileItemList &);
    void slotNewItems2(const KFileItemList &);
    void slotRefreshItems(const QList<QPair<KFileItem, KFileItem> > &);
    void slotRefreshItems2(const QList<QPair<KFileItem, KFileItem> > &);
    void slotOpenUrlOnRename(const QUrl &);

Q_SIGNALS:
    void refreshItemsReceived();

private:
    int fileCount() const;
    QString path() const
    {
        return m_tempDir.path() + '/';
    }
    void createSimpleFile(const QString &fileName);
    void fillDirLister2(MyDirLister &lister, const QString &path);
    void waitUntilMTimeChange(const QString &path);
    void waitUntilAfter(const QDateTime &ctime);

private:
    int m_exitCount;
    QEventLoop m_eventLoop;
    QTemporaryDir m_tempDir;
    MyDirLister m_dirLister;
    KFileItemList m_items;
    KFileItemList m_items2;
    QList<QPair<KFileItem, KFileItem> > m_refreshedItems, m_refreshedItems2;
};

#endif
