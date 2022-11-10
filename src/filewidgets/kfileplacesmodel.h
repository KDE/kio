/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESMODEL_H
#define KFILEPLACESMODEL_H

#include "kiofilewidgets_export.h"

#include <KBookmark>
#include <QAbstractItemModel>
#include <QUrl>

#include <solid/device.h>
#include <solid/solidnamespace.h>

#include <memory>

class KFilePlacesModelPrivate;

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

    Q_PROPERTY(QStringList supportedSchemes READ supportedSchemes WRITE setSupportedSchemes NOTIFY supportedSchemesChanged)

public:
    // Note: run   printf "0x%08X\n" $(($RANDOM*$RANDOM))
    // to define additional roles.
    enum AdditionalRoles {
        /** roleName is "url". @see url() */
        UrlRole = 0x069CD12B,

        /** roleName is "isHidden". @see isHidden() */
        HiddenRole = 0x0741CAAC,

        /** roleName is "isSetupNeeded". @see setupNeeded() */
        SetupNeededRole = 0x059A935D,

        /**
         * Whether the place is a fixed device (neither hotpluggable nor removable).
         * roleName is "isFixedDevice".
         */
        FixedDeviceRole = 0x332896C1,

        /**
         * Whether the place should have its free space displayed in a capacity bar.
         * roleName is "isCapacityBarRecommended".
         */
        CapacityBarRecommendedRole = 0x1548C5C4,

        /**
         * The name of the group, for example "Remote" or "Devices". roleName is "group".
         * @since 5.40
         */
        GroupRole = 0x0a5b64ee,

        /**
         * roleName is "iconName".
         * @see icon()
         * @since 5.41
         */
        IconNameRole = 0x00a45c00,

        /** roleName is "isGroupHidden".
         * @see isGroupHidden()
         * @since 5.42
         */
        GroupHiddenRole = 0x21a4b936,

        /** roleName is "isTeardownAllowed".
         * @see isTeardownAllowed().
         * @since 5.91
         */
        TeardownAllowedRole = 0x02533364,

        /** roleName is "isEjectAllowed".
         * @since 5.94.
         */
        EjectAllowedRole = 0x0A16AC5B,

        /**
         * roleName is "isTeardownOverlayRecommended".
         * @see isTeardownOverlayRecommended()
         * @since 5.95
         */
        TeardownOverlayRecommendedRole = 0x032EDCCE,

        /**
         * roleName is "deviceAccessibility".
         * @see deviceAccessibility()
         * @since 5.99
         */
        DeviceAccessibilityRole = 0x023FFD93,
    };

    /**
     * Describes the available group types used in this model.
     * @since 5.42
     */
    enum GroupType {
        PlacesType, ///< "Places" section
        RemoteType, ///< "Remote" section
        RecentlySavedType, ///< "Recent" section
        SearchForType, ///< "Search for" section
        DevicesType, ///< "Devices" section
        RemovableDevicesType, ///< "Removable Devices" section
        UnknownType, ///< Unknown GroupType
        TagsType, ///< "Tags" section. @since 5.54
    };
    Q_ENUM(GroupType)

    enum DeviceAccessibility { SetupNeeded, SetupInProgress, Accessible, TeardownInProgress };
    Q_ENUM(DeviceAccessibility)

    explicit KFilePlacesModel(QObject *parent = nullptr);
    /**
     * @brief Construct a new KFilePlacesModel with an alternativeApplicationName
     * @param alternativeApplicationName This value will be used to filter bookmarks in addition to the actual application name
     * @param parent Parent object
     * @since 5.43
     */
    // TODO KF6: merge constructors
    KFilePlacesModel(const QString &alternativeApplicationName, QObject *parent = nullptr);
    ~KFilePlacesModel() override;

    /**
     * @return The URL of the place at index @p index.
     */
    Q_INVOKABLE QUrl url(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index needs to be mounted before it can be used.
     */
    Q_INVOKABLE bool setupNeeded(const QModelIndex &index) const;

    /**
     * @return Whether the place is a device that can be unmounted, e.g. it is
     * mounted but does not point at system Root or the user's Home directory.
     *
     * It does not indicate whether the teardown can succeed.
     * @since 5.91
     */
    Q_INVOKABLE bool isTeardownAllowed(const QModelIndex &index) const;

    /**
     * @return Whether the place is a device that can be ejected, e.g. it is
     * a CD, DVD, etc.
     *
     * It does not indicate whether the eject can succeed.
     * @since 5.94
     */
    Q_INVOKABLE bool isEjectAllowed(const QModelIndex &index) const;

    /**
     * @return Whether showing an inline teardown button is recommended,
     * e.g. when it is a removable drive.
     *
     * @since 5.95
     **/
    Q_INVOKABLE bool isTeardownOverlayRecommended(const QModelIndex &index) const;

    /**
     * @return Whether this device is currently accessible or being (un)mounted.
     *
     * @since 5.99
     */
    Q_INVOKABLE KFilePlacesModel::DeviceAccessibility deviceAccessibility(const QModelIndex &index) const;

    /**
     * @return The icon of the place at index @p index.
     */
    Q_INVOKABLE QIcon icon(const QModelIndex &index) const;

    /**
     * @return The user-visible text of the place at index @p index.
     */
    Q_INVOKABLE QString text(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index is hidden or is inside an hidden group.
     */
    Q_INVOKABLE bool isHidden(const QModelIndex &index) const;

    /**
     * @return Whether the group type @p type is hidden.
     * @since 5.42
     */
    Q_INVOKABLE bool isGroupHidden(const GroupType type) const;

    /**
     * @return Whether the group of the place at index @p index is hidden.
     * @since 5.42
     */
    Q_INVOKABLE bool isGroupHidden(const QModelIndex &index) const;

    /**
     * @return Whether the place at index @p index is a device handled by Solid.
     * @see deviceForIndex()
     */
    Q_INVOKABLE bool isDevice(const QModelIndex &index) const;

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
    Q_INVOKABLE GroupType groupType(const QModelIndex &index) const;

    /**
     * @return The list of model indexes that have @ type as their group type.
     * @see groupType()
     * @since 5.42
     */
    Q_INVOKABLE QModelIndexList groupIndexes(const GroupType type) const;

    /**
     * @return A QAction with a proper translated label that can be used to trigger the requestTeardown()
     * method for the place at index @p index.
     * @see requestTeardown()
     */
    Q_INVOKABLE QAction *teardownActionForIndex(const QModelIndex &index) const;

    /**
     * @return A QAction with a proper translated label that can be used to trigger the requestEject()
     * method for the place at index @p index.
     * @see requestEject()
     */
    Q_INVOKABLE QAction *ejectActionForIndex(const QModelIndex &index) const;

    /**
     * Unmounts the place at index @p index by triggering the teardown functionality of its Solid device.
     * @see deviceForIndex()
     */
    Q_INVOKABLE void requestTeardown(const QModelIndex &index);

    /**
     * Ejects the place at index @p index by triggering the eject functionality of its Solid device.
     * @see deviceForIndex()
     */
    Q_INVOKABLE void requestEject(const QModelIndex &index);

    /**
     * Mounts the place at index @p index by triggering the setup functionality of its Solid device.
     * @see deviceForIndex()
     */
    Q_INVOKABLE void requestSetup(const QModelIndex &index);

    /**
     * Adds a new place to the model.
     * @param text The user-visible text for the place
     * @param url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     * @param iconName The icon of the place
     * @param appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     */
    Q_INVOKABLE void addPlace(const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /**
     * Adds a new place to the model.
     * @param text The user-visible text for the place
     * @param url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     * @param iconName The icon of the place
     * @param appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     * @param after The index after which the new place will be added.
     */
    Q_INVOKABLE void addPlace(const QString &text, const QUrl &url, const QString &iconName, const QString &appName, const QModelIndex &after);

    /**
     * Edits the place with index @p index.
     * @param text The new user-visible text for the place
     * @param url The new URL of the place
     * @param iconName The new icon of the place
     * @param appName The new application-local filter for the place (@see addPlace()).
     */
    Q_INVOKABLE void
    editPlace(const QModelIndex &index, const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /**
     * Deletes the place with index @p index from the model.
     */
    Q_INVOKABLE void removePlace(const QModelIndex &index) const;

    /**
     * Changes the visibility of the place with index @p index, but only if the place is not inside an hidden group.
     * @param hidden Whether the place should be hidden or visible.
     * @see isGroupHidden()
     */
    Q_INVOKABLE void setPlaceHidden(const QModelIndex &index, bool hidden);

    /**
     * Changes the visibility of the group with type @p type.
     * @param hidden Whether the group should be hidden or visible.
     * @see isGroupHidden()
     * @since 5.42
     */
    Q_INVOKABLE void setGroupHidden(const GroupType type, bool hidden);

    /**
     * @brief Move place at @p itemRow to a position before @p row
     * @return Whether the place has been moved.
     * @since 5.41
     */
    Q_INVOKABLE bool movePlace(int itemRow, int row);

    /**
     * @return The number of hidden places in the model.
     * @see isHidden()
     */
    Q_INVOKABLE int hiddenCount() const;

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

    /// Reimplemented from QAbstractItemModel.
    /// @see AdditionalRoles
    QHash<int, QByteArray> roleNames() const override;

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
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    /**
     * @brief Reload bookmark information
     * @since 5.41
     */
    Q_INVOKABLE void refresh() const;

    /**
     * @brief  Converts the URL, which contains "virtual" URLs for system-items like
     *         "timeline:/lastmonth" into a Query-URL "timeline:/2017-10"
     *         that will be handled by the corresponding KIO worker.
     *         Virtual URLs for bookmarks are used to be independent from
     *         internal format changes.
     * @param an url
     * @return the converted URL, which can be handled by a KIO worker
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
     * Emitted after the teardown of a device ends.
     *
     * @note In case of an error, the @p errorMessage signal
     * will also be emitted with a message describing the error.
     *
     * @param error Type of error that occurred, if any.
     * @param errorData More information about the error, if any.
     * @since 5.100
     */
    void teardownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData);

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

    /**
     * Emitted whenever the list of supported schemes has been changed
     *
     * @since 5.94
     */
    void supportedSchemesChanged();

private:
    friend class KFilePlacesModelPrivate;
    std::unique_ptr<KFilePlacesModelPrivate> d;
};

#endif
