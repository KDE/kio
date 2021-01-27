/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfileplacesview.h"
#include "kfileplacesview_p.h"

#include <QDir>
#include <QTimeLine>
#include <QTimer>
#include <QPainter>
#include <QAbstractItemDelegate>
#include <QKeyEvent>
#include <QApplication>
#include <QMenu>
#include <QPointer>
#include <QScrollBar>

#include <defaults-kfile.h> // ConfigGroup, PlacesIconsAutoresize, PlacesIconsStaticSize
#include <KConfig>
#include <KConfigGroup>
#include <kdirnotify.h>
#include <KLocalizedString>
#include <KMessageBox>
#include <kmountpoint.h>
#include <kpropertiesdialog.h>
#include <kio/emptytrashjob.h>
#include <kio/filesystemfreespacejob.h>
#include <kio/jobuidelegate.h>
#include <widgetsaskuseractionhandler.h>
#include <KJob>
#include <KJobWidgets>
#include <KCapacityBar>
#include <KSharedConfig>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>

#include "kfileplaceeditdialog.h"
#include "kfileplacesmodel.h"

#define LATERAL_MARGIN 4
#define CAPACITYBAR_HEIGHT 6

struct PlaceFreeSpaceInfo
{
    QDateTime lastUpdated;
    KIO::filesize_t used = 0;
    KIO::filesize_t size = 0;
    QPointer<KIO::FileSystemFreeSpaceJob> job;
};

class KFilePlacesViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    explicit KFilePlacesViewDelegate(KFilePlacesView *parent);
    ~KFilePlacesViewDelegate() override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    int iconSize() const;
    void setIconSize(int newSize);

    void addAppearingItem(const QModelIndex &index);
    void setAppearingItemProgress(qreal value);
    void addDisappearingItem(const QModelIndex &index);
    void addDisappearingItemGroup(const QModelIndex &index);
    void setDisappearingItemProgress(qreal value);

    void setShowHoverIndication(bool show);

    void addFadeAnimation(const QModelIndex &index, QTimeLine *timeLine);
    void removeFadeAnimation(const QModelIndex &index);
    QModelIndex indexForFadeAnimation(QTimeLine *timeLine) const;
    QTimeLine *fadeAnimationForIndex(const QModelIndex &index) const;

    qreal contentsOpacity(const QModelIndex &index) const;

    bool pointIsHeaderArea(const QPoint &pos);

    void startDrag();

    int sectionHeaderHeight() const;

    void clearFreeSpaceInfo();

private:
    QString groupNameFromIndex(const QModelIndex &index) const;
    QModelIndex previousVisibleIndex(const QModelIndex &index) const;
    bool indexIsSectionHeader(const QModelIndex &index) const;
    void drawSectionHeader(QPainter *painter,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) const;

    QColor textColor(const QStyleOption &option) const;
    QColor baseColor(const QStyleOption &option) const;
    QColor mixedColor(const QColor &c1, const QColor &c2, int c1Percent) const;

    KFilePlacesView *m_view;
    int m_iconSize;

    QList<QPersistentModelIndex> m_appearingItems;
    int m_appearingIconSize;
    qreal m_appearingOpacity;

    QList<QPersistentModelIndex> m_disappearingItems;
    int m_disappearingIconSize;
    qreal m_disappearingOpacity;

    bool m_showHoverIndication;
    mutable bool m_dragStarted;

    QMap<QPersistentModelIndex, QTimeLine *> m_timeLineMap;
    QMap<QTimeLine *, QPersistentModelIndex> m_timeLineInverseMap;

    mutable QMap<QPersistentModelIndex, PlaceFreeSpaceInfo> m_freeSpaceInfo;
};

KFilePlacesViewDelegate::KFilePlacesViewDelegate(KFilePlacesView *parent) :
    QAbstractItemDelegate(parent),
    m_view(parent),
    m_iconSize(48),
    m_appearingIconSize(0),
    m_appearingOpacity(0.0),
    m_disappearingIconSize(0),
    m_disappearingOpacity(0.0),
    m_showHoverIndication(true),
    m_dragStarted(false)
{
}

KFilePlacesViewDelegate::~KFilePlacesViewDelegate()
{
}

QSize KFilePlacesViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    int iconSize = m_iconSize;
    if (m_appearingItems.contains(index)) {
        iconSize = m_appearingIconSize;
    } else if (m_disappearingItems.contains(index)) {
        iconSize = m_disappearingIconSize;
    }

    int height = option.fontMetrics.height() / 2 + qMax(iconSize, option.fontMetrics.height());

    if (indexIsSectionHeader(index)) {
        height += sectionHeaderHeight();
    }

    return QSize(option.rect.width(), height);
}

void KFilePlacesViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QStyleOptionViewItem opt = option;

    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(index.model());

    // draw header when necessary
    if (indexIsSectionHeader(index)) {
        // If we are drawing the floating element used by drag/drop, do not draw the header
        if (!m_dragStarted) {
            drawSectionHeader(painter, opt, index);
        }

        // Move the target rect to the actual item rect
        const int headerHeight = sectionHeaderHeight();
        opt.rect.translate(0, headerHeight);
        opt.rect.setHeight(opt.rect.height() - headerHeight);
    }

    m_dragStarted = false;

    // draw item
    if (m_appearingItems.contains(index)) {
        painter->setOpacity(m_appearingOpacity);
    } else if (m_disappearingItems.contains(index)) {
        painter->setOpacity(m_disappearingOpacity);
    }

    if (placesModel->isHidden(index)) {
        painter->setOpacity(painter->opacity() * 0.6);
    }

    if (!m_showHoverIndication) {
        opt.state &= ~QStyle::State_MouseOver;
    }

    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);

    bool isLTR = opt.direction == Qt::LeftToRight;

    QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();
    QPixmap pm = icon.pixmap(m_iconSize, m_iconSize, (opt.state & QStyle::State_Selected) && (opt.state & QStyle::State_Active) ? QIcon::Selected : QIcon::Normal);
    QPoint point(isLTR ? opt.rect.left() + LATERAL_MARGIN
                 : opt.rect.right() - LATERAL_MARGIN - m_iconSize, opt.rect.top() + (opt.rect.height() - m_iconSize) / 2);
    painter->drawPixmap(point, pm);

    if (opt.state & QStyle::State_Selected) {
        QPalette::ColorGroup cg = QPalette::Active;
        if (!(opt.state & QStyle::State_Enabled)) {
            cg = QPalette::Disabled;
        } else if (!(opt.state & QStyle::State_Active)) {
            cg = QPalette::Inactive;
        }
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
    }

    QRect rectText;

    bool drawCapacityBar = false;
    if (placesModel->data(index, KFilePlacesModel::CapacityBarRecommendedRole).toBool()) {
        const QUrl url = placesModel->url(index);
        if (contentsOpacity(index) > 0) {
            QPersistentModelIndex persistentIndex(index);
            PlaceFreeSpaceInfo &info = m_freeSpaceInfo[persistentIndex];

            drawCapacityBar = info.size > 0;
            if (drawCapacityBar) {
                painter->save();
                painter->setOpacity(painter->opacity() * contentsOpacity(index));

                int height = opt.fontMetrics.height() + CAPACITYBAR_HEIGHT;
                rectText = QRect(isLTR ? m_iconSize + LATERAL_MARGIN * 2 + opt.rect.left()
                                 : 0, opt.rect.top() + (opt.rect.height() / 2 - height / 2), opt.rect.width() - m_iconSize - LATERAL_MARGIN * 2, opt.fontMetrics.height());
                painter->drawText(rectText, Qt::AlignLeft | Qt::AlignTop, opt.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));
                QRect capacityRect(isLTR ? rectText.x() : LATERAL_MARGIN, rectText.bottom() - 1, rectText.width() - LATERAL_MARGIN, CAPACITYBAR_HEIGHT);
                KCapacityBar capacityBar(KCapacityBar::DrawTextInline);
                capacityBar.setValue((info.used * 100) / info.size);
                capacityBar.drawCapacityBar(painter, capacityRect);

                painter->restore();

                painter->save();
                painter->setOpacity(painter->opacity() * (1 - contentsOpacity(index)));
            }

            if (!info.job &&
                    (!info.lastUpdated.isValid() || info.lastUpdated.secsTo(QDateTime::currentDateTimeUtc()) > 60)) {
                info.job = KIO::fileSystemFreeSpace(url);
                connect(info.job, &KIO::FileSystemFreeSpaceJob::result, this, [this, persistentIndex](KIO::Job *job, KIO::filesize_t size, KIO::filesize_t available) {
                    PlaceFreeSpaceInfo &info = m_freeSpaceInfo[persistentIndex];

                    // even if we receive an error we want to refresh lastUpdated to avoid repeatedly querying in this case
                    info.lastUpdated = QDateTime::currentDateTimeUtc();

                    if (job->error()) {
                        return;
                    }

                    info.size = size;
                    info.used = size - available;

                    // FIXME scheduleDelayedItemsLayout but we're in the delegate here, not the view
                });
            }
        }
    }

    rectText = QRect(isLTR ? m_iconSize + LATERAL_MARGIN * 2 + opt.rect.left()
                     : 0, opt.rect.top(), opt.rect.width() - m_iconSize - LATERAL_MARGIN * 2, opt.rect.height());
    painter->drawText(rectText, Qt::AlignLeft | Qt::AlignVCenter, opt.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));

    if (drawCapacityBar) {
        painter->restore();
    }

    painter->restore();
}

int KFilePlacesViewDelegate::iconSize() const
{
    return m_iconSize;
}

void KFilePlacesViewDelegate::setIconSize(int newSize)
{
    m_iconSize = newSize;
}

void KFilePlacesViewDelegate::addAppearingItem(const QModelIndex &index)
{
    m_appearingItems << index;
}

void KFilePlacesViewDelegate::setAppearingItemProgress(qreal value)
{
    if (value <= 0.25) {
        m_appearingOpacity = 0.0;
        m_appearingIconSize = iconSize() * value * 4;

        if (m_appearingIconSize >= m_iconSize) {
            m_appearingIconSize = m_iconSize;
        }
    } else {
        m_appearingIconSize = m_iconSize;
        m_appearingOpacity = (value - 0.25) * 4 / 3;

        if (value >= 1.0) {
            m_appearingItems.clear();
        }
    }
}

void KFilePlacesViewDelegate::addDisappearingItem(const QModelIndex &index)
{
    m_disappearingItems << index;
}

void KFilePlacesViewDelegate::addDisappearingItemGroup(const QModelIndex &index)
{
    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(index.model());
    const QModelIndexList indexesGroup = placesModel->groupIndexes(placesModel->groupType(index));

    m_disappearingItems.reserve(m_disappearingItems.count() + indexesGroup.count());
    std::transform(indexesGroup.begin(), indexesGroup.end(), std::back_inserter(m_disappearingItems),
                   [](const QModelIndex &idx){ return QPersistentModelIndex(idx); });
}

void KFilePlacesViewDelegate::setDisappearingItemProgress(qreal value)
{
    value = 1.0 - value;

    if (value <= 0.25) {
        m_disappearingOpacity = 0.0;
        m_disappearingIconSize = iconSize() * value * 4;

        if (m_disappearingIconSize >= m_iconSize) {
            m_disappearingIconSize = m_iconSize;
        }

        if (value <= 0.0) {
            m_disappearingItems.clear();
        }
    } else {
        m_disappearingIconSize = m_iconSize;
        m_disappearingOpacity = (value - 0.25) * 4 / 3;
    }
}

void KFilePlacesViewDelegate::setShowHoverIndication(bool show)
{
    m_showHoverIndication = show;
}

void KFilePlacesViewDelegate::addFadeAnimation(const QModelIndex &index, QTimeLine *timeLine)
{
    m_timeLineMap.insert(index, timeLine);
    m_timeLineInverseMap.insert(timeLine, index);
}

void KFilePlacesViewDelegate::removeFadeAnimation(const QModelIndex &index)
{
    QTimeLine *timeLine = m_timeLineMap.value(index, nullptr);
    m_timeLineMap.remove(index);
    m_timeLineInverseMap.remove(timeLine);
}

QModelIndex KFilePlacesViewDelegate::indexForFadeAnimation(QTimeLine *timeLine) const
{
    return m_timeLineInverseMap.value(timeLine, QModelIndex());
}

QTimeLine *KFilePlacesViewDelegate::fadeAnimationForIndex(const QModelIndex &index) const
{
    return m_timeLineMap.value(index, nullptr);
}

qreal KFilePlacesViewDelegate::contentsOpacity(const QModelIndex &index) const
{
    QTimeLine *timeLine = fadeAnimationForIndex(index);
    if (timeLine) {
        return timeLine->currentValue();
    }
    return 0;
}

