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
#include <QMouseEvent>
#include <QPointer>

class KFilePlacesView;
class QTimeLine;

struct PlaceFreeSpaceInfo {
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
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

    int iconSize() const;
    void setIconSize(int newSize);

    void addAppearingItem(const QModelIndex &index);
    void setAppearingItemProgress(qreal value);
    void addDisappearingItem(const QModelIndex &index);
    void addDisappearingItemGroup(const QModelIndex &index);
    void setDisappearingItemProgress(qreal value);

    void setShowHoverIndication(bool show);
    void setHoveredHeaderArea(const QModelIndex &index);
    void setHoveredAction(const QModelIndex &index);

    void addFadeAnimation(const QModelIndex &index, QTimeLine *timeLine);
    void removeFadeAnimation(const QModelIndex &index);
    QModelIndex indexForFadeAnimation(QTimeLine *timeLine) const;
    QTimeLine *fadeAnimationForIndex(const QModelIndex &index) const;

    qreal contentsOpacity(const QModelIndex &index) const;

    bool pointIsHeaderArea(const QPoint &pos) const;
    bool pointIsTeardownAction(const QPoint &pos) const;

    void startDrag();

    int sectionHeaderHeight(const QModelIndex &index) const;
    bool indexIsSectionHeader(const QModelIndex &index) const;
    int actionIconSize() const;

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

    bool m_showHoverIndication;
    QPersistentModelIndex m_hoveredHeaderArea;
    QPersistentModelIndex m_hoveredAction;
    mutable bool m_dragStarted;

    QMap<QPersistentModelIndex, QTimeLine *> m_timeLineMap;
    QMap<QTimeLine *, QPersistentModelIndex> m_timeLineInverseMap;

    mutable QMap<QPersistentModelIndex, PlaceFreeSpaceInfo> m_freeSpaceInfo;
};

class KFilePlacesEventWatcher : public QObject
{
    Q_OBJECT

public:
    explicit KFilePlacesEventWatcher(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    const QModelIndex &hoveredIndex() const
    {
        return m_hoveredIndex;
    }

    const QModelIndex &hoveredHeaderAreaIndex() const
    {
        return m_hoveredHeaderAreaIndex;
    }

    const QModelIndex &focusedIndex() const
    {
        return m_focusedIndex;
    }

    const QModelIndex &hoveredActionIndex() const
    {
        return m_hoveredActionIndex;
    }

Q_SIGNALS:
    void entryEntered(const QModelIndex &index);
    void entryLeft(const QModelIndex &index);
    void entryMiddleClicked(const QModelIndex &index);

    void headerAreaEntered(const QModelIndex &index);
    void headerAreaLeft(const QModelIndex &index);

    void actionEntered(const QModelIndex &index);
    void actionLeft(const QModelIndex &index);
    void actionClicked(const QModelIndex &index);

public Q_SLOTS:
    void currentIndexChanged(const QModelIndex &index)
    {
        if (m_focusedIndex.isValid() && m_focusedIndex != m_hoveredIndex) {
            Q_EMIT entryLeft(m_focusedIndex);
        }
        if (index == m_hoveredIndex) {
            m_focusedIndex = m_hoveredIndex;
            return;
        }
        if (index.isValid()) {
            Q_EMIT entryEntered(index);
        }
        m_focusedIndex = index;
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseMove: {
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
            const QPoint pos = static_cast<QMouseEvent *>(event)->pos();
            const QModelIndex index = view->indexAt(pos);
            if (index != m_hoveredIndex) {
                if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                    Q_EMIT entryLeft(m_hoveredIndex);
                }
                if (index.isValid() && index != m_focusedIndex) {
                    Q_EMIT entryEntered(index);
                }
                m_hoveredIndex = index;
            }

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
            if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                Q_EMIT entryLeft(m_hoveredIndex);
            }
            m_hoveredIndex = QModelIndex();

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
            if (mouseEvent->button() == Qt::LeftButton || mouseEvent->button() == Qt::MiddleButton) {
                QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
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
        default:
            return false;
        }

        return false;
    }

private:
    QPersistentModelIndex m_hoveredIndex;
    QPersistentModelIndex m_hoveredHeaderAreaIndex;
    QPersistentModelIndex m_focusedIndex;
    QPersistentModelIndex m_middleClickedIndex;
    QPersistentModelIndex m_hoveredActionIndex;
    QPersistentModelIndex m_clickedActionIndex;
};

#endif
