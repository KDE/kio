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

#include <QtCore/QTimeLine>
#include <QtCore/QTimer>
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
//#include <knotification.h>
#include <kio/job.h>
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
public:
    KFilePlacesViewDelegate(KFilePlacesView *parent);
    virtual ~KFilePlacesViewDelegate();
    virtual QSize sizeHint(const QStyleOptionViewItem &option,
                           const QModelIndex &index) const;
    virtual void paint(QPainter *painter,
                       const QStyleOptionViewItem &option,
                       const QModelIndex &index) const;

    int iconSize() const;
    void setIconSize(int newSize);

    void addAppearingItem(const QModelIndex &index);
    void setAppearingItemProgress(qreal value);
    void addDisappearingItem(const QModelIndex &index);
    void setDisappearingItemProgress(qreal value);

    void setShowHoverIndication(bool show);

    void addFadeAnimation(const QModelIndex &index, QTimeLine *timeLine);
    void removeFadeAnimation(const QModelIndex &index);
    QModelIndex indexForFadeAnimation(QTimeLine *timeLine) const;
    QTimeLine *fadeAnimationForIndex(const QModelIndex &index) const;

    qreal contentsOpacity(const QModelIndex &index) const;

private:
    KFilePlacesView *m_view;
    int m_iconSize;

    QList<QPersistentModelIndex> m_appearingItems;
    int m_appearingIconSize;
    qreal m_appearingOpacity;

    QList<QPersistentModelIndex> m_disappearingItems;
    int m_disappearingIconSize;
    qreal m_disappearingOpacity;

    bool m_showHoverIndication;

    QMap<QPersistentModelIndex, QTimeLine*> m_timeLineMap;
    QMap<QTimeLine*, QPersistentModelIndex> m_timeLineInverseMap;
};

KFilePlacesViewDelegate::KFilePlacesViewDelegate(KFilePlacesView *parent) :
    QAbstractItemDelegate(parent),
    m_view(parent),
    m_iconSize(48),
    m_appearingIconSize(0),
    m_appearingOpacity(0.0),
    m_disappearingIconSize(0),
    m_disappearingOpacity(0.0),
    m_showHoverIndication(true)
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

    const KFilePlacesModel *filePlacesModel = static_cast<const KFilePlacesModel*>(index.model());
    Solid::Device device = filePlacesModel->deviceForIndex(index);

    return QSize(option.rect.width(), option.fontMetrics.height() / 2 + qMax(iconSize, option.fontMetrics.height()));
}

void KFilePlacesViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    if (m_appearingItems.contains(index)) {
        painter->setOpacity(m_appearingOpacity);
    } else if (m_disappearingItems.contains(index)) {
        painter->setOpacity(m_disappearingOpacity);
    }

    QStyleOptionViewItemV4 opt = option;
    if (!m_showHoverIndication) {
        opt.state &= ~QStyle::State_MouseOver;
    }
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter);
    const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel*>(index.model());

    bool isLTR = option.direction == Qt::LeftToRight;

    QIcon icon = index.model()->data(index, Qt::DecorationRole).value<QIcon>();
    QPixmap pm = icon.pixmap(m_iconSize, m_iconSize);
    QPoint point(isLTR ? option.rect.left() + LATERAL_MARGIN
                       : option.rect.right() - LATERAL_MARGIN - m_iconSize, option.rect.top() + (option.rect.height() - m_iconSize) / 2);
    painter->drawPixmap(point, pm);

    if (option.state & QStyle::State_Selected) {
        QPalette::ColorGroup cg = QPalette::Active;
        if (!(option.state & QStyle::State_Enabled)) {
            cg = QPalette::Disabled;
        } else if (!(option.state & QStyle::State_Active)) {
            cg = QPalette::Inactive;
        }
        painter->setPen(option.palette.color(cg, QPalette::HighlightedText));
    }

    QRect rectText;

    const QUrl url = placesModel->url(index);
    bool drawCapacityBar = false;
    if (url.isLocalFile()) {
        const QString mountPointPath = placesModel->url(index).toLocalFile();
        const KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(mountPointPath);
        drawCapacityBar = info.size() != 0 &&
                placesModel->data(index, KFilePlacesModel::CapacityBarRecommendedRole).toBool();

        if (drawCapacityBar && contentsOpacity(index) > 0)
        {
            painter->save();
            painter->setOpacity(painter->opacity() * contentsOpacity(index));

            int height = option.fontMetrics.height() + CAPACITYBAR_HEIGHT;
            rectText = QRect(isLTR ? m_iconSize + LATERAL_MARGIN * 2 + option.rect.left()
                                   : 0, option.rect.top() + (option.rect.height() / 2 - height / 2), option.rect.width() - m_iconSize - LATERAL_MARGIN * 2, option.fontMetrics.height());
            painter->drawText(rectText, Qt::AlignLeft | Qt::AlignTop, option.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));
            QRect capacityRect(isLTR ? rectText.x() : LATERAL_MARGIN, rectText.bottom() - 1, rectText.width() - LATERAL_MARGIN, CAPACITYBAR_HEIGHT);
            KCapacityBar capacityBar(KCapacityBar::DrawTextInline);
            capacityBar.setValue((info.used() * 100) / info.size());
            capacityBar.drawCapacityBar(painter, capacityRect);

            painter->restore();

            painter->save();
            painter->setOpacity(painter->opacity() * (1 - contentsOpacity(index)));
        }
    }

    rectText = QRect(isLTR ? m_iconSize + LATERAL_MARGIN * 2 + option.rect.left()
                           : 0, option.rect.top(), option.rect.width() - m_iconSize - LATERAL_MARGIN * 2, option.rect.height());
    painter->drawText(rectText, Qt::AlignLeft | Qt::AlignVCenter, option.fontMetrics.elidedText(index.model()->data(index).toString(), Qt::ElideRight, rectText.width()));

    if (drawCapacityBar && contentsOpacity(index) > 0) {
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
    if (value<=0.25) {
        m_appearingOpacity = 0.0;
        m_appearingIconSize = iconSize()*value*4;

        if (m_appearingIconSize>=m_iconSize) {
            m_appearingIconSize = m_iconSize;
        }
    } else {
        m_appearingIconSize = m_iconSize;
        m_appearingOpacity = (value-0.25)*4/3;

        if (value>=1.0) {
            m_appearingItems.clear();
        }
    }
}

void KFilePlacesViewDelegate::addDisappearingItem(const QModelIndex &index)
{
    m_disappearingItems << index;
}

