/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>
    SPDX-FileCopyrightText: 2022 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESVIEW_P_H
#define KFILEPLACESVIEW_P_H

#include <KIO/FileSystemFreeSpaceJob>
#include <KIO/Global>

#include <QAbstractItemDelegate>
#include <QDateTime>
#include <QDeadlineTimer>
#include <QGestureEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QScroller>
#include <QTimer>

class KFilePlacesView;
class QTimeLine;

struct PlaceFreeSpaceInfo {
    QDeadlineTimer timeout;
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
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    int iconSize() const;
    void setIconSize(int newSize);

    void paletteChange();

    void addAppearingItem(const QModelIndex &index);
    void setAppearingItemProgress(qreal value);
    void addDisappearingItem(const QModelIndex &index);
    void addDisappearingItemGroup(const QModelIndex &index);
    void setDisappearingItemProgress(qreal value);
    void setDeviceBusyAnimationRotation(qreal angle);

    void setShowHoverIndication(bool show);
    void setHoveredHeaderArea(const QModelIndex &index);
    void setHoveredAction(const QModelIndex &index);

    qreal contentsOpacity(const QModelIndex &index) const;

    bool pointIsHeaderArea(const QPoint &pos) const;
    bool pointIsTeardownAction(const QPoint &pos) const;

    void startDrag();

    int sectionHeaderHeight(const QModelIndex &index) const;
    bool indexIsSectionHeader(const QModelIndex &index) const;
    int actionIconSize() const;

    void checkFreeSpace();
    void checkFreeSpace(const QModelIndex &index) const;
    void startPollingFreeSpace() const;
    void stopPollingFreeSpace() const;

    void clearFreeSpaceInfo();

protected:
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    QString groupNameFromIndex(const QModelIndex &index) const;
    QModelIndex previousVisibleIndex(const QModelIndex &index) const;
    void drawSectionHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

    QColor textColor(const QStyleOption &option) const;
    QColor baseColor(const QStyleOption &option) const;
    QColor mixedColor(const QColor &c1, const QColor &c2, int c1Percent) const;

    KFilePlacesView *m_view;
    int m_iconSize;

    QList<QPersistentModelIndex> m_appearingItems;
    qreal m_appearingHeightScale;
    qreal m_appearingOpacity;

    QList<QPersistentModelIndex> m_disappearingItems;
    qreal m_disappearingHeightScale;
    qreal m_disappearingOpacity;

    qreal m_busyAnimationRotation = 0.0;

    bool m_showHoverIndication;
    QPersistentModelIndex m_hoveredHeaderArea;
    QPersistentModelIndex m_hoveredAction;
    mutable bool m_dragStarted;

    QMap<QPersistentModelIndex, QTimeLine *> m_timeLineMap;
    QMap<QTimeLine *, QPersistentModelIndex> m_timeLineInverseMap;

    mutable QTimer m_pollFreeSpace;
    mutable QMap<QPersistentModelIndex, PlaceFreeSpaceInfo> m_freeSpaceInfo;
    // constructing KColorScheme is expensive, cache the negative color
    mutable QColor m_warningCapacityBarColor;
};

class KFilePlacesEventWatcher : public QObject
{
    Q_OBJECT

public:
    explicit KFilePlacesEventWatcher(KFilePlacesView *parent = nullptr)
        : QObject(parent)
        , m_scroller(nullptr)
        , q(parent)
        , m_rubberBand(nullptr)
        , m_isTouchEvent(false)
        , m_mousePressed(false)
        , m_tapAndHoldActive(false)
        , m_lastMouseSource(Qt::MouseEventNotSynthesized)
    {
        m_rubberBand = new QRubberBand(QRubberBand::Rectangle, parent);
    }

    const QModelIndex &hoveredHeaderAreaIndex() const
    {
        return m_hoveredHeaderAreaIndex;
    }

    const QModelIndex &hoveredActionIndex() const
    {
        return m_hoveredActionIndex;
    }

    QScroller *m_scroller;

public Q_SLOTS:
    void qScrollerStateChanged(const QScroller::State newState)
    {
        if (newState == QScroller::Inactive) {
            m_isTouchEvent = false;
        }
    }

Q_SIGNALS:
    void entryMiddleClicked(const QModelIndex &index);

    void headerAreaEntered(const QModelIndex &index);
    void headerAreaLeft(const QModelIndex &index);

    void actionEntered(const QModelIndex &index);
    void actionLeft(const QModelIndex &index);
    void actionClicked(const QModelIndex &index);

