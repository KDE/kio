/* This file is part of the KDE project
   Copyright (C) 2006 David Faure <faure@kde.org>

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

#include "kdirmodel.h"
#include "kdirlister.h"
#include "kfileitem.h"
#include <kiconloader.h>
#include <klocalizedstring.h>
#include <kjobuidelegate.h>
#include <kio/copyjob.h>
#include <kio/fileundomanager.h>
#include "joburlcache_p.h"
#include <kurlmimedata.h>
#include <kiconloader.h>
#include <QMimeData>
#include <QBitArray>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QLocale>
#include <qplatformdefs.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class KDirModelNode;
class KDirModelDirNode;

static QUrl cleanupUrl(const QUrl& url) {
    QUrl u = url;
    u.setPath(QDir::cleanPath(u.path())); // remove double slashes in the path, simplify "foo/." to "foo/", etc.
    u = u.adjusted(QUrl::StripTrailingSlash); // KDirLister does this too, so we remove the slash before comparing with the root node url.
    u.setQuery(QString());
    u.setFragment(QString());
    return u;
}

// We create our own tree behind the scenes to have fast lookup from an item to its parent,
// and also to get the children of an item fast.
class KDirModelNode
{
public:
    KDirModelNode( KDirModelDirNode* parent, const KFileItem& item ) :
        m_item(item),
        m_parent(parent),
        m_preview()
    {
    }
    // m_item is KFileItem() for the root item
    const KFileItem& item() const { return m_item; }
    void setItem(const KFileItem& item) { m_item = item; }
    KDirModelDirNode* parent() const { return m_parent; }
    // linear search
    int rowNumber() const; // O(n)
    QIcon preview() const { return m_preview; }
    void setPreview( const QPixmap& pix ) {  m_preview = QIcon(); m_preview.addPixmap(pix); }
    void setPreview( const QIcon& icn ) { m_preview = icn; }

private:
    KFileItem m_item;
    KDirModelDirNode* const m_parent;
    QIcon m_preview;
};

// Specialization for directory nodes
class KDirModelDirNode : public KDirModelNode
{
public:
    KDirModelDirNode( KDirModelDirNode* parent, const KFileItem& item)
        : KDirModelNode( parent, item),
          m_childNodes(),
          m_childCount(KDirModel::ChildCountUnknown),
          m_populated(false)
    {}
    ~KDirModelDirNode() {
        qDeleteAll(m_childNodes);
    }
    QList<KDirModelNode *> m_childNodes; // owns the nodes

    // If we listed the directory, the child count is known. Otherwise it can be set via setChildCount.
    int childCount() const { return m_childNodes.isEmpty() ? m_childCount : m_childNodes.count(); }
    void setChildCount(int count) { m_childCount = count; }
    bool isPopulated() const { return m_populated; }
    void setPopulated( bool populated ) { m_populated = populated; }
    bool isSlow() const { return item().isSlow(); }

    // For removing all child urls from the global hash.
    void collectAllChildUrls(QList<QUrl> &urls) const {
        Q_FOREACH(KDirModelNode* node, m_childNodes) {
            const KFileItem& item = node->item();
            urls.append(cleanupUrl(item.url()));
            if (item.isDir())
                static_cast<KDirModelDirNode*>(node)->collectAllChildUrls(urls);
        }
    }

private:
    int m_childCount:31;
    bool m_populated:1;
};

int KDirModelNode::rowNumber() const
{
    if (!m_parent) return 0;
    return m_parent->m_childNodes.indexOf(const_cast<KDirModelNode*>(this));
}

////

class KDirModelPrivate
{
public:
    KDirModelPrivate( KDirModel* model )
        : q(model), m_dirLister(0),
          m_rootNode(new KDirModelDirNode(0, KFileItem())),
          m_dropsAllowed(KDirModel::NoDrops), m_jobTransfersVisible(false)
    {
    }
    ~KDirModelPrivate() {
        delete m_rootNode;
    }

    void _k_slotNewItems(const QUrl& directoryUrl, const KFileItemList&);
    void _k_slotDeleteItems(const KFileItemList&);
    void _k_slotRefreshItems(const QList<QPair<KFileItem, KFileItem> >&);
    void _k_slotClear();
    void _k_slotRedirection(const QUrl& oldUrl, const QUrl& newUrl);
    void _k_slotJobUrlsChanged(const QStringList& urlList);

    void clear() {
        delete m_rootNode;
        m_rootNode = new KDirModelDirNode(0, KFileItem());
    }
    // Emit expand for each parent and then return the
    // last known parent if there is no node for this url
    KDirModelNode* expandAllParentsUntil(const QUrl& url) const;

    // Return the node for a given url, using the hash.
    KDirModelNode* nodeForUrl(const QUrl& url) const;
    KDirModelNode* nodeForIndex(const QModelIndex& index) const;
    QModelIndex indexForNode(KDirModelNode* node, int rowNumber = -1 /*unknown*/) const;
    bool isDir(KDirModelNode* node) const {
        return (node == m_rootNode) || node->item().isDir();
    }
    QUrl urlForNode(KDirModelNode* node) const {
        /**
         * Queries and fragments are removed from the URL, so that the URL of
         * child items really starts with the URL of the parent.
         *
         * For instance ksvn+http://url?rev=100 is the parent for ksvn+http://url/file?rev=100
         * so we have to remove the query in both to be able to compare the URLs
         */
        QUrl url(node == m_rootNode ? m_dirLister->url() : node->item().url());
        if (url.hasQuery() || url.hasFragment()) { // avoid detach if not necessary.
            url.setQuery(QString());
            url.setFragment(QString()); // kill ref (#171117)
        }
        return url;
    }
    void removeFromNodeHash(KDirModelNode* node, const QUrl& url);
