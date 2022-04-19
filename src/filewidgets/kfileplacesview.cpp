/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>
    SPDX-FileCopyrightText: 2022 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfileplacesview.h"
#include "kfileplacesview_p.h"

#include <QAbstractItemDelegate>
#include <QActionGroup>
#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QMenu>
#include <QMetaMethod>
#include <QMimeData>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QScroller>
#include <QTimeLine>
#include <QTimer>
#include <QToolTip>

#include <KColorScheme>
#include <KColorUtils>
#include <KConfig>
#include <KConfigGroup>
#include <KJob>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>
#include <defaults-kfile.h> // ConfigGroup, PlacesIconsAutoresize, PlacesIconsStaticSize
#include <kdirnotify.h>
#include <kio/emptytrashjob.h>
#include <kio/filesystemfreespacejob.h>
#include <kio/jobuidelegate.h>
#include <kmountpoint.h>
#include <kpropertiesdialog.h>
#include <solid/opticaldisc.h>
#include <solid/opticaldrive.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/storagevolume.h>
#include <widgetsaskuseractionhandler.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <memory>

#include "kfileplaceeditdialog.h"
#include "kfileplacesmodel.h"

using namespace std::chrono_literals;

static constexpr int s_lateralMargin = 4;
static constexpr int s_capacitybarHeight = 6;
static constexpr auto s_pollFreeSpaceInterval = 1min;

KFilePlacesViewDelegate::KFilePlacesViewDelegate(KFilePlacesView *parent)
    : QAbstractItemDelegate(parent)
    , m_view(parent)
    , m_iconSize(48)
    , m_appearingHeightScale(1.0)
    , m_appearingOpacity(0.0)
    , m_disappearingHeightScale(1.0)
    , m_disappearingOpacity(0.0)
    , m_showHoverIndication(true)
    , m_dragStarted(false)
{
    m_pollFreeSpace.setInterval(s_pollFreeSpaceInterval);
    connect(&m_pollFreeSpace, &QTimer::timeout, this, QOverload<>::of(&KFilePlacesViewDelegate::checkFreeSpace));
}

KFilePlacesViewDelegate::~KFilePlacesViewDelegate()
{
}

QSize KFilePlacesViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int height = std::max(m_iconSize, option.fontMetrics.height()) + s_lateralMargin;

    if (m_appearingItems.contains(index)) {
        height *= m_appearingHeightScale;
    } else if (m_disappearingItems.contains(index)) {
        height *= m_disappearingHeightScale;
    }

    if (indexIsSectionHeader(index)) {
        height += sectionHeaderHeight(index);
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
        const int headerHeight = sectionHeaderHeight(index);
        opt.rect.translate(0, headerHeight);
        opt.rect.setHeight(opt.rect.height() - headerHeight);
    }

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

    if (opt.state & QStyle::State_MouseOver) {
        if (index == m_hoveredHeaderArea) {
            opt.state &= ~QStyle::State_MouseOver;
        }
    }

    // Avoid a solid background for the drag pixmap so the drop indicator
    // is more easily seen.
    if (m_dragStarted) {
        opt.state.setFlag(QStyle::State_MouseOver, true);
        opt.state.setFlag(QStyle::State_Active, false);
        opt.state.setFlag(QStyle::State_Selected, false);
    }

    m_dragStarted = false;

    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);

    QIcon actionIcon;
    if (placesModel->isTeardownAllowed(index)) {
        actionIcon = QIcon::fromTheme(QStringLiteral("media-eject"));
    }

    bool isLTR = opt.direction == Qt::LeftToRight;
    const int iconAreaWidth = s_lateralMargin + m_iconSize;
    const int actionAreaWidth = !actionIcon.isNull() ? s_lateralMargin + actionIconSize() : 0;
    QRect rectText((isLTR ? iconAreaWidth : actionAreaWidth) + s_lateralMargin,
                   opt.rect.top(),
                   opt.rect.width() - iconAreaWidth - actionAreaWidth - 2 * s_lateralMargin,
                   opt.rect.height());

    const QPalette activePalette = KIconLoader::global()->customPalette();
    const bool changePalette = activePalette != opt.palette;
    if (changePalette) {
        KIconLoader::global()->setCustomPalette(opt.palette);
    }

    const bool selectedAndActive = (opt.state & QStyle::State_Selected) && (opt.state & QStyle::State_Active);
    QIcon::Mode mode = selectedAndActive ? QIcon::Selected : QIcon::Normal;
    QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();
    QPixmap pm = icon.pixmap(m_iconSize, m_iconSize, mode);
    QPoint point(isLTR ? opt.rect.left() + s_lateralMargin : opt.rect.right() - s_lateralMargin - m_iconSize,
                 opt.rect.top() + (opt.rect.height() - m_iconSize) / 2);
    painter->drawPixmap(point, pm);

    if (!actionIcon.isNull()) {
        QPoint actionPos(isLTR ? opt.rect.right() - actionAreaWidth : opt.rect.left() + s_lateralMargin,
                         opt.rect.top() + (opt.rect.height() - actionIconSize()) / 2);
        QIcon::Mode actionMode = QIcon::Normal;
        if (selectedAndActive) {
            actionMode = QIcon::Selected;
        } else if (m_hoveredAction == index) {
            actionMode = QIcon::Active;
        }
        QPixmap actionPix = actionIcon.pixmap(actionIconSize(), actionIconSize(), actionMode);
        painter->drawPixmap(actionPos, actionPix);
    }

    if (changePalette) {
        if (activePalette == QPalette()) {
            KIconLoader::global()->resetPalette();
        } else {
            KIconLoader::global()->setCustomPalette(activePalette);
        }
    }

    if (selectedAndActive) {
        painter->setPen(opt.palette.highlightedText().color());
    } else {
        painter->setPen(opt.palette.text().color());
    }

    if (placesModel->data(index, KFilePlacesModel::CapacityBarRecommendedRole).toBool()) {
        QPersistentModelIndex persistentIndex(index);
        const auto info = m_freeSpaceInfo.value(persistentIndex);

        checkFreeSpace(index); // async

        if (info.size > 0) {
            const int capacityBarHeight = std::ceil(m_iconSize / 8.0);
            const qreal usedSpace = info.used / qreal(info.size);

            // Vertically center text + capacity bar, so move text up a bit
            rectText.setTop(opt.rect.top() + (opt.rect.height() - opt.fontMetrics.height() - capacityBarHeight) / 2);
            rectText.setHeight(opt.fontMetrics.height());

            const int radius = capacityBarHeight / 2;
            QRect capacityBgRect(rectText.x(), rectText.bottom(), rectText.width(), capacityBarHeight);
            capacityBgRect.adjust(0.5, 0.5, -0.5, -0.5);
            QRect capacityFillRect = capacityBgRect;
            capacityFillRect.setWidth(capacityFillRect.width() * usedSpace);

            QPalette::ColorGroup cg = QPalette::Active;
            if (!(opt.state & QStyle::State_Enabled)) {
                cg = QPalette::Disabled;
            } else if (!m_view->isActiveWindow()) {
                cg = QPalette::Inactive;
            }

            // Adapted from Breeze style's progress bar rendering
            QColor capacityBgColor(opt.palette.color(QPalette::WindowText));
            capacityBgColor.setAlphaF(0.2 * capacityBgColor.alphaF());

            QColor capacityFgColor(selectedAndActive ? opt.palette.color(cg, QPalette::HighlightedText) : opt.palette.color(cg, QPalette::Highlight));
            if (usedSpace > 0.95) {
                if (!m_warningCapacityBarColor.isValid()) {
                    m_warningCapacityBarColor = KColorScheme(cg, KColorScheme::View).foreground(KColorScheme::NegativeText).color();
                }
                capacityFgColor = m_warningCapacityBarColor;
            }

            painter->save();

            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setPen(Qt::NoPen);

            painter->setBrush(capacityBgColor);
            painter->drawRoundedRect(capacityBgRect, radius, radius);

            painter->setBrush(capacityFgColor);
            painter->drawRoundedRect(capacityFillRect, radius, radius);

            painter->restore();
        }
    }

    painter->drawText(rectText,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      opt.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));

    painter->restore();
}

