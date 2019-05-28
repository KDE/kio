/*****************************************************************************
 * Copyright (C) 2007 by Peter Penz <peter.penz@gmx.at>                      *
 * Copyright (C) 2019 by Méven Car <meven.car@kdemail.net>                   *
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

#ifndef KDIROPERATORICONVIEW_P_H
#define KDIROPERATORICONVIEW_P_H

#include <QListView>

/**
 * Default icon view for KDirOperator using
 * custom view options.
 */
class KDirOperatorIconView : public QListView
{
    Q_OBJECT
public:
    KDirOperatorIconView(QWidget *parent = nullptr, QStyleOptionViewItem::Position decorationPosition = QStyleOptionViewItem::Position::Top);
    virtual ~KDirOperatorIconView();
    void setDecorationPosition(QStyleOptionViewItem::Position decorationPosition);

protected:
    QStyleOptionViewItem viewOptions() const override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

protected Q_SLOT:
    void updateLayout();
private:
    QStyleOptionViewItem::Position decorationPosition;
};


#endif // KDIROPERATORICONVIEW_P_H