void KFilePlacesViewDelegate::setDisappearingItemProgress(qreal value)
{
    value = 1.0 - value;

    if (value<=0.25) {
        m_disappearingOpacity = 0.0;
        m_disappearingIconSize = iconSize()*value*4;

        if (m_disappearingIconSize>=m_iconSize) {
            m_disappearingIconSize = m_iconSize;
        }

        if (value<=0.0) {
            m_disappearingItems.clear();
        }
    } else {
        m_disappearingIconSize = m_iconSize;
        m_disappearingOpacity = (value-0.25)*4/3;
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
    QTimeLine *timeLine = m_timeLineMap.value(index, 0);
    m_timeLineMap.remove(index);
    m_timeLineInverseMap.remove(timeLine);
}

QModelIndex KFilePlacesViewDelegate::indexForFadeAnimation(QTimeLine *timeLine) const
{
    return m_timeLineInverseMap.value(timeLine, QModelIndex());
}

QTimeLine *KFilePlacesViewDelegate::fadeAnimationForIndex(const QModelIndex &index) const
{
    return m_timeLineMap.value(index, 0);
}

qreal KFilePlacesViewDelegate::contentsOpacity(const QModelIndex &index) const
{
    QTimeLine *timeLine = fadeAnimationForIndex(index);
    if (timeLine) {
        return timeLine->currentValue();
    }
    return 0;
}

class KFilePlacesView::Private
{
public:
    Private(KFilePlacesView *parent) : q(parent), watcher(new KFilePlacesEventWatcher(q)) { }

    enum FadeType {
        FadeIn = 0,
        FadeOut
    };

    KFilePlacesView * const q;

    QUrl currentUrl;
    bool autoResizeItems;
    bool showAll;
    bool smoothItemResizing;
    bool dropOnPlace;
    bool dragging;
    Solid::StorageAccess *lastClickedStorage;
    QPersistentModelIndex lastClickedIndex;

    QRect dropRect;

    void setCurrentIndex(const QModelIndex &index);
    void adaptItemSize();
    void updateHiddenRows();
    bool insertAbove(const QRect &itemRect, const QPoint &pos) const;
    bool insertBelow(const QRect &itemRect, const QPoint &pos) const;
    int insertIndicatorHeight(int itemHeight) const;
    void fadeCapacityBar(const QModelIndex &index, FadeType fadeType);

    void _k_placeClicked(const QModelIndex &index);
    void _k_placeEntered(const QModelIndex &index);
    void _k_placeLeft(const QModelIndex &index);
    void _k_storageSetupDone(const QModelIndex &index, bool success);
    void _k_adaptItemsUpdate(qreal value);
    void _k_itemAppearUpdate(qreal value);
    void _k_itemDisappearUpdate(qreal value);
    void _k_enableSmoothItemResizing();
    void _k_trashUpdated(KJob *job);
    void _k_capacityBarFadeValueChanged();
    void _k_triggerDevicePolling();

    QTimeLine adaptItemsTimeline;
    int oldSize, endSize;

    QTimeLine itemAppearTimeline;
    QTimeLine itemDisappearTimeline;

    KFilePlacesEventWatcher *const watcher;
    KFilePlacesViewDelegate *delegate;
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
    d->lastClickedStorage = 0;
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
    QUrl oldUrl = d->currentUrl;
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());

    if (placesModel==0) return;

    QModelIndex index = placesModel->closestItem(url);
    QModelIndex current = selectionModel()->currentIndex();

    if (index.isValid()) {
        if (current!=index && placesModel->isHidden(current) && !d->showAll) {
            KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate*>(itemDelegate());
            delegate->addDisappearingItem(current);

            if (d->itemDisappearTimeline.state()!=QTimeLine::Running) {
                delegate->setDisappearingItemProgress(0.0);
                d->itemDisappearTimeline.start();
            }
        }

        if (current!=index && placesModel->isHidden(index) && !d->showAll) {
            KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate*>(itemDelegate());
            delegate->addAppearingItem(index);

            if (d->itemAppearTimeline.state()!=QTimeLine::Running) {
                delegate->setAppearingItemProgress(0.0);
                d->itemAppearTimeline.start();
            }

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
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());

    if (placesModel==0) return;

    d->showAll = showAll;

    KFilePlacesViewDelegate *delegate = static_cast<KFilePlacesViewDelegate*>(itemDelegate());

    int rowCount = placesModel->rowCount();
    QModelIndex current = placesModel->closestItem(d->currentUrl);

    if (showAll) {
        d->updateHiddenRows();

        for (int i=0; i<rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index!=current && placesModel->isHidden(index)) {
                delegate->addAppearingItem(index);
            }
        }

        if (d->itemAppearTimeline.state()!=QTimeLine::Running) {
            delegate->setAppearingItemProgress(0.0);
            d->itemAppearTimeline.start();
        }
    } else {
        for (int i=0; i<rowCount; ++i) {
            QModelIndex index = placesModel->index(i, 0);
            if (index!=current && placesModel->isHidden(index)) {
                delegate->addDisappearingItem(index);
            }
        }

        if (d->itemDisappearTimeline.state()!=QTimeLine::Running) {
            delegate->setDisappearingItemProgress(0.0);
            d->itemDisappearTimeline.start();
        }
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
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());
    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(itemDelegate());

    if (placesModel==0) return;

    QModelIndex index = indexAt(event->pos());
    QString label = placesModel->text(index).replace('&',"&&");

    QMenu menu;

    QAction *edit = 0;
    QAction *hide = 0;
    QAction *emptyTrash = 0;
    QAction *eject = 0;
    QAction *teardown = 0;
    QAction *add = 0;
    QAction *mainSeparator = 0;

    if (index.isValid()) {
        if (!placesModel->isDevice(index)) {
            if (placesModel->url(index).toString() == QLatin1String("trash:/")) {
                emptyTrash = menu.addAction(QIcon::fromTheme("trash-empty"), i18nc("@action:inmenu", "Empty Trash"));
                KConfig trashConfig("trashrc", KConfig::SimpleConfig);
                emptyTrash->setEnabled(!trashConfig.group("Status").readEntry("Empty", true));
                menu.addSeparator();
            }
            add = menu.addAction(QIcon::fromTheme("document-new"), i18n("Add Entry..."));
            mainSeparator = menu.addSeparator();
            edit = menu.addAction(QIcon::fromTheme("document-properties"), i18n("&Edit Entry '%1'...", label));
        } else {
            eject = placesModel->ejectActionForIndex(index);
            if (eject!=0) {
                eject->setParent(&menu);
                menu.addAction(eject);
            }

            teardown = placesModel->teardownActionForIndex(index);
            if (teardown!=0) {
                teardown->setParent(&menu);
                menu.addAction(teardown);
            }

            if (teardown!=0 || eject!=0) {
                mainSeparator = menu.addSeparator();
            }
        }
        if (add == 0) {
            add = menu.addAction(QIcon::fromTheme("document-new"), i18n("Add Entry..."));
        }

        hide = menu.addAction(i18n("&Hide Entry '%1'", label));
        hide->setCheckable(true);
        hide->setChecked(placesModel->isHidden(index));
    } else {
        add = menu.addAction(QIcon::fromTheme("document-new"), i18n("Add Entry..."));
    }

    QAction *showAll = 0;
    if (placesModel->hiddenCount()>0) {
        showAll = new QAction(i18n("&Show All Entries"), &menu);
        showAll->setCheckable(true);
        showAll->setChecked(d->showAll);
        if (mainSeparator == 0) {
            mainSeparator = menu.addSeparator();
        }
        menu.insertAction(mainSeparator, showAll);
    }

    QAction* remove = 0;
    if (index.isValid() && !placesModel->isDevice(index)) {
        remove = menu.addAction( QIcon::fromTheme("edit-delete"), i18n("&Remove Entry '%1'", label));
    }

    menu.addActions(actions());

    if (menu.isEmpty()) {
        return;
    }

    QAction *result = menu.exec(event->globalPos());

    if (emptyTrash != 0 && result == emptyTrash) {
        const QString text = i18nc("@info", "Do you really want to empty the Trash? All items will be deleted.");
        const bool del = KMessageBox::warningContinueCancel(window(),
                                                            text,
                                                            QString(),
                                                            KGuiItem(i18nc("@action:button", "Empty Trash"),
                                                                     QIcon::fromTheme("user-trash"))
                                                           ) == KMessageBox::Continue;
        if (del) {
            QByteArray packedArgs;
            QDataStream stream(&packedArgs, QIODevice::WriteOnly);
            stream << int(1);
            KIO::Job *job = KIO::special(QUrl("trash:/"), packedArgs);
            //KNotification::event("Trash: emptied", QString() , QPixmap() , 0, KNotification::DefaultEvent);
            KJobWidgets::setWindow(job, parentWidget());
            connect(job, SIGNAL(result(KJob*)), SLOT(_k_trashUpdated(KJob*)));
        }
    } else if (edit != 0 && result == edit) {
        KBookmark bookmark = placesModel->bookmarkForIndex(index);
        QUrl url = bookmark.url();
        QString label = bookmark.text();
        QString iconName = bookmark.icon();
        bool appLocal = !bookmark.metaDataItem("OnlyInApp").isEmpty();

        if (KFilePlaceEditDialog::getInformation(true, url, label,
                                                 iconName, false, appLocal, 64, this))
        {
            QString appName;
            if (appLocal) appName = QCoreApplication::instance()->applicationName();

            placesModel->editPlace(index, label, url, iconName, appName);
        }

    } else if (remove != 0 && result == remove) {
        placesModel->removePlace(index);
    } else if (hide != 0 && result == hide) {
        placesModel->setPlaceHidden(index, hide->isChecked());
        QModelIndex current = placesModel->closestItem(d->currentUrl);

        if (index!=current && !d->showAll && hide->isChecked()) {
            delegate->addDisappearingItem(index);

            if (d->itemDisappearTimeline.state()!=QTimeLine::Running) {
                delegate->setDisappearingItemProgress(0.0);
                d->itemDisappearTimeline.start();
            }
        }
    } else if (showAll != 0 && result == showAll) {
        setShowAll(showAll->isChecked());
    } else if (teardown != 0 && result == teardown) {
        placesModel->requestTeardown(index);
    } else if (eject != 0 && result == eject) {
        placesModel->requestEject(index);
    } else if (add != 0 && result == add) {
        QUrl url = d->currentUrl;
        QString label;
        QString iconName = "folder";
        bool appLocal = true;
        if (KFilePlaceEditDialog::getInformation(true, url, label,
                                                 iconName, true, appLocal, 64, this))
        {
            QString appName;
            if (appLocal) appName = QCoreApplication::instance()->applicationName();

            placesModel->addPlace(label, url, iconName, appName, index);
        }
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

    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(itemDelegate());
    delegate->setShowHoverIndication(false);

    d->dropRect = QRect();
}

void KFilePlacesView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QListView::dragLeaveEvent(event);
    d->dragging = false;

    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(itemDelegate());
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
            KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());
            Q_ASSERT(placesModel != 0);
            emit urlsDropped(placesModel->url(index), event, this);
            event->acceptProposedAction();
        }
    }

    QListView::dropEvent(event);
    d->dragging = false;

    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(itemDelegate());
    delegate->setShowHoverIndication(true);
}

