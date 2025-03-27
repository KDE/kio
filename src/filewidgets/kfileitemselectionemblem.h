/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEMSELECTIONEMBLEM_H
#define KFILEITEMSELECTIONEMBLEM_H

#include <QAbstractItemView>
#include <QModelIndex>

class KFileItemDelegate;
class QPoint;

class KFileItemSelectionEmblem
{
public:
    KFileItemSelectionEmblem(QAbstractItemView *itemView, QModelIndex index);
    ~KFileItemSelectionEmblem();

    void updateSelectionEmblemRectForIndex(const int iconSize);
    bool isSelectionEmblemClicked(const QPoint mousePos);
    bool isEmblemEnabled();

private:
    KFileItemDelegate *fileItemDelegate();

    QAbstractItemView *m_itemView;
    QModelIndex m_index;
    KFileItemDelegate *m_fileItemDelegate;
};

#endif
