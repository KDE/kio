/*
    SPDX-FileCopyrightText: 2008-2009 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfilepreviewgenerator.h"

#include "defaultviewadapter_p.h"
#include <imagefilter_p.h> // from kiowidgets
#include <config-kiofilewidgets.h> // for HAVE_XRENDER
#include <KConfigGroup>
#include <kfileitem.h>
#include <KIconEffect>
#include <kio/previewjob.h>
#include <kio/paste.h>
#include <kdirlister.h>
#include <kdirmodel.h>
#include <KIconLoader>
#include <KSharedConfig>
#include <KUrlMimeData>

#include <QApplication>
#include <QAbstractItemView>
#include <QAbstractProxyModel>
#include <QClipboard>
#include <QHash>
#include <QList>
#include <QListView>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QTimer>
#include <QIcon>

#if HAVE_X11 && HAVE_XRENDER
#  include <QX11Info>
#  include <X11/Xlib.h>
#  include <X11/extensions/Xrender.h>
#endif

class KFilePreviewGeneratorPrivate
{
    class TileSet;
    class LayoutBlocker;

public:
    KFilePreviewGeneratorPrivate(KFilePreviewGenerator *qq,
                                 KAbstractViewAdapter *viewAdapter,
                                 QAbstractItemModel *model);

    ~KFilePreviewGeneratorPrivate();
    /**
     * Requests a new icon for the item \a index.
     * @param sequenceIndex If this is zero, the standard icon is requested, else another one.
     */
    void requestSequenceIcon(const QModelIndex &index, int sequenceIndex);

    /**
     * Generates previews for the items \a items asynchronously.
     */
    void updateIcons(const KFileItemList &items);

    /**
     * Generates previews for the indices within \a topLeft
     * and \a bottomRight asynchronously.
     */
    void updateIcons(const QModelIndex &topLeft, const QModelIndex &bottomRight);

    /**
     * Adds the preview \a pixmap for the item \a item to the preview
     * queue and starts a timer which will dispatch the preview queue
     * later.
     */
    void addToPreviewQueue(const KFileItem &item, const QPixmap &pixmap, KIO::PreviewJob *job);

    /**
     * Is invoked when the preview job has been finished and
     * removes the job from the m_previewJobs list.
     */
    void slotPreviewJobFinished(KJob *job);

    /** Synchronizes the icon of all items with the clipboard of cut items. */
    void updateCutItems();

    /**
     * Reset all icons of the items from m_cutItemsCache and clear
     * the cache.
     */
    void clearCutItemsCache();

    /**
     * Dispatches the preview queue  block by block within
     * time slices.
     */
    void dispatchIconUpdateQueue();

    /**
     * Pauses all icon updates and invokes KFilePreviewGenerator::resumeIconUpdates()
     * after a short delay. Is invoked as soon as the user has moved
     * a scrollbar.
     */
    void pauseIconUpdates();

    /**
     * Resumes the icons updates that have been paused after moving the
     * scrollbar. The previews for the current visible area are
     * generated first.
     */
    void resumeIconUpdates();

    /**
     * Starts the resolving of the MIME types from
     * the m_pendingItems queue.
     */
    void startMimeTypeResolving();

    /**
     * Resolves the MIME type for exactly one item of the
     * m_pendingItems queue.
     */
    void resolveMimeType();

    /**
     * Returns true, if the item \a item has been cut into
     * the clipboard.
     */
    bool isCutItem(const KFileItem &item) const;

    /**
     * Applies a cut-item effect to all given \a items, if they
     * are marked as cut in the clipboard.
     */
    void applyCutItemEffect(const KFileItemList &items);

    /**
     * Applies a frame around the icon. False is returned if
     * no frame has been added because the icon is too small.
     */
    bool applyImageFrame(QPixmap &icon);

    /**
     * Resizes the icon to \a maxSize if the icon size does not
     * fit into the maximum size. The aspect ratio of the icon
     * is kept.
     */
    void limitToSize(QPixmap &icon, const QSize &maxSize);

    /**
     * Creates previews by starting new preview jobs for the items
     * and triggers the preview timer.
     */
    void createPreviews(const KFileItemList &items);

    /**
     * Helper method for createPreviews(): Starts a preview job for the given
     * items. For each returned preview addToPreviewQueue() will get invoked.
     */
    void startPreviewJob(const KFileItemList &items, int width, int height);

    /** Kills all ongoing preview jobs. */
    void killPreviewJobs();

    /**
     * Orders the items \a items in a way that the visible items
     * are moved to the front of the list. When passing this
     * list to a preview job, the visible items will get generated
     * first.
     */
    void orderItems(KFileItemList &items);

    /**
     * Helper method for KFilePreviewGenerator::updateIcons(). Adds
     * recursively all items from the model to the list \a list.
     */
    void addItemsToList(const QModelIndex &index, KFileItemList &list);

    /**
     * Updates the icons of files that are constantly changed due to a copy
     * operation. See m_changedItems and m_changedItemsTimer for details.
     */
    void delayedIconUpdate();

    /**
     * Any items that are removed from the model are also removed from m_changedItems.
     */
    void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end);

    /** Remembers the pixmap for an item specified by an URL. */
    struct ItemInfo {
        QUrl url;
        QPixmap pixmap;
    };

    /**
     * During the lifetime of a DataChangeObtainer instance changing
     * the data of the model won't trigger generating a preview.
     */
    class DataChangeObtainer
    {
    public:
        DataChangeObtainer(KFilePreviewGeneratorPrivate *generator)
            : m_gen(generator)
        {
            ++m_gen->m_internalDataChange;
        }

        ~DataChangeObtainer()
        {
            --m_gen->m_internalDataChange;
        }

    private:
        KFilePreviewGeneratorPrivate *m_gen;
    };

    KFilePreviewGenerator *const q;

    bool m_previewShown = true;

    /**
     * True, if m_pendingItems and m_dispatchedItems should be
     * cleared when the preview jobs have been finished.
     */
    bool m_clearItemQueues = true;

    /**
     * True if a selection has been done which should cut items.
     */
    bool m_hasCutSelection = false;

    /**
     * True if the updates of icons has been paused by pauseIconUpdates().
     * The value is reset by resumeIconUpdates().
     */
    bool m_iconUpdatesPaused = false;

    /**
     * If the value is 0, the slot
     * updateIcons(const QModelIndex&, const QModelIndex&) has
     * been triggered by an external data change.
     */
    int m_internalDataChange = 0;

    int m_pendingVisibleIconUpdates = 0;

    KAbstractViewAdapter *m_viewAdapter = nullptr;
    QAbstractItemView *m_itemView = nullptr;
    QTimer *m_iconUpdateTimer = nullptr;
    QTimer *m_scrollAreaTimer = nullptr;
    QList<KJob *> m_previewJobs;
    QPointer<KDirModel> m_dirModel;
    QAbstractProxyModel *m_proxyModel = nullptr;

    /**
      * Set of all items that already have the 'cut' effect applied, together with the pixmap it was applied to
      * This is used to make sure that the 'cut' effect is applied max. once for each pixmap
      *
      * Referencing the pixmaps here imposes no overhead, as they were also given to KDirModel::setData(),
      * and thus are held anyway.
      */
    QHash<QUrl, QPixmap> m_cutItemsCache;
    QList<ItemInfo> m_previews;
    QMap<QUrl, int> m_sequenceIndices;

    /**
     * When huge items are copied, it must be prevented that a preview gets generated
     * for each item size change. m_changedItems keeps track of the changed items and it
     * is assured that a final preview is only done if an item does not change within
     * at least 5 seconds.
     */
    QHash<QUrl, bool> m_changedItems;
    QTimer *m_changedItemsTimer = nullptr;

    /**
     * Contains all items where a preview must be generated, but
     * where the preview job has not dispatched the items yet.
     */
    KFileItemList m_pendingItems;

    /**
     * Contains all items, where a preview has already been
     * generated by the preview jobs.
     */
    KFileItemList m_dispatchedItems;

    KFileItemList m_resolvedMimeTypes;

    QStringList m_enabledPlugins;

    std::unique_ptr<TileSet> m_tileSet;

};