void KFilePlacesView::paintEvent(QPaintEvent* event)
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
            QStyleOptionViewItemV4 opt;
            opt.initFrom(this);
            opt.rect = itemRect;
            opt.state = QStyle::State_Enabled | QStyle::State_MouseOver;
            style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, &painter, this);
        }
    }
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
    connect(selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            d->watcher, SLOT(currentIndexChanged(QModelIndex)));
}

void KFilePlacesView::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QListView::rowsInserted(parent, start, end);
    setUrl(d->currentUrl);

    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(itemDelegate());
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());

    for (int i=start; i<=end; ++i) {
        QModelIndex index = placesModel->index(i, 0, parent);
        if (d->showAll || !placesModel->isHidden(index)) {
            delegate->addAppearingItem(index);
        } else {
            setRowHidden(i, true);
        }
    }

    if (d->itemAppearTimeline.state()!=QTimeLine::Running) {
        delegate->setAppearingItemProgress(0.0);
        d->itemAppearTimeline.start();
    }

    d->adaptItemSize();
}

QSize KFilePlacesView::sizeHint() const
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(model());
    if (!placesModel) {
        return QListView::sizeHint();
    }
    const int height = QListView::sizeHint().height();
    QFontMetrics fm = d->q->fontMetrics();
    int textWidth = 0;

    for (int i=0; i<placesModel->rowCount(); ++i) {
        QModelIndex index = placesModel->index(i, 0);
        if (!placesModel->isHidden(index))
           textWidth = qMax(textWidth,fm.width(index.data(Qt::DisplayRole).toString()));
    }

    const int iconSize = KIconLoader::global()->currentSize(KIconLoader::Small) + 3 * LATERAL_MARGIN;
    return QSize(iconSize + textWidth + fm.height() / 2, height);
}