    void windowActivated();
    void windowDeactivated();

    void paletteChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseMove: {
            if (m_isTouchEvent && !m_tapAndHoldActive) {
                return true;
            }

            m_tapAndHoldActive = false;
            if (m_rubberBand->isVisible()) {
                m_rubberBand->hide();
            }

            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
            const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
            const QModelIndex index = view->indexAt(pos);

            QModelIndex headerAreaIndex;
            QModelIndex actionIndex;
            if (index.isValid()) {
                if (auto *delegate = qobject_cast<KFilePlacesViewDelegate *>(view->itemDelegate())) {
                    if (delegate->pointIsHeaderArea(pos)) {
                        headerAreaIndex = index;
                    } else if (delegate->pointIsTeardownAction(pos)) {
                        actionIndex = index;
                    }
                }
            }

            if (headerAreaIndex != m_hoveredHeaderAreaIndex) {
                if (m_hoveredHeaderAreaIndex.isValid()) {
                    Q_EMIT headerAreaLeft(m_hoveredHeaderAreaIndex);
                }
                m_hoveredHeaderAreaIndex = headerAreaIndex;
                if (headerAreaIndex.isValid()) {
                    Q_EMIT headerAreaEntered(headerAreaIndex);
                }
            }

            if (actionIndex != m_hoveredActionIndex) {
                if (m_hoveredActionIndex.isValid()) {
                    Q_EMIT actionLeft(m_hoveredActionIndex);
                }
                m_hoveredActionIndex = actionIndex;
                if (actionIndex.isValid()) {
                    Q_EMIT actionEntered(actionIndex);
                }
            }

            break;
        }
        case QEvent::Leave:
            if (m_hoveredHeaderAreaIndex.isValid()) {
                Q_EMIT headerAreaLeft(m_hoveredHeaderAreaIndex);
            }
            m_hoveredHeaderAreaIndex = QModelIndex();

            if (m_hoveredActionIndex.isValid()) {
                Q_EMIT actionLeft(m_hoveredActionIndex);
            }
            m_hoveredActionIndex = QModelIndex();

            break;
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            m_mousePressed = true;
            m_lastMouseSource = mouseEvent->source();

