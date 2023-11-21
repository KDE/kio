/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2007 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2023 Harald Sitter <sitter@kde.org>
    SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KFILEPLACESMODEL_P_H
#define KFILEPLACESMODEL_P_H

#include <solid/predicate.h>
#include <solid/solidnamespace.h>

#include <QList>
#include <QMap>
#include <QPersistentModelIndex>
#include <QStringList>

class KBookmarkManager;
class KCoreDirLister;
class KFilePlacesItem;
class KFilePlacesModel;

class QUrl;

namespace Solid
{
class StorageAccess;
}

class KFilePlacesModelPrivate
{
public:
    explicit KFilePlacesModelPrivate(KFilePlacesModel *qq);

    KFilePlacesModel *const q;

    static QString ignoreMimeType();
    static QString internalMimeType(const KFilePlacesModel *model);

    QList<KFilePlacesItem *> items;
    QList<Solid::Device> availableDevices;
    QMap<QObject *, QPersistentModelIndex> setupInProgress;
    QMap<QObject *, QPersistentModelIndex> teardownInProgress;
    QStringList supportedSchemes;

    Solid::Predicate predicate;
    KBookmarkManager *bookmarkManager;

    const bool fileIndexingEnabled;

    void reloadAndSignal();
    QList<KFilePlacesItem *> loadBookmarkList();
    int findNearestPosition(int source, int target);

    QList<QString> tags;
    const QString tagsUrlBase = QStringLiteral("tags:/");
    KCoreDirLister *tagsLister = nullptr;

    void initDeviceList();
    void deviceAdded(const QString &udi);
    void deviceRemoved(const QString &udi);
    void itemChanged(const QString &udi, const QList<int> &roles);
    void reloadBookmarks();
    void storageSetupDone(Solid::ErrorType error, const QVariant &errorData, Solid::StorageAccess *sender);
    void storageTeardownDone(const QString &filePath, Solid::ErrorType error, const QVariant &errorData, QObject *sender);

private:
    bool isBalooUrl(const QUrl &url) const;
};

#endif // KFILEPLACESMODEL_P_H