void KFilePlacesView::Private::setCurrentIndex(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(q->model());

    if (placesModel==0) return;

    QUrl url = placesModel->url(index);

    if (url.isValid()) {
        currentUrl = url;
        updateHiddenRows();
        emit q->urlChanged(url);
        if (showAll) {
            q->setShowAll(false);
        }
    } else {
        q->setUrl(currentUrl);
    }
}

void KFilePlacesView::Private::adaptItemSize()
{
    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(q->itemDelegate());
    if (!delegate) return;

    if (!autoResizeItems) {
        int size = q->iconSize().width(); // Assume width == height
        delegate->setIconSize(size);
        q->scheduleDelayedItemsLayout();
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(q->model());

    if (placesModel==0) return;

    int rowCount = placesModel->rowCount();

    if (!showAll) {
        rowCount-= placesModel->hiddenCount();

        QModelIndex current = placesModel->closestItem(currentUrl);

        if (placesModel->isHidden(current)) {
            rowCount++;
        }
    }

    if (rowCount==0) return; // We've nothing to display anyway

    const int minSize = IconSize(KIconLoader::Small);
    const int maxSize = 64;

    int textWidth = 0;
    QFontMetrics fm = q->fontMetrics();
    for (int i=0; i<placesModel->rowCount(); ++i) {
        QModelIndex index = placesModel->index(i, 0);

        if (!placesModel->isHidden(index))
           textWidth = qMax(textWidth,fm.width(index.data(Qt::DisplayRole).toString()));
    }

    const int margin = q->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, q) + 1;
    const int maxWidth = q->viewport()->width() - textWidth - 4 * margin - 1;
    const int maxHeight = ((q->height() - (fm.height() / 2) * rowCount) / rowCount) - 1;

    int size = qMin(maxHeight, maxWidth);

    if (size<minSize) {
        size = minSize;
    } else if (size>maxSize) {
        size = maxSize;
    } else {
        // Make it a multiple of 16
        size &= ~0xf;
    }

    if (size==delegate->iconSize()) return;

    if (smoothItemResizing) {
        oldSize = delegate->iconSize();
        endSize = size;
        if (adaptItemsTimeline.state()!=QTimeLine::Running) {
            adaptItemsTimeline.start();
        }
    } else {
        delegate->setIconSize(size);
        q->scheduleDelayedItemsLayout();
    }
}

