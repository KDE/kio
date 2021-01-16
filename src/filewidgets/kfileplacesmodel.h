/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESMODEL_H
#define KFILEPLACESMODEL_H

#include "kiofilewidgets_export.h"

#include <QAbstractItemModel>
#include <QUrl>
#include <KBookmark>

#include <solid/device.h>

class QMimeData;
class QAction;

/**
 * @class KFilePlacesModel kfileplacesmodel.h <KFilePlacesModel>
 *
 * This class is a list view model. Each entry represents a "place"
 * where user can access files. Only relevant when
 * used with QListView or QTableView.
 */
class KIOFILEWIDGETS_EXPORT KFilePlacesModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    // Note: run   printf "0x%08X\n" $(($RANDOM*$RANDOM))
    // to define additional roles.
    enum AdditionalRoles {
        UrlRole = 0x069CD12B, /// @see url()
        HiddenRole = 0x0741CAAC, /// @see isHidden()
        SetupNeededRole = 0x059A935D, /// @see setupNeeded()
        FixedDeviceRole = 0x332896C1, /// Whether the place is a fixed device (neither hotpluggable nor removable).
        CapacityBarRecommendedRole = 0x1548C5C4, /// Whether the place should have its free space displayed in a capacity bar.
        GroupRole = 0x0a5b64ee, ///< @since 5.40 /// The name of the group, for example "Remote" or "Devices".
        IconNameRole = 0x00a45c00, ///< @since 5.41 @see icon()
        GroupHiddenRole = 0x21a4b936, ///< @since 5.42 @see isGroupHidden()
    };

    /// @since 5.42
    enum GroupType {
        PlacesType,
        RemoteType,
        RecentlySavedType,
        SearchForType,
        DevicesType,
        RemovableDevicesType,
        UnknownType,
        TagsType, ///< @since 5.54
    };

    explicit KFilePlacesModel(QObject *parent = nullptr);
    /**
    * @brief Construct a new KFilePlacesModel with an alternativeApplicationName
    * @param alternativeApplicationName This value will be used to filter bookmarks in addition to the actual application name
    * @param parent Parent object
    * @since 5.43
    * @todo kf6: merge constructors
    */
    KFilePlacesModel(const QString &alternativeApplicationName, QObject *parent = nullptr);
    ~KFilePlacesModel() override;

    /**
     * @return The URL of the place at index @p index.
     */
    QUrl url(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index needs to be mounted before it can be used.
     */
    bool setupNeeded(const QModelIndex &index) const;

    /**
     * @return The icon of the place at index @p index.
     */
    QIcon icon(const QModelIndex &index) const;

    /**
     * @return The user-visible text of the place at index @p index.
     */
    QString text(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index is hidden or is inside an hidden group.
     */
    bool isHidden(const QModelIndex &index) const;

    /**
     * @return Whether the group type @p type is hidden.
     * @since 5.42
     */
    bool isGroupHidden(const GroupType type) const;

    /**
     * @return Whether the group of the place at index @p index is hidden.
     * @since 5.42
     */
    bool isGroupHidden(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index is a device handled by Solid.
     * @see deviceForIndex()
     */
    bool isDevice(const QModelIndex &index) const;

    /**
     * @return The solid device of the place at index @p index, if it is a device. Otherwise a default Solid::Device() instance is returned.
     * @see isDevice()
     */
    Solid::Device deviceForIndex(const QModelIndex &index) const;

    /**
     * @return The KBookmark instance of the place at index @p index.
     * If the index is not valid, a default KBookmark instance is returned.
     */
    KBookmark bookmarkForIndex(const QModelIndex &index) const;
    
    /**
     * @return The KBookmark instance of the place with url @p searchUrl.
     * If the bookmark corresponding to searchUrl is not found, a default KBookmark instance is returned.
     * @since 5.63
     */
    KBookmark bookmarkForUrl(const QUrl &searchUrl) const;

    /**
     * @return The group type of the place at index @p index.
     * @since 5.42
     */
    GroupType groupType(const QModelIndex &index) const;

    /**
    * @return The list of model indexes that have @ type as their group type.
    * @see groupType()
    * @since 5.42
    */
    QModelIndexList groupIndexes(const GroupType type) const;

    /**
     * @return A QAction with a proper translated label that can be used to trigger the requestTeardown()
     * method for the place at index @p index.
     * @see requestTeardown()
     */
    QAction *teardownActionForIndex(const QModelIndex &index) const;

    /**
     * @return A QAction with a proper translated label that can be used to trigger the requestEject()
     * method for the place at index @p index.
     * @see requestEject()
     */
    QAction *ejectActionForIndex(const QModelIndex &index) const;

    /**
     * Unmounts the place at index @p index by triggering the teardown functionality of its Solid device.
     * @see deviceForIndex()
     */
    void requestTeardown(const QModelIndex &index);

    /**
     * Ejects the place at index @p index by triggering the ejetc functionality of its Solid device.
     * @see deviceForIndex()
     */
    void requestEject(const QModelIndex &index);

    /**
     * Mounts the place at index @p index by triggering the setup functionality of its Solid device.
     * @see deviceForIndex()
     */
    void requestSetup(const QModelIndex &index);

    /**
     * Adds a new place to the model.
     * @param text The user-visible text for the place
     * @param url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     * @param iconName The icon of the place
     * @param appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     */
    void addPlace(const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /**
     * Adds a new place to the model.
     * @param text The user-visible text for the place
     * @param url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     * @param iconName The icon of the place
     * @param appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     * @param after The index after which the new place will be added.
     */
    void addPlace(const QString &text, const QUrl &url, const QString &iconName, const QString &appName, const QModelIndex &after);

    /**
     * Edits the place with index @p index.
     * @param text The new user-visible text for the place
     * @param url The new URL of the place
     * @param iconName The new icon of the place
     * @param appName The new application-local filter for the place (@see addPlace()).
     */
    void editPlace(const QModelIndex &index, const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /**
     * Deletes the place with index @p index from the model.
     */
    void removePlace(const QModelIndex &index) const;

    /**
     * Changes the visibility of the place with index @p index, but only if the place is not inside an hidden group.
     * @param hidden Whether the place should be hidden or visible.
     * @see isGroupHidden()
     */
    void setPlaceHidden(const QModelIndex &index, bool hidden);

    /**
     * Changes the visibility of the group with type @p type.
     * @param hidden Whether the group should be hidden or visible.
     * @see isGroupHidden()
     * @since 5.42
     */
    void setGroupHidden(const GroupType type, bool hidden);

    /**
     * @brief Move place at @p itemRow to a position before @p row
     * @return Whether the place has been moved.
     * @since 5.41
     */
    bool movePlace(int itemRow, int row);

    /**
     * @return The number of hidden places in the model.
     * @see isHidden()
     */
    int hiddenCount() const;

    /**
     * @brief Get a visible data based on Qt role for the given index.
     * Return the device information for the give index.
     *
     * @param index The QModelIndex which contains the row, column to fetch the data.
     * @param role The Interview data role(ex: Qt::DisplayRole).
     *
     * @return the data for the given index and role.
     */
    QVariant data(const QModelIndex &index, int role) const override;

    /**
     * @brief Get the children model index for the given row and column.
     */
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Get the parent QModelIndex for the given model child.
     */
    QModelIndex parent(const QModelIndex &child) const override;

    /**
     * @brief Get the number of rows for a model index.
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * @brief Get the number of columns for a model index.
     */
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /**
     * Returns the closest item for the URL \a url.
     * The closest item is defined as item which is equal to
     * the URL or at least is a parent URL. If there are more than
     * one possible parent URL candidates, the item which covers
     * the bigger range of the URL is returned.
     *
     * Example: the url is '/home/peter/Documents/Music'.
     * Available items are:
     * - /home/peter
     * - /home/peter/Documents
     *
     * The returned item will the one for '/home/peter/Documents'.
     */
    QModelIndex closestItem(const QUrl &url) const;

    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action,
                      int row, int column, const QModelIndex &parent) override;

    /**
     * @brief Reload bookmark information
     * @since 5.41
     */
    void refresh() const;

    /**
     * @brief  Converts the URL, which contains "virtual" URLs for system-items like
     *         "timeline:/lastmonth" into a Query-URL "timeline:/2017-10"
     *         that will be handled by the corresponding IO-slave.
     *         Virtual URLs for bookmarks are used to be independent from
     *         internal format changes.
     * @param an url
     * @return the converted URL, which can be handled by an ioslave
     * @since 5.41
     */
    static QUrl convertedUrl(const QUrl &url);

    /**
     * Set the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported. Examples for
     * schemes are @c "file" or @c "ftp".
     *
     * @sa QFileDialog::setSupportedSchemes
     * @since 5.43
     */
    void setSupportedSchemes(const QStringList &schemes);

    /**
     * Returns the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported.
     *
     * @sa QFileDialog::supportedSchemes
     * @since 5.43
     */
    QStringList supportedSchemes() const;

Q_SIGNALS:
    /**
     * @p message An error message explaining what went wrong.
     */
    void errorMessage(const QString &message);

    /**
     * Emitted after the Solid setup ends.
     * @param success Whether the Solid setup has been successful.
     * @see requestSetup()
     */
    void setupDone(const QModelIndex &index, bool success);

    /**
     * Emitted whenever the visibility of the group @p group changes.
     * @param hidden The new visibility of the group.
     * @see setGroupHidden()
     * @since 5.42
     */
    void groupHiddenChanged(KFilePlacesModel::GroupType group, bool hidden);

    /**
     * Called once the model has been reloaded
     *
     * @since 5.71
     */
    void reloaded();

private:
    Q_PRIVATE_SLOT(d, void _k_initDeviceList())
    Q_PRIVATE_SLOT(d, void _k_deviceAdded(const QString &))
    Q_PRIVATE_SLOT(d, void _k_deviceRemoved(const QString &))
    Q_PRIVATE_SLOT(d, void _k_itemChanged(const QString &))
    Q_PRIVATE_SLOT(d, void _k_reloadBookmarks())
    Q_PRIVATE_SLOT(d, void _k_storageSetupDone(Solid::ErrorType, const QVariant &))
    Q_PRIVATE_SLOT(d, void _k_storageTeardownDone(Solid::ErrorType, const QVariant &))

    class Private;
    Private *const d;
    friend class Private;
};

#endif