/**
 * If the passed item view is an instance of QListView, expensive
 * layout operations are blocked in the constructor and are unblocked
 * again in the destructor.
 *
 * This helper class is a workaround for the following huge performance
 * problem when having directories with several 1000 items:
 * - each change of an icon emits a dataChanged() signal from the model
 * - QListView iterates through all items on each dataChanged() signal
 *   and invokes QItemDelegate::sizeHint()
 * - the sizeHint() implementation of KFileItemDelegate is quite complex,
 *   invoking it 1000 times for each icon change might block the UI
 *
 * QListView does not invoke QItemDelegate::sizeHint() when the
 * uniformItemSize property has been set to true, so this property is
 * set before exchanging a block of icons.
 */
class KFilePreviewGeneratorPrivate::LayoutBlocker
{
public:
    explicit LayoutBlocker(QAbstractItemView *view)
        : m_uniformSizes(false)
        , m_view(qobject_cast<QListView *>(view))
    {
        if (m_view) {
            m_uniformSizes = m_view->uniformItemSizes();
            m_view->setUniformItemSizes(true);
        }
    }

    ~LayoutBlocker()
    {
        if (m_view) {
            m_view->setUniformItemSizes(m_uniformSizes);
            /* The QListView did the layout with uniform item
             * sizes, so trigger a relayout with the expected sizes. */
            if (!m_uniformSizes) {
                m_view->setGridSize(m_view->gridSize());
            }
        }
    }

private:
    bool m_uniformSizes = false;
    QListView *m_view = nullptr;
};

/** Helper class for drawing frames for image previews. */
class KFilePreviewGeneratorPrivate::TileSet
{
public:
    enum { LeftMargin = 3, TopMargin = 2, RightMargin = 3, BottomMargin = 4 };

    enum Tile {
        TopLeftCorner = 0,
        TopSide,
        TopRightCorner,
        LeftSide,
        RightSide,
        BottomLeftCorner,
        BottomSide,
        BottomRightCorner,
        NumTiles,
    };

