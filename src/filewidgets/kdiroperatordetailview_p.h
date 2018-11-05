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
    explicit KDirOperatorDetailView(QWidget *parent = nullptr);
    virtual ~KDirOperatorDetailView();

    /**
    * Displays either Detail, Tree or DetailTree modes.
    */
    virtual bool setViewMode(KFile::FileView viewMode);

protected:
    bool event(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

private:
    bool m_hideDetailColumns;
};

#endif
