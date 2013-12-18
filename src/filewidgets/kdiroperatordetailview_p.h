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

#ifndef KDIROPERATORDETAILVIEW_P_H
#define KDIROPERATORDETAILVIEW_P_H

#include <QTreeView>

#include <kfile.h>

class QAbstractItemModel;

/**
 * Default detail view for KDirOperator using
 * custom resizing options and columns.
 */
class KDirOperatorDetailView : public QTreeView
{
    Q_OBJECT

public:
    KDirOperatorDetailView(QWidget *parent = 0);
    virtual ~KDirOperatorDetailView();
    virtual void setModel(QAbstractItemModel *model);

    /**
    * Displays either Detail, Tree or DetailTree modes.
    */
    virtual bool setViewMode(KFile::FileView viewMode);

protected:
    virtual bool event(QEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void currentChanged(const QModelIndex& current, const QModelIndex& previous);

private Q_SLOTS:
    void resetResizing();
    void disableColumnResizing();
    void slotLayoutChanged();

private:
    bool m_resizeColumns;
    bool m_hideDetailColumns;
};

#endif
