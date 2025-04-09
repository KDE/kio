/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEMSELECTIONEMBLEM_H
#define KFILEITEMSELECTIONEMBLEM_H

#include <KDirOperator>
#include <QAbstractItemView>
#include <QModelIndex>

class KFileItem;
class KFileItemDelegate;
class QPoint;

class KFileItemSelectionEmblem
{
public:
    KFileItemSelectionEmblem(QAbstractItemView *itemView, QModelIndex index, KDirOperator *dirOperator);
    ~KFileItemSelectionEmblem();

    void updateSelectionEmblemRectForIndex(const int iconSize);
    bool handleMousePressEvent(const QPoint mousePos);
    bool isEmblemEnabled();

private:
    KFileItemDelegate *fileItemDelegate();

    QAbstractItemView *m_itemView;
    QModelIndex m_index;
    KDirOperator *m_dirOperator;
    KFileItemDelegate *m_fileItemDelegate;
    KFileItem m_fileItem;
    bool m_isDir;
};

#endif