bool KFilePlacesViewDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QHelpEvent::ToolTip) {
        if (pointIsTeardownAction(event->pos())) {
            if (auto *placesModel = qobject_cast<const KFilePlacesModel *>(index.model())) {
                Q_ASSERT(placesModel->isTeardownAllowed(index));

                QString toolTipText;

                if (auto eject = std::unique_ptr<QAction>{placesModel->ejectActionForIndex(index)}) {
                    toolTipText = eject->toolTip();
                } else if (auto teardown = std::unique_ptr<QAction>{placesModel->teardownActionForIndex(index)}) {
                    toolTipText = teardown->toolTip();
                }

                if (!toolTipText.isEmpty()) {
                    // TODO rect
                    QToolTip::showText(event->globalPos(), toolTipText, m_view);
                    event->setAccepted(true);
                    return true;
                }
            }
        }
    }
    return QAbstractItemDelegate::helpEvent(event, view, option, index);
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
        m_appearingHeightScale = std::min(1.0, value * 4);
    } else {
        m_appearingHeightScale = 1.0;
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
    std::transform(indexesGroup.begin(), indexesGroup.end(), std::back_inserter(m_disappearingItems), [](const QModelIndex &idx) {
        return QPersistentModelIndex(idx);
    });
}

void KFilePlacesViewDelegate::setDisappearingItemProgress(qreal value)
{
    value = 1.0 - value;

    if (value <= 0.25) {
        m_disappearingOpacity = 0.0;
        m_disappearingHeightScale = std::min(1.0, value * 4);

        if (value <= 0.0) {
            m_disappearingItems.clear();
        }
    } else {
        m_disappearingHeightScale = 1.0;
        m_disappearingOpacity = (value - 0.25) * 4 / 3;
    }
}

void KFilePlacesViewDelegate::setShowHoverIndication(bool show)
{
    m_showHoverIndication = show;
}

void KFilePlacesViewDelegate::setHoveredHeaderArea(const QModelIndex &index)
{
    m_hoveredHeaderArea = index;
}

void KFilePlacesViewDelegate::setHoveredAction(const QModelIndex &index)
{
    m_hoveredAction = index;
}

bool KFilePlacesViewDelegate::pointIsHeaderArea(const QPoint &pos) const
{
    // we only accept drag events starting from item body, ignore drag request from header
    QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid()) {
        return false;
    }

    if (indexIsSectionHeader(index)) {
        const QRect vRect = m_view->visualRect(index);
        const int delegateY = pos.y() - vRect.y();
        if (delegateY <= sectionHeaderHeight(index)) {
            return true;
        }
    }
    return false;
}

bool KFilePlacesViewDelegate::pointIsTeardownAction(const QPoint &pos) const
{
    QModelIndex index = m_view->indexAt(pos);
    if (!index.isValid()) {
        return false;
    }

    if (!index.data(KFilePlacesModel::TeardownAllowedRole).toBool()) {
        return false;
    }

    const QRect vRect = m_view->visualRect(index);
    const bool isLTR = m_view->layoutDirection() == Qt::LeftToRight;

    const int delegateX = pos.x() - vRect.x();

    if (isLTR) {
        if (delegateX < (vRect.width() - 2 * s_lateralMargin - actionIconSize())) {
            return false;
        }
    } else {
        if (delegateX >= 2 * s_lateralMargin + actionIconSize()) {
            return false;
        }
    }

    return true;
}

void KFilePlacesViewDelegate::startDrag()
{
    m_dragStarted = true;
}

void KFilePlacesViewDelegate::checkFreeSpace()
{
    if (!m_view->model()) {
        return;
    }

    bool hasChecked = false;

    for (int i = 0; i < m_view->model()->rowCount(); ++i) {
        if (m_view->isRowHidden(i)) {
            continue;
        }

        const QModelIndex idx = m_view->model()->index(i, 0);
        if (!idx.data(KFilePlacesModel::CapacityBarRecommendedRole).toBool()) {
            continue;
        }

        checkFreeSpace(idx);
        hasChecked = true;
    }

    if (!hasChecked) {
        // Stop timer, there are no more devices
        stopPollingFreeSpace();
    }
}

void KFilePlacesViewDelegate::startPollingFreeSpace() const
{
    if (m_pollFreeSpace.isActive()) {
        return;
    }

    if (!m_view->isActiveWindow() || !m_view->isVisible()) {
        return;
    }

    m_pollFreeSpace.start();
}

void KFilePlacesViewDelegate::stopPollingFreeSpace() const
{
    m_pollFreeSpace.stop();
}