    explicit TileSet()
    {
        QImage image(8 * 3, 8 * 3, QImage::Format_ARGB32_Premultiplied);

        QPainter p(&image);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(image.rect(), Qt::transparent);
        p.fillRect(image.rect().adjusted(3, 3, -3, -3), Qt::black);
        p.end();

        KIO::ImageFilter::shadowBlur(image, 3, Qt::black);

        QPixmap pixmap = QPixmap::fromImage(image);
        m_tiles[TopLeftCorner] = pixmap.copy(0, 0, 8, 8);
        m_tiles[TopSide] = pixmap.copy(8, 0, 8, 8);
        m_tiles[TopRightCorner] = pixmap.copy(16, 0, 8, 8);
        m_tiles[LeftSide] = pixmap.copy(0, 8, 8, 8);
        m_tiles[RightSide] = pixmap.copy(16, 8, 8, 8);
        m_tiles[BottomLeftCorner] = pixmap.copy(0, 16, 8, 8);
        m_tiles[BottomSide] = pixmap.copy(8, 16, 8, 8);
        m_tiles[BottomRightCorner] = pixmap.copy(16, 16, 8, 8);
    }

    void paint(QPainter *p, const QRect &r)
    {
        p->drawPixmap(r.topLeft(), m_tiles[TopLeftCorner]);
        if (r.width() - 16 > 0) {
            p->drawTiledPixmap(r.x() + 8, r.y(), r.width() - 16, 8, m_tiles[TopSide]);
        }
        p->drawPixmap(r.right() - 8 + 1, r.y(), m_tiles[TopRightCorner]);
        if (r.height() - 16 > 0) {
            p->drawTiledPixmap(r.x(), r.y() + 8, 8, r.height() - 16, m_tiles[LeftSide]);
            p->drawTiledPixmap(r.right() - 8 + 1, r.y() + 8, 8, r.height() - 16, m_tiles[RightSide]);
        }
        p->drawPixmap(r.x(), r.bottom() - 8 + 1, m_tiles[BottomLeftCorner]);
        if (r.width() - 16 > 0) {
            p->drawTiledPixmap(r.x() + 8, r.bottom() - 8 + 1, r.width() - 16, 8, m_tiles[BottomSide]);
        }
        p->drawPixmap(r.right() - 8 + 1, r.bottom() - 8 + 1, m_tiles[BottomRightCorner]);

        const QRect contentRect = r.adjusted(LeftMargin + 1, TopMargin + 1, -(RightMargin + 1), -(BottomMargin + 1));
        p->fillRect(contentRect, Qt::transparent);
    }

private:
    QPixmap m_tiles[NumTiles];
};

KFilePreviewGeneratorPrivate::KFilePreviewGeneratorPrivate(KFilePreviewGenerator *qq,
                                                           KAbstractViewAdapter *viewAdapter,
                                                           QAbstractItemModel *model)
    : q(qq)
    , m_viewAdapter(viewAdapter)
{
    if (!m_viewAdapter->iconSize().isValid()) {
        m_previewShown = false;
    }

    m_proxyModel = qobject_cast<QAbstractProxyModel *>(model);
    m_dirModel = (m_proxyModel == nullptr) ?
                 qobject_cast<KDirModel *>(model) :
                 qobject_cast<KDirModel *>(m_proxyModel->sourceModel());
    if (!m_dirModel) {
        // previews can only get generated for directory models
        m_previewShown = false;
    } else {
        KDirModel *dirModel = m_dirModel.data();
        q->connect(dirModel->dirLister(), &KCoreDirLister::newItems,
                   q, [this](const KFileItemList &items) { updateIcons(items); });

        q->connect(dirModel, &KDirModel::dataChanged,
                   q, [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
            updateIcons(topLeft, bottomRight);
        });

        q->connect(dirModel, &KDirModel::needSequenceIcon,
                   q, [this](const QModelIndex &index, int sequenceIndex) {
            requestSequenceIcon(index, sequenceIndex);
        });

        q->connect(dirModel, &KDirModel::rowsAboutToBeRemoved,
                   q, [this](const QModelIndex &parent, int first, int last) {
            rowsAboutToBeRemoved(parent, first, last);
        });
    }

    QClipboard *clipboard = QApplication::clipboard();
    q->connect(clipboard, &QClipboard::dataChanged, q, [this]() { updateCutItems(); });

    m_iconUpdateTimer = new QTimer(q);
    m_iconUpdateTimer->setSingleShot(true);
    m_iconUpdateTimer->setInterval(200);
    q->connect(m_iconUpdateTimer, &QTimer::timeout, q, [this]() { dispatchIconUpdateQueue(); });

    // Whenever the scrollbar values have been changed, the pending previews should
    // be reordered in a way that the previews for the visible items are generated
    // first. The reordering is done with a small delay, so that during moving the
    // scrollbars the CPU load is kept low.
    m_scrollAreaTimer = new QTimer(q);
    m_scrollAreaTimer->setSingleShot(true);
    m_scrollAreaTimer->setInterval(200);
    q->connect(m_scrollAreaTimer, &QTimer::timeout, q, [this]() { resumeIconUpdates(); });
    m_viewAdapter->connect(KAbstractViewAdapter::IconSizeChanged, q, SLOT(updateIcons()));
    m_viewAdapter->connect(KAbstractViewAdapter::ScrollBarValueChanged, q, SLOT(pauseIconUpdates()));

    m_changedItemsTimer = new QTimer(q);
    m_changedItemsTimer->setSingleShot(true);
    m_changedItemsTimer->setInterval(5000);
    q->connect(m_changedItemsTimer, &QTimer::timeout, q, [this]() { delayedIconUpdate(); });

    KConfigGroup globalConfig(KSharedConfig::openConfig(QStringLiteral("dolphinrc")), "PreviewSettings");
    m_enabledPlugins = globalConfig.readEntry("Plugins", QStringList{
                                                              QStringLiteral("directorythumbnail"),
                                                              QStringLiteral("imagethumbnail"),
                                                              QStringLiteral("jpegthumbnail")});

    // Compatibility update: in 4.7, jpegrotatedthumbnail was merged into (or
    // replaced with?) jpegthumbnail
    if (m_enabledPlugins.contains(QLatin1String("jpegrotatedthumbnail"))) {
        m_enabledPlugins.removeAll(QStringLiteral("jpegrotatedthumbnail"));
        m_enabledPlugins.append(QStringLiteral("jpegthumbnail"));
        globalConfig.writeEntry("Plugins", m_enabledPlugins);
        globalConfig.sync();
    }
}

