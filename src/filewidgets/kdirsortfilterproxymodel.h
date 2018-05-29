/*
   Copyright (C) 2006 by Peter Penz <peter.penz@gmx.at>
   Copyright (C) 2006 by Dominic Battre <dominic@battre.de>
   Copyright (C) 2006 by Martin Pool <mbp@canonical.com>

   Separated from Dolphin by Nick Shaforostoff <shafff@ukr.net>

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

#ifndef KDIRSORTFILTERPROXYMODEL_H
#define KDIRSORTFILTERPROXYMODEL_H

#include <QFileInfo>

#include <kcategorizedsortfilterproxymodel.h>

#include "kiofilewidgets_export.h"

/**
 * @class KDirSortFilterProxyModel kdirsortfilterproxymodel.h <KDirSortFilterProxyModel>
 *
 * @brief Acts as proxy model for KDirModel to sort and filter
 *        KFileItems.
 *
 * A natural sorting is done. This means that items like:
 * - item_10.png
 * - item_1.png
 * - item_2.png
 *
 * are sorted like
 * - item_1.png
 * - item_2.png
 * - item_10.png
 *
 * Don't use it with non-KDirModel derivatives.
 *
 * @author Dominic Battre, Martin Pool and Peter Penz
 */
class KIOFILEWIDGETS_EXPORT KDirSortFilterProxyModel
    : public KCategorizedSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit KDirSortFilterProxyModel(QObject *parent = nullptr);
    ~KDirSortFilterProxyModel() override;

    /** Reimplemented from QAbstractItemModel. Returns true for directories. */
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * Reimplemented from QAbstractItemModel.
     * Returns true for 'empty' directories so they can be populated later.
     */
    bool canFetchMore(const QModelIndex &parent) const override;

    /**
     * Returns the permissions in "points". This is useful for sorting by
     * permissions.
     */
    static int pointsForPermissions(const QFileInfo &info);

    /**
     * Choose if files and folders are sorted separately (with folders first) or not.
     * @since 4.3
     */
    void setSortFoldersFirst(bool foldersFirst);

    /**
     * Returns if files and folders are sorted separately (with folders first) or not.
     * @since 4.3
     */
    bool sortFoldersFirst() const;

    Qt::DropActions supportedDragOptions() const;

protected:
    /**
     * Reimplemented from KCategorizedSortFilterProxyModel.
     */
    virtual bool subSortLessThan(const QModelIndex &left,
                                 const QModelIndex &right) const override;
private:
    Q_PRIVATE_SLOT(d, void slotNaturalSortingChanged())

private:
    class KDirSortFilterProxyModelPrivate;
    KDirSortFilterProxyModelPrivate *const d;
};

#endif
