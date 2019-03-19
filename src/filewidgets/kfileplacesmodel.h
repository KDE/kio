/*  This file is part of the KDE project
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2007 David Faure <faure@kde.org>

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
#ifndef KFILEPLACESMODEL_H
#define KFILEPLACESMODEL_H

#include "kiofilewidgets_export.h"

#include <QAbstractItemModel>
#include <QUrl>
#include <kbookmark.h>

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
    enum AdditionalRoles {
        UrlRole = 0x069CD12B,
        HiddenRole = 0x0741CAAC,
        SetupNeededRole = 0x059A935D,
        FixedDeviceRole = 0x332896C1,
        CapacityBarRecommendedRole = 0x1548C5C4,
        GroupRole = 0x0a5b64ee,
        /// @since 5.41
        IconNameRole = 0x00a45c00,
        GroupHiddenRole = 0x21a4b936
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
        /// @since 5.54
        TagsType
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

    QUrl url(const QModelIndex &index) const;
    bool setupNeeded(const QModelIndex &index) const;
    QIcon icon(const QModelIndex &index) const;
    QString text(const QModelIndex &index) const;
    bool isHidden(const QModelIndex &index) const;
    /// @since 5.42
    bool isGroupHidden(const GroupType type) const;
    /// @since 5.42
    bool isGroupHidden(const QModelIndex &index) const;
    bool isDevice(const QModelIndex &index) const;
    Solid::Device deviceForIndex(const QModelIndex &index) const;
    KBookmark bookmarkForIndex(const QModelIndex &index) const;
    /// @since 5.42
    GroupType groupType(const QModelIndex &index) const;
    QModelIndexList groupIndexes(const GroupType type) const;

    QAction *teardownActionForIndex(const QModelIndex &index) const;
    QAction *ejectActionForIndex(const QModelIndex &index) const;
    void requestTeardown(const QModelIndex &index);
    void requestEject(const QModelIndex &index);
    void requestSetup(const QModelIndex &index);

    void addPlace(const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());
    void addPlace(const QString &text, const QUrl &url, const QString &iconName, const QString &appName, const QModelIndex &after);
    void editPlace(const QModelIndex &index, const QString &text, const QUrl &url, const QString &iconName = QString(), const QString &appName = QString());
    void removePlace(const QModelIndex &index) const;
    void setPlaceHidden(const QModelIndex &index, bool hidden);
    /// @since 5.42
    void setGroupHidden(const GroupType type, bool hidden);

    /**
     * @brief Move place at @p itemRow to a position before @p row
     * @since 5.41
     */
    bool movePlace(int itemRow, int row);

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
    void errorMessage(const QString &message);
    void setupDone(const QModelIndex &index, bool success);
    void groupHiddenChanged(KFilePlacesModel::GroupType group, bool hidden);

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