void KFilePlacesView::Private::updateHiddenRows()
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(q->model());

    if (placesModel==0) return;

    int rowCount = placesModel->rowCount();
    QModelIndex current = placesModel->closestItem(currentUrl);

    for (int i=0; i<rowCount; ++i) {
        QModelIndex index = placesModel->index(i, 0);
        if (index!=current && placesModel->isHidden(index) && !showAll) {
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

void KFilePlacesView::Private::_k_placeClicked(const QModelIndex &index)
{
    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(q->model());

    if (placesModel==0) return;

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
    if (index!=lastClickedIndex) {
        return;
    }

    KFilePlacesModel *placesModel = qobject_cast<KFilePlacesModel*>(q->model());

    QObject::disconnect(placesModel, SIGNAL(setupDone(QModelIndex,bool)),
                        q, SLOT(_k_storageSetupDone(QModelIndex,bool)));

    if (success) {
        setCurrentIndex(lastClickedIndex);
    } else {
        q->setUrl(currentUrl);
    }

    lastClickedIndex = QPersistentModelIndex();
}

void KFilePlacesView::Private::_k_adaptItemsUpdate(qreal value)
{
    int add = (endSize-oldSize)*value;

    int size = oldSize+add;

    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(q->itemDelegate());
    delegate->setIconSize(size);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemAppearUpdate(qreal value)
{
    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(q->itemDelegate());

    delegate->setAppearingItemProgress(value);
    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_itemDisappearUpdate(qreal value)
{
    KFilePlacesViewDelegate *delegate = dynamic_cast<KFilePlacesViewDelegate*>(q->itemDelegate());

    delegate->setDisappearingItemProgress(value);

    if (value>=1.0) {
        updateHiddenRows();
    }

    q->scheduleDelayedItemsLayout();
}

void KFilePlacesView::Private::_k_enableSmoothItemResizing()
{
    smoothItemResizing = true;
}

void KFilePlacesView::Private::_k_trashUpdated(KJob *job)
{
    if (job->error()) {
        static_cast<KIO::Job*>(job)->ui()->showErrorMessage();
    }
    org::kde::KDirNotify::emitFilesAdded(QUrl("trash:/"));
}

void KFilePlacesView::Private::_k_capacityBarFadeValueChanged()
{
    const QModelIndex index = delegate->indexForFadeAnimation(static_cast<QTimeLine*>(q->sender()));
    if (!index.isValid()) {
        return;
    }
    q->update(index);
}

void KFilePlacesView::Private::_k_triggerDevicePolling()
{
    const QModelIndex hoveredIndex = watcher->hoveredIndex();
    if (hoveredIndex.isValid()) {
        const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel*>(hoveredIndex.model());
        if (placesModel->isDevice(hoveredIndex)) {
            q->update(hoveredIndex);
        }
    }
    const QModelIndex focusedIndex = watcher->focusedIndex();
    if (focusedIndex.isValid() && focusedIndex != hoveredIndex) {
        const KFilePlacesModel *placesModel = static_cast<const KFilePlacesModel*>(focusedIndex.model());
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
