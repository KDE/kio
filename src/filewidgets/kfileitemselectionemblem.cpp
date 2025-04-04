/*
    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfileitemselectionemblem.h"
#include "kfileitemdelegate.h"

#include <QAbstractItemDelegate>
#include <QAbstractItemView>
#include <QApplication>
#include <QModelIndex>
#include <QPoint>

KFileItemSelectionEmblem::KFileItemSelectionEmblem(QAbstractItemView *itemView, QModelIndex index)
{
    m_itemView = itemView;
    m_index = index;
    m_fileItemDelegate = fileItemDelegate();
    m_isDir = m_fileItemDelegate->isDir(m_index);
}

KFileItemSelectionEmblem::~KFileItemSelectionEmblem()
{
}

bool KFileItemSelectionEmblem::isEmblemEnabled()
{
    return !m_isDir && m_itemView->selectionMode() == QAbstractItemView::ExtendedSelection
        && qApp->style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick);
}

KFileItemDelegate *KFileItemSelectionEmblem::fileItemDelegate()
{
    auto itemDelegate = m_itemView->itemDelegateForIndex(m_index);
    if (itemDelegate) {
        return qobject_cast<KFileItemDelegate *>(itemDelegate);
    }
    return nullptr;
}

void KFileItemSelectionEmblem::updateSelectionEmblemRectForIndex(const int iconSize)
{
    if (isEmblemEnabled() && m_fileItemDelegate) {
        m_fileItemDelegate->setSelectionEmblemRect(m_itemView->visualRect(m_index), iconSize);
    }
}

bool KFileItemSelectionEmblem::isSelectionEmblemClicked(const QPoint mousePos)
{
    return (isEmblemEnabled() && m_fileItemDelegate && m_fileItemDelegate->selectionEmblemRect().contains(mousePos));
}
