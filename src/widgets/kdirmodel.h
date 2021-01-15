/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KDIRMODEL_H
#define KDIRMODEL_H

#include <QAbstractItemModel>
#include "kiowidgets_export.h"
#include <kfileitem.h>

class KDirLister;
class KDirModelPrivate;
class JobUrlCache;

/**
 * @class KDirModel kdirmodel.h <KDirModel>
 *
 * @short A model for a KIO-based directory tree.
 *
 * KDirModel implements the QAbstractItemModel interface (for use with Qt's model/view widgets)
 * around the directory listing for one directory or a tree of directories.
 *
 * Note that there are some cases when using QPersistentModelIndexes from this model will not give
 * expected results. QPersistentIndexes will remain valid and updated if its siblings are added or
 * removed. However, if the QPersistentIndex or one of its ancestors is moved, the QPersistentIndex will become
 * invalid. For example, if a file or directory is renamed after storing a QPersistentModelIndex for it,
 * the index (along with any stored children) will become invalid even though it is still in the model. The reason
 * for this is that moves of files and directories are treated as separate insert and remove actions.
 *
 * @see KDirSortFilterProxyModel
 *
 * @author David Faure
 * Based on work by Hamish Rodda and Pascal Letourneau
 */
class KIOWIDGETS_EXPORT KDirModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    /**
     * @param parent parent qobject
     */
    explicit KDirModel(QObject *parent = nullptr);
    ~KDirModel();

    /**
     * Flags for the openUrl() method
     * @see OpenUrlFlags
     * @since 5.69
     */
    enum OpenUrlFlag {
        NoFlags = 0x0,   ///< No additional flags specified.
        Reload = 0x1,    ///< Indicates whether to use the cache or to reread
                         ///< the directory from the disk.
                         ///< Use only when opening a dir not yet listed by our dirLister()
                         ///< without using the cache. Otherwise use dirLister()->updateDirectory().
        ShowRoot = 0x2,  ///< Display a root node for the URL being opened.
    };
    /**
     * Stores a combination of #OpenUrlFlag values.
     */
    Q_DECLARE_FLAGS(OpenUrlFlags, OpenUrlFlag)

    /**
     * Display the contents of @p url in the model.
     * Apart from the support for the ShowRoot flag, this is equivalent to dirLister()->openUrl(url, flags)
     * @param url   the URL of the directory whose contents should be listed.
     *              Unless ShowRoot is set, the item for this directory will NOT be shown, the model starts at its children.
     * @param flags see OpenUrlFlag
     * @since 5.69
     */
    void openUrl(const QUrl &url, OpenUrlFlags flags = NoFlags);

    /**
     * Set the directory lister to use by this model, instead of the default KDirLister created internally.
     * The model takes ownership.
     */
    void setDirLister(KDirLister *dirLister);

    /**
     * Return the directory lister used by this model.
     */
    KDirLister *dirLister() const;

    /**
     * Return the fileitem for a given index. This is O(1), i.e. fast.
     */
    KFileItem itemForIndex(const QModelIndex &index) const;

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(4, 0)
    /**
     * Return the index for a given kfileitem. This can be slow.
     * @deprecated Since 4.0, use the method that takes a KFileItem by value
     */
    KIOWIDGETS_DEPRECATED_VERSION(4, 0, "Use KDirModel::indexForItem(const KFileItem &)")
    QModelIndex indexForItem(const KFileItem *) const;
