/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Dominic Battre <dominic@battre.de>
    SPDX-FileCopyrightText: 2006 Martin Pool <mbp@canonical.com>

    Separated from Dolphin by Nick Shaforostoff <shafff@ukr.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KDIRSORTFILTERPROXYMODEL_H
#define KDIRSORTFILTERPROXYMODEL_H

#include <QFileInfo>

#include <KCategorizedSortFilterProxyModel>

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