#ifndef NDEBUG
    void dump();
#endif

    KDirModel* q;
    KDirLister* m_dirLister;
    KDirModelDirNode* m_rootNode;
    KDirModel::DropsAllowed m_dropsAllowed;
    bool m_jobTransfersVisible;
    // key = current known parent node (always a KDirModelDirNode but KDirModelNode is more convenient),
    // value = final url[s] being fetched
    QMap<KDirModelNode*, QList<QUrl> > m_urlsBeingFetched;
    QHash<QUrl, KDirModelNode *> m_nodeHash; // global node hash: url -> node
    QStringList m_allCurrentDestUrls; //list of all dest urls that have jobs on them (e.g. copy, download)
};

KDirModelNode* KDirModelPrivate::nodeForUrl(const QUrl& _url) const // O(1), well, O(length of url as a string)
{
    QUrl url = cleanupUrl(_url);
    if (url == urlForNode(m_rootNode))
        return m_rootNode;
    return m_nodeHash.value(url);
}

void KDirModelPrivate::removeFromNodeHash(KDirModelNode* node, const QUrl& url)
{
    if (node->item().isDir()) {
        QList<QUrl> urls;
        static_cast<KDirModelDirNode *>(node)->collectAllChildUrls(urls);
        Q_FOREACH(const QUrl& u, urls) {
            m_nodeHash.remove(u);
        }
    }
    m_nodeHash.remove(cleanupUrl(url));
}

KDirModelNode* KDirModelPrivate::expandAllParentsUntil(const QUrl& _url) const // O(depth)
{
    QUrl url = cleanupUrl(_url);

    //qDebug() << url;
    QUrl nodeUrl = urlForNode(m_rootNode);
    if (url == nodeUrl)
        return m_rootNode;

    // Protocol mismatch? Don't even start comparing paths then. #171721
    if (url.scheme() != nodeUrl.scheme())
        return 0;

    const QString pathStr = url.path(); // no trailing slash
    KDirModelDirNode* dirNode = m_rootNode;

    if (!pathStr.startsWith(nodeUrl.path())) {
        return 0;
    }

    for (;;) {
        QString nodePath = nodeUrl.path();
        if (!nodePath.endsWith('/')) {
            nodePath += '/';
        }
        if(!pathStr.startsWith(nodePath)) {
            qWarning() << "The kioslave for" << url.scheme() << "violates the hierarchy structure:"
                         << "I arrived at node" << nodePath << ", but" << pathStr << "does not start with that path.";
            return 0;
        }

        // E.g. pathStr is /a/b/c and nodePath is /a/. We want to find the node with url /a/b
        const int nextSlash = pathStr.indexOf('/', nodePath.length());
        const QString newPath = pathStr.left(nextSlash); // works even if nextSlash==-1
        nodeUrl.setPath(newPath);
        nodeUrl = nodeUrl.adjusted(QUrl::StripTrailingSlash); // #172508
        KDirModelNode* node = nodeForUrl(nodeUrl);
        if (!node) {
            //qDebug() << "child equal or starting with" << url << "not found";
            // return last parent found:
            return dirNode;
        }

        emit q->expand(indexForNode(node));

        //qDebug() << " nodeUrl=" << nodeUrl;
        if (nodeUrl == url) {
            //qDebug() << "Found node" << node << "for" << url;
            return node;
        }
        //qDebug() << "going into" << node->item().url();
        Q_ASSERT(isDir(node));
        dirNode = static_cast<KDirModelDirNode *>(node);
    }
    // NOTREACHED
    //return 0;
}

#ifndef NDEBUG
void KDirModelPrivate::dump()
{
    qDebug() << "Dumping contents of KDirModel" << q << "dirLister url:" << m_dirLister->url();
    QHashIterator<QUrl, KDirModelNode *> it(m_nodeHash);
    while (it.hasNext()) {
        it.next();
        qDebug() << it.key() << it.value();
    }
}
#endif

// node -> index. If rowNumber is set (or node is root): O(1). Otherwise: O(n).
QModelIndex KDirModelPrivate::indexForNode(KDirModelNode* node, int rowNumber) const
{
    if (node == m_rootNode)
        return QModelIndex();

    Q_ASSERT(node->parent());
    return q->createIndex(rowNumber == -1 ? node->rowNumber() : rowNumber, 0, node);
}

// index -> node. O(1)
KDirModelNode* KDirModelPrivate::nodeForIndex(const QModelIndex& index) const
{
    return index.isValid()
        ? static_cast<KDirModelNode*>(index.internalPointer())
        : m_rootNode;
}

