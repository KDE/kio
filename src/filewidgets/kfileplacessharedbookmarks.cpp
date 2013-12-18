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

#include "kfileplacessharedbookmarks_p.h"

#include <QtCore/QObject>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <kbookmarkmanager.h>
#include <kbookmark.h>
#include <QDebug>

//////////////// utility functions

static bool compareBookmarks(const KBookmark & bookmark1, const KBookmark & bookmark2)
{
    return (bookmark1.url() == bookmark2.url() || bookmark1.text() == bookmark2.text());
}

static bool deepCompareDomNodes(const QDomNode & node1, const QDomNode & node2)
{

    // compare name and value
    if (node1.nodeName() != node2.nodeName() || node1.nodeValue() != node2.nodeValue())
        return false;

    // recursively compare children
    const QDomNodeList node1Children  = node1.childNodes();
    const QDomNodeList node2Children  = node2.childNodes();

    if (node1Children.count () != node2Children.count ())
        return false;

    for (int i=0; i<node1Children.count ();i++) {
        if (!deepCompareDomNodes(node1Children.at(i), node2Children.at(i) ))
            return false;
    }
    return true;
}

/*
static QString nodeAsString(const QDomNode & node1)
{
    QString str;
    QTextStream ts( &str, QIODevice::WriteOnly );
    ts << node1;
    return str;
}
*/

static bool exactCompareBookmarks(const KBookmark & bookmark1, const KBookmark & bookmark2)
{
    //qDebug() << "excat comparing:\n" << nodeAsString(bookmark1.internalElement()) << "\nwith:\n" << nodeAsString(bookmark2.internalElement());
    return deepCompareDomNodes(bookmark1.internalElement(), bookmark2.internalElement());
}

static void cloneBookmarkContents(const KBookmark & target, const KBookmark & source)
{
    const QDomElement targetEl = target.internalElement();
    QDomNode parent = targetEl.parentNode ();
    QDomNode clonedNode = source.internalElement().cloneNode(true);
    parent.replaceChild (clonedNode , targetEl );
}

static KBookmark cloneBookmark(const KBookmark & toClone)
{
    const QDomNode cloned = toClone.internalElement().cloneNode(true);
    return KBookmark(cloned.toElement ());
}


static void emptyBookmarkGroup(KBookmarkGroup & root)
{
    KBookmark bookmark = root.first();
    while (!bookmark.isNull()) {
        KBookmark bookmarkToRemove = bookmark;
        bookmark = root.next(bookmark);
        root.deleteBookmark(bookmarkToRemove);
    }
}

static int bookmarkGroupSize(KBookmarkGroup & root)
{
    int count=0;
    KBookmark bookmark = root.first();
    while (!bookmark.isNull()) {
        count++;
        bookmark = root.next(bookmark);
    }
    return count;
}

//////////////// class KFilePlacesSharedBookmarks

KFilePlacesSharedBookmarks::KFilePlacesSharedBookmarks(KBookmarkManager * mgr)
{
    m_placesBookmarkManager = mgr;

    // we check later if the directory exists
    const QString datadir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QDir().mkpath(datadir);
    const QString file = datadir + "user-places.xbel";
    m_sharedBookmarkManager = KBookmarkManager::managerForExternalFile(file);

    connect(m_sharedBookmarkManager, SIGNAL(changed(QString,QString)),
              this, SLOT(slotSharedBookmarksChanged()));
    connect(m_sharedBookmarkManager, SIGNAL(bookmarksChanged(QString)),
              this, SLOT(slotSharedBookmarksChanged()));

    connect(m_placesBookmarkManager, SIGNAL(changed(QString,QString)),
              this, SLOT(slotBookmarksChanged()));
    connect(m_placesBookmarkManager, SIGNAL(bookmarksChanged(QString)),
              this, SLOT(slotBookmarksChanged()));

    integrateSharedBookmarks();
}

