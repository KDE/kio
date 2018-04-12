/*****************************************************************************
 * Copyright (C) 2007 by Peter Penz <peter.penz@gmx.at>                      *
 *                                                                           *
 * This library is free software; you can redistribute it and/or             *
 * modify it under the terms of the GNU Library General Public               *
 * License version 2 as published by the Free Software Foundation.           *
 *                                                                           *
 * This library is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         *
 * Library General Public License for more details.                          *
 *                                                                           *
 * You should have received a copy of the GNU Library General Public License *
 * along with this library; see the file COPYING.LIB.  If not, write to      *
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,      *
 * Boston, MA 02110-1301, USA.                                               *
 *****************************************************************************/

#include "kdiroperatordetailview_p.h"

#include <kdirlister.h>
#include <kdirmodel.h>
#include <QDebug>

#include <QEvent>
#include <QTimer>
#include <QAbstractProxyModel>
#include <QApplication>
#include <QHeaderView>
#include <QListView>
#include <QMimeData>
#include <QResizeEvent>
#include <QScrollBar>

KDirOperatorDetailView::KDirOperatorDetailView(QWidget *parent) :
    QTreeView(parent),
    m_hideDetailColumns(false)
{
    setRootIsDecorated(false);
    setSortingEnabled(true);
    setUniformRowHeights(true);
    setDragDropMode(QListView::DragOnly);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setVerticalScrollMode(QListView::ScrollPerPixel);
    setHorizontalScrollMode(QListView::ScrollPerPixel);
}

KDirOperatorDetailView::~KDirOperatorDetailView()
{
}

bool KDirOperatorDetailView::setViewMode(KFile::FileView viewMode)
{
    bool tree = false;

    if (KFile::isDetailView(viewMode)) {
        m_hideDetailColumns = false;
    } else if (KFile::isTreeView(viewMode)) {
        m_hideDetailColumns = true;
        tree = true;
    } else if (KFile::isDetailTreeView(viewMode)) {
        m_hideDetailColumns = false;
        tree = true;
    } else {
        return false;
    }

    setRootIsDecorated(tree);
    setItemsExpandable(tree);
    // This allows to have a horizontal scrollbar in case this view is used as
    // a plain treeview instead of cutting off filenames, especially useful when
    // using KDirOperator in horizontally limited parts of an app.
    if (tree && m_hideDetailColumns) {
        header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    } else {
        header()->setSectionResizeMode(QHeaderView::Interactive);
    }

    return true;
}

bool KDirOperatorDetailView::event(QEvent *event)
{
    if (event->type() == QEvent::Polish) {
        QHeaderView *headerView = header();
        headerView->setSectionResizeMode(0, QHeaderView::Stretch);
        headerView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        headerView->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        headerView->setStretchLastSection(false);
        headerView->setSectionsMovable(false);

        setColumnHidden(KDirModel::Size, m_hideDetailColumns);
        setColumnHidden(KDirModel::ModifiedTime, m_hideDetailColumns);
        hideColumn(KDirModel::Type);
        hideColumn(KDirModel::Permissions);
        hideColumn(KDirModel::Owner);
        hideColumn(KDirModel::Group);
    } else if (event->type() == QEvent::UpdateRequest) {
        // A wheel movement will scroll 4 items
        if (model()->rowCount()) {
            verticalScrollBar()->setSingleStep((sizeHintForRow(0) / 3) * 4);
        }
    }

    return QTreeView::event(event);
}

void KDirOperatorDetailView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void KDirOperatorDetailView::mousePressEvent(QMouseEvent *event)
{
    QTreeView::mousePressEvent(event);

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid() || (index.column() != KDirModel::Name)) {
        const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
        if (!(modifiers & Qt::ShiftModifier) && !(modifiers & Qt::ControlModifier)) {
            clearSelection();
        }
    }
}

void KDirOperatorDetailView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QTreeView::currentChanged(current, previous);
}
