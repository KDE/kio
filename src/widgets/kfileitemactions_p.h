/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KFILEITEMACTIONS_P_H
#define KFILEITEMACTIONS_P_H

#include <kfileitemlistproperties.h>
#include <kfileitem.h>
#include <KServiceAction>
#include <KService>
#include <KConfig>

#include <QActionGroup>
#include <QObject>

class KFileItemActions;

typedef QList<KServiceAction> ServiceList;

class KFileItemActionsPrivate : public QObject
{
    Q_OBJECT
    friend class KFileItemActions;
public:
    explicit KFileItemActionsPrivate(KFileItemActions *qq);
    ~KFileItemActionsPrivate();

    int insertServicesSubmenus(const QMap<QString, ServiceList> &list, QMenu *menu, bool isBuiltin);
    int insertServices(const ServiceList &list, QMenu *menu, bool isBuiltin);

    // For "open with"
    KService::List associatedApplications(const QString &traderConstraint);
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
    QStringList listPreferredServiceIds(const QStringList &mimeTypeList, const QString &traderConstraint);

    QPair<int, QMenu *> addServiceActionsTo(QMenu *mainMenu, const QList<QAction *> &additionalActions, const QStringList &excludeList);
    int addPluginActionsTo(QMenu *mainMenu, QMenu *actionsMenu, const QStringList &excludeList);

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
    QString m_traderConstraint;
    KFileItemList m_fileOpenList;
    QActionGroup m_executeServiceActionGroup;
    QActionGroup m_runApplicationActionGroup;
    QWidget *m_parentWidget;
    KConfig m_config;
};

Q_DECLARE_METATYPE(KService::Ptr)
Q_DECLARE_METATYPE(KServiceAction)

#endif /* KFILEITEMACTIONS_P_H */