KFilePreviewGeneratorPrivate::~KFilePreviewGeneratorPrivate()
{
    killPreviewJobs();
    m_pendingItems.clear();
    m_dispatchedItems.clear();
}

void KFilePreviewGeneratorPrivate::requestSequenceIcon(const QModelIndex &index,
        int sequenceIndex)
{
    if (m_pendingItems.isEmpty() || (sequenceIndex == 0)) {
        KDirModel *dirModel = m_dirModel.data();
        if (!dirModel) {
            return;
        }

        KFileItem item = dirModel->itemForIndex(index);
        if (sequenceIndex == 0) {
            m_sequenceIndices.remove(item.url());
        } else {
            m_sequenceIndices.insert(item.url(), sequenceIndex);
        }

        ///@todo Update directly, without using m_sequenceIndices
        updateIcons(KFileItemList{item});
    }
}

void KFilePreviewGeneratorPrivate::updateIcons(const KFileItemList &items)
{
    if (items.isEmpty()) {
        return;
    }

    applyCutItemEffect(items);

    KFileItemList orderedItems = items;
    orderItems(orderedItems);

    m_pendingItems.reserve(m_pendingItems.size() + orderedItems.size());
    for (const KFileItem &item : qAsConst(orderedItems)) {
        m_pendingItems.append(item);
    }

    if (m_previewShown) {
        createPreviews(orderedItems);
    } else {
        startMimeTypeResolving();
    }
}

void KFilePreviewGeneratorPrivate::updateIcons(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (m_internalDataChange > 0) {
        // QAbstractItemModel::setData() has been invoked internally by the KFilePreviewGenerator.
        // The signal dataChanged() is connected with this method, but previews only need
        // to be generated when an external data change has occurred.
        return;
    }

    // dataChanged emitted for the root dir (e.g. permission changes)
    if (!topLeft.isValid() || !bottomRight.isValid()) {
        return;
    }

    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    KFileItemList itemList;
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        const QModelIndex index = dirModel->index(row, 0);
        if (!index.isValid()) {
            continue;
        }
        const KFileItem item = dirModel->itemForIndex(index);
        Q_ASSERT(!item.isNull());

        if (m_previewShown) {
            const QUrl url = item.url();
            const bool hasChanged = m_changedItems.contains(url); // O(1)
            m_changedItems.insert(url, hasChanged);
            if (!hasChanged) {
                // only update the icon if it has not been already updated within
                // the last 5 seconds (the other icons will be updated later with
                // the help of m_changedItemsTimer)
                itemList.append(item);
            }
        } else {
            itemList.append(item);
        }
    }

    updateIcons(itemList);
    m_changedItemsTimer->start();
}

void KFilePreviewGeneratorPrivate::addToPreviewQueue(const KFileItem &item,
                                                     const QPixmap &pixmap,
                                                     KIO::PreviewJob *job)
{
    Q_ASSERT(job);
    if (job) {
        QMap<QUrl, int>::iterator it = m_sequenceIndices.find(item.url());
        if (job->sequenceIndex() && (it == m_sequenceIndices.end() || *it != job->sequenceIndex())) {
            return; // the sequence index does not match the one we want
        }
        if (!job->sequenceIndex() && it != m_sequenceIndices.end()) {
            return; // the sequence index does not match the one we want
        }

        m_sequenceIndices.erase(it);
    }

    if (!m_previewShown) {
        // the preview has been canceled in the meantime
        return;
    }

    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    // check whether the item is part of the directory lister (it is possible
    // that a preview from an old directory lister is received)
    bool isOldPreview = true;

    const QUrl itemParentDir = item.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);

    const QList<QUrl> dirs = dirModel->dirLister()->directories();
    for (const QUrl &dir : dirs) {
        if (dir == itemParentDir || dir.path().isEmpty()) {
            isOldPreview = false;
            break;
        }
    }
    if (isOldPreview) {
        return;
    }

    QPixmap icon = pixmap;

    const QString mimeType = item.mimetype();
    const int slashIndex = mimeType.indexOf(QLatin1Char('/'));
    const QStringRef mimeTypeGroup = mimeType.leftRef(slashIndex);
    if ((mimeTypeGroup != QLatin1String("image")) || !applyImageFrame(icon)) {
        limitToSize(icon, m_viewAdapter->iconSize());
    }

    if (m_hasCutSelection && isCutItem(item)) {
        // apply the disabled effect to the icon for marking it as "cut item"
        // and apply the icon to the item
        KIconEffect *iconEffect = KIconLoader::global()->iconEffect();
        icon = iconEffect->apply(icon, KIconLoader::Desktop, KIconLoader::DisabledState);
    }

    KIconLoader::global()->drawOverlays(item.overlays(), icon, KIconLoader::Desktop);

    // remember the preview and URL, so that it can be applied to the model
    // in KFilePreviewGenerator::dispatchIconUpdateQueue()
    ItemInfo preview;
    preview.url = item.url();
    preview.pixmap = icon;
    m_previews.append(preview);

    m_pendingItems.removeOne(item);

    m_dispatchedItems.append(item);
}