void KFilePlacesViewDelegate::checkFreeSpace(const QModelIndex &index) const
{
    Q_ASSERT(index.data(KFilePlacesModel::CapacityBarRecommendedRole).toBool());

    const QUrl url = index.data(KFilePlacesModel::UrlRole).toUrl();

    QPersistentModelIndex persistentIndex{index};

    auto &info = m_freeSpaceInfo[persistentIndex];

    if (info.job || !info.timeout.hasExpired()) {
        return;
    }

    // Restarting timeout before job finishes, so that when we poll all devices
    // and then get the result, the next poll will again update and not have
    // a remaining time of 99% because it came in shortly afterwards.
    // Also allow a bit of Timer slack.
    info.timeout.setRemainingTime(s_pollFreeSpaceInterval - 100ms);

    info.job = KIO::fileSystemFreeSpace(url);
    QObject::connect(info.job,
                     &KIO::FileSystemFreeSpaceJob::result,
                     this,
                     [this, persistentIndex](KIO::Job *job, KIO::filesize_t size, KIO::filesize_t available) {
                         if (!persistentIndex.isValid()) {
                             return;
                         }

                         if (job->error()) {
                             return;
                         }

                         PlaceFreeSpaceInfo &info = m_freeSpaceInfo[persistentIndex];

                         info.size = size;
                         info.used = size - available;

                         m_view->update(persistentIndex);
                     });

    startPollingFreeSpace();
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
    if (!index.isValid() || index.row() == 0) {
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

    const auto groupName = groupNameFromIndex(index);
    const auto previousGroupName = groupNameFromIndex(previousVisibleIndex(index));
    return groupName != previousGroupName;
}

void KFilePlacesViewDelegate::drawSectionHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel *>(index.model());

    const QString groupLabel = index.data(KFilePlacesModel::GroupRole).toString();
    const QString category = placesModel->isGroupHidden(index)
            // Avoid showing "(hidden)" during disappear animation when hiding a group
            && !m_disappearingItems.contains(index)
        ? i18n("%1 (hidden)", groupLabel)
        : groupLabel;

    QRect textRect(option.rect);
    textRect.setLeft(textRect.left() + 3);
    /* Take spacing into account:
       The spacing to the previous section compensates for the spacing to the first item.*/
    textRect.setY(textRect.y() /* + qMax(2, m_view->spacing()) - qMax(2, m_view->spacing())*/);
    textRect.setHeight(sectionHeaderHeight(index) - s_lateralMargin - m_view->spacing());

    painter->save();

    // based on dolphin colors
    const QColor c1 = textColor(option);
    const QColor c2 = baseColor(option);
    QColor penColor = mixedColor(c1, c2, 60);

    painter->setPen(penColor);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignBottom, option.fontMetrics.elidedText(category, Qt::ElideRight, textRect.width()));
    painter->restore();
}

void KFilePlacesViewDelegate::paletteChange()
{
    // Reset cache, will be re-created when painted
    m_warningCapacityBarColor = QColor();
}

QColor KFilePlacesViewDelegate::textColor(const QStyleOption &option) const
{
    const QPalette::ColorGroup group = m_view->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    return option.palette.color(group, QPalette::WindowText);
}

QColor KFilePlacesViewDelegate::baseColor(const QStyleOption &option) const
{
    const QPalette::ColorGroup group = m_view->isActiveWindow() ? QPalette::Active : QPalette::Inactive;
    return option.palette.color(group, QPalette::Window);
}

QColor KFilePlacesViewDelegate::mixedColor(const QColor &c1, const QColor &c2, int c1Percent) const
{
    Q_ASSERT(c1Percent >= 0 && c1Percent <= 100);

    const int c2Percent = 100 - c1Percent;
    return QColor((c1.red() * c1Percent + c2.red() * c2Percent) / 100,
                  (c1.green() * c1Percent + c2.green() * c2Percent) / 100,
                  (c1.blue() * c1Percent + c2.blue() * c2Percent) / 100);
}

int KFilePlacesViewDelegate::sectionHeaderHeight(const QModelIndex &index) const
{
    // Account for the spacing between header and item
    const int spacing = (s_lateralMargin + m_view->spacing());
    int height = m_view->fontMetrics().height() + spacing;
    if (index.row() != 0) {
        height += 2 * spacing;
    }
    return height;
}

int KFilePlacesViewDelegate::actionIconSize() const
{
    return qApp->style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, m_view);
}

class KFilePlacesViewPrivate
{
public:
    explicit KFilePlacesViewPrivate(KFilePlacesView *qq)
        : q(qq)
        , m_watcher(new KFilePlacesEventWatcher(q))
        , m_delegate(new KFilePlacesViewDelegate(q))
    {
    }

    using ActivationSignal = void (KFilePlacesView::*)(const QUrl &);

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
    int sectionsCount() const;

    void addPlace(const QModelIndex &index);
    void editPlace(const QModelIndex &index);

    void addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index);
    void triggerItemAppearingAnimation();
    void triggerItemDisappearingAnimation();
    bool shouldAnimate() const;

    void writeConfig();
    void readConfig();
    // Sets the size of the icons in the places panel
    void relayoutIconSize(int size);
    // Adds the "Icon Size" sub-menu items
    void setupIconSizeSubMenu(QMenu *submenu);

    void placeClicked(const QModelIndex &index, ActivationSignal activationSignal);
    void headerAreaEntered(const QModelIndex &index);
    void headerAreaLeft(const QModelIndex &index);
    void actionClicked(const QModelIndex &index);
    void actionEntered(const QModelIndex &index);
    void actionLeft(const QModelIndex &index);
    void teardown(const QModelIndex &index);
    void storageSetupDone(const QModelIndex &index, bool success);
    void adaptItemsUpdate(qreal value);
    void itemAppearUpdate(qreal value);
    void itemDisappearUpdate(qreal value);
    void enableSmoothItemResizing();

    KFilePlacesView *const q;

    KFilePlacesEventWatcher *const m_watcher;
    KFilePlacesViewDelegate *m_delegate;

    Solid::StorageAccess *m_lastClickedStorage = nullptr;
    QPersistentModelIndex m_lastClickedIndex;
    ActivationSignal m_lastActivationSignal = nullptr;

    QTimer *m_dragActivationTimer = nullptr;
    QPersistentModelIndex m_pendingDragActivation;

    QPersistentModelIndex m_pendingDropUrlsIndex;
    std::unique_ptr<QDropEvent> m_dropUrlsEvent;
    std::unique_ptr<QMimeData> m_dropUrlsMimeData;

    KFilePlacesView::TeardownFunction m_teardownFunction = nullptr;

    std::unique_ptr<KIO::WidgetsAskUserActionHandler> m_askUserHandler;

    QTimeLine m_adaptItemsTimeline;
    QTimeLine m_itemAppearTimeline;
    QTimeLine m_itemDisappearTimeline;

    QRect m_dropRect;
    QPersistentModelIndex m_dropIndex;

    QUrl m_currentUrl;

    int m_oldSize = 0;
    int m_endSize = 0;

    bool m_autoResizeItems = true;
    bool m_smoothItemResizing = false;
    bool m_showAll = false;
    bool m_dropOnPlace = false;
    bool m_dragging = false;
};

