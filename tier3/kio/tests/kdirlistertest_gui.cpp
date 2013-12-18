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

#include <QApplication>
#include <QLayout>
#include <QPushButton>
#include <QtCore/QDir>

#include <kdirlister.h>
#include <QDebug>
#include "kdirlistertest_gui.h"

#include <cstdlib>


KDirListerTest::KDirListerTest( QWidget *parent )
  : QWidget( parent )
{
  lister = new KDirLister(this);
  debug = new PrintSignals;

  QVBoxLayout* layout = new QVBoxLayout( this );

  QPushButton* startH = new QPushButton( "Start listing Home", this );
  QPushButton* startR= new QPushButton( "Start listing Root", this );
  QPushButton* test = new QPushButton( "Many", this );
  QPushButton* startT = new QPushButton( "tarfile", this );

  layout->addWidget( startH );
  layout->addWidget( startR );
  layout->addWidget( startT );
  layout->addWidget( test );
  resize( layout->sizeHint() );

  connect( startR, SIGNAL(clicked()), SLOT(startRoot()) );
  connect( startH, SIGNAL(clicked()), SLOT(startHome()) );
  connect( startT, SIGNAL(clicked()), SLOT(startTar()) );
  connect( test, SIGNAL(clicked()), SLOT(test()) );

  connect( lister, SIGNAL(started(QUrl)),
           debug,  SLOT(started(QUrl)) );
  connect( lister, SIGNAL(completed()),
           debug,  SLOT(completed()) );
  connect( lister, SIGNAL(completed(QUrl)),
           debug,  SLOT(completed(QUrl)) );
  connect( lister, SIGNAL(canceled()),
           debug,  SLOT(canceled()) );
  connect( lister, SIGNAL(canceled(QUrl)),
           debug,  SLOT(canceled(QUrl)) );
  connect( lister, SIGNAL(redirection(QUrl)),
           debug,  SLOT(redirection(QUrl)) );
  connect( lister, SIGNAL(redirection(QUrl,QUrl)),
           debug,  SLOT(redirection(QUrl,QUrl)) );
  connect( lister, SIGNAL(clear()),
           debug,  SLOT(clear()) );
  connect( lister, SIGNAL(newItems(KFileItemList)),
           debug,  SLOT(newItems(KFileItemList)) );
  connect( lister, SIGNAL(itemsFilteredByMime(KFileItemList)),
           debug,  SLOT(itemsFilteredByMime(KFileItemList)) );
  connect( lister, SIGNAL(deleteItem(KFileItem)),
           debug,  SLOT(deleteItem(KFileItem)) );
  connect( lister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
           debug,  SLOT(refreshItems(QList<QPair<KFileItem,KFileItem> >)) );
  connect( lister, SIGNAL(infoMessage(QString)),
           debug,  SLOT(infoMessage(QString)) );
  connect( lister, SIGNAL(percent(int)),
           debug,  SLOT(percent(int)) );
  connect( lister, SIGNAL(totalSize(KIO::filesize_t)),
           debug,  SLOT(totalSize(KIO::filesize_t)) );
  connect( lister, SIGNAL(processedSize(KIO::filesize_t)),
           debug,  SLOT(processedSize(KIO::filesize_t)) );
  connect( lister, SIGNAL(speed(int)),
           debug,  SLOT(speed(int)) );

  connect( lister, SIGNAL(completed()),
           this,  SLOT(completed()) );
}

KDirListerTest::~KDirListerTest()
{
}

void KDirListerTest::startHome()
{
  QUrl home = QUrl::fromLocalFile( QDir::homePath() );
  lister->openUrl( home, KDirLister::NoFlags );
//  lister->stop();
}

void KDirListerTest::startRoot()
{
  QUrl root = QUrl::fromLocalFile( QDir::rootPath() );
  lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
// lister->stop( root );
}

void KDirListerTest::startTar()
{
  QUrl root = QUrl::fromLocalFile( QDir::homePath()+"/aclocal_1.tgz" );
  lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
// lister->stop( root );
}

void KDirListerTest::test()
{
  QUrl home = QUrl::fromLocalFile( QDir::homePath() );
  QUrl root = QUrl::fromLocalFile( QDir::rootPath() );
#ifdef Q_OS_WIN
  lister->openUrl( home, KDirLister::Keep );
  lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
#else
/*  lister->openUrl( home, KDirLister::Keep );
  lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
  lister->openUrl( QUrl::fromLocalFile("file:/etc"), KDirLister::Keep | KDirLister::Reload );
  lister->openUrl( root, KDirLister::Keep | KDirLister::Reload );
  lister->openUrl( QUrl::fromLocalFile("file:/dev"), KDirLister::Keep | KDirLister::Reload );
  lister->openUrl( QUrl::fromLocalFile("file:/tmp"), KDirLister::Keep | KDirLister::Reload );
  lister->openUrl( QUrl::fromLocalFile("file:/usr/include"), KDirLister::Keep | KDirLister::Reload );
  lister->updateDirectory( QUrl::fromLocalFile("file:/usr/include") );
  lister->updateDirectory( QUrl::fromLocalFile("file:/usr/include") );
  lister->openUrl( QUrl::fromLocalFile("file:/usr/"), KDirLister::Keep | KDirLister::Reload );
*/
  lister->openUrl( QUrl::fromLocalFile("file:/dev"), KDirLister::Keep | KDirLister::Reload );
#endif
}

void KDirListerTest::completed()
{
    if ( lister->url().toLocalFile() == QDir::rootPath() )
    {
        const KFileItem item = lister->findByUrl( QUrl::fromLocalFile( QDir::tempPath() ) );
        if ( !item.isNull() )
            qDebug() << "Found " << QDir::tempPath() << ": " << item.name();
        else
            qWarning() << QDir::tempPath() << " not found! Bug in findByURL?";
    }
}

int main ( int argc, char *argv[] )
{
  QApplication::setApplicationName("kdirlistertest");
  QApplication app(argc, argv);

  KDirListerTest *test = new KDirListerTest( 0 );
  test->show();
  return app.exec();
}

#include "moc_kdirlistertest_gui.cpp"