void KFilePreviewGeneratorPrivate::slotPreviewJobFinished(KJob *job)
{
    const int index = m_previewJobs.indexOf(job);
    m_previewJobs.removeAt(index);

    if (m_previewJobs.isEmpty()) {
        for (const KFileItem &item : qAsConst(m_pendingItems)) {
            if (item.isMimeTypeKnown()) {
                m_resolvedMimeTypes.append(item);
            }
        }

        if (m_clearItemQueues) {
            m_pendingItems.clear();
            m_dispatchedItems.clear();
            m_pendingVisibleIconUpdates = 0;
            QMetaObject::invokeMethod(q, [this]() { dispatchIconUpdateQueue(); }, Qt::QueuedConnection);
        }
        m_sequenceIndices.clear(); // just to be sure that we don't leak anything
    }
}

void KFilePreviewGeneratorPrivate::updateCutItems()
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    DataChangeObtainer obt(this);
    clearCutItemsCache();

    KFileItemList items;
    KDirLister *dirLister = dirModel->dirLister();
    const QList<QUrl> dirs = dirLister->directories();
    items.reserve(dirs.size());
    for (const QUrl &url : dirs) {
        items << dirLister->itemsForDir(url);
    }
    applyCutItemEffect(items);
}

void KFilePreviewGeneratorPrivate::clearCutItemsCache()
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    DataChangeObtainer obt(this);
    KFileItemList previews;
    // Reset the icons of all items that are stored in the cache
    // to use their default MIME type icon.
    for (auto it = m_cutItemsCache.cbegin(); it != m_cutItemsCache.cend(); ++it) {
        const QModelIndex index = dirModel->indexForUrl(it.key());
        if (index.isValid()) {
            dirModel->setData(index, QIcon(), Qt::DecorationRole);
            if (m_previewShown) {
                previews.append(dirModel->itemForIndex(index));
            }
        }
    }
    m_cutItemsCache.clear();

    if (!previews.isEmpty()) {
        // assure that the previews gets restored
        Q_ASSERT(m_previewShown);
        orderItems(previews);
        updateIcons(previews);
    }
}

void KFilePreviewGeneratorPrivate::dispatchIconUpdateQueue()
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    const int count = m_previews.count() + m_resolvedMimeTypes.count();
    if (count > 0) {
        LayoutBlocker blocker(m_itemView);
        DataChangeObtainer obt(this);

        if (m_previewShown) {
            // dispatch preview queue
            for (const ItemInfo &preview : qAsConst(m_previews)) {
                const QModelIndex idx = dirModel->indexForUrl(preview.url);
                if (idx.isValid() && (idx.column() == 0)) {
                    dirModel->setData(idx, QIcon(preview.pixmap), Qt::DecorationRole);
                }
            }
            m_previews.clear();
        }

        // dispatch MIME type queue
        for (const KFileItem &item : qAsConst(m_resolvedMimeTypes)) {
            const QModelIndex idx = dirModel->indexForItem(item);
            dirModel->itemChanged(idx);
        }
        m_resolvedMimeTypes.clear();

        m_pendingVisibleIconUpdates -= count;
        if (m_pendingVisibleIconUpdates < 0) {
            m_pendingVisibleIconUpdates = 0;
        }
    }

    if (m_pendingVisibleIconUpdates > 0) {
        // As long as there are pending previews for visible items, poll
        // the preview queue periodically. If there are no pending previews,
        // the queue is dispatched in slotPreviewJobFinished().
        m_iconUpdateTimer->start();
    }
}

void KFilePreviewGeneratorPrivate::pauseIconUpdates()
{
    m_iconUpdatesPaused = true;
    for (KJob *job : qAsConst(m_previewJobs)) {
        Q_ASSERT(job);
        job->suspend();
    }
    m_scrollAreaTimer->start();
}