bool KFilePlacesViewDelegate::pointIsHeaderArea(const QPoint &pos)
{
    // we only accept drag events starting from item body, ignore drag request from header
    QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid()) {
        return false;
    }

    if (indexIsSectionHeader(index)) {
        const QRect vRect = m_view->visualRect(index);
        const int delegateY = pos.y() - vRect.y();
        if (delegateY <= sectionHeaderHeight()) {
            return true;
        }
    }
    return false;
}

void KFilePlacesViewDelegate::startDrag()
{
    m_dragStarted = true;
}

void KFilePlacesViewDelegate::clearFreeSpaceInfo()
{
    m_freeSpaceInfo.clear();
}

QString KFilePlacesViewDelegate::groupNameFromIndex(const QModelIndex &index) const
{
    if (index.isValid()) {
        return index.data(KFilePlacesModel::GroupRole).toString();
    } else {
        return QString();
    }
}

QModelIndex KFilePlacesViewDelegate::previousVisibleIndex(const QModelIndex &index) const
{
    if (index.row() == 0) {
        return QModelIndex();
    }

    const QAbstractItemModel *model = index.model();
    QModelIndex prevIndex = model->index(index.row() - 1, index.column(), index.parent());

    while (m_view->isRowHidden(prevIndex.row())) {
        if (prevIndex.row() == 0) {
            return QModelIndex();
        }
        prevIndex = model->index(prevIndex.row() - 1, index.column(), index.parent());
    }

    return prevIndex;
}

bool KFilePlacesViewDelegate::indexIsSectionHeader(const QModelIndex &index) const
{
    if (m_view->isRowHidden(index.row())) {
        return false;
    }

    if (index.row() == 0) {
        return true;
    }

    const auto groupName = groupNameFromIndex(index);
    const auto previousGroupName = groupNameFromIndex(previousVisibleIndex(index));
    return groupName != previousGroupName;
}

void KFilePlacesViewDelegate::drawSectionHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(index.model());

    const QString groupLabel = index.data(KFilePlacesModel::GroupRole).toString();
    const QString category = placesModel->isGroupHidden(index) ? i18n("%1 (hidden)", groupLabel) : groupLabel;

    QRect textRect(option.rect);
    textRect.setLeft(textRect.left() + 3);
    /* Take spacing into account:
       The spacing to the previous section compensates for the spacing to the first item.*/
    textRect.setY(textRect.y() /* + qMax(2, m_view->spacing()) - qMax(2, m_view->spacing())*/);
    textRect.setHeight(sectionHeaderHeight());

    painter->save();

    // based on dolphin colors
    const QColor c1 = textColor(option);
    const QColor c2 = baseColor(option);
    QColor penColor = mixedColor(c1, c2, 60);

    painter->setPen(penColor);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignBottom, category);
    painter->restore();
}

QColor KFilePlacesViewDelegate::textColor(const QStyleOption &option) const
{
    const QPalette::ColorGroup group = m_view->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    return option.palette.color(group, QPalette::WindowText);
}

QColor KFilePlacesViewDelegate::baseColor(const QStyleOption &option) const
{
    const QPalette::ColorGroup group = m_view->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    return option.palette.color(group,  QPalette::Window);
}

QColor KFilePlacesViewDelegate::mixedColor(const QColor& c1, const QColor& c2, int c1Percent) const
{
    Q_ASSERT(c1Percent >= 0 && c1Percent <= 100);

    const int c2Percent = 100 - c1Percent;
    return QColor((c1.red()   * c1Percent + c2.red()   * c2Percent) / 100,
                  (c1.green() * c1Percent + c2.green() * c2Percent) / 100,
                  (c1.blue()  * c1Percent + c2.blue()  * c2Percent) / 100);
}

int KFilePlacesViewDelegate::sectionHeaderHeight() const
{
    // Account for the spacing between header and item
    return QApplication::fontMetrics().height() + qMax(2, m_view->spacing());
}


class Q_DECL_HIDDEN KFilePlacesView::Private
{
public:
    explicit Private(KFilePlacesView *parent)
        : q(parent),
          m_watcher(new KFilePlacesEventWatcher(q)),
          m_delegate(new KFilePlacesViewDelegate(q))
    {}

    enum FadeType {
        FadeIn = 0,
        FadeOut,
    };

    void setCurrentIndex(const QModelIndex &index);
    // If m_autoResizeItems is true, calculates a proper size for the icons in the places panel
    void adaptItemSize();
    void updateHiddenRows();
    void clearFreeSpaceInfos();
    bool insertAbove(const QRect &itemRect, const QPoint &pos) const;
    bool insertBelow(const QRect &itemRect, const QPoint &pos) const;
    int insertIndicatorHeight(int itemHeight) const;
    void fadeCapacityBar(const QModelIndex &index, FadeType fadeType);
    int sectionsCount() const;

    void addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index);
    void triggerItemAppearingAnimation();
    void triggerItemDisappearingAnimation();

    void writeConfig();
    void readConfig();
    // Sets the size of the icons in the places panel
    void relayoutIconSize(int size);
    // Adds the "Icon Size" sub-menu items
    void setupIconSizeSubMenu(QMenu *submenu);

    void _k_placeClicked(const QModelIndex &index);
    void _k_placeEntered(const QModelIndex &index);
    void _k_placeLeft(const QModelIndex &index);
    void _k_storageSetupDone(const QModelIndex &index, bool success);
    void _k_adaptItemsUpdate(qreal value);
    void _k_itemAppearUpdate(qreal value);
    void _k_itemDisappearUpdate(qreal value);
    void _k_enableSmoothItemResizing();
    void _k_capacityBarFadeValueChanged();
    void _k_triggerDevicePolling();

    KFilePlacesView *const q;

    KFilePlacesEventWatcher *const m_watcher;
    KFilePlacesViewDelegate *m_delegate;

    Solid::StorageAccess *m_lastClickedStorage = nullptr;
    QPersistentModelIndex m_lastClickedIndex;

    std::unique_ptr<KIO::WidgetsAskUserActionHandler> m_askUserHandler;

    QTimeLine m_adaptItemsTimeline;
    QTimeLine m_itemAppearTimeline;
    QTimeLine m_itemDisappearTimeline;

    QTimer m_pollDevices;

    QRect m_dropRect;

    QUrl m_currentUrl;

    int m_oldSize = 0;
    int m_endSize = 0;
    int m_iconSz = 0;
    int m_pollingRequestCount = 0;

    bool m_autoResizeItems = true;
    bool m_smoothItemResizing = false;
    bool m_showAll = false;
    bool m_dropOnPlace = false;
    bool m_dragging = false;
};