KFilePlacesView::KFilePlacesView(QWidget *parent)
    : QListView(parent)
    , d(std::make_unique<KFilePlacesViewPrivate>(this))
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

    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    d->m_watcher->m_scroller = QScroller::scroller(viewport());
    QScrollerProperties scrollerProp;
    scrollerProp.setScrollMetric(QScrollerProperties::AcceleratingFlickMaximumTime, 0.2); //QTBUG-88249
    d->m_watcher->m_scroller->setScrollerProperties(scrollerProp);
    d->m_watcher->m_scroller->grabGesture(viewport());
    connect(d->m_watcher->m_scroller, &QScroller::stateChanged, d->m_watcher, &KFilePlacesEventWatcher::qScrollerStateChanged);

    setAttribute(Qt::WA_AcceptTouchEvents);
    viewport()->grabGesture(Qt::TapGesture);
    viewport()->grabGesture(Qt::TapAndHoldGesture);

    // Note: Don't connect to the activated() signal, as the behavior when it is
    // committed depends on the used widget style. The click behavior of
    // KFilePlacesView should be style independent.
    connect(this, &KFilePlacesView::clicked, this, [this](const QModelIndex &index) {
        const auto modifiers = qGuiApp->keyboardModifiers();
        if (modifiers == (Qt::ControlModifier | Qt::ShiftModifier) && isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::activeTabRequested))) {
            d->placeClicked(index, &KFilePlacesView::activeTabRequested);
        } else if (modifiers == Qt::ControlModifier && isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::tabRequested))) {
            d->placeClicked(index, &KFilePlacesView::tabRequested);
        } else if (modifiers == Qt::ShiftModifier && isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::newWindowRequested))) {
            d->placeClicked(index, &KFilePlacesView::newWindowRequested);
        } else {
            d->placeClicked(index, &KFilePlacesView::placeActivated);
        }
    });

    connect(this, &QAbstractItemView::iconSizeChanged, this, [this](const QSize &newSize) {
        d->m_autoResizeItems = (newSize.width() < 1 || newSize.height() < 1);

        if (d->m_autoResizeItems) {
            d->adaptItemSize();
        } else {
            const int iconSize = qMin(newSize.width(), newSize.height());
            d->relayoutIconSize(iconSize);
        }
        d->writeConfig();
    });

    connect(&d->m_adaptItemsTimeline, &QTimeLine::valueChanged, this, [this](qreal value) {
        d->adaptItemsUpdate(value);
    });
    d->m_adaptItemsTimeline.setDuration(500);
    d->m_adaptItemsTimeline.setUpdateInterval(5);
    d->m_adaptItemsTimeline.setEasingCurve(QEasingCurve::InOutSine);

    connect(&d->m_itemAppearTimeline, &QTimeLine::valueChanged, this, [this](qreal value) {
        d->itemAppearUpdate(value);
    });
    d->m_itemAppearTimeline.setDuration(500);
    d->m_itemAppearTimeline.setUpdateInterval(5);
    d->m_itemAppearTimeline.setEasingCurve(QEasingCurve::InOutSine);

    connect(&d->m_itemDisappearTimeline, &QTimeLine::valueChanged, this, [this](qreal value) {
        d->itemDisappearUpdate(value);
    });
    d->m_itemDisappearTimeline.setDuration(500);
    d->m_itemDisappearTimeline.setUpdateInterval(5);
    d->m_itemDisappearTimeline.setEasingCurve(QEasingCurve::InOutSine);

    viewport()->installEventFilter(d->m_watcher);
    connect(d->m_watcher, &KFilePlacesEventWatcher::entryMiddleClicked, this, [this](const QModelIndex &index) {
        if (qGuiApp->keyboardModifiers() == Qt::ShiftModifier && isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::activeTabRequested))) {
            d->placeClicked(index, &KFilePlacesView::activeTabRequested);
        } else if (isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::tabRequested))) {
            d->placeClicked(index, &KFilePlacesView::tabRequested);
        } else {
            d->placeClicked(index, &KFilePlacesView::placeActivated);
        }
    });

    connect(d->m_watcher, &KFilePlacesEventWatcher::headerAreaEntered, this, [this](const QModelIndex &index) {
        d->headerAreaEntered(index);
    });
    connect(d->m_watcher, &KFilePlacesEventWatcher::headerAreaLeft, this, [this](const QModelIndex &index) {
        d->headerAreaLeft(index);
    });

    connect(d->m_watcher, &KFilePlacesEventWatcher::actionClicked, this, [this](const QModelIndex &index) {
        d->actionClicked(index);
    });
    connect(d->m_watcher, &KFilePlacesEventWatcher::actionEntered, this, [this](const QModelIndex &index) {
        d->actionEntered(index);
    });
    connect(d->m_watcher, &KFilePlacesEventWatcher::actionLeft, this, [this](const QModelIndex &index) {
        d->actionLeft(index);
    });

    connect(d->m_watcher, &KFilePlacesEventWatcher::windowActivated, this, [this] {
        d->m_delegate->checkFreeSpace();
        // Start polling even if checkFreeSpace() wouldn't because we might just have checked
        // free space before the timeout and so the poll timer would never get started again
        d->m_delegate->startPollingFreeSpace();
    });
    connect(d->m_watcher, &KFilePlacesEventWatcher::windowDeactivated, this, [this] {
        d->m_delegate->stopPollingFreeSpace();
    });

    connect(d->m_watcher, &KFilePlacesEventWatcher::paletteChanged, this, [this] {
        d->m_delegate->paletteChange();
    });

    // FIXME: this is necessary to avoid flashes of black with some widget styles.
    // could be a bug in Qt (e.g. QAbstractScrollArea) or KFilePlacesView, but has not
    // yet been tracked down yet. until then, this works and is harmlessly enough.
    // in fact, some QStyle (Oxygen, Skulpture, others?) do this already internally.
    // See br #242358 for more information
    verticalScrollBar()->setAttribute(Qt::WA_OpaquePaintEvent, false);
}

KFilePlacesView::~KFilePlacesView()
{
    viewport()->removeEventFilter(d->m_watcher);
}

void KFilePlacesView::setDropOnPlaceEnabled(bool enabled)
{
    d->m_dropOnPlace = enabled;
}

bool KFilePlacesView::isDropOnPlaceEnabled() const
{
    return d->m_dropOnPlace;
}