bool KFilePlacesSharedBookmarks::integrateSharedBookmarks()
{
    KBookmarkGroup root = m_placesBookmarkManager->root();
    KBookmark bookmark = root.first();

    KBookmarkGroup sharedRoot = m_sharedBookmarkManager->root();
    KBookmark sharedBookmark = sharedRoot.first();

    bool dirty = false;

    while (!bookmark.isNull()) {
        //qDebug() << "importing" << bookmark.text();

        // skip over system items
        if (bookmark.metaDataItem("isSystemItem") == "true") {
            bookmark = root.next(bookmark);
            continue;
        }

        // do the bookmarks match?
        if (!sharedBookmark.isNull() && compareBookmarks(bookmark, sharedBookmark)) {
            //qDebug() << "excat comparing: targetbk:\n" << nodeAsString(bookmark.internalElement()) << "\nsourcbk:\n" << nodeAsString(sharedBookmark.internalElement());

            if (!exactCompareBookmarks(bookmark, sharedBookmark)) {
                KBookmark cloneTarget=bookmark;
                KBookmark cloneSource = sharedBookmark;

                sharedBookmark = sharedRoot.next(sharedBookmark);
                bookmark = root.next(bookmark);

                //qDebug() << "cloning" << cloneSource.text();
                //qDebug() << "cloning: target=\n" << nodeAsString(cloneTarget.internalElement()) << "\n source:\n" << nodeAsString(cloneSource.internalElement());

                cloneBookmarkContents(cloneTarget, cloneSource);
                dirty = true;
                continue;
            } else {
                //qDebug() << "keeping" << bookmark.text();
            }
            sharedBookmark = sharedRoot.next(sharedBookmark);
            bookmark = root.next(bookmark);
            continue;
        }

        // they don't match -> remove
        //qDebug() << "removing" << bookmark.text();
        KBookmark bookmarkToRemove = bookmark;
        bookmark = root.next(bookmark);
        root.deleteBookmark(bookmarkToRemove);

        dirty = true;
    }

    // append the remaining shared bookmarks
    while(!sharedBookmark.isNull()) {
        root.addBookmark(cloneBookmark(sharedBookmark));
        sharedBookmark = sharedRoot.next(sharedBookmark);
        dirty = true;
    }

    return dirty;
}

bool KFilePlacesSharedBookmarks::exportSharedBookmarks()
{
    KBookmarkGroup root = m_placesBookmarkManager->root();
    KBookmark bookmark = root.first();

    KBookmarkGroup sharedRoot = m_sharedBookmarkManager->root();
    KBookmark sharedBookmark = sharedRoot.first();

    bool dirty = false;

    // first check if they are the same
    int count=0;
    while (!bookmark.isNull()) {
        //qDebug() << "exporting..." << bookmark.text();

        // skip over system items
        if (bookmark.metaDataItem("isSystemItem") == "true") {
          bookmark = root.next(bookmark);
          continue;
        }
        count++;

        // end of sharedBookmarks?
        if (sharedBookmark.isNull()) {
            dirty=true;
            break;
        }

        // do the bookmarks match?
        if (compareBookmarks(bookmark, sharedBookmark)) {
            if (!exactCompareBookmarks(bookmark, sharedBookmark)) {
                dirty = true;
                break;
            }
        } else {
            dirty=true;
            break;
        }
        sharedBookmark = sharedRoot.next(sharedBookmark);
        bookmark = root.next(bookmark);
    }

    //qDebug() << "dirty=" << dirty << " oldsize=" << bookmarkGroupSize(sharedRoot) << " count=" << count;

    if (bookmarkGroupSize(sharedRoot) != count)
        dirty=true;

    if (dirty) {
        emptyBookmarkGroup(sharedRoot);

        // append all bookmarks
        KBookmark bookmark = root.first();

        while(!bookmark.isNull()) {

            if (bookmark.metaDataItem("isSystemItem") == "true") {
              bookmark = root.next(bookmark);
              continue;
            }

            sharedRoot.addBookmark(cloneBookmark(bookmark));
            bookmark = root.next(bookmark);
            dirty = true;
        }
    }

    return dirty;

}

void KFilePlacesSharedBookmarks::slotSharedBookmarksChanged()
{
    //qDebug() << "shared bookmarks changed";
    bool dirty = integrateSharedBookmarks();
    if (dirty) m_placesBookmarkManager->emitChanged();
}

void KFilePlacesSharedBookmarks::slotBookmarksChanged()
{
    //qDebug() << "places bookmarks changed";
    bool dirty = exportSharedBookmarks();
    if (dirty) m_sharedBookmarkManager->emitChanged();
}

#include "moc_kfileplacessharedbookmarks_p.cpp"
