/*
    This file is part of the KDE desktop environment
    SPDX-FileCopyrightText: 2001, 2002 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KDIRLISTERTEST_GUI_H_
#define _KDIRLISTERTEST_GUI_H_

#include <QWidget>
#include <QString>
#include <QUrl>

#include <kdirlister.h>
#include <kfileitem.h>

#include <iostream>

using namespace std;

class PrintSignals : public QObject
{
    Q_OBJECT
public:
    PrintSignals() : QObject() { }

public Q_SLOTS:
    void started(const QUrl &url)
    {
        cout << "*** started( " << url.url().toLocal8Bit().data() << " )" << endl;
    }
    void canceled()
    {
        cout << "canceled()" << endl;
    }
    void listingDirCanceled(const QUrl &url)
    {
        cout << "*** canceled( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
    }
    void completed()
    {
        cout << "*** completed()" << endl;
    }
    void listingDirCompleted(const QUrl &url)
    {
        cout << "*** completed( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
    }
    void redirection(const QUrl &url)
    {
        cout << "*** redirection( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
    }
    void redirection(const QUrl &src, const QUrl &dest)
    {
        cout << "*** redirection( " << src.toDisplayString().toLocal8Bit().data() << ", "
             << dest.toDisplayString().toLocal8Bit().data() << " )" << endl;
    }
    void clear()
    {
        cout << "*** clear()" << endl;
    }
    void newItems(const KFileItemList &items)
    {
        cout << "*** newItems: " << endl;
        KFileItemList::const_iterator it, itEnd = items.constEnd();
        for (it = items.constBegin(); it != itEnd; ++it) {
            cout << (*it).name().toLocal8Bit().data() << endl;
        }
    }
    void itemsDeleted(const KFileItemList &)
    {
        cout << "*** itemsDeleted: " << endl;
        // TODO
    }
    void itemsFilteredByMime(const KFileItemList &)
    {
        cout << "*** itemsFilteredByMime: " << endl;
        // TODO
    }
    void refreshItems(const QList<QPair<KFileItem, KFileItem> > &)
    {
        cout << "*** refreshItems: " << endl;
        // TODO
    }
    void infoMessage(const QString &msg)
    {
        cout << "*** infoMessage: " << msg.toLocal8Bit().data() << endl;
    }

    void percent(int percent)
    {
        cout << "*** percent: " << percent << endl;
    }

    void totalSize(KIO::filesize_t size)
    {
        cout << "*** totalSize: " << (long)size << endl;
    }

    void processedSize(KIO::filesize_t size)
    {
        cout << "*** processedSize: " << (long)size << endl;
    }

    void speed(int bytes_per_second)
    {
        cout << "*** speed: " << bytes_per_second << endl;
    }
};

class KDirListerTest : public QWidget
{
    Q_OBJECT
public:
    KDirListerTest(QWidget *parent = nullptr, const QUrl &url = {});
    ~KDirListerTest();

public Q_SLOTS:
    void startRoot();
    void startHome();
    void startTar();
    void test();
    void completed();

private:
    KDirLister *lister;
    PrintSignals *debug;
};

#endif