KFilePlacesView::KFilePlacesView(QWidget *parent)
    : QListView(parent), d(new Private(this))
{
    setItemDelegate(d->m_delegate);

    d->readConfig();

    setSelectionRectVisible(false);
    setSelectionMode(SingleSelection);

    setDragEnabled(true);
    setAcceptDrops(true);
    setMouseTracking(true);
    setDropIndicatorShown(false);
    setFrameStyle(QFrame::NoFrame);

    setResizeMode(Adjust);

    QPalette palette = viewport()->palette();
    palette.setColor(viewport()->backgroundRole(), Qt::transparent);
    palette.setColor(viewport()->foregroundRole(), palette.color(QPalette::WindowText));
    viewport()->setPalette(palette);

    connect(this, &KFilePlacesView::clicked,
            this, [this](const QModelIndex &index) { d->_k_placeClicked(index); });
    // Note: Don't connect to the activated() signal, as the behavior when it is
    // committed depends on the used widget style. The click behavior of
    // KFilePlacesView should be style independent.

    connect(&d->m_adaptItemsTimeline, &QTimeLine::valueChanged,
            this, [this](qreal value) { d->_k_adaptItemsUpdate(value); });
    d->m_adaptItemsTimeline.setDuration(500);
    d->m_adaptItemsTimeline.setUpdateInterval(5);
    d->m_adaptItemsTimeline.setEasingCurve(QEasingCurve::InOutSine);

    connect(&d->m_itemAppearTimeline, &QTimeLine::valueChanged,
            this, [this](qreal value) { d->_k_itemAppearUpdate(value); });
    d->m_itemAppearTimeline.setDuration(500);
    d->m_itemAppearTimeline.setUpdateInterval(5);
    d->m_itemAppearTimeline.setEasingCurve(QEasingCurve::InOutSine);

    connect(&d->m_itemDisappearTimeline, &QTimeLine::valueChanged,
            this, [this](qreal value) { d->_k_itemDisappearUpdate(value); });
    d->m_itemDisappearTimeline.setDuration(500);
    d->m_itemDisappearTimeline.setUpdateInterval(5);
    d->m_itemDisappearTimeline.setEasingCurve(QEasingCurve::InOutSine);

    viewport()->installEventFilter(d->m_watcher);
    connect(d->m_watcher, &KFilePlacesEventWatcher::entryEntered,
            this, [this](const QModelIndex &index) { d->_k_placeEntered(index); });
    connect(d->m_watcher, &KFilePlacesEventWatcher::entryLeft,
            this, [this](const QModelIndex &index) { d->_k_placeLeft(index); });

    d->m_pollDevices.setInterval(5000);
    connect(&d->m_pollDevices, &QTimer::timeout, this, [this]() { d->_k_triggerDevicePolling(); });

    // FIXME: this is necessary to avoid flashes of black with some widget styles.
    // could be a bug in Qt (e.g. QAbstractScrollArea) or KFilePlacesView, but has not
    // yet been tracked down yet. until then, this works and is harmlessly enough.
    // in fact, some QStyle (Oxygen, Skulpture, others?) do this already internally.
    // See br #242358 for more information
    verticalScrollBar()->setAttribute(Qt::WA_OpaquePaintEvent, false);
}

KFilePlacesView::~KFilePlacesView()
{
    delete d;
}

void KFilePlacesView::setDropOnPlaceEnabled(bool enabled)
{
    d->m_dropOnPlace = enabled;
}

bool KFilePlacesView::isDropOnPlaceEnabled() const
{
    return d->m_dropOnPlace;
}

void KFilePlacesView::setAutoResizeItemsEnabled(bool enabled)
{
    d->m_autoResizeItems = enabled;
}

bool KFilePlacesView::isAutoResizeItemsEnabled() const
{
    return d->m_autoResizeItems;
}

void KFilePlacesView::setUrl(const QUrl &url)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());

    if (placesModel == nullptr) {
        return;
    }

    QModelIndex index = placesModel->closestItem(url);
    QModelIndex current = selectionModel()->currentIndex();

    if (index.isValid()) {
        if (current != index && placesModel->isHidden(current) && !d->m_showAll) {
            d->addDisappearingItem(d->m_delegate, current);
        }

        if (current != index && placesModel->isHidden(index) && !d->m_showAll) {
            d->m_delegate->addAppearingItem(index);
            d->triggerItemAppearingAnimation();
            setRowHidden(index.row(), false);
        }

        d->m_currentUrl = url;

        if (placesModel->url(index) == url.adjusted(QUrl::StripTrailingSlash) ) {
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        } else {
            selectionModel()->clear();
        }
    } else {
        d->m_currentUrl = QUrl();
        selectionModel()->clear();
    }

    if (!current.isValid()) {
        d->updateHiddenRows();
    }
}

void KFilePlacesView::setShowAll(bool showAll)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());

    if (placesModel == nullptr) {
        return;
    }

    d->m_showAll = showAll;

    int rowCount = placesModel->rowCount();
    QModelIndex current = placesModel->closestItem(d->m_currentUrl);

    if (showAll) {
        d->updateHiddenRows();

        for (int i = 0; i < rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index != current && placesModel->isHidden(index)) {
                d->m_delegate->addAppearingItem(index);
            }
        }
        d->triggerItemAppearingAnimation();
    } else {
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index != current && placesModel->isHidden(index)) {
                d->m_delegate->addDisappearingItem(index);
            }
        }
        d->triggerItemDisappearingAnimation();
    }
}

void KFilePlacesView::keyPressEvent(QKeyEvent *event)
{
    QListView::keyPressEvent(event);
    if ((event->key() == Qt::Key_Return) || (event->key() == Qt::Key_Enter)) {
        d->_k_placeClicked(currentIndex());
    }
}

void KFilePlacesView::Private::readConfig()
{
    KConfigGroup cg(KSharedConfig::openConfig(), ConfigGroup);
    m_autoResizeItems = cg.readEntry(PlacesIconsAutoresize, true);
    m_delegate->setIconSize(cg.readEntry(PlacesIconsStaticSize, static_cast<int>(KIconLoader::SizeMedium)));
}

void KFilePlacesView::Private::writeConfig()
{
    KConfigGroup cg(KSharedConfig::openConfig(), ConfigGroup);
    cg.writeEntry(PlacesIconsAutoresize, m_autoResizeItems);

    if (!m_autoResizeItems) {
        cg.writeEntry(PlacesIconsStaticSize, m_iconSz);
    }

    cg.sync();
}

