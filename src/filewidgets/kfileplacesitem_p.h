/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESITEM_P_H
#define KFILEPLACESITEM_P_H

#include "kfileplacesmodel.h"
#include <KBookmark>
#include <KBookmarkManager>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <solid/device.h>

class KDirLister;
namespace Solid
{
class Block;
class StorageAccess;
class StorageVolume;
class StorageDrive;
class NetworkShare;
class OpticalDrive;
class OpticalDisc;
class PortableMediaPlayer;
}

class KFilePlacesItem : public QObject
{
    Q_OBJECT
public:
    KFilePlacesItem(KBookmarkManager *manager, const QString &address, const QString &udi, KFilePlacesModel *parent);
    ~KFilePlacesItem() override;

    QString id() const;

    bool isDevice() const;
    KFilePlacesModel::DeviceAccessibility deviceAccessibility() const;
    bool isTeardownAllowed() const;
    bool isTeardownOverlayRecommended() const;
    bool isEjectAllowed() const;
    KBookmark bookmark() const;
    void setBookmark(const KBookmark &bookmark);
    Solid::Device device() const;
    QVariant data(int role) const;
    KFilePlacesModel::GroupType groupType() const;
    bool isHidden() const;
    void setHidden(bool hide);

    bool hasSupportedScheme(const QStringList &schemes) const;

    static KBookmark
    createBookmark(KBookmarkManager *manager, const QString &label, const QUrl &url, const QString &iconName, KFilePlacesItem *after = nullptr);
    /*!
     * @param untranslatedLabel text for label. If to be translated, should be set by kli18nc("KFile System Bookmarks", "Label text").untranslatedText().
     */
    static KBookmark createSystemBookmark(KBookmarkManager *manager,
                                          const char *untranslatedLabel,
                                          const QUrl &url,
                                          const QString &iconName,
                                          const KBookmark &after = KBookmark());
    static KBookmark createDeviceBookmark(KBookmarkManager *manager, const Solid::Device &device);
    static KBookmark createTagBookmark(KBookmarkManager *manager, const QString &tag);

Q_SIGNALS:
    void itemChanged(const QString &id, const QList<int> &roles = {});

private Q_SLOTS:
    void onAccessibilityChanged(bool);

private:
    QVariant bookmarkData(int role) const;
    QVariant deviceData(int role) const;

    QString iconNameForBookmark(const KBookmark &bookmark) const;

    static QString generateNewId();
    bool updateDeviceInfo(const QString &udi);

    KBookmarkManager *m_manager;
    KBookmark m_bookmark;
    bool m_folderIsEmpty;
    bool m_isCdrom;
    bool m_isAccessible;
    bool m_isTeardownAllowed;
    bool m_isTeardownOverlayRecommended;
    bool m_isTeardownInProgress;
    bool m_isSetupInProgress;
    bool m_isEjectInProgress;
    bool m_isReadOnly;
    QString m_text;
    Solid::Device m_device;
    QPointer<Solid::StorageAccess> m_access;
    QPointer<Solid::StorageVolume> m_volume;
    QPointer<Solid::StorageDrive> m_drive;
    QPointer<Solid::Block> m_block;
    QPointer<Solid::OpticalDrive> m_opticalDrive;
    QPointer<Solid::OpticalDisc> m_disc;
    QPointer<Solid::PortableMediaPlayer> m_player;
    QPointer<Solid::NetworkShare> m_networkShare;
    QString m_deviceIconName;
    QStringList m_emblems;
    QString m_backingFile;
    KFilePlacesModel::GroupType m_groupType = KFilePlacesModel::UnknownType;
    QString m_groupName;
    mutable QString m_deviceDisplayName;
};

#endif
