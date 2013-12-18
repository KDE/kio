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

#ifndef KFILEBOOKMARKHANDLER_H
#define KFILEBOOKMARKHANDLER_H

#include <kbookmarkmanager.h>
#include <kbookmarkmenu.h>

class QMenu;
class KFileWidget;

/**
 * Note: Ported to new KBookmarkMenu, but untested
 */
class KFileBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    KFileBookmarkHandler( KFileWidget *widget );
    ~KFileBookmarkHandler();

    QMenu * popupMenu();

    // KBookmarkOwner interface:
    virtual QString currentTitle() const;
    virtual QUrl currentUrl() const;
    virtual QString currentIcon() const;

    QMenu *menu() const { return m_menu; }

public Q_SLOTS:
    void openBookmark(const KBookmark & bm, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers );

Q_SIGNALS:
    void openUrl( const QString& url );

private:
    void importOldBookmarks( const QString& path, KBookmarkManager *manager );

    KFileWidget *m_widget;
    QMenu *m_menu;
    KBookmarkMenu *m_bookmarkMenu;

private:
    class KFileBookmarkHandlerPrivate;
    KFileBookmarkHandlerPrivate *d;
};


#endif // KFILEBOOKMARKHANDLER_H
