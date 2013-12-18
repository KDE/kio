/*  This file is part of the KDE project
    Copyright (C) 2008 Norbert Frese <nf2@scheinwelt.at>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#ifndef KFILEPLACESSHAREDBOOKMARKS_P_H
#define KFILEPLACESSHAREDBOOKMARKS_P_H

#include <QtCore/QObject>
#include <kbookmarkmanager.h>

/**
 *  keeps the KFilePlacesModel bookmarks and the shared bookmark spec
 *  shortcuts in sync
 */
class KFilePlacesSharedBookmarks : public QObject
{
    Q_OBJECT
public:  
  
    KFilePlacesSharedBookmarks(KBookmarkManager * mgr);
    ~KFilePlacesSharedBookmarks() { /* delete m_sharedBookmarkManager; */} 
      
private:
  
    bool integrateSharedBookmarks();
    bool exportSharedBookmarks();
  
    KBookmarkManager *m_placesBookmarkManager;
    KBookmarkManager *m_sharedBookmarkManager;
    
private Q_SLOTS:    

    void slotSharedBookmarksChanged();
    void slotBookmarksChanged();
  
};




#endif /*KFILEPLACESSHARED_P_H_*/
