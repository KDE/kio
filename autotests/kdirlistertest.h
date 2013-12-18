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
#ifndef KDIRLISTERTEST_H
#define KDIRLISTERTEST_H

#include <QSignalSpy>
#include <QtCore/QObject>
#include <qtemporarydir.h>
#include <QtCore/QDate>
#include <kdirlister.h>
#include <QtCore/QEventLoop>

Q_DECLARE_METATYPE(KFileItemList)

class GlobalInits
{
public:
    GlobalInits() {
        // Must be done before the QSignalSpys connect
        qRegisterMetaType<KFileItem>();
        qRegisterMetaType<KFileItemList>();
    }
};

class MyDirLister : public KDirLister, GlobalInits
{
public:
    MyDirLister()
        : spyStarted(this, SIGNAL(started(QUrl))),
          spyClear(this, SIGNAL(clear())),
          spyClearQUrl(this, SIGNAL(clear(QUrl))),
          spyCompleted(this, SIGNAL(completed())),
          spyCompletedQUrl(this, SIGNAL(completed(QUrl))),
          spyCanceled(this, SIGNAL(canceled())),
          spyCanceledQUrl(this, SIGNAL(canceled(QUrl))),
          spyRedirection(this, SIGNAL(redirection(QUrl))),
          spyDeleteItem(this, SIGNAL(deleteItem(KFileItem))),
          spyItemsDeleted(this, SIGNAL(itemsDeleted(KFileItemList)))
    {}

    void clearSpies()
    {
        spyStarted.clear();
        spyClear.clear();
        spyClearQUrl.clear();
        spyCompleted.clear();
        spyCompletedQUrl.clear();
        spyCanceled.clear();
        spyCanceledQUrl.clear();
        spyRedirection.clear();
        spyDeleteItem.clear();
        spyItemsDeleted.clear();
    }

    QSignalSpy spyStarted;
    QSignalSpy spyClear;
    QSignalSpy spyClearQUrl;
    QSignalSpy spyCompleted;
    QSignalSpy spyCompletedQUrl;
    QSignalSpy spyCanceled;
    QSignalSpy spyCanceledQUrl;
    QSignalSpy spyRedirection;
    QSignalSpy spyDeleteItem;
    QSignalSpy spyItemsDeleted;
protected:
    virtual void handleError(KIO::Job* job);
};

class KDirListerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();
    void testOpenUrl();
    void testOpenUrlFromCache();
    void testNewItems();
    void testNewItemByCopy();
    void testNewItemsInSymlink();
    void testRefreshItems();
    void testRefreshRootItem();
    void testDeleteItem();
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
    void testRedirection();
    void testWatchingAfterCopyJob();
    void testRemoveWatchedDirectory();
    void testDirPermissionChange();
    void testDeleteCurrentDir(); // must be last!

protected Q_SLOTS: // 'more private than private slots' - i.e. not seen by qtestlib
    void exitLoop();
    void slotNewItems(const KFileItemList&);
    void slotNewItems2(const KFileItemList&);
    void slotRefreshItems(const QList<QPair<KFileItem, KFileItem> >&);
    void slotRefreshItems2(const QList<QPair<KFileItem, KFileItem> >&);

Q_SIGNALS:
    void refreshItemsReceived();

private:
    void enterLoop(int exitCount = 1);
    int fileCount() const;
    QString path() const { return m_tempDir.path() + '/'; }
    void waitForRefreshedItems();
    void createSimpleFile(const QString& fileName);
    void fillDirLister2(MyDirLister& lister, const QString& path);

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