void KFilePlacesView::contextMenuEvent(QContextMenuEvent *event)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());

    if (!placesModel) {
        return;
    }

    QModelIndex index = indexAt(event->pos());
    const QString label = placesModel->text(index).replace(QLatin1Char('&'), QLatin1String("&&"));
    const QUrl placeUrl = placesModel->url(index);

    QMenu menu;

    QAction *edit = nullptr;
    QAction *hide = nullptr;
    QAction *emptyTrash = nullptr;
    QAction *eject = nullptr;
    QAction *teardown = nullptr;
    QAction *add = nullptr;
    QAction *mainSeparator = nullptr;
    QAction *hideSection = nullptr;
    QAction *properties = nullptr;
    QAction *mount = nullptr;

    const bool clickOverHeader = d->m_delegate->pointIsHeaderArea(event->pos());
    if (clickOverHeader) {
        const KFilePlacesModel::GroupType type = placesModel->groupType(index);
        hideSection = menu.addAction(QIcon::fromTheme(QStringLiteral("hint")), i18n("Hide Section"));
        hideSection->setCheckable(true);
        hideSection->setChecked(placesModel->isGroupHidden(type));
    }
    else if (index.isValid()) {
        if (!placesModel->isDevice(index)) {
            if (placeUrl.toString() == QLatin1String("trash:/")) {
                emptyTrash = menu.addAction(QIcon::fromTheme(QStringLiteral("trash-empty")), i18nc("@action:inmenu", "Empty Trash"));
                KConfig trashConfig(QStringLiteral("trashrc"), KConfig::SimpleConfig);
                emptyTrash->setEnabled(!trashConfig.group("Status").readEntry("Empty", true));
                menu.addSeparator();
            }
            add = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Add Entry..."));
            mainSeparator = menu.addSeparator();
        } else {
            eject = placesModel->ejectActionForIndex(index);
            if (eject != nullptr) {
                eject->setParent(&menu);
                menu.addAction(eject);
            }

            teardown = placesModel->teardownActionForIndex(index);
            if (teardown != nullptr) {
                // Disable teardown option for root and home partitions
                bool teardownEnabled = placeUrl != QUrl::fromLocalFile(QDir::rootPath());
                if (teardownEnabled) {
                    KMountPoint::Ptr mountPoint = KMountPoint::currentMountPoints().findByPath(QDir::homePath());
                    if (mountPoint && placeUrl == QUrl::fromLocalFile(mountPoint->mountPoint())) {
                        teardownEnabled = false;
                    }
                }
                teardown->setEnabled(teardownEnabled);

                teardown->setParent(&menu);
                menu.addAction(teardown);
            }

            if (placesModel->setupNeeded(index)) {
                mount = menu.addAction(QIcon::fromTheme(QStringLiteral("media-mount")), i18nc("@action:inmenu", "Mount"));
            }

            if (teardown != nullptr || eject != nullptr || mount != nullptr) {
                mainSeparator = menu.addSeparator();
            }
        }
        if (add == nullptr) {
            add = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Add Entry..."));
        }
        if (placeUrl.isLocalFile()) {
            properties = menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Properties"));
        }
        if (!placesModel->isDevice(index)) {
            edit = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-entry")), i18n("&Edit Entry '%1'...", label));
        }

        hide = menu.addAction(QIcon::fromTheme(QStringLiteral("hint")), i18n("&Hide Entry '%1'", label));
        hide->setCheckable(true);
        hide->setChecked(placesModel->isHidden(index));
        // if a parent is hidden no interaction should be possible with children, show it first to do so
        hide->setEnabled(!placesModel->isGroupHidden(placesModel->groupType(index)));

    } else {
        add = menu.addAction(QIcon::fromTheme(QStringLiteral("document-new")), i18n("Add Entry..."));
    }

    QAction *showAll = nullptr;
    if (placesModel->hiddenCount() > 0) {
        showAll = new QAction(QIcon::fromTheme(QStringLiteral("visibility")), i18n("&Show All Entries"), &menu);
        showAll->setCheckable(true);
        showAll->setChecked(d->m_showAll);
        if (mainSeparator == nullptr) {
            mainSeparator = menu.addSeparator();
        }
        menu.insertAction(mainSeparator, showAll);
    }

    QAction *remove = nullptr;
    if (!clickOverHeader && index.isValid() && !placesModel->isDevice(index)) {
        remove = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("&Remove Entry '%1'", label));
    }

    QMenu *iconSizeMenu = new QMenu(i18nc("@item:inmenu", "Icon Size"), &menu);
    menu.insertMenu(mainSeparator, iconSizeMenu);
    d->setupIconSizeSubMenu(iconSizeMenu);

    menu.addActions(actions());

    if (menu.isEmpty()) {
        return;
    }

    QAction *result = menu.exec(event->globalPos());

    if (emptyTrash && (result == emptyTrash)) {
        auto *parentWindow = window();

        if (!d->m_askUserHandler) {
            d->m_askUserHandler.reset(new KIO::WidgetsAskUserActionHandler{});

            connect(d->m_askUserHandler.get(),
                    &KIO::AskUserActionInterface::askUserDeleteResult,
                    this,
                    [parentWindow](bool allowDelete, const QList<QUrl> &, KIO::AskUserActionInterface::DeletionType, QWidget *parent) {
                        if (parent != parentWindow || !allowDelete) {
                            return;
                        }

                        KIO::Job *job = KIO::emptyTrash();
                        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, parentWindow));
                    });
        }

        d->m_askUserHandler->askUserDelete(QList<QUrl>{}, KIO::AskUserActionInterface::EmptyTrash,
                                           KIO::AskUserActionInterface::DefaultConfirmation,
                                           parentWindow);

    } else if (properties && (result == properties)) {
        KPropertiesDialog::showDialog(placeUrl, this);
    } else if (edit && (result == edit)) {
        KBookmark bookmark = placesModel->bookmarkForIndex(index);
        QUrl url = bookmark.url();
        QString label = bookmark.text();
        QString iconName = bookmark.icon();
        bool appLocal = !bookmark.metaDataItem(QStringLiteral("OnlyInApp")).isEmpty();

        if (KFilePlaceEditDialog::getInformation(true, url, label,
                iconName, false, appLocal, 64, this)) {
            QString appName;
            if (appLocal) {
                appName = QCoreApplication::instance()->applicationName();
            }

            placesModel->editPlace(index, label, url, iconName, appName);
        }

    } else if (remove && (result == remove)) {
        placesModel->removePlace(index);
    } else if (hideSection && (result == hideSection)) {
        const KFilePlacesModel::GroupType type = placesModel->groupType(index);
        placesModel->setGroupHidden(type, hideSection->isChecked());

        if (!d->m_showAll && hideSection->isChecked()) {
            d->m_delegate->addDisappearingItemGroup(index);
            d->triggerItemDisappearingAnimation();
        }
    } else if (hide && (result == hide)) {
        placesModel->setPlaceHidden(index, hide->isChecked());
        QModelIndex current = placesModel->closestItem(d->m_currentUrl);

        if (index != current && !d->m_showAll && hide->isChecked()) {
            d->m_delegate->addDisappearingItem(index);
            d->triggerItemDisappearingAnimation();
        }
    } else if (showAll && (result == showAll)) {
        setShowAll(showAll->isChecked());
    } else if (teardown && (result == teardown)) {
        placesModel->requestTeardown(index);
    } else if (eject && (result == eject)) {
        placesModel->requestEject(index);
    } else if (add && (result == add)) {
        QUrl url = d->m_currentUrl;
        QString label;
        QString iconName = QStringLiteral("folder");
        bool appLocal = true;
        if (KFilePlaceEditDialog::getInformation(true, url, label,
                iconName, true, appLocal, 64, this)) {
            QString appName;
            if (appLocal) {
                appName = QCoreApplication::instance()->applicationName();
            }

            placesModel->addPlace(label, url, iconName, appName, index);
        }
    } else if (mount && (result == mount)) {
        placesModel->requestSetup(index);
    }

    index = placesModel->closestItem(d->m_currentUrl);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

