/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>

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
class KBookmarkManager;

class QMimeData;
class QAction;

/*!
 * \class KFilePlacesModel
 * \inmodule KIOFileWidgets
 *
 * This class is a list view model. Each entry represents a "place"
 * where user can access files. Only relevant when
 * used with QListView or QTableView.
 *
 * \reentrant
 */
class KIOFILEWIDGETS_EXPORT KFilePlacesModel : public QAbstractItemModel
{
    Q_OBJECT

    /*!
     * \property KFilePlacesModel::supportedSchemes
     */
    Q_PROPERTY(QStringList supportedSchemes READ supportedSchemes WRITE setSupportedSchemes NOTIFY supportedSchemesChanged)

public:
    // Note: run   printf "0x%08X\n" $(($RANDOM*$RANDOM))
    // to define additional roles.
    /*!
     * \value UrlRole roleName is "url". See url()
     * \value HiddenRole roleName is "isHidden". See isHidden()
     * \value SetupNeededRole roleName is "isSetupNeeded". See setupNeeded()
     * \value FixedDeviceRole Whether the place is a fixed device (neither hotpluggable nor removable). roleName is "isFixedDevice".
     * \value CapacityBarRecommendedRole Whether the place should have its free space displayed in a capacity bar. roleName is "isCapacityBarRecommended".
     * \value[since 5.40] GroupRole The name of the group, for example "Remote" or "Devices". roleName is "group".
     * \value[since 5.41] IconNameRole roleName is "iconName". See icon().
     * \value[since 5.42] GroupHiddenRole roleName is "isGroupHidden".
     * \value[since 5.91] TeardownAllowedRole roleName is "isTeardownAllowed".
     * \value[since 5.94] EjectAllowedRole roleName is "isEjectAllowed".
     * \value[since 5.95] TeardownOverlayRecommendedRole roleName is "isTeardownOverlayRecommended".
     * \value[since 5.99] DeviceAccessibilityRole roleName is "deviceAccessibility".
     */
    enum AdditionalRoles {
        UrlRole = 0x069CD12B,
        HiddenRole = 0x0741CAAC,
        SetupNeededRole = 0x059A935D,
        FixedDeviceRole = 0x332896C1,
        CapacityBarRecommendedRole = 0x1548C5C4,
        GroupRole = 0x0a5b64ee,
        IconNameRole = 0x00a45c00,
        GroupHiddenRole = 0x21a4b936,
        TeardownAllowedRole = 0x02533364,
        EjectAllowedRole = 0x0A16AC5B,
        TeardownOverlayRecommendedRole = 0x032EDCCE,
        DeviceAccessibilityRole = 0x023FFD93,
    };

    /*!
     * Describes the available group types used in this model.
     * \since 5.42
     *
     * \value PlacesType "Places" section
     * \value RemoteType "Remote" section
     * \value RecentlySavedType "Recent" section
     * \value SearchForType "Search for" section
     * \value DevicesType "Devices" section
     * \value RemovableDevicesType "Removable Devices" section
     * \value UnknownType Unknown GroupType
     * \value[since 5.54] TagsType "Tags" section
     */
    enum GroupType {
        PlacesType,
        RemoteType,
        RecentlySavedType,
        SearchForType,
        DevicesType,
        RemovableDevicesType,
        UnknownType,
        TagsType,
    };
    Q_ENUM(GroupType)

    /*!
     * \value SetupNeeded
     * \value SetupInProgress
     * \value Accessible
     * \value TeardownInProgress
     */
    enum DeviceAccessibility {
        SetupNeeded,
        SetupInProgress,
        Accessible,
        TeardownInProgress
    };
    Q_ENUM(DeviceAccessibility)

    /*!
     *
     */
    explicit KFilePlacesModel(QObject *parent = nullptr);
    ~KFilePlacesModel() override;

    /*!
     * Returns The URL of the place at index \a index.
     */
    Q_INVOKABLE QUrl url(const QModelIndex &index) const;

    /*!
     * Returns Whether the place at index \a index needs to be mounted before it can be used.
     */
    Q_INVOKABLE bool setupNeeded(const QModelIndex &index) const;

    /*!
     * Returns Whether the place is a device that can be unmounted, e.g. it is
     * mounted but does not point at system Root or the user's Home directory.
     *
     * It does not indicate whether the teardown can succeed.
     * \since 5.91
     */
    Q_INVOKABLE bool isTeardownAllowed(const QModelIndex &index) const;