void KFilePlacesView::setDragAutoActivationDelay(int delay)
{
    if (delay <= 0) {
        delete d->m_dragActivationTimer;
        d->m_dragActivationTimer = nullptr;
        return;
    }

    if (!d->m_dragActivationTimer) {
        d->m_dragActivationTimer = new QTimer(this);
        d->m_dragActivationTimer->setSingleShot(true);
        connect(d->m_dragActivationTimer, &QTimer::timeout, this, [this] {
            if (d->m_pendingDragActivation.isValid()) {
                d->placeClicked(d->m_pendingDragActivation, &KFilePlacesView::placeActivated);
            }
        });
    }
    d->m_dragActivationTimer->setInterval(delay);
}

int KFilePlacesView::dragAutoActivationDelay() const
{
    return d->m_dragActivationTimer ? d->m_dragActivationTimer->interval() : 0;
}

void KFilePlacesView::setAutoResizeItemsEnabled(bool enabled)
{
    d->m_autoResizeItems = enabled;
}

bool KFilePlacesView::isAutoResizeItemsEnabled() const
{
    return d->m_autoResizeItems;
}

void KFilePlacesView::setTeardownFunction(TeardownFunction teardownFunc)
{
    d->m_teardownFunction = teardownFunc;
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

        if (placesModel->url(index) == url.adjusted(QUrl::StripTrailingSlash)) {
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

bool KFilePlacesView::allPlacesShown() const
{
    return d->m_showAll;
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

    Q_EMIT allPlacesShownChanged(showAll);
}

void KFilePlacesView::keyPressEvent(QKeyEvent *event)
{
    QListView::keyPressEvent(event);
    if ((event->key() == Qt::Key_Return) || (event->key() == Qt::Key_Enter)) {
        // TODO Modifier keys for requesting tabs
        // Browsers do Ctrl+Click but *Alt*+Return for new tab
        d->placeClicked(currentIndex(), &KFilePlacesView::placeActivated);
    }
}

void KFilePlacesViewPrivate::readConfig()
{
    KConfigGroup cg(KSharedConfig::openConfig(), ConfigGroup);
    m_autoResizeItems = cg.readEntry(PlacesIconsAutoresize, true);
    m_delegate->setIconSize(cg.readEntry(PlacesIconsStaticSize, static_cast<int>(KIconLoader::SizeMedium)));
}

void KFilePlacesViewPrivate::writeConfig()
{
    KConfigGroup cg(KSharedConfig::openConfig(), ConfigGroup);
    cg.writeEntry(PlacesIconsAutoresize, m_autoResizeItems);

    if (!m_autoResizeItems) {
        const int iconSize = qMin(q->iconSize().width(), q->iconSize().height());
        cg.writeEntry(PlacesIconsStaticSize, iconSize);
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
    const QString groupName = index.data(KFilePlacesModel::GroupRole).toString();
    const QUrl placeUrl = placesModel->url(index);
    const bool clickOverHeader = d->m_delegate->pointIsHeaderArea(event->pos());
    const bool clickOverEmptyArea = clickOverHeader || !index.isValid();
    const KFilePlacesModel::GroupType type = placesModel->groupType(index);

    QMenu menu;

    QAction *emptyTrash = nullptr;
    QAction *eject = nullptr;
    QAction *mount = nullptr;
    QAction *teardown = nullptr;

    QAction *newTab = nullptr;
    QAction *newWindow = nullptr;
    QAction *highPriorityActionsPlaceholder = new QAction();
    QAction *properties = nullptr;

    QAction *add = nullptr;
    QAction *edit = nullptr;
    QAction *remove = nullptr;

    QAction *hide = nullptr;
    QAction *hideSection = nullptr;
    QAction *showAll = nullptr;
    QMenu *iconSizeMenu = nullptr;

    if (!clickOverEmptyArea) {
        if (placeUrl.scheme() == QLatin1String("trash")) {
            emptyTrash = new QAction(QIcon::fromTheme(QStringLiteral("trash-empty")), i18nc("@action:inmenu", "Empty Trash"), &menu);
            KConfig trashConfig(QStringLiteral("trashrc"), KConfig::SimpleConfig);
            emptyTrash->setEnabled(!trashConfig.group("Status").readEntry("Empty", true));
        }

        if (placesModel->isDevice(index)) {
            eject = placesModel->ejectActionForIndex(index);
            if (eject) {
                eject->setParent(&menu);
            }

            teardown = placesModel->teardownActionForIndex(index);
            if (teardown) {
                teardown->setParent(&menu);
                teardown->setEnabled(placesModel->isTeardownAllowed(index));
            }

            if (placesModel->setupNeeded(index)) {
                mount = new QAction(QIcon::fromTheme(QStringLiteral("media-mount")), i18nc("@action:inmenu", "Mount"), &menu);
            }
        }

        // TODO What about active tab?
        if (isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::tabRequested))) {
            newTab = new QAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@item:inmenu", "Open in New Tab"), &menu);
        }
        if (isSignalConnected(QMetaMethod::fromSignal(&KFilePlacesView::newWindowRequested))) {
            newWindow = new QAction(QIcon::fromTheme(QStringLiteral("window-new")), i18nc("@item:inmenu", "Open in New Window"), &menu);
        }

        if (placeUrl.isLocalFile()) {
            properties = new QAction(QIcon::fromTheme(QStringLiteral("document-properties")), i18n("Properties"), &menu);
        }
    }

    if (clickOverEmptyArea) {
        add = new QAction(QIcon::fromTheme(QStringLiteral("document-new")), i18nc("@action:inmenu", "Add Entry…"), &menu);
    }

    if (index.isValid()) {
        if (!clickOverHeader) {
            if (!placesModel->isDevice(index)) {
                edit = new QAction(QIcon::fromTheme(QStringLiteral("edit-entry")), i18nc("@action:inmenu", "&Edit…"), &menu);

                KBookmark bookmark = placesModel->bookmarkForIndex(index);
                const bool isSystemItem = bookmark.metaDataItem(QStringLiteral("isSystemItem")) == QLatin1String("true");
                if (!isSystemItem) {
                    remove = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Remove"), &menu);
                }
            }

            hide = new QAction(QIcon::fromTheme(QStringLiteral("hint")), i18nc("@action:inmenu", "&Hide"), &menu);
            hide->setCheckable(true);
            hide->setChecked(placesModel->isHidden(index));
            // if a parent is hidden no interaction should be possible with children, show it first to do so
            hide->setEnabled(!placesModel->isGroupHidden(placesModel->groupType(index)));
        }

        hideSection = new QAction(QIcon::fromTheme(QStringLiteral("hint")),
                                  !groupName.isEmpty() ? i18nc("@item:inmenu", "Hide Section '%1'", groupName) : i18nc("@item:inmenu", "Hide Section"),
                                  &menu);
        hideSection->setCheckable(true);
        hideSection->setChecked(placesModel->isGroupHidden(type));
    }

    if (clickOverEmptyArea) {
        if (placesModel->hiddenCount() > 0) {
            showAll = new QAction(QIcon::fromTheme(QStringLiteral("visibility")), i18n("&Show All Entries"), &menu);
            showAll->setCheckable(true);
            showAll->setChecked(d->m_showAll);
        }

        iconSizeMenu = new QMenu(i18nc("@item:inmenu", "Icon Size"), &menu);
        d->setupIconSizeSubMenu(iconSizeMenu);
    }

    auto addActionToMenu = [&menu](QAction *action) {
        if (action) { // silence warning when adding null action
            menu.addAction(action);
        }
    };

    addActionToMenu(emptyTrash);

    addActionToMenu(eject);
    addActionToMenu(mount);
    addActionToMenu(teardown);
    menu.addSeparator();

    addActionToMenu(newTab);
    addActionToMenu(newWindow);
    addActionToMenu(highPriorityActionsPlaceholder);
    addActionToMenu(properties);
    menu.addSeparator();

    addActionToMenu(add);
    addActionToMenu(edit);
    addActionToMenu(remove);
    addActionToMenu(hide);
    addActionToMenu(hideSection);
    addActionToMenu(showAll);
    if (iconSizeMenu) {
        menu.addMenu(iconSizeMenu);
    }

    menu.addSeparator();

    // Clicking a header should be treated as clicking no device, hence passing an invalid model index
    // Emit the signal before adding any custom actions to give the user a chance to dynamically add/remove them
    Q_EMIT contextMenuAboutToShow(clickOverHeader ? QModelIndex() : index, &menu);

    const auto additionalActions = actions();
    for (QAction *action : additionalActions) {
        if (action->priority() == QAction::HighPriority) {
            menu.insertAction(highPriorityActionsPlaceholder, action);
        } else {
            menu.addAction(action);
        }
    }
    delete highPriorityActionsPlaceholder;

    QAction *result = menu.exec(event->globalPos());

    if (result) {
        if (result == emptyTrash) {
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

            d->m_askUserHandler->askUserDelete(QList<QUrl>{},
                                               KIO::AskUserActionInterface::EmptyTrash,
                                               KIO::AskUserActionInterface::DefaultConfirmation,
                                               parentWindow);
        } else if (result == eject) {
            placesModel->requestEject(index);
        } else if (result == mount) {
            placesModel->requestSetup(index);
        } else if (result == teardown) {
            d->teardown(index);
        } else if (result == newTab) {
            d->placeClicked(index, &KFilePlacesView::tabRequested);
        } else if (result == newWindow) {
            d->placeClicked(index, &KFilePlacesView::newWindowRequested);
        } else if (result == properties) {
            KPropertiesDialog::showDialog(placeUrl, this);
        } else if (result == add) {
            d->addPlace(index);
        } else if (result == edit) {
            d->editPlace(index);
        } else if (result == remove) {
            placesModel->removePlace(index);
        } else if (result == hide) {
            placesModel->setPlaceHidden(index, hide->isChecked());
            QModelIndex current = placesModel->closestItem(d->m_currentUrl);

            if (index != current && !d->m_showAll && hide->isChecked()) {
                d->m_delegate->addDisappearingItem(index);
                d->triggerItemDisappearingAnimation();
            }
        } else if (result == hideSection) {
            placesModel->setGroupHidden(type, hideSection->isChecked());

            if (!d->m_showAll && hideSection->isChecked()) {
                d->m_delegate->addDisappearingItemGroup(index);
                d->triggerItemDisappearingAnimation();
            }
        } else if (result == showAll) {
            setShowAll(showAll->isChecked());
        }
    }

    index = placesModel->closestItem(d->m_currentUrl);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
}

void KFilePlacesViewPrivate::setupIconSizeSubMenu(QMenu *submenu)
{
    QActionGroup *group = new QActionGroup(submenu);

    auto *autoAct = new QAction(i18nc("@item:inmenu Auto set icon size based on available space in"
                                      "the Places side-panel",
                                      "Auto Resize"),
                                group);
    autoAct->setCheckable(true);
    autoAct->setChecked(m_autoResizeItems);
    QObject::connect(autoAct, &QAction::toggled, q, [this]() {
        q->setIconSize(QSize(-1, -1));
    });
    submenu->addAction(autoAct);

    static constexpr KIconLoader::StdSizes iconSizes[] = {KIconLoader::SizeSmall,
                                                          KIconLoader::SizeSmallMedium,
                                                          KIconLoader::SizeMedium,
                                                          KIconLoader::SizeLarge};

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
            q->setIconSize(QSize(iconSize, iconSize));
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

    d->m_delegate->checkFreeSpace();
    // Start polling even if checkFreeSpace() wouldn't because we might just have checked
    // free space before the timeout and so the poll timer would never get started again
    d->m_delegate->startPollingFreeSpace();

    QTimer::singleShot(100, this, [this]() {
        d->enableSmoothItemResizing();
    });
}

void KFilePlacesView::hideEvent(QHideEvent *event)
{
    QListView::hideEvent(event);
    d->m_delegate->stopPollingFreeSpace();
    d->m_smoothItemResizing = false;
}

void KFilePlacesView::dragEnterEvent(QDragEnterEvent *event)
{
    QListView::dragEnterEvent(event);
    d->m_dragging = true;

    d->m_delegate->setShowHoverIndication(false);

    d->m_dropRect = QRect();
    d->m_dropIndex = QPersistentModelIndex();
}

void KFilePlacesView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QListView::dragLeaveEvent(event);
    d->m_dragging = false;

    d->m_delegate->setShowHoverIndication(true);

    if (d->m_dragActivationTimer) {
        d->m_dragActivationTimer->stop();
    }
    d->m_pendingDragActivation = QPersistentModelIndex();

    setDirtyRegion(d->m_dropRect);
}

