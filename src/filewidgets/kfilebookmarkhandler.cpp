/* This file is part of the KDE libraries
    Copyright (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "kfilebookmarkhandler_p.h"

#include <stdio.h>
#include <stdlib.h>

#include <QMenu>

#include <kbookmarkimporter.h>
#include <kbookmarkdombuilder.h>
#include <kio/global.h>
#include <qstandardpaths.h>

#include "kfilewidget.h"

KFileBookmarkHandler::KFileBookmarkHandler( KFileWidget *widget )
    : QObject( widget ),
      KBookmarkOwner(),
      m_widget( widget )
{
    setObjectName( "KFileBookmarkHandler" );
    m_menu = new QMenu( widget );
    m_menu->setObjectName( "bookmark menu" );

    QString file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kfile/bookmarks.xml" );
    if ( file.isEmpty() )
        file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + "kfile/bookmarks.xml" ;

    KBookmarkManager *manager = KBookmarkManager::managerForFile( file, "kfile" );
    manager->setUpdate( true );

    m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                        widget->actionCollection() );
}

KFileBookmarkHandler::~KFileBookmarkHandler()
{
    delete m_bookmarkMenu;
}

void KFileBookmarkHandler::openBookmark( const KBookmark & bm, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    emit openUrl(bm.url().toString());
}

QUrl KFileBookmarkHandler::currentUrl() const
{
    return m_widget->baseUrl();
}

QString KFileBookmarkHandler::currentTitle() const
{
    return m_widget->baseUrl().toDisplayString();
}

QString KFileBookmarkHandler::currentIcon() const
{
    return KIO::iconNameForUrl(currentUrl());
}

#include "moc_kfilebookmarkhandler_p.cpp"