    /*!
     * Returns Whether the place is a device that can be ejected, e.g. it is
     * a CD, DVD, etc.
     *
     * It does not indicate whether the eject can succeed.
     * \since 5.94
     */
    Q_INVOKABLE bool isEjectAllowed(const QModelIndex &index) const;

    /*!
     * Returns Whether showing an inline teardown button is recommended,
     * e.g. when it is a removable drive.
     *
     * \since 5.95
     **/
    Q_INVOKABLE bool isTeardownOverlayRecommended(const QModelIndex &index) const;

    /*!
     * Returns Whether this device is currently accessible or being (un)mounted.
     *
     * \since 5.99
     */
    Q_INVOKABLE KFilePlacesModel::DeviceAccessibility deviceAccessibility(const QModelIndex &index) const;

    /*!
     * Returns The icon of the place at index \a index.
     */
    Q_INVOKABLE QIcon icon(const QModelIndex &index) const;

    /*!
     * Returns The user-visible text of the place at index \a index.
     */
    Q_INVOKABLE QString text(const QModelIndex &index) const;

    /*!
     * Returns Whether the place at index \a index is hidden or is inside an hidden group.
     */
    Q_INVOKABLE bool isHidden(const QModelIndex &index) const;

    /*!
     * Returns Whether the group type \a type is hidden.
     * \since 5.42
     */
    Q_INVOKABLE bool isGroupHidden(const GroupType type) const;

    /*!
     * Returns Whether the group of the place at index \a index is hidden.
     * \since 5.42
     */
    Q_INVOKABLE bool isGroupHidden(const QModelIndex &index) const;

    /*!
     * Returns Whether the place at index \a index is a device handled by Solid.
     * \sa deviceForIndex()
     */
    Q_INVOKABLE bool isDevice(const QModelIndex &index) const;

    /*!
     * Returns The solid device of the place at index \a index, if it is a device. Otherwise a default Solid::Device() instance is returned.
     * \sa isDevice()
     */
    Solid::Device deviceForIndex(const QModelIndex &index) const;

    /*!
     * Returns The KBookmark instance of the place at index \a index.
     * If the index is not valid, a default KBookmark instance is returned.
     */
    KBookmark bookmarkForIndex(const QModelIndex &index) const;

    /*!
     * Returns The KBookmark instance of the place with url \a searchUrl.
     * If the bookmark corresponding to searchUrl is not found, a default KBookmark instance is returned.
     * \since 5.63
     */
    KBookmark bookmarkForUrl(const QUrl &searchUrl) const;

    /*!
     * Returns The group type of the place at index \a index.
     * \since 5.42
     */
    Q_INVOKABLE GroupType groupType(const QModelIndex &index) const;

    /*!
     * Returns The list of model indexes that have @ type as their group type.
     * \sa groupType()
     * \since 5.42
     */
    Q_INVOKABLE QModelIndexList groupIndexes(const GroupType type) const;

    /*!
     * Returns A QAction with a proper translated label that can be used to trigger the requestTeardown()
     * method for the place at index \a index.
     * \sa requestTeardown()
     */
    Q_INVOKABLE QAction *teardownActionForIndex(const QModelIndex &index) const;

    /*!
     * Returns A QAction with a proper translated label that can be used to trigger the requestEject()
     * method for the place at index \a index.
     * \sa requestEject()
     */
    Q_INVOKABLE QAction *ejectActionForIndex(const QModelIndex &index) const;

    /*!
     * Returns A QAction with a proper translated label that can be used to open a partitioning menu for the device. nullptr if not a device.
     */
    Q_INVOKABLE QAction *partitionActionForIndex(const QModelIndex &index) const;

    /*!
     * Unmounts the place at index \a index by triggering the teardown functionality of its Solid device.
     * \sa deviceForIndex()
     */
    Q_INVOKABLE void requestTeardown(const QModelIndex &index);

    /*!
     * Ejects the place at index \a index by triggering the eject functionality of its Solid device.
     * \sa deviceForIndex()
     */
    Q_INVOKABLE void requestEject(const QModelIndex &index);

    /*!
     * Mounts the place at index \a index by triggering the setup functionality of its Solid device.
     * \sa deviceForIndex()
     */
    Q_INVOKABLE void requestSetup(const QModelIndex &index);

