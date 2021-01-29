/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006-2019 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirmodel.h"
#include "kdirlister.h"
#include "kfileitem.h"

#include <KLocalizedString>
#include <KJobUiDelegate>
#include <kio/simplejob.h>
#include <kio/statjob.h>
#include <kio/fileundomanager.h>
#include "joburlcache_p.h"
#include <KUrlMimeData>
#include <KIconLoader>

#include <QLocale>
#include <QMimeData>
#include <QBitArray>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QLoggingCategory>
#include <qplatformdefs.h>

#include <algorithm>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

static QLoggingCategory category("kf.kio.widgets.kdirmodel", QtInfoMsg);

class KDirModelNode;
class KDirModelDirNode;

static QUrl cleanupUrl(const QUrl &url)
{
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
    KDirModelNode(KDirModelDirNode *parent, const KFileItem &item) :
        m_item(item),
        m_parent(parent),
        m_preview()
    {
    }
    virtual ~KDirModelNode()
    {
        // Required, code will delete ptrs to this or a subclass.
    }
    // m_item is KFileItem() for the root item
    const KFileItem &item() const
    {
        return m_item;
    }
    void setItem(const KFileItem &item)
    {
        m_item = item;
    }
    KDirModelDirNode *parent() const
    {
        return m_parent;
    }
    // linear search
    int rowNumber() const; // O(n)
    QIcon preview() const
    {
        return m_preview;
    }
    void setPreview(const QPixmap &pix)
    {
        m_preview = QIcon();
        m_preview.addPixmap(pix);
    }
    void setPreview(const QIcon &icn)
    {
        m_preview = icn;
    }

private:
    KFileItem m_item;
    KDirModelDirNode *const m_parent;
    QIcon m_preview;
};

// Specialization for directory nodes
class KDirModelDirNode : public KDirModelNode
{
public:
    KDirModelDirNode(KDirModelDirNode *parent, const KFileItem &item)
        : KDirModelNode(parent, item),
          m_childNodes(),
          m_childCount(KDirModel::ChildCountUnknown),
          m_populated(false)
    {}
    ~KDirModelDirNode() override
    {
        qDeleteAll(m_childNodes);
    }
    QList<KDirModelNode *> m_childNodes; // owns the nodes

    // If we listed the directory, the child count is known. Otherwise it can be set via setChildCount.
    int childCount() const
    {
        return m_childNodes.isEmpty() ? m_childCount : m_childNodes.count();
    }
    void setChildCount(int count)
    {
        m_childCount = count;
    }
    bool isPopulated() const
    {
        return m_populated;
    }
    void setPopulated(bool populated)
    {
        m_populated = populated;
    }
    bool isSlow() const
    {
        return item().isSlow();
    }

    // For removing all child urls from the global hash.
    void collectAllChildUrls(QList<QUrl> &urls) const
    {
        urls.reserve(urls.size() + m_childNodes.size());
        for (KDirModelNode *node : m_childNodes) {
            const KFileItem &item = node->item();
            urls.append(cleanupUrl(item.url()));
            if (item.isDir()) {
                static_cast<KDirModelDirNode *>(node)->collectAllChildUrls(urls);
            }
        }
    }

private:
    int m_childCount: 31;
    bool m_populated: 1;
};

int KDirModelNode::rowNumber() const
{
    if (!m_parent) {
        return 0;
    }
    return m_parent->m_childNodes.indexOf(const_cast<KDirModelNode *>(this));
}

////

class KDirModelPrivate
{
public:
    explicit KDirModelPrivate(KDirModel *model)
        : q(model), m_dirLister(nullptr),
          m_rootNode(new KDirModelDirNode(nullptr, KFileItem())),
          m_dropsAllowed(KDirModel::NoDrops), m_jobTransfersVisible(false)
    {
    }
    ~KDirModelPrivate()
    {
        delete m_rootNode;
    }

    void _k_slotNewItems(const QUrl &directoryUrl, const KFileItemList &);
    void _k_slotCompleted(const QUrl &directoryUrl);
    void _k_slotDeleteItems(const KFileItemList &);
    void _k_slotRefreshItems(const QList<QPair<KFileItem, KFileItem> > &);
    void _k_slotClear();
    void _k_slotRedirection(const QUrl &oldUrl, const QUrl &newUrl);
    void _k_slotJobUrlsChanged(const QStringList &urlList);

    void clear()
    {
        delete m_rootNode;
        m_rootNode = new KDirModelDirNode(nullptr, KFileItem());
        m_showNodeForListedUrl = false;
    }
    // Emit expand for each parent and then return the
    // last known parent if there is no node for this url
    KDirModelNode *expandAllParentsUntil(const QUrl &url) const;

    // Return the node for a given url, using the hash.
    KDirModelNode *nodeForUrl(const QUrl &url) const;
    KDirModelNode *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(KDirModelNode *node, int rowNumber = -1 /*unknown*/) const;

