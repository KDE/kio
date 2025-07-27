/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILEITEMACTIONS_P_H
#define KFILEITEMACTIONS_P_H

#include "config-kiowidgets.h"
#include "kabstractfileitemactionplugin.h"
#include <KConfig>
#include <KDesktopFileAction>
#include <KService>
#include <kfileitem.h>
#include <kfileitemlistproperties.h>

#include <QActionGroup>
#include <QObject>

class KFileItemActions;

typedef QList<KDesktopFileAction> ServiceList;

class KFileItemActionsPrivate : public QObject
{
    Q_OBJECT
    friend class KFileItemActions;

public:
    explicit KFileItemActionsPrivate(KFileItemActions *qq);
    ~KFileItemActionsPrivate() override;

    int insertServicesSubmenus(const QMap<QString, ServiceList> &list, QMenu *menu);
    int insertServices(const ServiceList &list, QMenu *menu);

    // For "open with"
    KService::List associatedApplications();
    QAction *createAppAction(const KService::Ptr &service, bool singleOffer);

    struct ServiceRank {
        int score;
        KService::Ptr service;
    };

    // Inline function for sorting lists of ServiceRank
    static bool lessRank(const ServiceRank &id1, const ServiceRank &id2)
    {
        return id1.score < id2.score;
    }

    QStringList listMimeTypes(const KFileItemList &items);
    QStringList listPreferredServiceIds(const QStringList &mimeTypeList, const QStringList &excludedDesktopEntryNames);

    struct ServiceActionInfo {
        int userItemCount = 0;
        QMenu *menu = nullptr;
    };
    ServiceActionInfo addServiceActionsTo(QMenu *mainMenu, const QList<QAction *> &additionalActions, const QStringList &excludeList);
    int addPluginActionsTo(QMenu *mainMenu, QMenu *actionsMenu, const QStringList &excludeList);
    void insertOpenWithActionsTo(QAction *before, QMenu *topMenu, const QStringList &excludedDesktopEntryNames);
    static KService::List associatedApplications(const QStringList &mimeTypeList, const QStringList &excludedDesktopEntryNames);

    QStringList serviceMenuFilePaths();

public Q_SLOTS:
    void slotRunPreferredApplications();

private:
    void openWithByMime(const KFileItemList &fileItems);

    // Utility function which returns true if the service menu should be displayed
    bool shouldDisplayServiceMenu(const KConfigGroup &cfg, const QString &protocol) const;
    // Utility functions which returns true if the types for the service are set and the exclude types are not contained
    bool checkTypesMatch(const KConfigGroup &cfg) const;

private Q_SLOTS:
    // For servicemenus
    void slotExecuteService(QAction *act);
    // For "open with" applications
    void slotRunApplication(QAction *act);
    void slotOpenWithDialog();

public:
    KFileItemActions *const q;
    KFileItemListProperties m_props;
    QStringList m_mimeTypeList;
    KFileItemList m_fileOpenList;
    QActionGroup m_executeServiceActionGroup;
    QActionGroup m_runApplicationActionGroup;
    QWidget *m_parentWidget;
    KConfig m_config;
    QHash<QString, KAbstractFileItemActionPlugin *> m_loadedPlugins;
};

Q_DECLARE_METATYPE(KService::Ptr)
Q_DECLARE_METATYPE(KServiceAction)

#endif /* KFILEITEMACTIONS_P_H */
