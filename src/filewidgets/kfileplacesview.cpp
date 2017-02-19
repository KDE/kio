/*  This file is part of the KDE project
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2008 Rafael Fernández López <ereslibre@kde.org>

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
#include <QScrollBar>

#include <QDebug>

#include <kconfig.h>
#include <kconfiggroup.h>
#include <kdirnotify.h>
#include <klocalizedstring.h>
#include <kmessagebox.h>
#include <kmountpoint.h>
#include <kpropertiesdialog.h>
#include <kio/emptytrashjob.h>
#include <kio/jobuidelegate.h>
#include <kjob.h>
#include <kjobwidgets.h>
#include <kcapacitybar.h>
#include <kdiskfreespaceinfo.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>

#include "kfileplaceeditdialog.h"
#include "kfileplacesmodel.h"

#define LATERAL_MARGIN 4
#define CAPACITYBAR_HEIGHT 6

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

    if (!m_showHoverIndication) {
        opt.state &= ~QStyle::State_MouseOver;
    }

    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);
    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(index.model());

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
        if (url.isLocalFile() && contentsOpacity(index) > 0) {
            const QString mountPointPath = url.toLocalFile();

            const KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(mountPointPath);
            drawCapacityBar = info.size() != 0;
            if (drawCapacityBar) {
                painter->save();
                painter->setOpacity(painter->opacity() * contentsOpacity(index));

                int height = opt.fontMetrics.height() + CAPACITYBAR_HEIGHT;
                rectText = QRect(isLTR ? m_iconSize + LATERAL_MARGIN * 2 + opt.rect.left()
                                 : 0, opt.rect.top() + (opt.rect.height() / 2 - height / 2), opt.rect.width() - m_iconSize - LATERAL_MARGIN * 2, opt.fontMetrics.height());
                painter->drawText(rectText, Qt::AlignLeft | Qt::AlignTop, opt.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));
                QRect capacityRect(isLTR ? rectText.x() : LATERAL_MARGIN, rectText.bottom() - 1, rectText.width() - LATERAL_MARGIN, CAPACITYBAR_HEIGHT);
                KCapacityBar capacityBar(KCapacityBar::DrawTextInline);
                capacityBar.setValue((info.used() * 100) / info.size());
                capacityBar.drawCapacityBar(painter, capacityRect);

                painter->restore();

                painter->save();
                painter->setOpacity(painter->opacity() * (1 - contentsOpacity(index)));
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
        : q(parent)
        , watcher(new KFilePlacesEventWatcher(q))
    {}

    enum FadeType {
        FadeIn = 0,
        FadeOut
    };

    KFilePlacesView *const q;

    QUrl currentUrl;
    bool autoResizeItems;
    bool showAll;
    bool smoothItemResizing;
    bool dropOnPlace;
    bool dragging;
    Solid::StorageAccess *lastClickedStorage = nullptr;
    QPersistentModelIndex lastClickedIndex;

    QRect dropRect;

    void setCurrentIndex(const QModelIndex &index);
    void adaptItemSize();
    void updateHiddenRows();
    bool insertAbove(const QRect &itemRect, const QPoint &pos) const;
    bool insertBelow(const QRect &itemRect, const QPoint &pos) const;
    int insertIndicatorHeight(int itemHeight) const;
    void fadeCapacityBar(const QModelIndex &index, FadeType fadeType);
    int sectionsCount() const;

    void addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index);

    void triggerItemAppearingAnimation();
    void triggerItemDisappearingAnimation();

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

    QTimeLine adaptItemsTimeline;
    int oldSize, endSize;

    QTimeLine itemAppearTimeline;
    QTimeLine itemDisappearTimeline;

    KFilePlacesEventWatcher *const watcher;
    KFilePlacesViewDelegate *delegate = nullptr;
    QTimer pollDevices;
    int pollingRequestCount;
};

KFilePlacesView::KFilePlacesView(QWidget *parent)
    : QListView(parent), d(new Private(this))
{
    d->showAll = false;
    d->smoothItemResizing = false;
    d->dropOnPlace = false;
    d->autoResizeItems = true;
    d->dragging = false;
    d->lastClickedStorage = nullptr;
    d->pollingRequestCount = 0;
    d->delegate = new KFilePlacesViewDelegate(this);

    setSelectionRectVisible(false);
    setSelectionMode(SingleSelection);

    setDragEnabled(true);
    setAcceptDrops(true);
    setMouseTracking(true);
    setDropIndicatorShown(false);
    setFrameStyle(QFrame::NoFrame);

    setResizeMode(Adjust);
    setItemDelegate(d->delegate);

    QPalette palette = viewport()->palette();
    palette.setColor(viewport()->backgroundRole(), Qt::transparent);
    palette.setColor(viewport()->foregroundRole(), palette.color(QPalette::WindowText));
    viewport()->setPalette(palette);

    connect(this, SIGNAL(clicked(QModelIndex)),
            this, SLOT(_k_placeClicked(QModelIndex)));
    // Note: Don't connect to the activated() signal, as the behavior when it is
    // committed depends on the used widget style. The click behavior of
    // KFilePlacesView should be style independent.

    connect(&d->adaptItemsTimeline, SIGNAL(valueChanged(qreal)),
            this, SLOT(_k_adaptItemsUpdate(qreal)));
    d->adaptItemsTimeline.setDuration(500);
    d->adaptItemsTimeline.setUpdateInterval(5);
    d->adaptItemsTimeline.setCurveShape(QTimeLine::EaseInOutCurve);

    connect(&d->itemAppearTimeline, SIGNAL(valueChanged(qreal)),
            this, SLOT(_k_itemAppearUpdate(qreal)));
    d->itemAppearTimeline.setDuration(500);
    d->itemAppearTimeline.setUpdateInterval(5);
    d->itemAppearTimeline.setCurveShape(QTimeLine::EaseInOutCurve);

    connect(&d->itemDisappearTimeline, SIGNAL(valueChanged(qreal)),
            this, SLOT(_k_itemDisappearUpdate(qreal)));
    d->itemDisappearTimeline.setDuration(500);
    d->itemDisappearTimeline.setUpdateInterval(5);
    d->itemDisappearTimeline.setCurveShape(QTimeLine::EaseInOutCurve);

    viewport()->installEventFilter(d->watcher);
    connect(d->watcher, SIGNAL(entryEntered(QModelIndex)),
            this, SLOT(_k_placeEntered(QModelIndex)));
    connect(d->watcher, SIGNAL(entryLeft(QModelIndex)),
            this, SLOT(_k_placeLeft(QModelIndex)));

    d->pollDevices.setInterval(5000);
    connect(&d->pollDevices, SIGNAL(timeout()), this, SLOT(_k_triggerDevicePolling()));

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
    d->dropOnPlace = enabled;
}

bool KFilePlacesView::isDropOnPlaceEnabled() const
{
    return d->dropOnPlace;
}

void KFilePlacesView::setAutoResizeItemsEnabled(bool enabled)
{
    d->autoResizeItems = enabled;
}

bool KFilePlacesView::isAutoResizeItemsEnabled() const
{
    return d->autoResizeItems;
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
        if (current != index && placesModel->isHidden(current) && !d->showAll) {
            KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
            d->addDisappearingItem(delegate, current);
        }

        if (current != index && placesModel->isHidden(index) && !d->showAll) {
            KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
            delegate->addAppearingItem(index);
            d->triggerItemAppearingAnimation();
            setRowHidden(index.row(), false);
        }

        d->currentUrl = url;
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        d->currentUrl = QUrl();
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

    d->showAll = showAll;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());

    int rowCount = placesModel->rowCount();
    QModelIndex current = placesModel->closestItem(d->currentUrl);

    if (showAll) {
        d->updateHiddenRows();

        for (int i = 0; i < rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index != current && placesModel->isHidden(index)) {
                delegate->addAppearingItem(index);
            }
        }
        d->triggerItemAppearingAnimation();
    } else {
        for (int i = 0; i < rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index != current && placesModel->isHidden(index)) {
                delegate->addDisappearingItem(index);
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

void KFilePlacesView::contextMenuEvent(QContextMenuEvent *event)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(model());

    if (!placesModel) {
        return;
    }

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());

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

    const bool clickOverHeader = delegate->pointIsHeaderArea(event->pos());
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
        showAll->setChecked(d->showAll);
        if (mainSeparator == nullptr) {
            mainSeparator = menu.addSeparator();
        }
        menu.insertAction(mainSeparator, showAll);
    }

    QAction *remove = nullptr;
    if (!clickOverHeader && index.isValid() && !placesModel->isDevice(index)) {
        remove = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18n("&Remove Entry '%1'", label));
    }

    menu.addActions(actions());

    if (menu.isEmpty()) {
        return;
    }

    QAction *result = menu.exec(event->globalPos());

    if (emptyTrash && (result == emptyTrash)) {

        KIO::JobUiDelegate uiDelegate;
        uiDelegate.setWindow(window());
        if (uiDelegate.askDeleteConfirmation(QList<QUrl>(), KIO::JobUiDelegate::EmptyTrash, KIO::JobUiDelegate::DefaultConfirmation)) {
            KIO::Job* job = KIO::emptyTrash();
            KJobWidgets::setWindow(job, window());
            job->uiDelegate()->setAutoErrorHandlingEnabled(true);
        }
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

        if (!d->showAll && hideSection->isChecked()) {
            delegate->addDisappearingItemGroup(index);
            d->triggerItemDisappearingAnimation();
        }
    } else if (hide && (result == hide)) {
        placesModel->setPlaceHidden(index, hide->isChecked());
        QModelIndex current = placesModel->closestItem(d->currentUrl);

        if (index != current && !d->showAll && hide->isChecked()) {
            delegate->addDisappearingItem(index);
            d->triggerItemDisappearingAnimation();
        }
    } else if (showAll && (result == showAll)) {
        setShowAll(showAll->isChecked());
    } else if (teardown && (result == teardown)) {
        placesModel->requestTeardown(index);
    } else if (eject && (result == eject)) {
        placesModel->requestEject(index);
    } else if (add && (result == add)) {
        QUrl url = d->currentUrl;
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

    index = placesModel->closestItem(d->currentUrl);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
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
    d->smoothItemResizing = false;
}

void KFilePlacesView::dragEnterEvent(QDragEnterEvent *event)
{
    QListView::dragEnterEvent(event);
    d->dragging = true;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
    delegate->setShowHoverIndication(false);

    d->dropRect = QRect();
}

void KFilePlacesView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QListView::dragLeaveEvent(event);
    d->dragging = false;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
    delegate->setShowHoverIndication(true);

    setDirtyRegion(d->dropRect);
}

void KFilePlacesView::dragMoveEvent(QDragMoveEvent *event)
{
    QListView::dragMoveEvent(event);

    // update the drop indicator
    const QPoint pos = event->pos();
    const QModelIndex index = indexAt(pos);
    setDirtyRegion(d->dropRect);
    if (index.isValid()) {
        const QRect rect = visualRect(index);
        const int gap = d->insertIndicatorHeight(rect.height());
        if (d->insertAbove(rect, pos)) {
            // indicate that the item will be inserted above the current place
            d->dropRect = QRect(rect.left(), rect.top() - gap / 2,
                                rect.width(), gap);
        } else if (d->insertBelow(rect, pos)) {
            // indicate that the item will be inserted below the current place
            d->dropRect = QRect(rect.left(), rect.bottom() + 1 -  gap / 2,
                                rect.width(), gap);
        } else {
            // indicate that the item be dropped above the current place
            d->dropRect = rect;
        }
    }

    setDirtyRegion(d->dropRect);
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
            emit urlsDropped(placesModel->url(index), event, this);
            event->acceptProposedAction();
        }
    }

    QListView::dropEvent(event);
    d->dragging = false;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
    delegate->setShowHoverIndication(true);
}

void KFilePlacesView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (d->dragging && !d->dropRect.isEmpty()) {
        // draw drop indicator
        QPainter painter(viewport());

        const QModelIndex index = indexAt(d->dropRect.topLeft());
        const QRect itemRect = visualRect(index);
        const bool drawInsertIndicator = !d->dropOnPlace ||
                                         d->dropRect.height() <= d->insertIndicatorHeight(itemRect.height());

        if (drawInsertIndicator) {
            // draw indicator for inserting items
            QBrush blendedBrush = viewOptions().palette.brush(QPalette::Normal, QPalette::Highlight);
            QColor color = blendedBrush.color();

            const int y = (d->dropRect.top() + d->dropRect.bottom()) / 2;
            const int thickness = d->dropRect.height() / 2;
            Q_ASSERT(thickness >= 1);
            int alpha = 255;
            const int alphaDec = alpha / (thickness + 1);
            for (int i = 0; i < thickness; i++) {
                color.setAlpha(alpha);
                alpha -= alphaDec;
                painter.setPen(color);
                painter.drawLine(d->dropRect.left(), y - i, d->dropRect.right(), y - i);
                painter.drawLine(d->dropRect.left(), y + i, d->dropRect.right(), y + i);
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
    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());

    delegate->startDrag();
    QListView::startDrag(supportedActions);
}

void KFilePlacesView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
        // does not accept drags from section header area
        if (delegate->pointIsHeaderArea(event->pos())) {
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
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(adaptItemSize()), Qt::QueuedConnection);
    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            d->watcher, &KFilePlacesEventWatcher::currentIndexChanged);
}

void KFilePlacesView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QListView::rowsInserted(parent, start, end);
    setUrl(d->currentUrl);

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(itemDelegate());
    KFilePlacesModel *placesModel = static_cast<KFilePlacesModel *>(model());

    for (int i = start; i <= end; ++i) {
        QModelIndex index = placesModel->index(i, 0, parent);
        if (d->showAll || !placesModel->isHidden(index)) {
            delegate->addAppearingItem(index);
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
            textWidth = qMax(textWidth, fm.width(index.data(Qt::DisplayRole).toString()));
        }
    }

    const int iconSize = KIconLoader::global()->currentSize(KIconLoader::Small) + 3 * LATERAL_MARGIN;
    return QSize(iconSize + textWidth + fm.height() / 2, height);
}

void KFilePlacesView::Private::addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index)
{
    delegate->addDisappearingItem(index);
    if (itemDisappearTimeline.state() != QTimeLine::Running) {
        delegate->setDisappearingItemProgress(0.0);
        itemDisappearTimeline.start();
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
        currentUrl = url;
        updateHiddenRows();
        emit q->urlChanged(KFilePlacesModel::convertedUrl(url));
        if (showAll) {
            q->setShowAll(false);
        }
    } else {
        q->setUrl(currentUrl);
    }
}

void KFilePlacesView::Private::adaptItemSize()
{
    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(q->itemDelegate());

    if (!autoResizeItems) {
        const int size = q->iconSize().width(); // Assume width == height
        delegate->setIconSize(size);
        q->scheduleDelayedItemsLayout();
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    int rowCount = placesModel->rowCount();

    if (!showAll) {
        rowCount -= placesModel->hiddenCount();

        QModelIndex current = placesModel->closestItem(currentUrl);

        if (placesModel->isHidden(current)) {
            rowCount++;
        }
    }

    if (rowCount == 0) {
        return;    // We've nothing to display anyway
    }

    const int minSize = IconSize(KIconLoader::Small);
    const int maxSize = 64;

    int textWidth = 0;
    QFontMetrics fm = q->fontMetrics();
    for (int i = 0; i < placesModel->rowCount(); ++i) {
        QModelIndex index = placesModel->index(i, 0);

        if (!placesModel->isHidden(index)) {
            textWidth = qMax(textWidth, fm.width(index.data(Qt::DisplayRole).toString()));
        }
    }

    const int margin = q->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, q) + 1;
    const int maxWidth = q->viewport()->width() - textWidth - 4 * margin - 1;

    const int totalItemsHeight = (fm.height() / 2) * rowCount;
    const int totalSectionsHeight = delegate->sectionHeaderHeight() * sectionsCount();
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

    if (size == delegate->iconSize()) {
        return;
    }

    if (smoothItemResizing) {
        oldSize = delegate->iconSize();
        endSize = size;
        if (adaptItemsTimeline.state() != QTimeLine::Running) {
            adaptItemsTimeline.start();
        }
    } else {
        delegate->setIconSize(size);
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
    QModelIndex current = placesModel->closestItem(currentUrl);

    for (int i = 0; i < rowCount; ++i) {
        QModelIndex index = placesModel->index(i, 0);
        if (index != current && placesModel->isHidden(index) && !showAll) {
            q->setRowHidden(i, true);
        } else {
            q->setRowHidden(i, false);
        }
    }

    adaptItemSize();
}

bool KFilePlacesView::Private::insertAbove(const QRect &itemRect, const QPoint &pos) const
{
    if (dropOnPlace) {
        return pos.y() < itemRect.top() + insertIndicatorHeight(itemRect.height()) / 2;
    }

    return pos.y() < itemRect.top() + (itemRect.height() / 2);
}

bool KFilePlacesView::Private::insertBelow(const QRect &itemRect, const QPoint &pos) const
{
    if (dropOnPlace) {
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
    QTimeLine *timeLine = delegate->fadeAnimationForIndex(index);
    delete timeLine;
    delegate->removeFadeAnimation(index);
    timeLine = new QTimeLine(250, q);
    connect(timeLine, SIGNAL(valueChanged(qreal)), q, SLOT(_k_capacityBarFadeValueChanged()));
    if (fadeType == FadeIn) {
        timeLine->setDirection(QTimeLine::Forward);
        timeLine->setCurrentTime(0);
    } else {
        timeLine->setDirection(QTimeLine::Backward);
        timeLine->setCurrentTime(250);
    }
    delegate->addFadeAnimation(index, timeLine);
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
                count++;
            }
        }
    }

    return count;
}

void KFilePlacesView::Private::triggerItemAppearingAnimation()
{
    if (itemAppearTimeline.state() != QTimeLine::Running) {
        delegate->setAppearingItemProgress(0.0);
        itemAppearTimeline.start();
    }
}

void KFilePlacesView::Private::triggerItemDisappearingAnimation()
{
    if (itemDisappearTimeline.state() != QTimeLine::Running) {
        delegate->setDisappearingItemProgress(0.0);
        itemDisappearTimeline.start();
    }
}

void KFilePlacesView::Private::_k_placeClicked(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    lastClickedIndex = QPersistentModelIndex();

    if (placesModel->setupNeeded(index)) {
        QObject::connect(placesModel, SIGNAL(setupDone(QModelIndex,bool)),
                         q, SLOT(_k_storageSetupDone(QModelIndex,bool)));

        lastClickedIndex = index;
        placesModel->requestSetup(index);
        return;
    }

    setCurrentIndex(index);
}

void KFilePlacesView::Private::_k_placeEntered(const QModelIndex &index)
{
    fadeCapacityBar(index, FadeIn);
    pollingRequestCount++;
    if (pollingRequestCount == 1) {
        pollDevices.start();
    }
}

void KFilePlacesView::Private::_k_placeLeft(const QModelIndex &index)
{
    fadeCapacityBar(index, FadeOut);
    pollingRequestCount--;
    if (!pollingRequestCount) {
        pollDevices.stop();
    }
}

void KFilePlacesView::Private::_k_storageSetupDone(const QModelIndex &index, bool success)
{
    if (index != lastClickedIndex) {
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel) {
        QObject::disconnect(placesModel, SIGNAL(setupDone(QModelIndex,bool)),
                            q, SLOT(_k_storageSetupDone(QModelIndex,bool)));
    }

    if (success) {
        setCurrentIndex(lastClickedIndex);
    } else {
        q->setUrl(currentUrl);
    }

    lastClickedIndex = QPersistentModelIndex();
}

void KFilePlacesView::Private::_k_adaptItemsUpdate(qreal value)
{
    int add = (endSize - oldSize) * value;

    int size = oldSize + add;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(q->itemDelegate());
    delegate->setIconSize(size);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemAppearUpdate(qreal value)
{
    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(q->itemDelegate());

    delegate->setAppearingItemProgress(value);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemDisappearUpdate(qreal value)
{
    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate *>(q->itemDelegate());

    delegate->setDisappearingItemProgress(value);

    if (value >= 1.0) {
        updateHiddenRows();
    }

    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_enableSmoothItemResizing()
{
    smoothItemResizing = true;
}

void KFilePlacesView::Private::_k_capacityBarFadeValueChanged()
{
    const QModelIndex index = delegate->indexForFadeAnimation(static_cast<QTimeLine *>(q->sender()));
    if (!index.isValid()) {
        return;
    }
    q->update(index);
}

void KFilePlacesView::Private::_k_triggerDevicePolling()
{
    const QModelIndex hoveredIndex = watcher->hoveredIndex();
    if (hoveredIndex.isValid()) {
        const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(hoveredIndex.model());
        if (placesModel->isDevice(hoveredIndex)) {
            q->update(hoveredIndex);
        }
    }
    const QModelIndex focusedIndex = watcher->focusedIndex();
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