/*
 * This model wraps the data held by KDirLister.
 *
 * The internal pointer of the QModelIndex for a given file is the node for that file in our own tree.
 * E.g. index(2,0) returns a QModelIndex with row=2 internalPointer=<KDirModelNode for the 3rd child of the root>
 *
 * Invalid parent index means root of the tree, m_rootNode
 */

#ifndef NDEBUG
static QString debugIndex(const QModelIndex& index)
{
    QString str;
    if (!index.isValid())
        str = "[invalid index, i.e. root]";
    else {
        KDirModelNode* node = static_cast<KDirModelNode*>(index.internalPointer());
        str = "[index for " + node->item().url().toString();
        if (index.column() > 0)
            str += ", column " + QString::number(index.column());
        str += ']';
    }
    return str;
}
#endif

KDirModel::KDirModel(QObject* parent)
    : QAbstractItemModel(parent),
      d(new KDirModelPrivate(this))
{
    setDirLister(new KDirLister(this));
}

KDirModel::~KDirModel()
{
    delete d;
}

void KDirModel::setDirLister(KDirLister* dirLister)
{
    if (d->m_dirLister) {
        d->clear();
        delete d->m_dirLister;
    }
    d->m_dirLister = dirLister;
    d->m_dirLister->setParent(this);
    connect( d->m_dirLister, SIGNAL(itemsAdded(QUrl,KFileItemList)),
             this, SLOT(_k_slotNewItems(QUrl,KFileItemList)) );
    connect( d->m_dirLister, SIGNAL(itemsDeleted(KFileItemList)),
             this, SLOT(_k_slotDeleteItems(KFileItemList)) );
    connect( d->m_dirLister, SIGNAL(refreshItems(QList<QPair<KFileItem,KFileItem> >)),
             this, SLOT(_k_slotRefreshItems(QList<QPair<KFileItem,KFileItem> >)) );
    connect( d->m_dirLister, SIGNAL(clear()),
             this, SLOT(_k_slotClear()) );
    connect(d->m_dirLister, SIGNAL(redirection(QUrl,QUrl)),
            this, SLOT(_k_slotRedirection(QUrl,QUrl)));
}

KDirLister* KDirModel::dirLister() const
{
    return d->m_dirLister;
}

void KDirModelPrivate::_k_slotNewItems(const QUrl& directoryUrl, const KFileItemList& items)
{
    //qDebug() << "directoryUrl=" << directoryUrl;

    KDirModelNode* result = nodeForUrl(directoryUrl); // O(depth)
    // If the directory containing the items wasn't found, then we have a big problem.
    // Are you calling KDirLister::openUrl(url,true,false)? Please use expandToUrl() instead.
    if (!result) {
        qWarning() << "Items emitted in directory" << directoryUrl
                     << "but that directory isn't in KDirModel!"
                     << "Root directory:" << urlForNode(m_rootNode);
        Q_FOREACH(const KFileItem& item, items) {
            qDebug() << "Item:" << item.url();
        }
#ifndef NDEBUG
        dump();
#endif
        Q_ASSERT(result);
    }
    Q_ASSERT(isDir(result));
    KDirModelDirNode* dirNode = static_cast<KDirModelDirNode *>(result);

    const QModelIndex index = indexForNode(dirNode); // O(n)
    const int newItemsCount = items.count();
    const int newRowCount = dirNode->m_childNodes.count() + newItemsCount;
#if 0
#ifndef NDEBUG // debugIndex only defined in debug mode
    //qDebug() << items.count() << "in" << directoryUrl
             << "index=" << debugIndex(index) << "newRowCount=" << newRowCount;
#endif
#endif

    q->beginInsertRows( index, newRowCount - newItemsCount, newRowCount - 1 ); // parent, first, last

    const QList<QUrl> urlsBeingFetched = m_urlsBeingFetched.value(dirNode);
    //qDebug() << "urlsBeingFetched for dir" << dirNode << directoryUrl << ":" << urlsBeingFetched;

    QList<QModelIndex> emitExpandFor;

    KFileItemList::const_iterator it = items.begin();
    KFileItemList::const_iterator end = items.end();
    for ( ; it != end ; ++it ) {
        const bool isDir = it->isDir();
        KDirModelNode* node = isDir
                              ? new KDirModelDirNode( dirNode, *it )
                              : new KDirModelNode( dirNode, *it );
#ifndef NDEBUG
        // Test code for possible duplication of items in the childnodes list,
        // not sure if/how it ever happened.
        //if (dirNode->m_childNodes.count() &&
        //    dirNode->m_childNodes.last()->item().name() == (*it).name()) {
        //    qWarning() << "Already having" << (*it).name() << "in" << directoryUrl
        //             << "url=" << dirNode->m_childNodes.last()->item().url();
        //    abort();
        //}
#endif
        dirNode->m_childNodes.append(node);
        const QUrl url = it->url();
        m_nodeHash.insert(cleanupUrl(url), node);
        //qDebug() << url;

        if (!urlsBeingFetched.isEmpty()) {
            const QUrl dirUrl(url);
            foreach(const QUrl& urlFetched, urlsBeingFetched) {
                if (dirUrl.matches(urlFetched, QUrl::StripTrailingSlash) || dirUrl.isParentOf(urlFetched)) {
                    //qDebug() << "Listing found" << dirUrl.url() << "which is a parent of fetched url" << urlFetched;
                    const QModelIndex parentIndex = indexForNode(node, dirNode->m_childNodes.count()-1);
                    Q_ASSERT(parentIndex.isValid());
                    emitExpandFor.append(parentIndex);
                    if (isDir && dirUrl != urlFetched) {
                        q->fetchMore(parentIndex);
                        m_urlsBeingFetched[node].append(urlFetched);
                    }
                }
            }
        }
    }

    m_urlsBeingFetched.remove(dirNode);

    q->endInsertRows();

    // Emit expand signal after rowsInserted signal has been emitted,
    // so that any proxy model will have updated its mapping already
    Q_FOREACH(const QModelIndex& idx, emitExpandFor) {
        emit q->expand(idx);
    }
}