void KFilePlacesView::Private::setupIconSizeSubMenu(QMenu *submenu)
{
    QActionGroup *group = new QActionGroup(submenu);

    auto *autoAct = new QAction(i18nc("@item:inmenu Auto set icon size based on available space in"
                                      "the Places side-panel", "Auto Resize"),
                                group);
    autoAct->setCheckable(true);
    autoAct->setChecked(m_autoResizeItems);
    QObject::connect(autoAct, &QAction::toggled, q, [this]() {
        m_autoResizeItems = true;
        adaptItemSize();
        writeConfig();
    });
    submenu->addAction(autoAct);

    constexpr KIconLoader::StdSizes iconSizes[] = { KIconLoader::SizeSmall,
                                                    KIconLoader::SizeSmallMedium,
                                                    KIconLoader::SizeMedium,
                                                    KIconLoader::SizeLarge
                                                  };

    for (const auto iconSize : iconSizes) {
        auto *act = new QAction(group);
        act->setCheckable(true);

        switch (iconSize) {
        case KIconLoader::SizeSmall:
            act->setText(i18nc("Small icon size", "Small (%1x%1)", KIconLoader::SizeSmall));
            break;
        case KIconLoader::SizeSmallMedium:
            act->setText(i18nc("Medium icon size", "Medium (%1x%1)", KIconLoader::SizeSmallMedium));
            break;
        case KIconLoader::SizeMedium:
            act->setText(i18nc("Large icon size", "Large (%1x%1)", KIconLoader::SizeMedium));
            break;
        case KIconLoader::SizeLarge:
            act->setText(i18nc("Huge icon size", "Huge (%1x%1)", KIconLoader::SizeLarge));
            break;
        default:
            break;
        }

        QObject::connect(act, &QAction::toggled, q, [this, iconSize]() {
            m_autoResizeItems = false;
            relayoutIconSize(iconSize);
            // Store the new icon size in m_iconSz; which will be used by writeConfig(),
            // otherwise if m_smoothItemResizing is true, the delegate icon size will be
            // changed after the m_adaptItemsTimeline times out, by which time writeConfig
            // has already finished, which means it won't save the new icon size
            m_iconSz = iconSize;
            writeConfig();
        });

        if (!m_autoResizeItems) {
            act->setChecked(iconSize == m_delegate->iconSize());
        }

        submenu->addAction(act);
    }
}

void KFilePlacesView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    d->adaptItemSize();
}

void KFilePlacesView::showEvent(QShowEvent *event)
{
    QListView::showEvent(event);
    QTimer::singleShot(100, this, SLOT(_k_enableSmoothItemResizing()));
}

void KFilePlacesView::hideEvent(QHideEvent *event)
{
    QListView::hideEvent(event);
    d->m_smoothItemResizing = false;
}

void KFilePlacesView::dragEnterEvent(QDragEnterEvent *event)
{
    QListView::dragEnterEvent(event);
    d->m_dragging = true;

    d->m_delegate->setShowHoverIndication(false);

    d->m_dropRect = QRect();
}

void KFilePlacesView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QListView::dragLeaveEvent(event);
    d->m_dragging = false;

    d->m_delegate->setShowHoverIndication(true);

    setDirtyRegion(d->m_dropRect);
}

void KFilePlacesView::dragMoveEvent(QDragMoveEvent *event)
{
    QListView::dragMoveEvent(event);

    // update the drop indicator
    const QPoint pos = event->pos();
    const QModelIndex index = indexAt(pos);
    setDirtyRegion(d->m_dropRect);
    if (index.isValid()) {
        const QRect rect = visualRect(index);
        const int gap = d->insertIndicatorHeight(rect.height());
        if (d->insertAbove(rect, pos)) {
            // indicate that the item will be inserted above the current place
            d->m_dropRect = QRect(rect.left(), rect.top() - gap / 2,
                                rect.width(), gap);
        } else if (d->insertBelow(rect, pos)) {
            // indicate that the item will be inserted below the current place
            d->m_dropRect = QRect(rect.left(), rect.bottom() + 1 -  gap / 2,
                                rect.width(), gap);
        } else {
            // indicate that the item be dropped above the current place
            d->m_dropRect = rect;
        }
    }

    setDirtyRegion(d->m_dropRect);
}

void KFilePlacesView::dropEvent(QDropEvent *event)
{
    const QPoint pos = event->pos();
    const QModelIndex index = indexAt(pos);
    if (index.isValid()) {
        const QRect rect = visualRect(index);
        if (!d->insertAbove(rect, pos) && !d->insertBelow(rect, pos)) {
            KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());
            Q_ASSERT(placesModel != nullptr);
            Q_EMIT urlsDropped(placesModel->url(index), event, this);
            event->acceptProposedAction();
        }
    }

    QListView::dropEvent(event);
    d->m_dragging = false;

    d->m_delegate->setShowHoverIndication(true);
}

