/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESITEM_P_H
#define KFILEPLACESITEM_P_H

#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <KBookmark>
#include <solid/device.h>
#include "kfileplacesmodel.h"

class KDirLister;
namespace Solid
{
class StorageAccess;
class StorageVolume;
class StorageDrive;
class NetworkShare;
class OpticalDisc;
class PortableMediaPlayer;
}

class KFilePlacesItem : public QObject
{
    Q_OBJECT
public:
    enum GroupType
    {
        PlacesType,
        RemoteType,
        RecentlySavedType,
        SearchForType,
        DevicesType,
        RemovableDevicesType,
        TagsType,
    };

    KFilePlacesItem(KBookmarkManager *manager,
                    const QString &address,
                    const QString &udi = QString());
    ~KFilePlacesItem();

    QString id() const;

    bool isDevice() const;
    KBookmark bookmark() const;
    void setBookmark(const KBookmark &bookmark);
    Solid::Device device() const;
    QVariant data(int role) const;
    KFilePlacesModel::GroupType groupType() const;
    bool isHidden() const;
    void setHidden(bool hide);

    bool hasSupportedScheme(const QStringList &schemes) const;

    static KBookmark createBookmark(KBookmarkManager *manager,
                                    const QString &label,
                                    const QUrl &url,
                                    const QString &iconName,
                                    KFilePlacesItem *after = nullptr);
    static KBookmark createSystemBookmark(KBookmarkManager *manager,
                                          const char *translationContext,
                                          const QByteArray &untranslatedLabel,
                                          const QUrl &url,
                                          const QString &iconName,
                                          const KBookmark &after = KBookmark());
    static KBookmark createDeviceBookmark(KBookmarkManager *manager,
                                          const QString &udi);
    static KBookmark createTagBookmark(KBookmarkManager *manager,
                                          const QString &tag);

Q_SIGNALS:
    void itemChanged(const QString &id);

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
    QString m_text;
    Solid::Device m_device;
    QPointer<Solid::StorageAccess> m_access;
    QPointer<Solid::StorageVolume> m_volume;
    QPointer<Solid::StorageDrive> m_drive;
    QPointer<Solid::OpticalDisc> m_disc;
    QPointer<Solid::PortableMediaPlayer> m_player;
    QPointer<Solid::NetworkShare> m_networkShare;
    QString m_iconPath;
    QStringList m_emblems;
    QString m_groupName;
};

#endif