void KDirModelPrivate::_k_slotDeleteItems(const KFileItemList& items)
{
    //qDebug() << items.count();

    // I assume all items are from the same directory.
    // From KDirLister's code, this should be the case, except maybe emitChanges?
    const KFileItem item = items.first();
    Q_ASSERT(!item.isNull());
    QUrl url = item.url();
    KDirModelNode* node = nodeForUrl(url); // O(depth)
    if (!node) {
        qWarning() << "No node found for item that was just removed:" << url;
        return;
    }

    KDirModelDirNode* dirNode = node->parent();
    if (!dirNode)
        return;

    QModelIndex parentIndex = indexForNode(dirNode); // O(n)

    // Short path for deleting a single item
    if (items.count() == 1) {
        const int r = node->rowNumber();
        q->beginRemoveRows(parentIndex, r, r);
        removeFromNodeHash(node, url);
        delete dirNode->m_childNodes.takeAt(r);
        q->endRemoveRows();
        return;
    }

    // We need to make lists of consecutive row numbers, for the beginRemoveRows call.
    // Let's use a bit array where each bit represents a given child node.
    const int childCount = dirNode->m_childNodes.count();
    QBitArray rowNumbers(childCount, false);
    Q_FOREACH(const KFileItem& item, items) {
        if (!node) { // don't lookup the first item twice
            url = item.url();
            node = nodeForUrl(url);
            if (!node) {
                qWarning() << "No node found for item that was just removed:" << url;
                continue;
            }
            if (!node->parent()) {
                // The root node has been deleted, but it was not first in the list 'items'.
                // see https://bugs.kde.org/show_bug.cgi?id=196695
                return;
            }
        }
        rowNumbers.setBit(node->rowNumber(), 1); // O(n)
        removeFromNodeHash(node, url);
        node = 0;
    }

    int start = -1;
    int end = -1;
    bool lastVal = false;
    // Start from the end, otherwise all the row numbers are offset while we go
    for (int i = childCount - 1; i >= 0; --i) {
        const bool val = rowNumbers.testBit(i);
        if (!lastVal && val) {
            end = i;
            //qDebug() << "end=" << end;
        }
        if ((lastVal && !val) || (i == 0 && val)) {
            start = val ? i : i + 1;
            //qDebug() << "beginRemoveRows" << start << end;
            q->beginRemoveRows(parentIndex, start, end);
            for (int r = end; r >= start; --r) { // reverse because takeAt changes indexes ;)
                //qDebug() << "Removing from m_childNodes at" << r;
                delete dirNode->m_childNodes.takeAt(r);
            }
            q->endRemoveRows();
        }
        lastVal = val;
    }
}

void KDirModelPrivate::_k_slotRefreshItems(const QList<QPair<KFileItem, KFileItem> >& items)
{
    QModelIndex topLeft, bottomRight;

    // Solution 1: we could emit dataChanged for one row (if items.size()==1) or all rows
    // Solution 2: more fine-grained, actually figure out the beginning and end rows.
    for ( QList<QPair<KFileItem, KFileItem> >::const_iterator fit = items.begin(), fend = items.end() ; fit != fend ; ++fit ) {
        Q_ASSERT(!fit->first.isNull());
        Q_ASSERT(!fit->second.isNull());
        const QUrl oldUrl = fit->first.url();
        const QUrl newUrl = fit->second.url();
        KDirModelNode* node = nodeForUrl(oldUrl); // O(n); maybe we could look up to the parent only once
        //qDebug() << "in model for" << m_dirLister->url() << ":" << oldUrl << "->" << newUrl << "node=" << node;
        if (!node) // not found [can happen when renaming a dir, redirection was emitted already]
            continue;
        if (node != m_rootNode) { // we never set an item in the rootnode, we use m_dirLister->rootItem instead.
            bool hasNewNode = false;
            // A file became directory (well, it was overwritten)
            if (fit->first.isDir() != fit->second.isDir()) {
                //qDebug() << "DIR/FILE STATUS CHANGE";
                const int r = node->rowNumber();
                removeFromNodeHash(node, oldUrl);
                KDirModelDirNode* dirNode = node->parent();
                delete dirNode->m_childNodes.takeAt(r); // i.e. "delete node"
                node = fit->second.isDir() ? new KDirModelDirNode(dirNode, fit->second)
                       : new KDirModelNode(dirNode, fit->second);
                dirNode->m_childNodes.insert(r, node); // same position!
                hasNewNode = true;
            } else {
                node->setItem(fit->second);
            }

            if (oldUrl != newUrl || hasNewNode) {
                // What if a renamed dir had children? -> kdirlister takes care of emitting for each item
                //qDebug() << "Renaming" << oldUrl << "to" << newUrl << "in node hash";
                m_nodeHash.remove(cleanupUrl(oldUrl));
                m_nodeHash.insert(cleanupUrl(newUrl), node);
            }
            // Mimetype changed -> forget cached icon (e.g. from "cut", #164185 comment #13)
            if (fit->first.determineMimeType().name() != fit->second.determineMimeType().name()) {
                node->setPreview(QIcon());
            }

            const QModelIndex index = indexForNode(node);
            if (!topLeft.isValid() || index.row() < topLeft.row()) {
                topLeft = index;
            }
            if (!bottomRight.isValid() || index.row() > bottomRight.row()) {
                bottomRight = index;
            }
        }
    }
#ifndef NDEBUG // debugIndex only defined in debug mode
    //qDebug() << "dataChanged(" << debugIndex(topLeft) << " - " << debugIndex(bottomRight);
    Q_UNUSED(debugIndex(QModelIndex())); // fix compiler warning
#endif
    bottomRight = bottomRight.sibling(bottomRight.row(), q->columnCount(QModelIndex())-1);
    emit q->dataChanged(topLeft, bottomRight);
}

