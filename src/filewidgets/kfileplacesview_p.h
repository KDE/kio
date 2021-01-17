/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESVIEW_P_H
#define KFILEPLACESVIEW_P_H

#include <QMouseEvent>

class KFilePlacesEventWatcher
    : public QObject
{
    Q_OBJECT

public:
    explicit KFilePlacesEventWatcher(QObject *parent = nullptr)
        : QObject(parent) {}

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
        }
        break;
        case QEvent::Leave:
            if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                Q_EMIT entryLeft(m_hoveredIndex);
            }
            m_hoveredIndex = QModelIndex();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick: {
            // Prevent the selection clearing by clicking on the viewport directly
            QAbstractItemView *view = qobject_cast<QAbstractItemView *>(watched->parent());
            if (!view->indexAt(static_cast<QMouseEvent *>(event)->pos()).isValid()) {
                return true;
            }
        }
        break;
        default:
            return false;
        }

        return false;
    }

private:
    QPersistentModelIndex m_hoveredIndex;
    QPersistentModelIndex m_focusedIndex;
};

#endif