void KFilePlacesView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (d->m_dragging && !d->m_dropRect.isEmpty()) {
        // draw drop indicator
        QPainter painter(viewport());

        const QModelIndex index = indexAt(d->m_dropRect.topLeft());
        const QRect itemRect = visualRect(index);
        const bool drawInsertIndicator = !d->m_dropOnPlace ||
                                         d->m_dropRect.height() <= d->insertIndicatorHeight(itemRect.height());

        if (drawInsertIndicator) {
            // draw indicator for inserting items
            QBrush blendedBrush = viewOptions().palette.brush(QPalette::Normal, QPalette::Highlight);
            QColor color = blendedBrush.color();

            const int y = (d->m_dropRect.top() + d->m_dropRect.bottom()) / 2;
            const int thickness = d->m_dropRect.height() / 2;
            Q_ASSERT(thickness >= 1);
            int alpha = 255;
            const int alphaDec = alpha / (thickness + 1);
            for (int i = 0; i < thickness; i++) {
                color.setAlpha(alpha);
                alpha -= alphaDec;
                painter.setPen(color);
                painter.drawLine(d->m_dropRect.left(), y - i, d->m_dropRect.right(), y - i);
                painter.drawLine(d->m_dropRect.left(), y + i, d->m_dropRect.right(), y + i);
            }
        } else {
            // draw indicator for copying/moving/linking to items
            QStyleOptionViewItem opt;
            opt.initFrom(this);
            opt.rect = itemRect;
            opt.state = QStyle::State_Enabled | QStyle::State_MouseOver;
            style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, &painter, this);
        }
    }
}

void KFilePlacesView::startDrag(Qt::DropActions supportedActions)
{
    d->m_delegate->startDrag();
    QListView::startDrag(supportedActions);
}

void KFilePlacesView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // does not accept drags from section header area
        if (d->m_delegate->pointIsHeaderArea(event->pos())) {
            return;
        }
    }
    QListView::mousePressEvent(event);
}

void KFilePlacesView::setModel(QAbstractItemModel *model)
{
    QListView::setModel(model);
    d->updateHiddenRows();
    // Uses Qt::QueuedConnection to delay the time when the slot will be
    // called. In case of an item move the remove+add will be done before
    // we adapt the item size (otherwise we'd get it wrong as we'd execute
    // it after the remove only).
    connect(model, &QAbstractItemModel::rowsRemoved, this, [this]() { d->adaptItemSize(); }, Qt::QueuedConnection);
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            d->m_watcher, &KFilePlacesEventWatcher::currentIndexChanged);

    d->m_delegate->clearFreeSpaceInfo();
}

void KFilePlacesView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QListView::rowsInserted(parent, start, end);
    setUrl(d->m_currentUrl);

    KFilePlacesModel *placesModel = static_cast<KFilePlacesModel *>(model());

    for (int i = start; i <= end; ++i) {
        QModelIndex index = placesModel->index(i, 0, parent);
        if (d->m_showAll || !placesModel->isHidden(index)) {
            d->m_delegate->addAppearingItem(index);
            d->triggerItemAppearingAnimation();
        } else {
            setRowHidden(i, true);
        }
    }

    d->triggerItemAppearingAnimation();

    d->adaptItemSize();
}

QSize KFilePlacesView::sizeHint() const
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());
    if (!placesModel) {
        return QListView::sizeHint();
    }
    const int height = QListView::sizeHint().height();
    QFontMetrics fm = d->q->fontMetrics();
    int textWidth = 0;

    for (int i = 0; i < placesModel->rowCount(); ++i) {
        QModelIndex index = placesModel->index(i, 0);
        if (!placesModel->isHidden(index)) {
            textWidth = qMax(textWidth, fm.boundingRect(index.data(Qt::DisplayRole).toString()).width());
        }
    }

    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize) + 3 * LATERAL_MARGIN;
    return QSize(iconSize + textWidth + fm.height() / 2, height);
}

void KFilePlacesView::Private::addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index)
{
    delegate->addDisappearingItem(index);
    if (m_itemDisappearTimeline.state() != QTimeLine::Running) {
        delegate->setDisappearingItemProgress(0.0);
        m_itemDisappearTimeline.start();
    }
}

void KFilePlacesView::Private::setCurrentIndex(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    QUrl url = placesModel->url(index);

    if (url.isValid()) {
        m_currentUrl = url;
        updateHiddenRows();
        Q_EMIT q->urlChanged(KFilePlacesModel::convertedUrl(url));
        if (m_showAll) {
            q->setShowAll(false);
        }
    } else {
        q->setUrl(m_currentUrl);
    }
}

void KFilePlacesView::Private::adaptItemSize()
{
    if (!m_autoResizeItems) {
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    int rowCount = placesModel->rowCount();

    if (!m_showAll) {
        rowCount -= placesModel->hiddenCount();

        QModelIndex current = placesModel->closestItem(m_currentUrl);

        if (placesModel->isHidden(current)) {
            ++rowCount;
        }
    }

    if (rowCount == 0) {
        return;    // We've nothing to display anyway
    }

    const int minSize = q->style()->pixelMetric(QStyle::PM_SmallIconSize);
    const int maxSize = 64;

    int textWidth = 0;
    QFontMetrics fm = q->fontMetrics();
    for (int i = 0; i < placesModel->rowCount(); ++i) {
        QModelIndex index = placesModel->index(i, 0);

        if (!placesModel->isHidden(index)) {
            textWidth = qMax(textWidth, fm.boundingRect(index.data(Qt::DisplayRole).toString()).width());
        }
    }

    const int margin = q->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, q) + 1;
    const int maxWidth = q->viewport()->width() - textWidth - 4 * margin - 1;

    const int totalItemsHeight = (fm.height() / 2) * rowCount;
    const int totalSectionsHeight = m_delegate->sectionHeaderHeight() * sectionsCount();
    const int maxHeight = ((q->height() - totalSectionsHeight - totalItemsHeight) / rowCount) - 1;

    int size = qMin(maxHeight, maxWidth);

    if (size < minSize) {
        size = minSize;
    } else if (size > maxSize) {
        size = maxSize;
    } else {
        // Make it a multiple of 16
        size &= ~0xf;
    }

    relayoutIconSize(size);
}

void KFilePlacesView::Private::relayoutIconSize(const int size)
{
    if (size == m_delegate->iconSize()) {
        return;
    }

    if (m_smoothItemResizing) {
        m_oldSize = m_delegate->iconSize();
        m_endSize = size;
        if (m_adaptItemsTimeline.state() != QTimeLine::Running) {
            m_adaptItemsTimeline.start();
        }
    } else {
        m_delegate->setIconSize(size);
        q->scheduleDelayedItemsLayout();
    }
}