void KFilePreviewGeneratorPrivate::resumeIconUpdates()
{
    m_iconUpdatesPaused = false;

    // Before creating new preview jobs the m_pendingItems queue must be
    // cleaned up by removing the already dispatched items. Implementation
    // note: The order of the m_dispatchedItems queue and the m_pendingItems
    // queue is usually equal. So even when having a lot of elements the
    // nested loop is no performance bottle neck, as the inner loop is only
    // entered once in most cases.
    for (const KFileItem &item : qAsConst(m_dispatchedItems)) {
        KFileItemList::iterator begin = m_pendingItems.begin();
        KFileItemList::iterator end   = m_pendingItems.end();
        for (KFileItemList::iterator it = begin; it != end; ++it) {
            if ((*it).url() == item.url()) {
                m_pendingItems.erase(it);
                break;
            }
        }
    }
    m_dispatchedItems.clear();

    m_pendingVisibleIconUpdates = 0;
    dispatchIconUpdateQueue();

    if (m_previewShown) {
        KFileItemList orderedItems = m_pendingItems;
        orderItems(orderedItems);

        // Kill all suspended preview jobs. Usually when a preview job
        // has been finished, slotPreviewJobFinished() clears all item queues.
        // This is not wanted in this case, as a new job is created afterwards
        // for m_pendingItems.
        m_clearItemQueues = false;
        killPreviewJobs();
        m_clearItemQueues = true;

        createPreviews(orderedItems);
    } else {
        orderItems(m_pendingItems);
        startMimeTypeResolving();
    }
}

void KFilePreviewGeneratorPrivate::startMimeTypeResolving()
{
    resolveMimeType();
    m_iconUpdateTimer->start();
}

void KFilePreviewGeneratorPrivate::resolveMimeType()
{
    if (m_pendingItems.isEmpty()) {
        return;
    }

    // resolve at least one MIME type
    bool resolved = false;
    do {
        KFileItem item = m_pendingItems.takeFirst();
        if (item.isMimeTypeKnown()) {
            if (m_pendingVisibleIconUpdates > 0) {
                // The item is visible and the MIME type already known.
                // Decrease the update counter for dispatchIconUpdateQueue():
                --m_pendingVisibleIconUpdates;
            }
        } else {
            // The MIME type is unknown and must get resolved. The
            // directory model is not informed yet, as a single update
            // would be very expensive. Instead the item is remembered in
            // m_resolvedMimeTypes and will be dispatched later
            // by dispatchIconUpdateQueue().
            item.determineMimeType();
            m_resolvedMimeTypes.append(item);
            resolved = true;
        }
    } while (!resolved && !m_pendingItems.isEmpty());

    if (m_pendingItems.isEmpty()) {
        // All MIME types have been resolved now. Assure
        // that the directory model gets informed about
        // this, so that an update of the icons is done.
        dispatchIconUpdateQueue();
    } else if (!m_iconUpdatesPaused) {
        // assure that the MIME type of the next
        // item will be resolved asynchronously
        QMetaObject::invokeMethod(q, [this]() { resolveMimeType(); }, Qt::QueuedConnection);
    }
}

bool KFilePreviewGeneratorPrivate::isCutItem(const KFileItem &item) const
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    const QList<QUrl> cutUrls = KUrlMimeData::urlsFromMimeData(mimeData);
    return cutUrls.contains(item.url());
}

void KFilePreviewGeneratorPrivate::applyCutItemEffect(const KFileItemList &items)
{
    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    m_hasCutSelection = mimeData && KIO::isClipboardDataCut(mimeData);
    if (!m_hasCutSelection) {
        return;
    }

    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    const QList<QUrl> urlsList = KUrlMimeData::urlsFromMimeData(mimeData);
    const QSet<QUrl> cutUrls(urlsList.begin(), urlsList.end());

    DataChangeObtainer obt(this);
    KIconEffect *iconEffect = KIconLoader::global()->iconEffect();
    for (const KFileItem &item : items) {
        if (cutUrls.contains(item.url())) {
            const QModelIndex index = dirModel->indexForItem(item);
            const QVariant value = dirModel->data(index, Qt::DecorationRole);
            if (value.type() == QVariant::Icon) {
                const QIcon icon(qvariant_cast<QIcon>(value));
                const QSize actualSize = icon.actualSize(m_viewAdapter->iconSize());
                QPixmap pixmap = icon.pixmap(actualSize);

                const QHash<QUrl, QPixmap>::const_iterator cacheIt = m_cutItemsCache.constFind(item.url());
                if ((cacheIt == m_cutItemsCache.constEnd()) || (cacheIt->cacheKey() != pixmap.cacheKey())) {
                    pixmap = iconEffect->apply(pixmap, KIconLoader::Desktop, KIconLoader::DisabledState);
                    dirModel->setData(index, QIcon(pixmap), Qt::DecorationRole);

                    m_cutItemsCache.insert(item.url(), pixmap);
                }
            }
        }
    }
}

bool KFilePreviewGeneratorPrivate::applyImageFrame(QPixmap &icon)
{
    const QSize maxSize = m_viewAdapter->iconSize();
    const bool applyFrame = (maxSize.width()  > KIconLoader::SizeSmallMedium) &&
                            (maxSize.height() > KIconLoader::SizeSmallMedium) &&
                            !icon.hasAlpha();
    if (!applyFrame) {
        // the maximum size or the image itself is too small for a frame
        return false;
    }

    // resize the icon to the maximum size minus the space required for the frame
    const QSize size(maxSize.width() - TileSet::LeftMargin - TileSet::RightMargin,
                     maxSize.height() - TileSet::TopMargin - TileSet::BottomMargin);
    limitToSize(icon, size);

    if (!m_tileSet) {
        m_tileSet.reset(new TileSet{});
    }

    QPixmap framedIcon(icon.size().width() + TileSet::LeftMargin + TileSet::RightMargin,
                       icon.size().height() + TileSet::TopMargin + TileSet::BottomMargin);
    framedIcon.fill(Qt::transparent);

    QPainter painter;
    painter.begin(&framedIcon);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    m_tileSet->paint(&painter, framedIcon.rect());
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawPixmap(TileSet::LeftMargin, TileSet::TopMargin, icon);
    painter.end();

    icon = framedIcon;
    return true;
}