            if (m_isTouchEvent) {
                return true;
            }
            onPressed(mouseEvent);
            Q_FALLTHROUGH();
        }
        case QEvent::MouseButtonDblClick: {
            // Prevent the selection clearing by clicking on the viewport directly
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
            if (!view->indexAt(static_cast<QMouseEvent *>(event)->pos()).isValid()) {
                return true;
            }
            break;
        }
        case QEvent::MouseButtonRelease: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton) {
                QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
                const QModelIndex index = view->indexAt(mouseEvent->pos());

                if (mouseEvent->button() == Qt::LeftButton) {
                    if (m_clickedActionIndex.isValid()) {
                        if (auto *delegate = qobject_cast<KFilePlacesViewDelegate *>(view->itemDelegate())) {
                            if (delegate->pointIsTeardownAction(mouseEvent->pos())) {
                                if (m_clickedActionIndex == index) {
                                    Q_EMIT actionClicked(m_clickedActionIndex);
                                    // filter out, avoid QAbstractItemView::clicked being emitted
                                    return true;
                                }
                            }
                        }
                    }
                    m_clickedActionIndex = index;
                } else if (mouseEvent->button() == Qt::MiddleButton) {
                    if (m_middleClickedIndex.isValid() && m_middleClickedIndex == index) {
                        Q_EMIT entryMiddleClicked(m_middleClickedIndex);
                    }
                    m_middleClickedIndex = QPersistentModelIndex();
                }
            }
            break;
        }
        case QEvent::WindowActivate:
            Q_EMIT windowActivated();
            break;
        case QEvent::WindowDeactivate:
            Q_EMIT windowDeactivated();
            break;
        case QEvent::PaletteChange:
            Q_EMIT paletteChanged();
            break;
        case QEvent::TouchBegin: {
            m_isTouchEvent = true;
            m_mousePressed = false;
            break;
        }
        case QEvent::Gesture: {
            gestureEvent(static_cast<QGestureEvent *>(event));
            event->accept();
            return true;
        }
        default:
            return false;
        }

        return false;
    }

    void onPressed(QMouseEvent *mouseEvent)
    {
        if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton) {
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(q);
            const QModelIndex index = view->indexAt(mouseEvent->pos());
            if (index.isValid()) {
                if (mouseEvent->button() == Qt::LeftButton) {
                    if (auto *delegate = qobject_cast<KFilePlacesViewDelegate *>(view->itemDelegate())) {
                        if (delegate->pointIsTeardownAction(mouseEvent->pos())) {
                            m_clickedActionIndex = index;
                        }
                    }
                } else if (mouseEvent->button() == Qt::MiddleButton) {
                    m_middleClickedIndex = index;
                }
            }
        }
    }

    void gestureEvent(QGestureEvent *event)
    {
        if (QGesture *gesture = event->gesture(Qt::TapGesture)) {
            tapTriggered(static_cast<QTapGesture *>(gesture));
        }
        if (QGesture *gesture = event->gesture(Qt::TapAndHoldGesture)) {
            tapAndHoldTriggered(static_cast<QTapAndHoldGesture *>(gesture));
        }
    }

    void tapAndHoldTriggered(QTapAndHoldGesture *tap)
    {
        if (tap->state() == Qt::GestureFinished) {
            if (!m_mousePressed) {
                return;
            }

            // the TapAndHold gesture is triggerable with the mouse and stylus, we don't want this
            if (m_lastMouseSource == Qt::MouseEventNotSynthesized || !m_isTouchEvent) {
                return;
            }

            m_tapAndHoldActive = true;
            m_scroller->stop();

            // simulate a mousePressEvent, to allow KFilePlacesView to select the items
            const QPoint tapViewportPos(q->viewport()->mapFromGlobal(tap->position().toPoint()));
            QMouseEvent fakeMousePress(QEvent::MouseButtonPress, tapViewportPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            onPressed(&fakeMousePress);
            q->mousePressEvent(&fakeMousePress);

            const QPoint tapIndicatorSize(80, 80);
            const QPoint pos(q->mapFromGlobal(tap->position().toPoint()));
            const QRect tapIndicatorRect(pos - (tapIndicatorSize / 2), pos + (tapIndicatorSize / 2));
            m_rubberBand->setGeometry(tapIndicatorRect.normalized());
            m_rubberBand->show();
        }
    }

    void tapTriggered(QTapGesture *tap)
    {
        static bool scrollerWasScrolling = false;

        if (tap->state() == Qt::GestureStarted) {
            m_tapAndHoldActive = false;
            // if QScroller state is Scrolling or Dragging, the user makes the tap to stop the scrolling
            auto const scrollerState = m_scroller->state();
            if (scrollerState == QScroller::Scrolling || scrollerState == QScroller::Dragging) {
                scrollerWasScrolling = true;
            } else {
                scrollerWasScrolling = false;
            }
        }

        if (tap->state() == Qt::GestureFinished && !scrollerWasScrolling) {
            m_isTouchEvent = false;

            // with touch you can touch multiple widgets at the same time, but only one widget will get a mousePressEvent.
            // we use this to select the right window
            if (!m_mousePressed) {
                return;
            }

            if (m_rubberBand->isVisible()) {
                m_rubberBand->hide();
            }
            // simulate a mousePressEvent, to allow KFilePlacesView to select the items
            QMouseEvent fakeMousePress(QEvent::MouseButtonPress,
                                       tap->position(),
                                       m_tapAndHoldActive ? Qt::RightButton : Qt::LeftButton,
                                       m_tapAndHoldActive ? Qt::RightButton : Qt::LeftButton,
                                       Qt::NoModifier);
            onPressed(&fakeMousePress);
            q->mousePressEvent(&fakeMousePress);

            if (m_tapAndHoldActive) {
                // simulate a contextMenuEvent
                QContextMenuEvent fakeContextMenu(QContextMenuEvent::Mouse, tap->position().toPoint(), q->mapToGlobal(tap->position().toPoint()));
                q->contextMenuEvent(&fakeContextMenu);
            }
            m_tapAndHoldActive = false;
        }
    }

private:
    QPersistentModelIndex m_hoveredHeaderAreaIndex;
    QPersistentModelIndex m_middleClickedIndex;
    QPersistentModelIndex m_hoveredActionIndex;
    QPersistentModelIndex m_clickedActionIndex;

    KFilePlacesView *const q;

    QRubberBand *m_rubberBand;
    bool m_isTouchEvent;
    bool m_mousePressed;
    bool m_tapAndHoldActive;
    Qt::MouseEventSource m_lastMouseSource;
};

#endif