// Called when a kioslave redirects (e.g. smb:/Workgroup -> smb://workgroup)
// and when renaming a directory.
void KDirModelPrivate::_k_slotRedirection(const QUrl& oldUrl, const QUrl& newUrl)
{
    KDirModelNode* node = nodeForUrl(oldUrl);
    if (!node)
        return;
    m_nodeHash.remove(cleanupUrl(oldUrl));
    m_nodeHash.insert(cleanupUrl(newUrl), node);

    // Ensure the node's URL is updated. In case of a listjob redirection
    // we won't get a refreshItem, and in case of renaming a directory
    // we'll get it too late (so the hash won't find the old url anymore).
    KFileItem item = node->item();
    if (!item.isNull()) { // null if root item, #180156
        item.setUrl(newUrl);
        node->setItem(item);
    }

    // The items inside the renamed directory have been handled before,
    // KDirLister took care of emitting refreshItem for each of them.
}

void KDirModelPrivate::_k_slotClear()
{
    const int numRows = m_rootNode->m_childNodes.count();
    if (numRows > 0) {
        q->beginRemoveRows( QModelIndex(), 0, numRows - 1 );
        q->endRemoveRows();
    }

    m_nodeHash.clear();
    //emit layoutAboutToBeChanged();
    clear();
    //emit layoutChanged();
}

void KDirModelPrivate::_k_slotJobUrlsChanged(const QStringList& urlList)
{
    m_allCurrentDestUrls = urlList;
}

void KDirModel::itemChanged( const QModelIndex& index )
{
    // This method is really a itemMimeTypeChanged(), it's mostly called by KMimeTypeResolver.
    // When the mimetype is determined, clear the old "preview" (could be
    // mimetype dependent like when cutting files, #164185)
    KDirModelNode* node = d->nodeForIndex(index);
    if (node)
        node->setPreview(QIcon());

#ifndef NDEBUG // debugIndex only defined in debug mode
    //qDebug() << "dataChanged(" << debugIndex(index);
#endif
    emit dataChanged(index, index);
}

int KDirModel::columnCount( const QModelIndex & ) const
{
    return ColumnCount;
}

