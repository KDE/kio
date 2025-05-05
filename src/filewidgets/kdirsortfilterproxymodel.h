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

#include <memory>

/*!
 * \class KDirSortFilterProxyModel
 * \inmodule KIOFileWidgets
 *
 * \brief Acts as proxy model for KDirModel to sort and filter
 *        KFileItems.
 *
 * A natural sorting is done. This means that items like:
 * \list
 * \li item_10.png
 * \li item_1.png
 * \li item_2.png
 * \endlist
 *
 * are sorted like
 * \list
 * \li item_1.png
 * \li item_2.png
 * \li item_10.png
 * \endlist
 *
 * Don't use it with non-KDirModel derivatives.
 */
class KIOFILEWIDGETS_EXPORT KDirSortFilterProxyModel : public KCategorizedSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit KDirSortFilterProxyModel(QObject *parent = nullptr);
    ~KDirSortFilterProxyModel() override;

    /*!
     * \reimp
     * Returns true for directories.
     */
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    /*!
     * \reimp
     * Returns true for 'empty' directories so they can be populated later.
     */
    bool canFetchMore(const QModelIndex &parent) const override;

    /*!
     * Returns the permissions in "points". This is useful for sorting by
     * permissions.
     */
    static int pointsForPermissions(const QFileInfo &info);

    /*!
     * Choose if files and folders are sorted separately (with folders first) or not.
     */
    void setSortFoldersFirst(bool foldersFirst);

    /*!
     * Returns if files and folders are sorted separately (with folders first) or not.
     */
    bool sortFoldersFirst() const;

    /*!
     * Sets a separate sorting with hidden files and folders last (true) or not (false).
     * \since 5.95
     */
    void setSortHiddenFilesLast(bool hiddenFilesLast);

    /*!
     *
     */
    bool sortHiddenFilesLast() const;

    /*!
     *
     */
    Qt::DropActions supportedDragOptions() const;

protected:
    bool subSortLessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    class KDirSortFilterProxyModelPrivate;
    std::unique_ptr<KDirSortFilterProxyModelPrivate> const d;
};

#endif
