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
class OpticalDisc;
class PortableMediaPlayer;
}

class KFilePlacesItem : public QObject
{
    Q_OBJECT
public:
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

    static KBookmark createBookmark(KBookmarkManager *manager,
                                    const QString &label,
                                    const QUrl &url,
                                    const QString &iconName,
                                    KFilePlacesItem *after = 0);
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
    void onListerCompleted();

private:
    QVariant bookmarkData(int role) const;
    QVariant deviceData(int role) const;

    bool hasFullIcon(const KBookmark &bookmark) const;
    QString iconNameForBookmark(const KBookmark &bookmark) const;

    static QString generateNewId();

    KBookmarkManager *m_manager;
    KBookmark m_bookmark;
    KDirLister *m_lister;
    bool m_folderIsEmpty;
    bool m_isCdrom;
    bool m_isAccessible;
    QString m_text;
    mutable Solid::Device m_device;
    mutable QPointer<Solid::StorageAccess> m_access;
    mutable QPointer<Solid::StorageVolume> m_volume;
    mutable QPointer<Solid::OpticalDisc> m_disc;
    mutable QPointer<Solid::PortableMediaPlayer> m_mtp;
    QString m_iconPath;
    QStringList m_emblems;
};

#endif