void KFilePlacesView::dragMoveEvent(QDragMoveEvent *event)
{
    QListView::dragMoveEvent(event);

    bool autoActivate = false;
    // update the drop indicator
    const QPoint pos = event->pos();
    const QModelIndex index = indexAt(pos);
    setDirtyRegion(d->m_dropRect);
    if (index.isValid()) {
        d->m_dropIndex = index;
        const QRect rect = visualRect(index);
        const int gap = d->insertIndicatorHeight(rect.height());
        if (d->insertAbove(rect, pos)) {
            // indicate that the item will be inserted above the current place
            d->m_dropRect = QRect(rect.left(), rect.top() - gap / 2, rect.width(), gap);
        } else if (d->insertBelow(rect, pos)) {
            // indicate that the item will be inserted below the current place
            d->m_dropRect = QRect(rect.left(), rect.bottom() + 1 - gap / 2, rect.width(), gap);
        } else {
            // indicate that the item be dropped above the current place
            d->m_dropRect = rect;
            // only auto-activate when dropping ontop of a place, not inbetween
            autoActivate = true;
        }
    }

    if (d->m_dragActivationTimer) {
        if (autoActivate && !d->m_delegate->pointIsHeaderArea(event->pos())) {
            QPersistentModelIndex persistentIndex(index);
            if (!d->m_pendingDragActivation.isValid() || d->m_pendingDragActivation != persistentIndex) {
                d->m_pendingDragActivation = persistentIndex;
                d->m_dragActivationTimer->start();
            }
        } else {
            d->m_dragActivationTimer->stop();
            d->m_pendingDragActivation = QPersistentModelIndex();
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
            if (placesModel->setupNeeded(index)) {
                d->m_pendingDropUrlsIndex = index;

                // Make a full copy of the Mime-Data
                d->m_dropUrlsMimeData = std::make_unique<QMimeData>();
                const auto formats = event->mimeData()->formats();
                for (const auto &format : formats) {
                    d->m_dropUrlsMimeData->setData(format, event->mimeData()->data(format));
                }

                d->m_dropUrlsEvent = std::make_unique<QDropEvent>(event->posF(),
                                                                  event->possibleActions(),
                                                                  d->m_dropUrlsMimeData.get(),
                                                                  event->mouseButtons(),
                                                                  event->keyboardModifiers());

                placesModel->requestSetup(index);
            } else {
                Q_EMIT urlsDropped(placesModel->url(index), event, this);
            }
            // HACK Qt eventually calls into QAIM::dropMimeData when a drop event isn't
            // accepted by the view. However, QListView::dropEvent calls ignore() on our
            // event afterwards when
            // "icon view didn't move the data, and moveRows not implemented, so fall back to default"
            // overriding the acceptProposedAction() below.
            // This special mime type tells KFilePlacesModel to ignore it.
            auto *mime = const_cast<QMimeData *>(event->mimeData());
            mime->setData(QStringLiteral("application/x-kfileplacesmodel-ignore"), QByteArrayLiteral("1"));
            event->acceptProposedAction();
        }
    }

    QListView::dropEvent(event);
    d->m_dragging = false;

    if (d->m_dragActivationTimer) {
        d->m_dragActivationTimer->stop();
    }
    d->m_pendingDragActivation = QPersistentModelIndex();

    d->m_delegate->setShowHoverIndication(true);
}