void KFilePreviewGeneratorPrivate::limitToSize(QPixmap &icon, const QSize &maxSize)
{
    if ((icon.width() > maxSize.width()) || (icon.height() > maxSize.height())) {
#pragma message("Cannot use XRender with QPixmap anymore. Find equivalent with Qt API.")
#if 0 // HAVE_X11 && HAVE_XRENDER
        // Assume that the texture size limit is 2048x2048
        if ((icon.width() <= 2048) && (icon.height() <= 2048) && icon.x11PictureHandle()) {
            QSize size = icon.size();
            size.scale(maxSize, Qt::KeepAspectRatio);

            const qreal factor = size.width() / qreal(icon.width());

            XTransform xform = {{
                    { XDoubleToFixed(1 / factor), 0, 0 },
                    { 0, XDoubleToFixed(1 / factor), 0 },
                    { 0, 0, XDoubleToFixed(1) }
                }
            };

            QPixmap pixmap(size);
            pixmap.fill(Qt::transparent);

            Display *dpy = QX11Info::display();

            XRenderPictureAttributes attr;
            attr.repeat = RepeatPad;
            XRenderChangePicture(dpy, icon.x11PictureHandle(), CPRepeat, &attr);

            XRenderSetPictureFilter(dpy, icon.x11PictureHandle(), FilterBilinear, 0, 0);
            XRenderSetPictureTransform(dpy, icon.x11PictureHandle(), &xform);
            XRenderComposite(dpy, PictOpOver, icon.x11PictureHandle(), None, pixmap.x11PictureHandle(),
                             0, 0, 0, 0, 0, 0, pixmap.width(), pixmap.height());
            icon = pixmap;
        } else {
            icon = icon.scaled(maxSize, Qt::KeepAspectRatio, Qt::FastTransformation);
        }
#else
        icon = icon.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
#endif
    }
}

void KFilePreviewGeneratorPrivate::createPreviews(const KFileItemList &items)
{
    if (items.isEmpty()) {
        return;
    }

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    m_hasCutSelection = mimeData && KIO::isClipboardDataCut(mimeData);

    // PreviewJob internally caches items always with the size of
    // 128 x 128 pixels or 256 x 256 pixels. A downscaling is done
    // by PreviewJob if a smaller size is requested. For images KFilePreviewGenerator must
    // do a downscaling anyhow because of the frame, so in this case only the provided
    // cache sizes are requested.
    KFileItemList imageItems;
    KFileItemList otherItems;
    QString mimeType;
    for (const KFileItem &item : items) {
        mimeType = item.mimetype();
        const int slashIndex = mimeType.indexOf(QLatin1Char('/'));
        const QStringRef mimeTypeGroup = mimeType.leftRef(slashIndex);
        if (mimeTypeGroup == QLatin1String("image")) {
            imageItems.append(item);
        } else {
            otherItems.append(item);
        }
    }
    const QSize size = m_viewAdapter->iconSize();
    startPreviewJob(otherItems, size.width(), size.height());

    const int cacheSize = (size.width() > 128) || (size.height() > 128) ? 256 : 128;
    startPreviewJob(imageItems, cacheSize, cacheSize);

    m_iconUpdateTimer->start();
}

void KFilePreviewGeneratorPrivate::startPreviewJob(const KFileItemList &items, int width, int height)
{
    if (items.isEmpty()) {
        return;
    }

    KIO::PreviewJob *job = KIO::filePreview(items, QSize(width, height), &m_enabledPlugins);

    // Set the sequence index to the target. We only need to check if items.count() == 1,
    // because requestSequenceIcon(..) creates exactly such a request.
    if (!m_sequenceIndices.isEmpty() && (items.count() == 1)) {
        const auto it = m_sequenceIndices.constFind(items[0].url());
        if (it != m_sequenceIndices.cend()) {
            job->setSequenceIndex(*it);
        }
    }

    q->connect(job, &KIO::PreviewJob::gotPreview,
                q, [this, job](const KFileItem &item, const QPixmap &pixmap) {
        addToPreviewQueue(item, pixmap, job);
    });

    q->connect(job, &KIO::PreviewJob::finished, q, [this, job]() { slotPreviewJobFinished(job); });
    m_previewJobs.append(job);
}

void KFilePreviewGeneratorPrivate::killPreviewJobs()
{
    for (KJob *job : qAsConst(m_previewJobs)) {
        Q_ASSERT(job);
        job->kill();
    }
    m_previewJobs.clear();
    m_sequenceIndices.clear();

    m_iconUpdateTimer->stop();
    m_scrollAreaTimer->stop();
    m_changedItemsTimer->stop();
}