#endif

    /**
     * Return the index for a given kfileitem. This can be slow.
     */
    QModelIndex indexForItem(const KFileItem &) const;

    /**
     * Return the index for a given url. This can be slow.
     */
    QModelIndex indexForUrl(const QUrl &url) const;

    /**
     * @short Lists subdirectories using fetchMore() as needed until the given @p url exists in the model.
     *
     * When the model is used by a treeview, call KDirLister::openUrl with the base url of the tree,
     * then the treeview will take care of calling fetchMore() when the user opens directories.
     * However if you want the tree to show a given URL (i.e. open the tree recursively until that URL),
     * call expandToUrl().
     * Note that this is asynchronous; the necessary listing of subdirectories will take time so
     * the model will not immediately have this url available.
     * The model emits the signal expand() when an index has become available; this can be connected
     * to the treeview in order to let it open that index.
     * @param url the url of a subdirectory of the directory model (or a file in a subdirectory)
     */
    void expandToUrl(const QUrl &url);

    /**
     * Notify the model that the item at this index has changed.
     * For instance because KMimeTypeResolver called determineMimeType on it.
     * This makes the model emit its dataChanged signal at this index, so that views repaint.
     * Note that for most things (renaming, changing size etc.), KDirLister's signals tell the model already.
     */
    void itemChanged(const QModelIndex &index);

    /**
     * Forget all previews (optimization for turning previews off).
     * The items will again have their default appearance (not controlled by the model).
     * @since 5.28
     */
    void clearAllPreviews();

    /**
     * Useful "default" columns. Views can use a proxy to have more control over this.
     */
    enum ModelColumns {
        Name = 0,
        Size,
        ModifiedTime,
        Permissions,
        Owner,
        Group,
        Type,
        ColumnCount,
    };

    /// Possible return value for data(ChildCountRole), meaning the item isn't a directory,
    /// or we haven't calculated its child count yet
    enum { ChildCountUnknown = -1 };

    enum AdditionalRoles {
        // Note: use   printf "0x%08X\n" $(($RANDOM*$RANDOM))
        // to define additional roles.
        FileItemRole = 0x07A263FF,  ///< returns the KFileItem for a given index
        ChildCountRole = 0x2C4D0A40, ///< returns the number of items in a directory, or ChildCountUnknown
        HasJobRole = 0x01E555A5,  ///< returns whether or not there is a job on an item (file/directory)
    };

    /**
     * @see DropsAllowed
     */
    enum DropsAllowedFlag {
        NoDrops = 0,
        DropOnDirectory = 1, ///< allow drops on any directory
        DropOnAnyFile = 2, ///< allow drops on any file
        DropOnLocalExecutable = 4, ///< allow drops on local executables, shell scripts and desktop files. Can be used with DropOnDirectory.
    };
    /**
     * Stores a combination of #DropsAllowedFlag values.
     */
    Q_DECLARE_FLAGS(DropsAllowed, DropsAllowedFlag)

    /// Set whether dropping onto items should be allowed, and for which kind of item
    /// Drops are disabled by default.
    void setDropsAllowed(DropsAllowed dropsAllowed);

    /// Reimplemented from QAbstractItemModel. Returns true for empty directories.
    bool canFetchMore(const QModelIndex &parent) const override;
    /// Reimplemented from QAbstractItemModel. Returns ColumnCount.
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    /// Reimplemented from QAbstractItemModel.
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    /// Reimplemented from QAbstractItemModel. Not implemented yet.
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
    /// Reimplemented from QAbstractItemModel. Lists the subdirectory.
    void fetchMore(const QModelIndex &parent) override;
    /// Reimplemented from QAbstractItemModel.
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    /// Reimplemented from QAbstractItemModel. Returns true for directories.
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    /// Reimplemented from QAbstractItemModel. Returns the column titles.
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    /// Reimplemented from QAbstractItemModel. O(1)
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    /// Reimplemented from QAbstractItemModel.
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    /// Reimplemented from QAbstractItemModel.
    QStringList mimeTypes() const override;
    /// Reimplemented from QAbstractItemModel.
    QModelIndex parent(const QModelIndex &index) const override;
    /// Reimplemented from QAbstractItemModel.
    QModelIndex sibling(int row, int column, const QModelIndex &index) const override;
    /// Reimplemented from QAbstractItemModel.
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /// Reimplemented from QAbstractItemModel.
    /// Call this to set a new icon, e.g. a preview
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    /// Reimplemented from QAbstractItemModel. Not implemented. @see KDirSortFilterProxyModel
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /**
     * Remove urls from the list if an ancestor is present on the list. This can
     * be used to delete only the ancestor url and skip a potential error of a non-existent url.
     *
     * For example, for a list of "/home/foo/a", "/home/foo/a/a.txt", "/home/foo/a/a/a.txt", "/home/foo/a/b/b.txt",
     * "home/foo/b/b.txt", this method will return the list "/home/foo/a", "/home/foo/b/b.txt".
     *
     * @return the list @p urls without parented urls inside.
     * @since 4.2
     */
    static QList<QUrl> simplifiedUrlList(const QList<QUrl> &urls);

    /**
     * This emits the needSequenceIcon signal, requesting another sequence icon
     *
     * If there is a KFilePreviewGenerator attached to this model, that generator will care
     * about creating another preview.
     *
     * @param index Index of the item that should get another icon
     * @param sequenceIndex Index in the sequence. If it is zero, the standard icon will be assigned.
     *                                        For higher indices, arbitrary different meaningful icons will be generated.
     * @since 4.3
     */
    void requestSequenceIcon(const QModelIndex &index, int sequenceIndex);

    /**
     * Enable/Disable the displaying of an animated overlay that is shown for any destination
     * urls (in the view). When enabled, the animations (if any) will be drawn automatically.
     *
     * Only the files/folders that are visible and have jobs associated with them
     * will display the animation.
     * You would likely not want this enabled if you perform some kind of custom painting
     * that takes up a whole item, and will just make this(and what you paint) look funky.
     *
     * Default is disabled.
     *
     * Note: KFileItemDelegate needs to have it's method called with the same
     * value, when you make the call to this method.
     *
     * @since 4.5
     */
    void setJobTransfersVisible(bool show);

    /**
     * Returns whether or not displaying job transfers has been enabled.
     * @since 4.5
     */
    bool jobTransfersVisible() const;

    Qt::DropActions supportedDropActions() const override;

Q_SIGNALS:
    /**
     * Emitted for each subdirectory that is a parent of a url passed to expandToUrl
     * This allows to asynchronously open a tree view down to a given directory.
     * Also emitted for the final file, if expandToUrl is called with a file
     * (for instance so that it can be selected).
     */
    void expand(const QModelIndex &index);
    /**
     * Emitted when another icon sequence index is requested
     * @param index Index of the item that should get another icon
     * @param sequenceIndex Index in the sequence. If it is zero, the standard icon should be assigned.
     *                                        For higher indices, arbitrary different meaningful icons should be generated.
     *                                        This is usually slowly counted up while the user hovers the icon.
     *                                        If no meaningful alternative icons can be generated, this should be ignored.
     * @since 4.3
     */
    void needSequenceIcon(const QModelIndex &index, int sequenceIndex);

private:
    // Make those private, they shouldn't be called by applications
    bool insertRows(int, int, const QModelIndex & = QModelIndex()) override;
    bool insertColumns(int, int, const QModelIndex & = QModelIndex()) override;
    bool removeRows(int, int, const QModelIndex & = QModelIndex()) override;
    bool removeColumns(int, int, const QModelIndex & = QModelIndex()) override;

private:
    friend class KDirModelPrivate;
    KDirModelPrivate *const d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KDirModel::DropsAllowed)
Q_DECLARE_OPERATORS_FOR_FLAGS(KDirModel::OpenUrlFlags)

#endif /* KDIRMODEL_H */
