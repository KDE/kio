/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

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
    void drawSectionHeader(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

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

    const QModelIndex &focusedIndex() const
    {
        return m_focusedIndex;
    }

Q_SIGNALS:
    void entryEntered(const QModelIndex &index);
    void entryLeft(const QModelIndex &index);
    void entryMiddleClicked(const QModelIndex &index);

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
            const QModelIndex index = view->indexAt(static_cast<QMouseEvent *>(event)->pos());
            if (index != m_hoveredIndex) {
                if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                    Q_EMIT entryLeft(m_hoveredIndex);
                }
                if (index.isValid() && index != m_focusedIndex) {
                    Q_EMIT entryEntered(index);
                }
                m_hoveredIndex = index;
            }
            break;
        }
        case QEvent::Leave:
            if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                Q_EMIT entryLeft(m_hoveredIndex);
            }
            m_hoveredIndex = QModelIndex();
            break;
        case QEvent::MouseButtonPress: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::MiddleButton) {
                QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
                const QModelIndex index = view->indexAt(mouseEvent->pos());
                if (index.isValid()) {
                    m_middleClickedIndex = index;
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
            if (mouseEvent->button() == Qt::MiddleButton) {
                if (m_middleClickedIndex.isValid()) {
                    QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
                    const QModelIndex index = view->indexAt(mouseEvent->pos());
                    if (m_middleClickedIndex == index) {
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
    QPersistentModelIndex m_focusedIndex;
    QPersistentModelIndex m_middleClickedIndex;
};

#endif