void KFilePlacesView::paintEvent(QPaintEvent *event)
{
    QListView::paintEvent(event);
    if (d->m_dragging && !d->m_dropRect.isEmpty()) {
        // draw drop indicator
        QPainter painter(viewport());

        QRect itemRect = visualRect(d->m_dropIndex);
        // Take into account section headers
        if (d->m_delegate->indexIsSectionHeader(d->m_dropIndex)) {
            const int headerHeight = d->m_delegate->sectionHeaderHeight(d->m_dropIndex);
            itemRect.translate(0, headerHeight);
            itemRect.setHeight(itemRect.height() - headerHeight);
        }
        const bool drawInsertIndicator = !d->m_dropOnPlace || d->m_dropRect.height() <= d->insertIndicatorHeight(itemRect.height());

        if (drawInsertIndicator) {
            // draw indicator for inserting items
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QStyleOptionViewItem viewOpts;
            initViewItemOption(&viewOpts);
#else
            QStyleOptionViewItem viewOpts = viewOptions();
#endif

            QBrush blendedBrush = viewOpts.palette.brush(QPalette::Normal, QPalette::Highlight);
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
            opt.index = d->m_dropIndex;
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
        // teardown button is handled by KFilePlacesEventWatcher
        // NOTE "mouseReleaseEvent" side is also in there.
        if (d->m_delegate->pointIsTeardownAction(event->pos())) {
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
    connect(
        model,
        &QAbstractItemModel::rowsRemoved,
        this,
        [this]() {
            d->adaptItemSize();
        },
        Qt::QueuedConnection);

    QObject::connect(qobject_cast<KFilePlacesModel *>(model), &KFilePlacesModel::setupDone, this, [this](const QModelIndex &idx, bool success) {
        d->storageSetupDone(idx, success);
    });

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

    const int iconSize = style()->pixelMetric(QStyle::PM_SmallIconSize) + 3 * s_lateralMargin;
    return QSize(iconSize + textWidth + fm.height() / 2, height);
}

void KFilePlacesViewPrivate::addDisappearingItem(KFilePlacesViewDelegate *delegate, const QModelIndex &index)
{
    delegate->addDisappearingItem(index);
    if (m_itemDisappearTimeline.state() != QTimeLine::Running) {
        delegate->setDisappearingItemProgress(0.0);
        m_itemDisappearTimeline.start();
    }
}

void KFilePlacesViewPrivate::setCurrentIndex(const QModelIndex &index)
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
    } else {
        q->setUrl(m_currentUrl);
    }
}

void KFilePlacesViewPrivate::adaptItemSize()
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
        return; // We've nothing to display anyway
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
    const int totalSectionsHeight = m_delegate->sectionHeaderHeight(QModelIndex()) * sectionsCount();
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

void KFilePlacesViewPrivate::relayoutIconSize(const int size)
{
    if (size == m_delegate->iconSize()) {
        return;
    }

    if (shouldAnimate() && m_smoothItemResizing) {
        m_oldSize = m_delegate->iconSize();
        m_endSize = size;
        if (m_adaptItemsTimeline.state() != QTimeLine::Running) {
            m_adaptItemsTimeline.start();
        }
    } else {
        m_delegate->setIconSize(size);
        if (shouldAnimate()) {
            q->scheduleDelayedItemsLayout();
        }
    }
}

void KFilePlacesViewPrivate::updateHiddenRows()
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

bool KFilePlacesViewPrivate::insertAbove(const QRect &itemRect, const QPoint &pos) const
{
    if (m_dropOnPlace) {
        return pos.y() < itemRect.top() + insertIndicatorHeight(itemRect.height()) / 2;
    }

    return pos.y() < itemRect.top() + (itemRect.height() / 2);
}

bool KFilePlacesViewPrivate::insertBelow(const QRect &itemRect, const QPoint &pos) const
{
    if (m_dropOnPlace) {
        return pos.y() > itemRect.bottom() - insertIndicatorHeight(itemRect.height()) / 2;
    }

    return pos.y() >= itemRect.top() + (itemRect.height() / 2);
}

int KFilePlacesViewPrivate::insertIndicatorHeight(int itemHeight) const
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

int KFilePlacesViewPrivate::sectionsCount() const
{
    int count = 0;
    QString prevSection;
    const int rowCount = q->model()->rowCount();

    for (int i = 0; i < rowCount; i++) {
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

void KFilePlacesViewPrivate::addPlace(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    QUrl url = m_currentUrl;
    QString label;
    QString iconName = QStringLiteral("folder");
    bool appLocal = true;
    if (KFilePlaceEditDialog::getInformation(true, url, label, iconName, true, appLocal, 64, q)) {
        QString appName;
        if (appLocal) {
            appName = QCoreApplication::instance()->applicationName();
        }

        placesModel->addPlace(label, url, iconName, appName, index);
    }
}

void KFilePlacesViewPrivate::editPlace(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    KBookmark bookmark = placesModel->bookmarkForIndex(index);
    QUrl url = bookmark.url();
    // KBookmark::text() would be untranslated for system bookmarks
    QString label = placesModel->text(index);
    QString iconName = bookmark.icon();
    bool appLocal = !bookmark.metaDataItem(QStringLiteral("OnlyInApp")).isEmpty();

    if (KFilePlaceEditDialog::getInformation(true, url, label, iconName, false, appLocal, 64, q)) {
        QString appName;
        if (appLocal) {
            appName = QCoreApplication::instance()->applicationName();
        }

        placesModel->editPlace(index, label, url, iconName, appName);
    }
}

bool KFilePlacesViewPrivate::shouldAnimate() const
{
    return q->style()->styleHint(QStyle::SH_Widget_Animation_Duration) > 0;
}

void KFilePlacesViewPrivate::triggerItemAppearingAnimation()
{
    if (m_itemAppearTimeline.state() == QTimeLine::Running) {
        return;
    }

    if (shouldAnimate()) {
        m_delegate->setAppearingItemProgress(0.0);
        m_itemAppearTimeline.start();
    } else {
        itemAppearUpdate(1.0);
    }
}

void KFilePlacesViewPrivate::triggerItemDisappearingAnimation()
{
    if (m_itemDisappearTimeline.state() == QTimeLine::Running) {
        return;
    }

    if (shouldAnimate()) {
        m_delegate->setDisappearingItemProgress(0.0);
        m_itemDisappearTimeline.start();
    } else {
        itemDisappearUpdate(1.0);
    }
}

void KFilePlacesViewPrivate::placeClicked(const QModelIndex &index, ActivationSignal activationSignal)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());

    if (placesModel == nullptr) {
        return;
    }

    m_lastClickedIndex = QPersistentModelIndex();
    m_lastActivationSignal = nullptr;

    if (placesModel->setupNeeded(index)) {
        m_lastClickedIndex = index;
        m_lastActivationSignal = activationSignal;
        placesModel->requestSetup(index);
        return;
    }

    setCurrentIndex(index);

    const QUrl url = KFilePlacesModel::convertedUrl(placesModel->url(index));

    /*Q_EMIT*/ std::invoke(activationSignal, q, url);
}

void KFilePlacesViewPrivate::headerAreaEntered(const QModelIndex &index)
{
    m_delegate->setHoveredHeaderArea(index);
    q->update(index);
}

void KFilePlacesViewPrivate::headerAreaLeft(const QModelIndex &index)
{
    m_delegate->setHoveredHeaderArea(QModelIndex());
    q->update(index);
}

void KFilePlacesViewPrivate::actionClicked(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel *>(q->model());
    if (!placesModel) {
        return;
    }

    Solid::Device device = placesModel->deviceForIndex(index);
    if (device.is<Solid::OpticalDisc>()) {
        placesModel->requestEject(index);
    } else {
        teardown(index);
    }
}

void KFilePlacesViewPrivate::actionEntered(const QModelIndex &index)
{
    m_delegate->setHoveredAction(index);
    q->update(index);
}

void KFilePlacesViewPrivate::actionLeft(const QModelIndex &index)
{
    m_delegate->setHoveredAction(QModelIndex());
    q->update(index);
}

void KFilePlacesViewPrivate::teardown(const QModelIndex &index)
{
    if (m_teardownFunction) {
        m_teardownFunction(index);
    } else if (auto *placesModel = qobject_cast<KFilePlacesModel *>(q->model())) {
        placesModel->requestTeardown(index);
    }
}

void KFilePlacesViewPrivate::storageSetupDone(const QModelIndex &index, bool success)
{
    KFilePlacesModel *placesModel = static_cast<KFilePlacesModel *>(q->model());

    if (m_lastClickedIndex.isValid()) {
        if (m_lastClickedIndex == index) {
            if (success) {
                setCurrentIndex(m_lastClickedIndex);
            } else {
                q->setUrl(m_currentUrl);
            }

            const QUrl url = KFilePlacesModel::convertedUrl(placesModel->url(index));
            /*Q_EMIT*/ std::invoke(m_lastActivationSignal, q, url);

            m_lastClickedIndex = QPersistentModelIndex();
            m_lastActivationSignal = nullptr;
        }
    }

    if (m_pendingDropUrlsIndex.isValid() && m_dropUrlsEvent) {
        if (m_pendingDropUrlsIndex == index) {
            if (success) {
                Q_EMIT q->urlsDropped(placesModel->url(index), m_dropUrlsEvent.get(), q);
            }

            m_pendingDropUrlsIndex = QPersistentModelIndex();
            m_dropUrlsEvent.reset();
            m_dropUrlsMimeData.reset();
        }
    }
}

void KFilePlacesViewPrivate::adaptItemsUpdate(qreal value)
{
    const int add = (m_endSize - m_oldSize) * value;
    const int size = m_oldSize + add;

    m_delegate->setIconSize(size);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesViewPrivate::itemAppearUpdate(qreal value)
{
    m_delegate->setAppearingItemProgress(value);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesViewPrivate::itemDisappearUpdate(qreal value)
{
    m_delegate->setDisappearingItemProgress(value);

    if (value >= 1.0) {
        updateHiddenRows();
    }

    q->scheduleDelayedItemsLayout();
}

void KFilePlacesViewPrivate::enableSmoothItemResizing()
{
    m_smoothItemResizing = true;
}

void KFilePlacesView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    QListView::dataChanged(topLeft, bottomRight, roles);
    d->adaptItemSize();
}

#include "moc_kfileplacesview_p.cpp"