void KFilePreviewGeneratorPrivate::orderItems(KFileItemList &items)
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    // Order the items in a way that the preview for the visible items
    // is generated first, as this improves the felt performance a lot.
    const bool hasProxy = m_proxyModel != nullptr;
    const int itemCount = items.count();
    const QRect visibleArea = m_viewAdapter->visibleArea();

    QModelIndex dirIndex;
    QRect itemRect;
    int insertPos = 0;
    for (int i = 0; i < itemCount; ++i) {
        dirIndex = dirModel->indexForItem(items.at(i)); // O(n) (n = number of rows)
        if (hasProxy) {
            const QModelIndex proxyIndex = m_proxyModel->mapFromSource(dirIndex);
            itemRect = m_viewAdapter->visualRect(proxyIndex);
        } else {
            itemRect = m_viewAdapter->visualRect(dirIndex);
        }

        if (itemRect.intersects(visibleArea)) {
            // The current item is (at least partly) visible. Move it
            // to the front of the list, so that the preview is
            // generated earlier.
            items.insert(insertPos, items.at(i));
            items.removeAt(i + 1);
            ++insertPos;
            ++m_pendingVisibleIconUpdates;
        }
    }
}

void KFilePreviewGeneratorPrivate::addItemsToList(const QModelIndex &index, KFileItemList &list)
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    const int rowCount = dirModel->rowCount(index);
    for (int row = 0; row < rowCount; ++row) {
        const QModelIndex subIndex = dirModel->index(row, 0, index);
        KFileItem item = dirModel->itemForIndex(subIndex);
        list.append(item);

        if (dirModel->rowCount(subIndex) > 0) {
            // the model is hierarchical (treeview)
            addItemsToList(subIndex, list);
        }
    }
}

void KFilePreviewGeneratorPrivate::delayedIconUpdate()
{
    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    // Precondition: No items have been changed within the last
    // 5 seconds. This means that items that have been changed constantly
    // due to a copy operation should be updated now.

    KFileItemList itemList;

    QHash<QUrl, bool>::const_iterator it = m_changedItems.constBegin();
    while (it != m_changedItems.constEnd()) {
        const bool hasChanged = it.value();
        if (hasChanged) {
            const QModelIndex index = dirModel->indexForUrl(it.key());
            const KFileItem item = dirModel->itemForIndex(index);
            itemList.append(item);
        }
        ++it;
    }
    m_changedItems.clear();

    updateIcons(itemList);
}

void KFilePreviewGeneratorPrivate::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
    if (m_changedItems.isEmpty()) {
        return;
    }

    KDirModel *dirModel = m_dirModel.data();
    if (!dirModel) {
        return;
    }

    for (int row = start; row <= end; row++) {
        const QModelIndex index = dirModel->index(row, 0, parent);

        const KFileItem item = dirModel->itemForIndex(index);
        if (!item.isNull()) {
            m_changedItems.remove(item.url());
        }

        if (dirModel->hasChildren(index)) {
            rowsAboutToBeRemoved(index, 0, dirModel->rowCount(index) - 1);
        }
    }
}

KFilePreviewGenerator::KFilePreviewGenerator(QAbstractItemView *parent)
    : QObject(parent)
    , d(new KFilePreviewGeneratorPrivate(this, new KIO::DefaultViewAdapter(parent, this), parent->model()))
{
    d->m_itemView = parent;
}

KFilePreviewGenerator::KFilePreviewGenerator(KAbstractViewAdapter *parent, QAbstractProxyModel *model)
    : QObject(parent)
    , d(new KFilePreviewGeneratorPrivate(this, parent, model))
{
}

KFilePreviewGenerator::~KFilePreviewGenerator() = default;

void KFilePreviewGenerator::setPreviewShown(bool show)
{
    if (d->m_previewShown == show) {
        return;
    }

    KDirModel *dirModel = d->m_dirModel.data();
    if (show && (!d->m_viewAdapter->iconSize().isValid() || !dirModel)) {
        // The view must provide an icon size and a directory model,
        // otherwise the showing the previews will get ignored
        return;
    }

    d->m_previewShown = show;
    if (!show) {
        dirModel->clearAllPreviews();
    }
    updateIcons();
}

bool KFilePreviewGenerator::isPreviewShown() const
{
    return d->m_previewShown;
}

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(4, 3)
void KFilePreviewGenerator::updatePreviews()
{
    updateIcons();
}
#endif

void KFilePreviewGenerator::updateIcons()
{
    d->killPreviewJobs();

    d->clearCutItemsCache();
    d->m_pendingItems.clear();
    d->m_dispatchedItems.clear();

    KFileItemList itemList;
    d->addItemsToList(QModelIndex(), itemList);

    d->updateIcons(itemList);
}

void KFilePreviewGenerator::cancelPreviews()
{
    d->killPreviewJobs();
    d->m_pendingItems.clear();
    d->m_dispatchedItems.clear();
    updateIcons();
}

void KFilePreviewGenerator::setEnabledPlugins(const QStringList &plugins)
{
    d->m_enabledPlugins = plugins;
}

QStringList KFilePreviewGenerator::enabledPlugins() const
{
    return d->m_enabledPlugins;
}

#include "moc_kfilepreviewgenerator.cpp"