    static QUrl rootParentOf(const QUrl &url) {
        // <url> is what we listed, and which is visible at the root of the tree
        // Here we want the (invisible) parent of that url
        QUrl parent(url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash));
        if (url.path() == QLatin1String("/")) {
            parent.setPath(QString());
        }
        return parent;
    }
    bool isDir(KDirModelNode *node) const
    {
        return (node == m_rootNode) || node->item().isDir();
    }
    QUrl urlForNode(KDirModelNode *node) const
    {
        /**
         * Queries and fragments are removed from the URL, so that the URL of
         * child items really starts with the URL of the parent.
         *
         * For instance ksvn+http://url?rev=100 is the parent for ksvn+http://url/file?rev=100
         * so we have to remove the query in both to be able to compare the URLs
         */
        QUrl url;
        if (node == m_rootNode && !m_showNodeForListedUrl) {
            url = m_dirLister->url();
        } else {
            url = node->item().url();
        }
        if (url.hasQuery() || url.hasFragment()) { // avoid detach if not necessary.
            url.setQuery(QString());
            url.setFragment(QString()); // kill ref (#171117)
        }
        return url;
    }
    void removeFromNodeHash(KDirModelNode *node, const QUrl &url);
    void clearAllPreviews(KDirModelDirNode *node);
#ifndef NDEBUG
    void dump();
#endif
    Q_DISABLE_COPY(KDirModelPrivate)

    KDirModel * const q;
    KDirLister *m_dirLister;
    KDirModelDirNode *m_rootNode;
    KDirModel::DropsAllowed m_dropsAllowed;
    bool m_jobTransfersVisible;
    bool m_showNodeForListedUrl = false;
    // key = current known parent node (always a KDirModelDirNode but KDirModelNode is more convenient),
    // value = final url[s] being fetched
    QMap<KDirModelNode *, QList<QUrl> > m_urlsBeingFetched;
    QHash<QUrl, KDirModelNode *> m_nodeHash; // global node hash: url -> node
    QStringList m_allCurrentDestUrls; //list of all dest urls that have jobs on them (e.g. copy, download)
};

KDirModelNode *KDirModelPrivate::nodeForUrl(const QUrl &_url) const // O(1), well, O(length of url as a string)
{
    QUrl url = cleanupUrl(_url);
    if (url == urlForNode(m_rootNode)) {
        return m_rootNode;
    }
    return m_nodeHash.value(url);
}

void KDirModelPrivate::removeFromNodeHash(KDirModelNode *node, const QUrl &url)
{
    if (node->item().isDir()) {
        QList<QUrl> urls;
        static_cast<KDirModelDirNode *>(node)->collectAllChildUrls(urls);
        for (const QUrl &u : qAsConst(urls)) {
            m_nodeHash.remove(u);
        }
    }
    m_nodeHash.remove(cleanupUrl(url));
}

KDirModelNode *KDirModelPrivate::expandAllParentsUntil(const QUrl &_url) const // O(depth)
{
    QUrl url = cleanupUrl(_url);

    //qDebug() << url;
    QUrl nodeUrl = urlForNode(m_rootNode);
    KDirModelDirNode *dirNode = m_rootNode;
    if (m_showNodeForListedUrl && !m_rootNode->m_childNodes.isEmpty()) {
        dirNode = static_cast<KDirModelDirNode *>(m_rootNode->m_childNodes.at(0)); // ### will be incorrect if we list drives on Windows
        nodeUrl = dirNode->item().url();
        qCDebug(category) << "listed URL is visible, adjusted starting point to" << nodeUrl;
    }
    if (url == nodeUrl) {
        return dirNode;
    }

    // Protocol mismatch? Don't even start comparing paths then. #171721
    if (url.scheme() != nodeUrl.scheme()) {
        qCWarning(category) << "protocol mismatch:" << url.scheme() << "vs" << nodeUrl.scheme();
        return nullptr;
    }

    const QString pathStr = url.path(); // no trailing slash

    if (!pathStr.startsWith(nodeUrl.path())) {
        qCDebug(category) << pathStr << "does not start with" << nodeUrl.path();
        return nullptr;
    }

    for (;;) {
        QString nodePath = nodeUrl.path();
        if (!nodePath.endsWith(QLatin1Char('/'))) {
            nodePath += QLatin1Char('/');
        }
        if (!pathStr.startsWith(nodePath)) {
            qCWarning(category) << "The kioslave for" << url.scheme() << "violates the hierarchy structure:"
                       << "I arrived at node" << nodePath << ", but" << pathStr << "does not start with that path.";
            return nullptr;
        }

        // E.g. pathStr is /a/b/c and nodePath is /a/. We want to find the node with url /a/b
        const int nextSlash = pathStr.indexOf(QLatin1Char('/'), nodePath.length());
        const QString newPath = pathStr.left(nextSlash); // works even if nextSlash==-1
        nodeUrl.setPath(newPath);
        nodeUrl = nodeUrl.adjusted(QUrl::StripTrailingSlash); // #172508
        KDirModelNode *node = nodeForUrl(nodeUrl);
        if (!node) {
            qCDebug(category) << nodeUrl << "not found, needs to be listed";
            // return last parent found:
            return dirNode;
        }

        Q_EMIT q->expand(indexForNode(node));

        //qDebug() << " nodeUrl=" << nodeUrl;
        if (nodeUrl == url) {
            qCDebug(category) << "Found node" << node << "for" << url;
            return node;
        }
        qCDebug(category) << "going into" << node->item().url();
        Q_ASSERT(isDir(node));
        dirNode = static_cast<KDirModelDirNode *>(node);
    }
    // NOTREACHED
    //return 0;
}

