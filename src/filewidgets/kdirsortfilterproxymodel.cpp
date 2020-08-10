/*
    SPDX-FileCopyrightText: 2006 Peter Penz <peter.penz@gmx.at>
    SPDX-FileCopyrightText: 2006 Dominic Battre <dominic@battre.de>
    SPDX-FileCopyrightText: 2006 Martin Pool <mbp@canonical.com>

    Separated from Dolphin by Nick Shaforostoff <shafff@ukr.net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kdirsortfilterproxymodel.h"

#include <kdirmodel.h>
#include <kfileitem.h>
#include <KLocalizedString>
#include <KConfigGroup>
#include <KSharedConfig>

#include <QCollator>

class Q_DECL_HIDDEN KDirSortFilterProxyModel::KDirSortFilterProxyModelPrivate
{
public:
    KDirSortFilterProxyModelPrivate();

    int compare(const QString &, const QString &, Qt::CaseSensitivity caseSensitivity  = Qt::CaseSensitive);
    void slotNaturalSortingChanged();

    bool m_sortFoldersFirst;
    bool m_naturalSorting;
    QCollator m_collator;
};

KDirSortFilterProxyModel::KDirSortFilterProxyModelPrivate::KDirSortFilterProxyModelPrivate() :
    m_sortFoldersFirst(true)
{
    slotNaturalSortingChanged();
}

int KDirSortFilterProxyModel::KDirSortFilterProxyModelPrivate::compare(const QString &a,
        const QString &b,
        Qt::CaseSensitivity caseSensitivity)
{
    int result;

    if (m_naturalSorting) {
        m_collator.setCaseSensitivity(caseSensitivity);
        result = m_collator.compare(a, b);
    } else {
        result = QString::compare(a, b, caseSensitivity);
    }

    if (caseSensitivity == Qt::CaseSensitive || result != 0) {
        // Only return the result, if the strings are not equal. If they are equal by a case insensitive
        // comparison, still a deterministic sort order is required. A case sensitive
        // comparison is done as fallback.
        return result;
    }

    return QString::compare(a, b, Qt::CaseSensitive);
}

void KDirSortFilterProxyModel::KDirSortFilterProxyModelPrivate::slotNaturalSortingChanged()
{
    KConfigGroup g(KSharedConfig::openConfig(), "KDE");
    m_naturalSorting = g.readEntry("NaturalSorting", true);
    m_collator.setNumericMode(m_naturalSorting);
}

KDirSortFilterProxyModel::KDirSortFilterProxyModel(QObject *parent)
    : KCategorizedSortFilterProxyModel(parent), d(new KDirSortFilterProxyModelPrivate)
{
    setDynamicSortFilter(true);

    // sort by the user visible string for now
    setSortCaseSensitivity(Qt::CaseInsensitive);
    sort(KDirModel::Name, Qt::AscendingOrder);

}

Qt::DropActions KDirSortFilterProxyModel::supportedDragOptions() const
{
    return Qt::CopyAction | Qt::MoveAction | Qt::LinkAction | Qt::IgnoreAction;
}

KDirSortFilterProxyModel::~KDirSortFilterProxyModel()
{
    delete d;
}

bool KDirSortFilterProxyModel::hasChildren(const QModelIndex &parent) const
{
    const QModelIndex sourceParent = mapToSource(parent);
    return sourceModel()->hasChildren(sourceParent);
}

bool KDirSortFilterProxyModel::canFetchMore(const QModelIndex &parent) const
{
    const QModelIndex sourceParent = mapToSource(parent);
    return sourceModel()->canFetchMore(sourceParent);
}

int KDirSortFilterProxyModel::pointsForPermissions(const QFileInfo &info)
{
    int points = 0;

    const QFile::Permission permissionsCheck[] = { QFile::ReadUser,
                                                   QFile::WriteUser,
                                                   QFile::ExeUser,
                                                   QFile::ReadGroup,
                                                   QFile::WriteGroup,
                                                   QFile::ExeGroup,
                                                   QFile::ReadOther,
                                                   QFile::WriteOther,
                                                   QFile::ExeOther
                                                 };

    for (QFile::Permission perm : permissionsCheck) {
        points += info.permission(perm) ? 1 : 0;
    }

    return points;
}

void KDirSortFilterProxyModel::setSortFoldersFirst(bool foldersFirst)
{
    d->m_sortFoldersFirst = foldersFirst;
}

bool KDirSortFilterProxyModel::sortFoldersFirst() const
{
    return d->m_sortFoldersFirst;
}

bool KDirSortFilterProxyModel::subSortLessThan(const QModelIndex &left,
        const QModelIndex &right) const
{
    KDirModel *dirModel = static_cast<KDirModel *>(sourceModel());

    const KFileItem leftFileItem  = dirModel->itemForIndex(left);
    const KFileItem rightFileItem = dirModel->itemForIndex(right);

    const bool isLessThan = (sortOrder() == Qt::AscendingOrder);

    // Folders go before files if the corresponding setting is set.
    if (d->m_sortFoldersFirst) {
        const bool leftItemIsDir  = leftFileItem.isDir();
        const bool rightItemIsDir = rightFileItem.isDir();
        if (leftItemIsDir && !rightItemIsDir) {
            return isLessThan;
        } else if (!leftItemIsDir && rightItemIsDir) {
            return !isLessThan;
        }
    }

    // Hidden elements go before visible ones.
    const bool leftItemIsHidden  = leftFileItem.isHidden();
    const bool rightItemIsHidden = rightFileItem.isHidden();
    if (leftItemIsHidden && !rightItemIsHidden) {
        return isLessThan;
    } else if (!leftItemIsHidden && rightItemIsHidden) {
        return !isLessThan;
    }

    switch (left.column()) {
    case KDirModel::Name: {
        int result = d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity());
        if (result == 0) {
            // KFileItem::text() may not be unique in case UDS_DISPLAY_NAME is used
            result = d->compare(leftFileItem.name(sortCaseSensitivity() == Qt::CaseInsensitive),
                                rightFileItem.name(sortCaseSensitivity() == Qt::CaseInsensitive),
                                sortCaseSensitivity());
            if (result == 0) {
                // If KFileItem::text() is also not unique most probably a search protocol is used
                // that allows showing the same file names from different directories
                result = d->compare(leftFileItem.url().toString(), rightFileItem.url().toString(), sortCaseSensitivity());
            }
        }

        return result < 0;
    }

    case KDirModel::Size: {
        // If we have two folders, what we have to measure is the number of
        // items that contains each other
        if (leftFileItem.isDir() && rightFileItem.isDir()) {
            QVariant leftValue = dirModel->data(left, KDirModel::ChildCountRole);
            int leftCount = (leftValue.type() == QVariant::Int) ? leftValue.toInt() : KDirModel::ChildCountUnknown;

            QVariant rightValue = dirModel->data(right, KDirModel::ChildCountRole);
            int rightCount = (rightValue.type() == QVariant::Int) ? rightValue.toInt() : KDirModel::ChildCountUnknown;

            // In the case they two have the same child items, we sort them by
            // their names. So we have always everything ordered. We also check
            // if we are taking in count their cases.
            if (leftCount == rightCount) {
                return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
            }

            // If one of them has unknown child items, place them on the end. If we
            // were comparing two unknown childed items, the previous comparison
            // sorted them by QCollator between them. This case is when we
            // have an unknown childed item, and another known.
            if (leftCount == KDirModel::ChildCountUnknown) {
                return false;
            }

            if (rightCount == KDirModel::ChildCountUnknown) {
                return true;
            }

            // If they had different number of items, we sort them depending
            // on how many items had each other.
            return leftCount < rightCount;
        }

        // If what we are measuring is two files and they have the same size,
        // sort them by their file names.
        if (leftFileItem.size() == rightFileItem.size()) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        // If their sizes are different, sort them by their sizes, as expected.
        return leftFileItem.size() < rightFileItem.size();
    }

    case KDirModel::ModifiedTime: {
        QDateTime leftModifiedTime = leftFileItem.time(KFileItem::ModificationTime).toLocalTime();
        QDateTime rightModifiedTime = rightFileItem.time(KFileItem::ModificationTime).toLocalTime();

        if (leftModifiedTime == rightModifiedTime) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        return leftModifiedTime < rightModifiedTime;
    }

    case KDirModel::Permissions: {
        const int leftPermissions = leftFileItem.permissions();
        const int rightPermissions = rightFileItem.permissions();

        if (leftPermissions == rightPermissions) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        return leftPermissions > rightPermissions;
    }

    case KDirModel::Owner: {
        if (leftFileItem.user() == rightFileItem.user()) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        return d->compare(leftFileItem.user(), rightFileItem.user()) < 0;
    }

    case KDirModel::Group: {
        if (leftFileItem.group() == rightFileItem.group()) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        return d->compare(leftFileItem.group(), rightFileItem.group()) < 0;
    }

    case KDirModel::Type: {
        if (leftFileItem.mimetype() == rightFileItem.mimetype()) {
            return d->compare(leftFileItem.text(), rightFileItem.text(), sortCaseSensitivity()) < 0;
        }

        return d->compare(leftFileItem.mimeComment(), rightFileItem.mimeComment()) < 0;
    }

    }

    // We have set a SortRole and trust the ProxyModel to do
    // the right thing for now.
    return KCategorizedSortFilterProxyModel::subSortLessThan(left, right);
}

#include "moc_kdirsortfilterproxymodel.cpp"
