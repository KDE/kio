/*  This file is part of the KDE project
    Copyright (C) 2007 Kevin Ottens <ervin@kde.org>

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

#ifndef KFILEPLACESITEM_P_H
#define KFILEPLACESITEM_P_H

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QModelIndex>
#include <QtCore/QStringList>
#include <QUrl>
#include <kbookmark.h>
#include <solid/device.h>

class KDirLister;
namespace Solid
{
class StorageAccess;
class StorageVolume;
class StorageDrive;
class OpticalDisc;
class PortableMediaPlayer;
}

class KFilePlacesItem : public QObject
{
    Q_OBJECT
public:
    enum GroupType
    {
        PlacesType = 0,
        RecentlySavedType = 1,
        SearchForType = 2,
        DevicesType = 3,
        RemovableDevicesType = 4
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
    GroupType groupType() const;

    static KBookmark createBookmark(KBookmarkManager *manager,
                                    const QString &label,
                                    const QUrl &url,
                                    const QString &iconName,
                                    KFilePlacesItem *after = nullptr);
    static KBookmark createSystemBookmark(KBookmarkManager *manager,
                                          const QString &untranslatedLabel,
                                          const QString &translatedLabel,
                                          const QUrl &url,
                                          const QString &iconName);
    static KBookmark createDeviceBookmark(KBookmarkManager *manager,
                                          const QString &udi);

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
    QPointer<Solid::PortableMediaPlayer> m_mtp;
    QString m_iconPath;
    QStringList m_emblems;
    QString m_groupName;
};

#endif