QVariant KDirModel::data( const QModelIndex & index, int role ) const
{
    if (index.isValid()) {
        KDirModelNode* node = static_cast<KDirModelNode*>(index.internalPointer());
        const KFileItem& item( node->item() );
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case Name:
                return item.text();
            case Size:
                //
                //return KIO::convertSize(item->size());
                // Default to "file size in bytes" like in kde3's filedialog
                return QLocale().toString(item.size());
            case ModifiedTime: {
                QDateTime dt = item.time(KFileItem::ModificationTime);
                return dt.toString();
            }
            case Permissions:
                return item.permissionsString();
            case Owner:
                return item.user();
            case Group:
                return item.group();
            case Type:
                return item.mimeComment();
            }
            break;
        case Qt::EditRole:
            switch (index.column()) {
            case Name:
                return item.text();
            }
            break;
        case Qt::DecorationRole:
            if (index.column() == Name) {
                if (!node->preview().isNull()) {
                    //qDebug() << item->url() << " preview found";
                    return node->preview();
                }
                Q_ASSERT(!item.isNull());
                //qDebug() << item->url() << " overlays=" << item->overlays();
                return KDE::icon(item.iconName(), item.overlays());
            }
            break;
        case Qt::TextAlignmentRole:
            if (index.column() == Size) {
                // use a right alignment for L2R and R2L languages
                const Qt::Alignment alignment = Qt::AlignRight | Qt::AlignVCenter;
                return int(alignment);
            }
            break;
        case Qt::ToolTipRole:
            return item.text();
        case FileItemRole:
            return QVariant::fromValue(item);
        case ChildCountRole:
            if (!item.isDir())
                return ChildCountUnknown;
            else {
                KDirModelDirNode* dirNode = static_cast<KDirModelDirNode *>(node);
                int count = dirNode->childCount();
                if (count == ChildCountUnknown && item.isReadable() && !dirNode->isSlow()) {
                    const QString path = item.localPath();
                    if (!path.isEmpty()) {
//                        slow
//                        QDir dir(path);
//                        count = dir.entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::System).count();
#ifdef Q_OS_WIN
                        QString s = path + QLatin1String( "\\*.*" );
                        s.replace('/', '\\');
                        count = 0;
                        WIN32_FIND_DATA findData;
                        HANDLE hFile = FindFirstFile( (LPWSTR)s.utf16(), &findData );
                        if( hFile != INVALID_HANDLE_VALUE ) {
                            do {
                                if (!( findData.cFileName[0] == '.' &&
                                       findData.cFileName[1] == '\0' ) &&
                                    !( findData.cFileName[0] == '.' &&
                                       findData.cFileName[1] == '.' &&
                                       findData.cFileName[2] == '\0' ) )
                                    ++count;
                            } while( FindNextFile( hFile, &findData ) != 0 );
                            FindClose( hFile );
                        }
#else
                        DIR* dir = QT_OPENDIR(QFile::encodeName(path));
                        if (dir) {
                            count = 0;
                            QT_DIRENT *dirEntry = 0;
                            while ((dirEntry = QT_READDIR(dir))) {
                                if (dirEntry->d_name[0] == '.') {
                                    if (dirEntry->d_name[1] == '\0') // skip "."
                                        continue;
                                    if (dirEntry->d_name[1] == '.' && dirEntry->d_name[2] == '\0') // skip ".."
                                        continue;
                                }
                                ++count;
                            }
                            QT_CLOSEDIR(dir);
                        }
#endif
                        //qDebug() << "child count for " << path << ":" << count;
                        dirNode->setChildCount(count);
                    }
                }
                return count;
            }
        case HasJobRole:
            if (d->m_jobTransfersVisible && d->m_allCurrentDestUrls.isEmpty() == false) {
                KDirModelNode* node = d->nodeForIndex(index);
                const QString url = node->item().url().toString();
                //return whether or not there are job dest urls visible in the view, so the delegate knows which ones to paint.
                return QVariant(d->m_allCurrentDestUrls.contains(url));
            }
        }
    }
    return QVariant();
}

void KDirModel::sort( int column, Qt::SortOrder order )
{
    // Not implemented - we should probably use QSortFilterProxyModel instead.
    return QAbstractItemModel::sort(column, order);
}