#ifndef NDEBUG
void KDirModelPrivate::dump()
{
    qCDebug(category) << "Dumping contents of KDirModel" << q << "dirLister url:" << m_dirLister->url();
    QHashIterator<QUrl, KDirModelNode *> it(m_nodeHash);
    while (it.hasNext()) {
        it.next();
        qCDebug(category) << it.key() << it.value();
    }
}
#endif

// node -> index. If rowNumber is set (or node is root): O(1). Otherwise: O(n).
QModelIndex KDirModelPrivate::indexForNode(KDirModelNode *node, int rowNumber) const
{
    if (node == m_rootNode) {
        return QModelIndex();
    }

    Q_ASSERT(node->parent());
    return q->createIndex(rowNumber == -1 ? node->rowNumber() : rowNumber, 0, node);
}

// index -> node. O(1)
KDirModelNode *KDirModelPrivate::nodeForIndex(const QModelIndex &index) const
{
    return index.isValid()
           ? static_cast<KDirModelNode *>(index.internalPointer())
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

static QString debugIndex(const QModelIndex &index)
{
    QString str;
    if (!index.isValid()) {
        str = QStringLiteral("[invalid index, i.e. root]");
    } else {
        KDirModelNode *node = static_cast<KDirModelNode *>(index.internalPointer());
        str = QLatin1String("[index for ") + node->item().url().toString();
        if (index.column() > 0) {
            str += QLatin1String(", column ") + QString::number(index.column());
        }
        str += QLatin1Char(']');
    }
    return str;
}

KDirModel::KDirModel(QObject *parent)
    : QAbstractItemModel(parent),
      d(new KDirModelPrivate(this))
{
    setDirLister(new KDirLister(this));
}

KDirModel::~KDirModel()
{
    delete d;
}

void KDirModel::setDirLister(KDirLister *dirLister)
{
    if (d->m_dirLister) {
        d->clear();
        delete d->m_dirLister;
    }
    d->m_dirLister = dirLister;
    d->m_dirLister->setParent(this);
    connect(d->m_dirLister, &KCoreDirLister::itemsAdded, this,
        [this](const QUrl &dirUrl, const KFileItemList &items){d->_k_slotNewItems(dirUrl, items);} );
    connect(d->m_dirLister, &KCoreDirLister::listingDirCompleted, this, [this](const QUrl &dirUrl) {
        d->_k_slotCompleted(dirUrl);
    });
    connect(d->m_dirLister, &KCoreDirLister::itemsDeleted, this,
        [this](const KFileItemList &items){d->_k_slotDeleteItems(items);} );
    connect(d->m_dirLister, &KCoreDirLister::refreshItems, this,
        [this](const QList<QPair<KFileItem,KFileItem> > &items){d->_k_slotRefreshItems(items);} );
    connect(d->m_dirLister, QOverload<>::of(&KCoreDirLister::clear), this,
        [this](){d->_k_slotClear();} );
    connect(d->m_dirLister, QOverload<const QUrl&, const QUrl&>::of(&KCoreDirLister::redirection), this,
        [this](const QUrl &oldUrl, const QUrl &newUrl){d->_k_slotRedirection(oldUrl, newUrl);} );
}

void KDirModel::openUrl(const QUrl &inputUrl, OpenUrlFlags flags)
{
    Q_ASSERT(d->m_dirLister);
    const QUrl url = cleanupUrl(inputUrl);
    if (flags & ShowRoot) {
        d->_k_slotClear();
        d->m_showNodeForListedUrl = true;
        // Store the parent URL into the invisible root node
        const QUrl parentUrl = d->rootParentOf(url);
        d->m_rootNode->setItem(KFileItem(parentUrl));
        // Stat the requested url, to create the visible node
        KIO::StatJob *statJob = KIO::stat(url, KIO::HideProgressInfo);
        connect(statJob, &KJob::result, this, [statJob, parentUrl, url, this]() {
            if (!statJob->error()) {
                const KIO::UDSEntry entry = statJob->statResult();
                KFileItem visibleRootItem(entry, url);
                visibleRootItem.setName(url.path() == QLatin1String("/") ? QStringLiteral("/") : url.fileName());
                d->_k_slotNewItems(parentUrl, QList<KFileItem>{visibleRootItem});
                Q_ASSERT(d->m_rootNode->m_childNodes.count() == 1);
                expandToUrl(url);
            } else {
                qWarning() << statJob->errorString();
            }
        });
    } else {
        d->m_dirLister->openUrl(url, (flags & Reload) ? KDirLister::Reload : KDirLister::NoFlags);
    }
}

Qt::DropActions KDirModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction | Qt::IgnoreAction;
}