    /*!
     * Adds a new place to the model.
     *
     * \a text The user-visible text for the place
     *
     * \a url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     *
     * \a iconName The icon of the place
     *
     * \a appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     */
    Q_INVOKABLE void addPlace(const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /*!
     * Adds a new place to the model.
     * \a text The user-visible text for the place
     *
     * \a url The URL of the place. It will be stored in its QUrl::FullyEncoded string format.
     *
     * \a iconName The icon of the place
     *
     * \a appName If set as the value of QCoreApplication::applicationName(), will make the place visible only in this application.
     *
     * \a after The index after which the new place will be added.
     */
    Q_INVOKABLE void addPlace(const QString &text, const QUrl &url, const QString &iconName, const QString &appName, const QModelIndex &after);

    /*!
     * Edits the place with index \a index.
     *
     * \a text The new user-visible text for the place
     *
     * \a url The new URL of the place
     *
     * \a iconName The new icon of the place
     *
     * \a appName The new application-local filter for the place (see addPlace()).
     */
    Q_INVOKABLE void
    editPlace(const QModelIndex &index, const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());

    /*!
     * Deletes the place with index \a index from the model.
     */
    Q_INVOKABLE void removePlace(const QModelIndex &index) const;

    /*!
     * Changes the visibility of the place with index \a index, but only if the place is not inside an hidden group.
     *
     * \a hidden Whether the place should be hidden or visible.
     * \sa isGroupHidden()
     */
    Q_INVOKABLE void setPlaceHidden(const QModelIndex &index, bool hidden);

    /*!
     * Changes the visibility of the group with type \a type.
     *
     * \a hidden Whether the group should be hidden or visible.
     * \sa isGroupHidden()
     * \since 5.42
     */
    Q_INVOKABLE void setGroupHidden(const GroupType type, bool hidden);

    /*!
     * Move place at \a itemRow to a position before \a row
     *
     * Returns whether the place has been moved.
     * \since 5.41
     */
    Q_INVOKABLE bool movePlace(int itemRow, int row);

    /*!
     * Returns the number of hidden places in the model.
     * \sa isHidden()
     */
    Q_INVOKABLE int hiddenCount() const;

    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &child) const override;

    QHash<int, QByteArray> roleNames() const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    /*!
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

    /*!
     * Reload bookmark information
     * \since 5.41
     */
    Q_INVOKABLE void refresh() const;

    /*!
     * Converts the URL, which contains "virtual" URLs for system-items like
     *         "timeline:/lastmonth" into a Query-URL "timeline:/2017-10"
     *         that will be handled by the corresponding KIO worker.
     *         Virtual URLs for bookmarks are used to be independent from
     *         internal format changes.
     *
     * \a an url
     *
     * Returns the converted URL, which can be handled by a KIO worker
     * \since 5.41
     */
    static QUrl convertedUrl(const QUrl &url);

    /*!
     * Set the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported. Examples for
     * schemes are "file" or "ftp".
     *
     * \sa QFileDialog::setSupportedSchemes
     * \since 5.43
     */
    void setSupportedSchemes(const QStringList &schemes);

    /*!
     * Returns the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported.
     *
     * \sa QFileDialog::supportedSchemes
     * \since 5.43
     */
    QStringList supportedSchemes() const;

Q_SIGNALS:
    /*!
     * \a message An error message explaining what went wrong.
     */
    void errorMessage(const QString &message);

    /*!
     * Emitted after the Solid setup ends.
     *
     * \a success Whether the Solid setup has been successful.
     * \sa requestSetup()
     */
    void setupDone(const QModelIndex &index, bool success);

    /*!
     * Emitted after the teardown of a device ends.
     *
     * \note In case of an error, the errorMessage() signal
     * will also be emitted with a message describing the error.
     *
     * \a error Type of error that occurred, if any.
     *
     * \a errorData More information about the error, if any.
     *
     * \since 5.100
     */
    void teardownDone(const QModelIndex &index, Solid::ErrorType error, const QVariant &errorData);

    /*!
     * Emitted whenever the visibility of the group \a group changes.
     *
     * \a hidden The new visibility of the group.
     *
     * \sa setGroupHidden()
     * \since 5.42
     */
    void groupHiddenChanged(KFilePlacesModel::GroupType group, bool hidden);

    /*!
     * Called once the model has been reloaded
     *
     * \since 5.71
     */
    void reloaded();

    /*!
     * Emitted whenever the list of supported schemes has been changed
     *
     * \since 5.94
     */
    void supportedSchemesChanged();

private:
    friend class KFilePlacesModelPrivate;
    std::unique_ptr<KFilePlacesModelPrivate> d;
};

#endif
