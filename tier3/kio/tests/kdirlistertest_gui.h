/* This file is part of the KDE desktop environment

   Copyright (C) 2001, 2002 Michael Brade <brade@kde.org>

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

#ifndef _KDIRLISTERTEST_GUI_H_
#define _KDIRLISTERTEST_GUI_H_

#include <QWidget>
#include <QtCore/QString>
#include <QUrl>

#include <kfileitem.h>

#include <iostream>

using namespace std;

class PrintSignals : public QObject
{
   Q_OBJECT
public:
   PrintSignals() : QObject() { }

public Q_SLOTS:
   void started( const QUrl &url )
   {
      cout << "*** started( " << url.url().toLocal8Bit().data() << " )" << endl;
   }
   void canceled() { cout << "canceled()" << endl; }
   void canceled( const QUrl& url )
   {
      cout << "*** canceled( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
   }
   void completed() { cout << "*** completed()" << endl; }
   void completed( const QUrl& url )
   {
      cout << "*** completed( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
   }
   void redirection( const QUrl& url )
   {
      cout << "*** redirection( " << url.toDisplayString().toLocal8Bit().data() << " )" << endl;
   }
   void redirection( const QUrl& src, const QUrl& dest )
   {
      cout << "*** redirection( " << src.toDisplayString().toLocal8Bit().data() << ", "
           << dest.toDisplayString().toLocal8Bit().data() << " )" << endl;
   }
   void clear() { cout << "*** clear()" << endl; }
   void newItems( const KFileItemList& items )
   {
      cout << "*** newItems: " << endl;
      KFileItemList::const_iterator it, itEnd = items.constEnd();
      for ( it = items.constBegin() ; it != itEnd ; ++it )
          cout << (*it).name().toLocal8Bit().data() << endl;
   }
   void deleteItem( const KFileItem& item )
   {
      cout << "*** deleteItem: " << item.url().toString().toLocal8Bit().constData() << endl;
   }
   void itemsFilteredByMime( const KFileItemList&  )
   {
      cout << "*** itemsFilteredByMime: " << endl;
      // TODO
   }
   void refreshItems( const QList<QPair<KFileItem, KFileItem> >& )
   {
      cout << "*** refreshItems: " << endl;
      // TODO
   }
   void infoMessage( const QString& msg )
   { cout << "*** infoMessage: " << msg.toLocal8Bit().data() << endl; }

   void percent( int percent )
   { cout << "*** percent: " << percent << endl; }

   void totalSize( KIO::filesize_t size )
   { cout << "*** totalSize: " << (long)size << endl; }

   void processedSize( KIO::filesize_t size )
   { cout << "*** processedSize: " << (long)size << endl; }

   void speed( int bytes_per_second )
   { cout << "*** speed: " << bytes_per_second << endl; }
};

class KDirListerTest : public QWidget
{
   Q_OBJECT
public:
   KDirListerTest( QWidget *parent=0 );
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