void KFilePlacesView::Private::updateHiddenRows()
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    int rowCount = placesModel->rowCount();
    QModelIndex current = placesModel->closestItem(m_currentUrl);

    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = placesModel->index(i, 0);
        if (index != current && placesModel->isHidden(index) && !m_showAll) {
            q->setRowHidden(i, true);
        } else {
            q->setRowHidden(i, false);
        }
    }

    adaptItemSize();
}

bool KFilePlacesView::Private::insertAbove(const QRect &itemRect, const QPoint &pos) const
{
    if (m_dropOnPlace) {
        return pos.y() < itemRect.top() + insertIndicatorHeight(itemRect.height()) / 2;
    }

    return pos.y() < itemRect.top() + (itemRect.height() / 2);
}

bool KFilePlacesView::Private::insertBelow(const QRect &itemRect, const QPoint &pos) const
{
    if (m_dropOnPlace) {
        return pos.y() > itemRect.bottom() - insertIndicatorHeight(itemRect.height()) / 2;
    }

    return pos.y() >= itemRect.top() + (itemRect.height() / 2);
}

int KFilePlacesView::Private::insertIndicatorHeight(int itemHeight) const
{
    const int min = 4;
    const int max = 12;

    int height = itemHeight / 4;
    if (height < min) {
        height = min;
    } else if (height > max) {
        height = max;
    }
    return height;
}

void KFilePlacesView::Private::fadeCapacityBar(const QModelIndex &index, FadeType fadeType)
{
    QTimeLine *timeLine = m_delegate->fadeAnimationForIndex(index);
    delete timeLine;
    m_delegate->removeFadeAnimation(index);
    timeLine = new QTimeLine(250, q);
    connect(timeLine, &QTimeLine::valueChanged, q, [this]() { _k_capacityBarFadeValueChanged(); });
    if (fadeType == FadeIn) {
        timeLine->setDirection(QTimeLine::Forward);
        timeLine->setCurrentTime(0);
    } else {
        timeLine->setDirection(QTimeLine::Backward);
        timeLine->setCurrentTime(250);
    }
    m_delegate->addFadeAnimation(index, timeLine);
    timeLine->start();
}

int KFilePlacesView::Private::sectionsCount() const
{
    int count = 0;
    QString prevSection;
    const int rowCount = q->model()->rowCount();

    for(int i = 0; i < rowCount; i++) {
        if (!q->isRowHidden(i)) {
            const QModelIndex index = q->model()->index(i, 0);
            const QString sectionName = index.data(KFilePlacesModel::GroupRole).toString();
            if (prevSection != sectionName) {
                prevSection = sectionName;
                ++count;
            }
        }
    }

    return count;
}

void KFilePlacesView::Private::triggerItemAppearingAnimation()
{
    if (m_itemAppearTimeline.state() != QTimeLine::Running) {
        m_delegate->setAppearingItemProgress(0.0);
        m_itemAppearTimeline.start();
    }
}

void KFilePlacesView::Private::triggerItemDisappearingAnimation()
{
    if (m_itemDisappearTimeline.state() != QTimeLine::Running) {
        m_delegate->setDisappearingItemProgress(0.0);
        m_itemDisappearTimeline.start();
    }
}

void KFilePlacesView::Private::_k_placeClicked(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    m_lastClickedIndex = QPersistentModelIndex();

    if (placesModel->setupNeeded(index)) {
        QObject::connect(placesModel, &KFilePlacesModel::setupDone,
                         q, [this](const QModelIndex &idx, bool success) { _k_storageSetupDone(idx, success); });

        m_lastClickedIndex = index;
        placesModel->requestSetup(index);
        return;
    }

    setCurrentIndex(index);
}

void KFilePlacesView::Private::_k_placeEntered(const QModelIndex &index)
{
    fadeCapacityBar(index, FadeIn);
    ++m_pollingRequestCount;
    if (m_pollingRequestCount == 1) {
        m_pollDevices.start();
    }
}

void KFilePlacesView::Private::_k_placeLeft(const QModelIndex &index)
{
    fadeCapacityBar(index, FadeOut);
    --m_pollingRequestCount;
    if (!m_pollingRequestCount) {
        m_pollDevices.stop();
    }
}

void KFilePlacesView::Private::_k_storageSetupDone(const QModelIndex &index, bool success)
{
    if (index != m_lastClickedIndex) {
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel) {
        QObject::disconnect(placesModel, &KFilePlacesModel::setupDone, q, nullptr);
    }

    if (success) {
        setCurrentIndex(m_lastClickedIndex);
    } else {
        q->setUrl(m_currentUrl);
    }

    m_lastClickedIndex = QPersistentModelIndex();
}

void KFilePlacesView::Private::_k_adaptItemsUpdate(qreal value)
{
    const int add = (m_endSize - m_oldSize) * value;
    const int size = m_oldSize + add;

    m_delegate->setIconSize(size);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemAppearUpdate(qreal value)
{
    m_delegate->setAppearingItemProgress(value);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemDisappearUpdate(qreal value)
{
    m_delegate->setDisappearingItemProgress(value);

    if (value >= 1.0) {
        updateHiddenRows();
    }

    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_enableSmoothItemResizing()
{
    m_smoothItemResizing = true;
}

void KFilePlacesView::Private::_k_capacityBarFadeValueChanged()
{
    const QModelIndex index = m_delegate->indexForFadeAnimation(static_cast<QTimeLine *>(q->sender()));
    if (!index.isValid()) {
        return;
    }
    q->update(index);
}

void KFilePlacesView::Private::_k_triggerDevicePolling()
{
    const QModelIndex hoveredIndex = m_watcher->hoveredIndex();
    if (hoveredIndex.isValid()) {
        const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(hoveredIndex.model());
        if (placesModel->isDevice(hoveredIndex)) {
            q->update(hoveredIndex);
        }
    }
    const QModelIndex focusedIndex = m_watcher->focusedIndex();
    if (focusedIndex.isValid() && focusedIndex != hoveredIndex) {
        const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(focusedIndex.model());
        if (placesModel->isDevice(focusedIndex)) {
            q->update(focusedIndex);
        }
    }
}

void KFilePlacesView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                                  const QVector<int> &roles)
{
    QListView::dataChanged(topLeft, bottomRight, roles);
    d->adaptItemSize();
}

#include "moc_kfileplacesview.cpp"
#include "moc_kfileplacesview_p.cpp"
#include "kfileplacesview.moc"
