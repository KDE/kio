/*
    SPDX-FileCopyrightText: 2008 Peter Penz <peter.penz@gmx.at>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "defaultviewadapter_p.h"

#include <QAbstractItemView>
#include <QScrollBar>

namespace KIO
{

DefaultViewAdapter::DefaultViewAdapter(QAbstractItemView *view, QObject *parent) :
    KAbstractViewAdapter(parent),
    m_view(view)
{
}

QAbstractItemModel *DefaultViewAdapter::model() const
{
    return m_view->model();
}

QSize DefaultViewAdapter::iconSize() const
{
    return m_view->iconSize();
}

QPalette DefaultViewAdapter::palette() const
{
    return m_view->palette();
}

QRect DefaultViewAdapter::visibleArea() const
{
    return m_view->viewport()->rect();
}

QRect DefaultViewAdapter::visualRect(const QModelIndex &index) const
{
    return m_view->visualRect(index);
}

void DefaultViewAdapter::connect(Signal signal, QObject *receiver, const char *slot)
{
    if (signal == ScrollBarValueChanged) {
        QObject::connect(m_view->horizontalScrollBar(), SIGNAL(valueChanged(int)), receiver, slot);
        QObject::connect(m_view->verticalScrollBar(), SIGNAL(valueChanged(int)), receiver, slot);
    }
}

}

