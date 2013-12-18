/* This file is part of the KDE project
   Copyright (C) 1998-2009 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2 of the License or
   ( at your option ) version 3 or, at the discretion of KDE e.V.
   ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef KFILEITEMACTIONS_P_H
#define KFILEITEMACTIONS_P_H

#include <kfileitemlistproperties.h>
#include <kfileitem.h>
#include <kserviceaction.h>
#include <kservice.h>
#include <QActionGroup>
#include <QObject>

class KFileItemActions;

typedef QList<KServiceAction> ServiceList;

class KFileItemActionsPrivate : public QObject
{
    Q_OBJECT
public:
    KFileItemActionsPrivate(KFileItemActions *qq);
    ~KFileItemActionsPrivate();

    int insertServicesSubmenus(const QMap<QString, ServiceList>& list, QMenu* menu, bool isBuiltin);
    int insertServices(const ServiceList& list, QMenu* menu, bool isBuiltin);

    // For "open with"
    KService::List associatedApplications(const QString& traderConstraint);
    QAction* createAppAction(const KService::Ptr& service, bool singleOffer);

    struct ServiceRank
    {
        int score;
        KService::Ptr service;
    };

    // Inline function for sorting lists of ServiceRank
    static bool lessRank(const ServiceRank& id1, const ServiceRank& id2)
    {
        return id1.score < id2.score;
    }

    QStringList listMimeTypes(const KFileItemList& items);
    QStringList listPreferredServiceIds(const QStringList& mimeTypeList, const QString& traderConstraint);

public Q_SLOTS:
    void slotRunPreferredApplications();

private:
    void openWithByMime(const KFileItemList& fileItems);

private Q_SLOTS:
    // For servicemenus
    void slotExecuteService(QAction* act);
    // For "open with" applications
    void slotRunApplication(QAction* act);
    void slotOpenWithDialog();

public:
    KFileItemActions* const q;
    KFileItemListProperties m_props;
    QStringList m_mimeTypeList;
    QString m_traderConstraint;
    KFileItemList m_fileOpenList;
    QActionGroup m_executeServiceActionGroup;
    QActionGroup m_runApplicationActionGroup;
    QList<QAction*> m_ownActions;
    QWidget* m_parentWidget;
};

Q_DECLARE_METATYPE(KService::Ptr)
Q_DECLARE_METATYPE(KServiceAction)

#endif /* KFILEITEMACTIONS_P_H */