bool KDirModel::setData( const QModelIndex & index, const QVariant & value, int role )
{
    switch (role) {
    case Qt::EditRole:
        if (index.column() == Name && value.type() == QVariant::String) {
            Q_ASSERT(index.isValid());
            KDirModelNode* node = static_cast<KDirModelNode*>(index.internalPointer());
            const KFileItem& item = node->item();
            const QString newName = value.toString();
            if (newName.isEmpty() || newName == item.text() || (newName == QLatin1String(".")) || (newName == QLatin1String("..")))
                return true;
            QUrl newUrl = item.url().adjusted(QUrl::RemoveFilename);
            newUrl.setPath(newUrl.path() + KIO::encodeFileName(newName));
            KIO::Job * job = KIO::moveAs(item.url(), newUrl, item.url().isLocalFile() ? KIO::HideProgressInfo : KIO::DefaultFlags);
            job->ui()->setAutoErrorHandlingEnabled(true);
            // undo handling
            KIO::FileUndoManager::self()->recordJob( KIO::FileUndoManager::Rename, QList<QUrl>() << item.url(), newUrl, job );
            return true;
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == Name) {
            Q_ASSERT(index.isValid());
            // Set new pixmap - e.g. preview
            KDirModelNode* node = static_cast<KDirModelNode*>(index.internalPointer());
            //qDebug() << "setting icon for " << node->item()->url();
            Q_ASSERT(node);
            if (value.type() == QVariant::Icon) {
                const QIcon icon(qvariant_cast<QIcon>(value));
                node->setPreview(icon);
            } else if (value.type() == QVariant::Pixmap) {
                node->setPreview(qvariant_cast<QPixmap>(value));
            }
            emit dataChanged(index, index);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

int KDirModel::rowCount( const QModelIndex & parent ) const
{
    KDirModelNode* node = d->nodeForIndex(parent);
    if (!node || !d->isDir(node)) // #176555
        return 0;

    KDirModelDirNode* parentNode = static_cast<KDirModelDirNode *>(node);
    Q_ASSERT(parentNode);
    const int count = parentNode->m_childNodes.count();
#if 0
    QStringList filenames;
    for (int i = 0; i < count; ++i) {
        filenames << d->urlForNode(parentNode->m_childNodes.at(i)).fileName();
    }
    //qDebug() << "rowCount for " << d->urlForNode(parentNode) << ": " << count << filenames;
#endif
    return count;
}

// sibling() calls parent() and isn't virtual! So parent() should be fast...
QModelIndex KDirModel::parent( const QModelIndex & index ) const
{
    if (!index.isValid())
        return QModelIndex();
    KDirModelNode* childNode = static_cast<KDirModelNode*>(index.internalPointer());
    Q_ASSERT(childNode);
    KDirModelNode* parentNode = childNode->parent();
    Q_ASSERT(parentNode);
    return d->indexForNode(parentNode); // O(n)
}

static bool lessThan(const QUrl &left, const QUrl &right)
{
    return left.toString().compare(right.toString()) < 0;
}

void KDirModel::requestSequenceIcon(const QModelIndex& index, int sequenceIndex)
{
    emit needSequenceIcon(index, sequenceIndex);
}

void KDirModel::setJobTransfersVisible(bool value)
{
    if(value) {
        d->m_jobTransfersVisible = true;
        connect(&JobUrlCache::instance(), SIGNAL(jobUrlsChanged(QStringList)), this, SLOT(_k_slotJobUrlsChanged(QStringList)), Qt::UniqueConnection);

        JobUrlCache::instance().requestJobUrlsChanged();
    } else {
        disconnect(this, SLOT(_k_slotJobUrlsChanged(QStringList)));
    }

}

bool KDirModel::jobTransfersVisible() const
{
    return d->m_jobTransfersVisible;
}

QList<QUrl> KDirModel::simplifiedUrlList(const QList<QUrl> &urls)
{
    if (!urls.count()) {
        return urls;
    }

    QList<QUrl> ret(urls);
    qSort(ret.begin(), ret.end(), lessThan);

    QList<QUrl>::iterator it = ret.begin();
    QUrl url = *it;
    ++it;
    while (it != ret.end()) {
        if (url.isParentOf(*it) || url == *it) {
            it = ret.erase(it);
        } else {
            url = *it;
            ++it;
        }
    }

    return ret;
}

QStringList KDirModel::mimeTypes( ) const
{
    return KUrlMimeData::mimeDataTypes();
}

QMimeData * KDirModel::mimeData( const QModelIndexList & indexes ) const
{
    QList<QUrl> urls, mostLocalUrls;
    bool canUseMostLocalUrls = true;
    foreach (const QModelIndex &index, indexes) {
        const KFileItem& item = d->nodeForIndex(index)->item();
        urls << item.url();
        bool isLocal;
        mostLocalUrls << item.mostLocalUrl(isLocal);
        if (!isLocal)
            canUseMostLocalUrls = false;
    }
    QMimeData *data = new QMimeData();
    const bool different = canUseMostLocalUrls && (mostLocalUrls != urls);
    urls = simplifiedUrlList(urls);
    if (different) {
        mostLocalUrls = simplifiedUrlList(mostLocalUrls);
        KUrlMimeData::setUrls(urls, mostLocalUrls, data);
    } else {
        data->setUrls(urls);
    }

    // for compatibility reasons (when dropping or pasting into kde3 applications)
    QString application_x_qiconlist;
    const int items = urls.count();
    for (int i = 0; i < items; i++) {
	const int offset = i*16;
	QString tmp("%1$@@$%2$@@$32$@@$32$@@$%3$@@$%4$@@$32$@@$16$@@$no data$@@$");
	application_x_qiconlist += tmp.arg(offset).arg(offset).arg(offset).arg(offset+40);
    }
    data->setData("application/x-qiconlist", application_x_qiconlist.toLatin1());

    return data;
}

// Public API; not much point in calling it internally
KFileItem KDirModel::itemForIndex( const QModelIndex& index ) const
{
    if (!index.isValid()) {
        return d->m_dirLister->rootItem();
    } else {
        return static_cast<KDirModelNode*>(index.internalPointer())->item();
    }
}

#ifndef KDE_NO_DEPRECATED
QModelIndex KDirModel::indexForItem( const KFileItem* item ) const
{
    // Note that we can only use the URL here, not the pointer.
    // KFileItems can be copied.
    return indexForUrl(item->url()); // O(n)
}
#endif

QModelIndex KDirModel::indexForItem( const KFileItem& item ) const
{
    // Note that we can only use the URL here, not the pointer.
    // KFileItems can be copied.
    return indexForUrl(item.url()); // O(n)
}

// url -> index. O(n)
QModelIndex KDirModel::indexForUrl(const QUrl& url) const
{
    KDirModelNode* node = d->nodeForUrl(url); // O(depth)
    if (!node) {
        //qDebug() << url << "not found";
        return QModelIndex();
    }
    return d->indexForNode(node); // O(n)
}

QModelIndex KDirModel::index( int row, int column, const QModelIndex & parent ) const
{
    KDirModelNode* parentNode = d->nodeForIndex(parent); // O(1)
    Q_ASSERT(parentNode);
    Q_ASSERT(d->isDir(parentNode));
    KDirModelNode* childNode = static_cast<KDirModelDirNode *>(parentNode)->m_childNodes.value(row); // O(1)
    if (childNode)
        return createIndex(row, column, childNode);
    else
        return QModelIndex();
}

QVariant KDirModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if (orientation == Qt::Horizontal) {
        switch (role) {
        case Qt::DisplayRole:
            switch (section) {
            case Name:
                return i18nc("@title:column","Name");
            case Size:
                return i18nc("@title:column","Size");
            case ModifiedTime:
                return i18nc("@title:column","Date");
            case Permissions:
                return i18nc("@title:column","Permissions");
            case Owner:
                return i18nc("@title:column","Owner");
            case Group:
                return i18nc("@title:column","Group");
            case Type:
                return i18nc("@title:column","Type");
            }
        }
    }
    return QVariant();
}

bool KDirModel::hasChildren( const QModelIndex & parent ) const
{
    if (!parent.isValid())
        return true;

    const KFileItem& parentItem = static_cast<KDirModelNode*>(parent.internalPointer())->item();
    Q_ASSERT(!parentItem.isNull());
    return parentItem.isDir();
}

Qt::ItemFlags KDirModel::flags( const QModelIndex & index ) const
{
    Qt::ItemFlags f = Qt::ItemIsEnabled;
    if (index.column() == Name) {
        f |= Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
    }

    // Allow dropping onto this item?
    if (d->m_dropsAllowed != NoDrops) {
        if(!index.isValid()) {
            if (d->m_dropsAllowed & DropOnDirectory) {
                f |= Qt::ItemIsDropEnabled;
            }
        } else {
            KFileItem item = itemForIndex(index);
            if (item.isNull()) {
                qWarning() << "Invalid item returned for index";
            } else if (item.isDir()) {
                if (d->m_dropsAllowed & DropOnDirectory) {
                    f |= Qt::ItemIsDropEnabled;
                }
            } else { // regular file item
                if (d->m_dropsAllowed & DropOnAnyFile)
                    f |= Qt::ItemIsDropEnabled;
                else if (d->m_dropsAllowed & DropOnLocalExecutable) {
                    if (!item.localPath().isEmpty()) {
                        // Desktop file?
                        if (item.determineMimeType().inherits("application/x-desktop"))
                            f |= Qt::ItemIsDropEnabled;
                        // Executable, shell script ... ?
                        else if ( QFileInfo( item.localPath() ).isExecutable() )
                            f |= Qt::ItemIsDropEnabled;
                    }
                }
            }
        }
    }

    return f;
}

bool KDirModel::canFetchMore( const QModelIndex & parent ) const
{
    if (!parent.isValid())
        return false;

    // We now have a bool KDirModelNode::m_populated,
    // to avoid calling fetchMore more than once on empty dirs.
    // But this wastes memory, and how often does someone open and re-open an empty dir in a treeview?
    // Maybe we can ask KDirLister "have you listed <url> already"? (to discuss with M. Brade)

    KDirModelNode* node = static_cast<KDirModelNode*>(parent.internalPointer());
    const KFileItem& item = node->item();
    return item.isDir() && !static_cast<KDirModelDirNode *>(node)->isPopulated()
        && static_cast<KDirModelDirNode *>(node)->m_childNodes.isEmpty();
}

void KDirModel::fetchMore( const QModelIndex & parent )
{
    if (!parent.isValid())
        return;

    KDirModelNode* parentNode = static_cast<KDirModelNode*>(parent.internalPointer());

    KFileItem parentItem = parentNode->item();
    Q_ASSERT(!parentItem.isNull());
    Q_ASSERT(parentItem.isDir());
    KDirModelDirNode* dirNode = static_cast<KDirModelDirNode *>(parentNode);
    if( dirNode->isPopulated() )
        return;
    dirNode->setPopulated( true );

    const QUrl parentUrl = parentItem.url();
    d->m_dirLister->openUrl(parentUrl, KDirLister::Keep);
}

bool KDirModel::dropMimeData( const QMimeData * data, Qt::DropAction action, int row, int column, const QModelIndex & parent )
{
    // Not sure we want to implement any drop handling at this level,
    // but for sure the default QAbstractItemModel implementation makes no sense for a dir model.
    Q_UNUSED(data);
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);
    return false;
}

void KDirModel::setDropsAllowed(DropsAllowed dropsAllowed)
{
    d->m_dropsAllowed = dropsAllowed;
}

void KDirModel::expandToUrl(const QUrl& url)
{
    // emit expand for each parent and return last parent
    KDirModelNode* result = d->expandAllParentsUntil(url); // O(depth)
    //qDebug() << url << result;

    if (!result) // doesn't seem related to our base url?
        return;
    if (!(result->item().isNull()) && result->item().url() == url) {
        // We have it already, nothing to do
        //qDebug() << "have it already item=" <<url /*result->item()*/;
        return;
    }

    d->m_urlsBeingFetched[result].append(url);

    if (result == d->m_rootNode) {
        //qDebug() << "Remembering to emit expand after listing the root url";
        // the root is fetched by default, so it must be currently being fetched
        return;
    }

    //qDebug() << "Remembering to emit expand after listing" << result->item().url();

    // start a new fetch to look for the next level down the URL
    const QModelIndex parentIndex = d->indexForNode(result); // O(n)
    Q_ASSERT(parentIndex.isValid());
    fetchMore(parentIndex);
}

bool KDirModel::insertRows(int , int, const QModelIndex&)
{
    return false;
}

bool KDirModel::insertColumns(int, int, const QModelIndex&)
{
    return false;
}

bool KDirModel::removeRows(int, int, const QModelIndex&)
{
    return false;
}

bool KDirModel::removeColumns(int, int, const QModelIndex&)
{
    return false;
}

#include "moc_kdirmodel.cpp"