KDirLister *KDirModel::dirLister() const
{
    return d->m_dirLister;
}

void KDirModelPrivate::_k_slotNewItems(const QUrl &directoryUrl, const KFileItemList &items)
{
    //qDebug() << "directoryUrl=" << directoryUrl;

    KDirModelNode *result = nodeForUrl(directoryUrl); // O(depth)
    // If the directory containing the items wasn't found, then we have a big problem.
    // Are you calling KDirLister::openUrl(url,Keep)? Please use expandToUrl() instead.
    if (!result) {
        qCWarning(category) << "Items emitted in directory" << directoryUrl
                   << "but that directory isn't in KDirModel!"
                   << "Root directory:" << urlForNode(m_rootNode);
        for (const KFileItem &item : items) {
            qDebug() << "Item:" << item.url();
        }
#ifndef NDEBUG
        dump();
#endif
        Q_ASSERT(result);
        return;
    }
    Q_ASSERT(isDir(result));
    KDirModelDirNode *dirNode = static_cast<KDirModelDirNode *>(result);

    const QModelIndex index = indexForNode(dirNode); // O(n)
    const int newItemsCount = items.count();
    const int newRowCount = dirNode->m_childNodes.count() + newItemsCount;

    qCDebug(category) << items.count() << "in" << directoryUrl
                      << "index=" << debugIndex(index) << "newRowCount=" << newRowCount;

    q->beginInsertRows(index, newRowCount - newItemsCount, newRowCount - 1);   // parent, first, last

    const QList<QUrl> urlsBeingFetched = m_urlsBeingFetched.value(dirNode);
    if (!urlsBeingFetched.isEmpty()) {
        qCDebug(category) << "urlsBeingFetched for dir" << dirNode << directoryUrl << ":" << urlsBeingFetched;
    }

    QList<QModelIndex> emitExpandFor;

    dirNode->m_childNodes.reserve(newRowCount);
    KFileItemList::const_iterator it = items.begin();
    KFileItemList::const_iterator end = items.end();
    for (; it != end; ++it) {
        const bool isDir = it->isDir();
        KDirModelNode *node = isDir
                              ? new KDirModelDirNode(dirNode, *it)
                              : new KDirModelNode(dirNode, *it);
#ifndef NDEBUG
        // Test code for possible duplication of items in the childnodes list,
        // not sure if/how it ever happened.
        //if (dirNode->m_childNodes.count() &&
        //    dirNode->m_childNodes.last()->item().name() == (*it).name()) {
        //    qCWarning(category) << "Already having" << (*it).name() << "in" << directoryUrl
        //             << "url=" << dirNode->m_childNodes.last()->item().url();
        //    abort();
        //}
#endif
        dirNode->m_childNodes.append(node);
        const QUrl url = it->url();
        m_nodeHash.insert(cleanupUrl(url), node);

        if (!urlsBeingFetched.isEmpty()) {
            const QUrl &dirUrl = url;
            for (const QUrl &urlFetched : qAsConst(urlsBeingFetched)) {
                if (dirUrl.matches(urlFetched, QUrl::StripTrailingSlash) || dirUrl.isParentOf(urlFetched)) {
                    //qDebug() << "Listing found" << dirUrl.url() << "which is a parent of fetched url" << urlFetched;
                    const QModelIndex parentIndex = indexForNode(node, dirNode->m_childNodes.count() - 1);
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

    q->endInsertRows();

    // Emit expand signal after rowsInserted signal has been emitted,
    // so that any proxy model will have updated its mapping already
    for (const QModelIndex &idx : qAsConst(emitExpandFor)) {
        Q_EMIT q->expand(idx);
    }
}

void KDirModelPrivate::_k_slotCompleted(const QUrl &directoryUrl)
{
    KDirModelNode *result = nodeForUrl(directoryUrl); // O(depth)
    Q_ASSERT(isDir(result));
    KDirModelDirNode *dirNode = static_cast<KDirModelDirNode *>(result);
    m_urlsBeingFetched.remove(dirNode);
}

void KDirModelPrivate::_k_slotDeleteItems(const KFileItemList &items)
{
    qCDebug(category) << items.count() << "items";

    // I assume all items are from the same directory.
    // From KDirLister's code, this should be the case, except maybe emitChanges?
    const KFileItem item = items.first();
    Q_ASSERT(!item.isNull());
    QUrl url = item.url();
    KDirModelNode *node = nodeForUrl(url); // O(depth)
    if (!node) {
        qCWarning(category) << "No node found for item that was just removed:" << url;
        return;
    }

    KDirModelDirNode *dirNode = node->parent();
    if (!dirNode) {
        return;
    }

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
    for (const KFileItem &item : items) {
        if (!node) { // don't lookup the first item twice
            url = item.url();
            node = nodeForUrl(url);
            if (!node) {
                qCWarning(category) << "No node found for item that was just removed:" << url;
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
        node = nullptr;
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

void KDirModelPrivate::_k_slotRefreshItems(const QList<QPair<KFileItem, KFileItem> > &items)
{
    QModelIndex topLeft, bottomRight;

    // Solution 1: we could emit dataChanged for one row (if items.size()==1) or all rows
    // Solution 2: more fine-grained, actually figure out the beginning and end rows.
    for (QList<QPair<KFileItem, KFileItem> >::const_iterator fit = items.begin(), fend = items.end(); fit != fend; ++fit) {
        Q_ASSERT(!fit->first.isNull());
        Q_ASSERT(!fit->second.isNull());
        const QUrl oldUrl = fit->first.url();
        const QUrl newUrl = fit->second.url();
        KDirModelNode *node = nodeForUrl(oldUrl); // O(n); maybe we could look up to the parent only once
        //qDebug() << "in model for" << m_dirLister->url() << ":" << oldUrl << "->" << newUrl << "node=" << node;
        if (!node) { // not found [can happen when renaming a dir, redirection was emitted already]
            continue;
        }
        if (node != m_rootNode) { // we never set an item in the rootnode, we use m_dirLister->rootItem instead.
            bool hasNewNode = false;
            // A file became directory (well, it was overwritten)
            if (fit->first.isDir() != fit->second.isDir()) {
                //qDebug() << "DIR/FILE STATUS CHANGE";
                const int r = node->rowNumber();
                removeFromNodeHash(node, oldUrl);
                KDirModelDirNode *dirNode = node->parent();
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
            // MIME type changed -> forget cached icon (e.g. from "cut", #164185 comment #13)
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
    //qDebug() << "dataChanged(" << debugIndex(topLeft) << " - " << debugIndex(bottomRight);
    bottomRight = bottomRight.sibling(bottomRight.row(), q->columnCount(QModelIndex()) - 1);
    Q_EMIT q->dataChanged(topLeft, bottomRight);
}

// Called when a kioslave redirects (e.g. smb:/Workgroup -> smb://workgroup)
// and when renaming a directory.
void KDirModelPrivate::_k_slotRedirection(const QUrl &oldUrl, const QUrl &newUrl)
{
    KDirModelNode *node = nodeForUrl(oldUrl);
    if (!node) {
        return;
    }
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
        q->beginRemoveRows(QModelIndex(), 0, numRows - 1);
        m_nodeHash.clear();
        clear();
        q->endRemoveRows();
    } else {
        m_nodeHash.clear();
        clear();
    }
}

void KDirModelPrivate::_k_slotJobUrlsChanged(const QStringList &urlList)
{
    QStringList dirtyUrls;

    std::set_symmetric_difference(urlList.begin(), urlList.end(),
                                  m_allCurrentDestUrls.constBegin(), m_allCurrentDestUrls.constEnd(),
                                  std::back_inserter(dirtyUrls));

    m_allCurrentDestUrls = urlList;

    for (const QString &dirtyUrl : qAsConst(dirtyUrls)) {
        if (KDirModelNode *node = nodeForUrl(QUrl(dirtyUrl))) {
            const QModelIndex idx = indexForNode(node);
            Q_EMIT q->dataChanged(idx, idx, {KDirModel::HasJobRole});
        }
    }
}

void KDirModelPrivate::clearAllPreviews(KDirModelDirNode *dirNode)
{
    const int numRows = dirNode->m_childNodes.count();
    if (numRows > 0) {
        KDirModelNode *lastNode = nullptr;
        for (KDirModelNode *node : qAsConst(dirNode->m_childNodes)) {
            node->setPreview(QIcon());
            //node->setPreview(QIcon::fromTheme(node->item().iconName()));
            if (isDir(node)) {
                // recurse into child dirs
                clearAllPreviews(static_cast<KDirModelDirNode *>(node));
            }
            lastNode = node;
        }
        Q_EMIT q->dataChanged(indexForNode(dirNode->m_childNodes.at(0), 0),  // O(1)
                            indexForNode(lastNode, numRows - 1));          // O(1)
    }

}

void KDirModel::clearAllPreviews()
{
    d->clearAllPreviews(d->m_rootNode);
}

void KDirModel::itemChanged(const QModelIndex &index)
{
    // This method is really a itemMimeTypeChanged(), it's mostly called by KFilePreviewGenerator.
    // When the MIME type is determined, clear the old "preview" (could be
    // MIME type dependent like when cutting files, #164185)
    KDirModelNode *node = d->nodeForIndex(index);
    if (node) {
        node->setPreview(QIcon());
    }

    qCDebug(category) << "dataChanged(" << debugIndex(index) << ")";
    Q_EMIT dataChanged(index, index);
}

int KDirModel::columnCount(const QModelIndex &) const
{
    return ColumnCount;
}

QVariant KDirModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid()) {
        KDirModelNode *node = static_cast<KDirModelNode *>(index.internalPointer());
        const KFileItem &item(node->item());
        switch (role) {
        case Qt::DisplayRole:
            switch (index.column()) {
            case Name:
                return item.text();
            case Size:
                return KIO::convertSize(item.size()); // size formatted as QString
            case ModifiedTime: {
                QDateTime dt = item.time(KFileItem::ModificationTime);
                return QLocale().toString(dt, QLocale::ShortFormat);
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
            if (!item.isDir()) {
                return ChildCountUnknown;
            } else {
                KDirModelDirNode *dirNode = static_cast<KDirModelDirNode *>(node);
                int count = dirNode->childCount();
                if (count == ChildCountUnknown && item.isReadable() && !dirNode->isSlow()) {
                    const QString path = item.localPath();
                    if (!path.isEmpty()) {
//                        slow
//                        QDir dir(path);
//                        count = dir.entryList(QDir::AllEntries|QDir::NoDotAndDotDot|QDir::System).count();
#ifdef Q_OS_WIN
                        QString s = path + QLatin1String("\\*.*");
                        s.replace(QLatin1Char('/'), QLatin1Char('\\'));
                        count = 0;
                        WIN32_FIND_DATA findData;
                        HANDLE hFile = FindFirstFile((LPWSTR)s.utf16(), &findData);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            do {
                                if (!(findData.cFileName[0] == '.' &&
                                        findData.cFileName[1] == '\0') &&
                                        !(findData.cFileName[0] == '.' &&
                                          findData.cFileName[1] == '.' &&
                                          findData.cFileName[2] == '\0')) {
                                    ++count;
                                }
                            } while (FindNextFile(hFile, &findData) != 0);
                            FindClose(hFile);
                        }
#else
                        DIR *dir = QT_OPENDIR(QFile::encodeName(path).constData());
                        if (dir) {
                            count = 0;
                            QT_DIRENT *dirEntry = nullptr;
                            while ((dirEntry = QT_READDIR(dir))) {
                                if (dirEntry->d_name[0] == '.') {
                                    if (dirEntry->d_name[1] == '\0') { // skip "."
                                        continue;
                                    }
                                    if (dirEntry->d_name[1] == '.' && dirEntry->d_name[2] == '\0') { // skip ".."
                                        continue;
                                    }
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
                KDirModelNode *node = d->nodeForIndex(index);
                const QString url = node->item().url().toString();
                //return whether or not there are job dest urls visible in the view, so the delegate knows which ones to paint.
                return QVariant(d->m_allCurrentDestUrls.contains(url));
            }
        }
    }
    return QVariant();
}

void KDirModel::sort(int column, Qt::SortOrder order)
{
    // Not implemented - we should probably use QSortFilterProxyModel instead.
    QAbstractItemModel::sort(column, order);
}

bool KDirModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    switch (role) {
    case Qt::EditRole:
        if (index.column() == Name && value.type() == QVariant::String) {
            Q_ASSERT(index.isValid());
            KDirModelNode *node = static_cast<KDirModelNode *>(index.internalPointer());
            const KFileItem &item = node->item();
            const QString newName = value.toString();
            if (newName.isEmpty() || newName == item.text() || (newName == QLatin1Char('.')) || (newName == QLatin1String(".."))) {
                return true;
            }
            QUrl newUrl = item.url().adjusted(QUrl::RemoveFilename);
            newUrl.setPath(newUrl.path() + KIO::encodeFileName(newName));
            KIO::Job *job = KIO::rename(item.url(), newUrl, item.url().isLocalFile() ? KIO::HideProgressInfo : KIO::DefaultFlags);
            job->uiDelegate()->setAutoErrorHandlingEnabled(true);
            // undo handling
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Rename, QList<QUrl>() << item.url(), newUrl, job);
            return true;
        }
        break;
    case Qt::DecorationRole:
        if (index.column() == Name) {
            Q_ASSERT(index.isValid());
            // Set new pixmap - e.g. preview
            KDirModelNode *node = static_cast<KDirModelNode *>(index.internalPointer());
            //qDebug() << "setting icon for " << node->item()->url();
            Q_ASSERT(node);
            if (value.type() == QVariant::Icon) {
                const QIcon icon(qvariant_cast<QIcon>(value));
                node->setPreview(icon);
            } else if (value.type() == QVariant::Pixmap) {
                node->setPreview(qvariant_cast<QPixmap>(value));
            }
            Q_EMIT dataChanged(index, index);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

int KDirModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) { // for QAbstractItemModelTester
        return 0;
    }
    KDirModelNode *node = d->nodeForIndex(parent);
    if (!node || !d->isDir(node)) { // #176555
        return 0;
    }

    KDirModelDirNode *parentNode = static_cast<KDirModelDirNode *>(node);
    Q_ASSERT(parentNode);
    const int count = parentNode->m_childNodes.count();
#if 0
    QStringList filenames;
    for (int i = 0; i < count; ++i) {
        filenames << d->urlForNode(parentNode->m_childNodes.at(i)).fileName();
    }
    qDebug() << "rowCount for " << d->urlForNode(parentNode) << ": " << count << filenames;
#endif
    return count;
}

QModelIndex KDirModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }
    KDirModelNode *childNode = static_cast<KDirModelNode *>(index.internalPointer());
    Q_ASSERT(childNode);
    KDirModelNode *parentNode = childNode->parent();
    Q_ASSERT(parentNode);
    return d->indexForNode(parentNode); // O(n)
}

// Reimplemented to avoid the default implementation which calls parent
// (O(n) for finding the parent's row number for nothing). This implementation is O(1).
QModelIndex KDirModel::sibling(int row, int column, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QModelIndex();
    }
    KDirModelNode *oldChildNode = static_cast<KDirModelNode *>(index.internalPointer());
    Q_ASSERT(oldChildNode);
    KDirModelNode *parentNode = oldChildNode->parent();
    Q_ASSERT(parentNode);
    Q_ASSERT(d->isDir(parentNode));
    KDirModelNode *childNode = static_cast<KDirModelDirNode *>(parentNode)->m_childNodes.value(row); // O(1)
    if (childNode) {
        return createIndex(row, column, childNode);
    }
    return QModelIndex();
}

void KDirModel::requestSequenceIcon(const QModelIndex &index, int sequenceIndex)
{
    Q_EMIT needSequenceIcon(index, sequenceIndex);
}

void KDirModel::setJobTransfersVisible(bool show)
{
    if (d->m_jobTransfersVisible == show) {
        return;
    }

    d->m_jobTransfersVisible = show;
    if (show) {
        connect(&JobUrlCache::instance(), &JobUrlCache::jobUrlsChanged,
                this, [this](const QStringList &urlList) { d->_k_slotJobUrlsChanged(urlList); });

        JobUrlCache::instance().requestJobUrlsChanged();
    } else {
        disconnect(&JobUrlCache::instance(), &JobUrlCache::jobUrlsChanged, this, nullptr);
    }

}

bool KDirModel::jobTransfersVisible() const
{
    return d->m_jobTransfersVisible;
}

QList<QUrl> KDirModel::simplifiedUrlList(const QList<QUrl> &urls)
{
    if (urls.isEmpty()) {
        return urls;
    }

    QList<QUrl> ret(urls);
    std::sort(ret.begin(), ret.end());

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

QStringList KDirModel::mimeTypes() const
{
    return KUrlMimeData::mimeDataTypes();
}

QMimeData *KDirModel::mimeData(const QModelIndexList &indexes) const
{
    QList<QUrl> urls, mostLocalUrls;
    urls.reserve(indexes.size());
    mostLocalUrls.reserve(indexes.size());
    bool canUseMostLocalUrls = true;
    for (const QModelIndex &index : indexes) {
        const KFileItem &item = d->nodeForIndex(index)->item();
        urls << item.url();
        bool isLocal;
        mostLocalUrls << item.mostLocalUrl(&isLocal);
        if (!isLocal) {
            canUseMostLocalUrls = false;
        }
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

    return data;
}

// Public API; not much point in calling it internally
KFileItem KDirModel::itemForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        if (d->m_showNodeForListedUrl) {
            return {};
        }
        return d->m_dirLister->rootItem();
    } else {
        return static_cast<KDirModelNode *>(index.internalPointer())->item();
    }
}

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(4, 0)
QModelIndex KDirModel::indexForItem(const KFileItem *item) const
{
    // Note that we can only use the URL here, not the pointer.
    // KFileItems can be copied.
    return indexForUrl(item->url()); // O(n)
}
#endif

QModelIndex KDirModel::indexForItem(const KFileItem &item) const
{
    // Note that we can only use the URL here, not the pointer.
    // KFileItems can be copied.
    return indexForUrl(item.url()); // O(n)
}

// url -> index. O(n)
QModelIndex KDirModel::indexForUrl(const QUrl &url) const
{
    KDirModelNode *node = d->nodeForUrl(url); // O(depth)
    if (!node) {
        //qDebug() << url << "not found";
        return QModelIndex();
    }
    return d->indexForNode(node); // O(n)
}

QModelIndex KDirModel::index(int row, int column, const QModelIndex &parent) const
{
    KDirModelNode *parentNode = d->nodeForIndex(parent); // O(1)
    Q_ASSERT(parentNode);
    if (d->isDir(parentNode)) {
        KDirModelNode *childNode = static_cast<KDirModelDirNode *>(parentNode)->m_childNodes.value(row); // O(1)
        if (childNode) {
            return createIndex(row, column, childNode);
        }
    }
    return QModelIndex();
}

QVariant KDirModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        switch (role) {
        case Qt::DisplayRole:
            switch (section) {
            case Name:
                return i18nc("@title:column", "Name");
            case Size:
                return i18nc("@title:column", "Size");
            case ModifiedTime:
                return i18nc("@title:column", "Date");
            case Permissions:
                return i18nc("@title:column", "Permissions");
            case Owner:
                return i18nc("@title:column", "Owner");
            case Group:
                return i18nc("@title:column", "Group");
            case Type:
                return i18nc("@title:column", "Type");
            }
        }
    }
    return QVariant();
}

bool KDirModel::hasChildren(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return true;
    }

    const KDirModelNode *parentNode = static_cast<KDirModelNode *>(parent.internalPointer());
    const KFileItem &parentItem = parentNode->item();
    Q_ASSERT(!parentItem.isNull());
    if (!parentItem.isDir()) {
        return false;
    }
    if (static_cast<const KDirModelDirNode *>(parentNode)->isPopulated()) {
        return !static_cast<const KDirModelDirNode *>(parentNode)->m_childNodes.isEmpty();
    }
    if (parentItem.isLocalFile()) {
        QDir::Filters filters = QDir::Dirs | QDir::NoDotAndDotDot;

        if (d->m_dirLister->dirOnlyMode()) {
            filters |= QDir::NoSymLinks;
        } else {
            filters |= QDir::Files | QDir::System;
        }

        if (d->m_dirLister->showingDotFiles()) {
            filters |= QDir::Hidden;
        }

        QDirIterator it(parentItem.localPath(), filters,  QDirIterator::Subdirectories);
        return it.hasNext();
    }
    // Remote and not listed yet, we can't know; let the user click on it so we'll find out
    return true;
}

Qt::ItemFlags KDirModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f;
    if (index.isValid()) {
        f |= Qt::ItemIsEnabled;
        if (index.column() == Name) {
            f |= Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
        }
    }

    // Allow dropping onto this item?
    if (d->m_dropsAllowed != NoDrops) {
        if (!index.isValid()) {
            if (d->m_dropsAllowed & DropOnDirectory) {
                f |= Qt::ItemIsDropEnabled;
            }
        } else {
            KFileItem item = itemForIndex(index);
            if (item.isNull()) {
                qCWarning(category) << "Invalid item returned for index";
            } else if (item.isDir()) {
                if (d->m_dropsAllowed & DropOnDirectory) {
                    f |= Qt::ItemIsDropEnabled;
                }
            } else { // regular file item
                if (d->m_dropsAllowed & DropOnAnyFile) {
                    f |= Qt::ItemIsDropEnabled;
                } else if (d->m_dropsAllowed & DropOnLocalExecutable) {
                    if (!item.localPath().isEmpty()) {
                        // Desktop file?
                        if (item.determineMimeType().inherits(QStringLiteral("application/x-desktop"))) {
                            f |= Qt::ItemIsDropEnabled;
                        }
                        // Executable, shell script ... ?
                        else if (QFileInfo(item.localPath()).isExecutable()) {
                            f |= Qt::ItemIsDropEnabled;
                        }
                    }
                }
            }
        }
    }

    return f;
}

bool KDirModel::canFetchMore(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return false;
    }

    // We now have a bool KDirModelNode::m_populated,
    // to avoid calling fetchMore more than once on empty dirs.
    // But this wastes memory, and how often does someone open and re-open an empty dir in a treeview?
    // Maybe we can ask KDirLister "have you listed <url> already"? (to discuss with M. Brade)

    KDirModelNode *node = static_cast<KDirModelNode *>(parent.internalPointer());
    const KFileItem &item = node->item();
    return item.isDir() && !static_cast<KDirModelDirNode *>(node)->isPopulated()
           && static_cast<KDirModelDirNode *>(node)->m_childNodes.isEmpty();
}

void KDirModel::fetchMore(const QModelIndex &parent)
{
    if (!parent.isValid()) {
        return;
    }

    KDirModelNode *parentNode = static_cast<KDirModelNode *>(parent.internalPointer());

    KFileItem parentItem = parentNode->item();
    Q_ASSERT(!parentItem.isNull());
    if (!parentItem.isDir()) {
        return;
    }
    KDirModelDirNode *dirNode = static_cast<KDirModelDirNode *>(parentNode);
    if (dirNode->isPopulated()) {
        return;
    }
    dirNode->setPopulated(true);

    const QUrl parentUrl = parentItem.url();
    d->m_dirLister->openUrl(parentUrl, KDirLister::Keep);
}

bool KDirModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
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

void KDirModel::expandToUrl(const QUrl &url)
{
    // emit expand for each parent and return last parent
    KDirModelNode *result = d->expandAllParentsUntil(url); // O(depth)

    if (!result) { // doesn't seem related to our base url?
        qCDebug(category) << url << "does not seem related to our base URL, aborting";
        return;
    }
    if (!result->item().isNull() && result->item().url() == url) {
        // We have it already, nothing to do
        qCDebug(category) << "we have it already:" << url;
        return;
    }

    d->m_urlsBeingFetched[result].append(url);

    if (result == d->m_rootNode) {
        qCDebug(category) << "Remembering to emit expand after listing the root url";
        // the root is fetched by default, so it must be currently being fetched
        return;
    }

    qCDebug(category) << "Remembering to emit expand after listing" << result->item().url();

    // start a new fetch to look for the next level down the URL
    const QModelIndex parentIndex = d->indexForNode(result); // O(n)
    Q_ASSERT(parentIndex.isValid());
    fetchMore(parentIndex);
}

bool KDirModel::insertRows(int, int, const QModelIndex &)
{
    return false;
}

bool KDirModel::insertColumns(int, int, const QModelIndex &)
{
    return false;
}

bool KDirModel::removeRows(int, int, const QModelIndex &)
{
    return false;
}

bool KDirModel::removeColumns(int, int, const QModelIndex &)
{
    return false;
}

#include "moc_kdirmodel.cpp"
