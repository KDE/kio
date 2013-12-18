/*  This file is part of the KDE project
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

#ifndef KFILEPLACESVIEW_P_H
#define KFILEPLACESVIEW_P_H

#include <QMouseEvent>

class KFilePlacesEventWatcher
    : public QObject
{
Q_OBJECT

public:
    KFilePlacesEventWatcher(QObject *parent = 0)
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
            emit entryLeft(m_focusedIndex);
        }
        if (index == m_hoveredIndex) {
            m_focusedIndex = m_hoveredIndex;
            return;
        }
        if (index.isValid()) {
            emit entryEntered(index);
        }
        m_focusedIndex = index;
    }

protected:
    virtual bool eventFilter(QObject *watched, QEvent *event)
    {
        switch (event->type()) {
            case QEvent::MouseMove: {
                    QAbstractItemView *view = qobject_cast<QAbstractItemView*>(watched->parent());
                    const QModelIndex index = view->indexAt(static_cast<QMouseEvent*>(event)->pos());
                    if (index != m_hoveredIndex) {
                        if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                            emit entryLeft(m_hoveredIndex);
                        }
                        if (index.isValid() && index != m_focusedIndex) {
                            emit entryEntered(index);
                        }
                        m_hoveredIndex = index;
                    }
                }
                break;
            case QEvent::Leave:
                if (m_hoveredIndex.isValid() && m_hoveredIndex != m_focusedIndex) {
                    emit entryLeft(m_hoveredIndex);
                }
                m_hoveredIndex = QModelIndex();
                break;
            case QEvent::MouseButtonPress:
            case QEvent::MouseButtonDblClick: {
                    // Prevent the selection clearing by clicking on the viewport directly
                    QAbstractItemView *view = qobject_cast<QAbstractItemView*>(watched->parent());
                    if (!view->indexAt(static_cast<QMouseEvent*>(event)->pos()).isValid()) {
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
