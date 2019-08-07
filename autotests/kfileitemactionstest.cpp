/* This file is part of the KDE project
   Copyright (C) 2014 Frank Reininghaus <frank78ac@googlemail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kfileitemactionstest.h"

#include <kfileitemlistproperties.h>
#include <kfileitemactions.h>

#include <QMenu>
#include <QTest>
#include <QStandardPaths>

/**
 * In KDE 4.x, calling KFileItemActions::setParentWidget(QWidget *widget) would
 * result in 'widget' not only being the parent of any dialogs created by,
 * KFileItemActions, but also of the actions. Nevertheless, the destructor of
 * KFileItemActions deleted all actions it created. This could lead to the deletion
 * of dangling pointers, and thus, a crash, if 'widget' was destroyed before the
 * KFileItemActions instance.
 */
void KFileItemActionsTest::testSetParentWidget()
{
    KFileItemActions fileItemActions;

    // Create a widget and make it the parent for any dialogs created by fileItemActions.
    QWidget *widget = new QWidget();
    fileItemActions.setParentWidget(widget);

    // Initialize fileItemActions with a KFileItemList that contains only the home URL.
    KFileItemList items;
    const QUrl homeUrl = QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).first());
    const KFileItem item(homeUrl, QStringLiteral("inode/directory"));
    items << item;
    const KFileItemListProperties properties(items);
    fileItemActions.setItemListProperties(properties);

    // Create the "Open With" actions and add them to a menu.
    QMenu menu;
    fileItemActions.addOpenWithActionsTo(&menu);

    // Delete the widget. In KDE 4.x, this would also delete the "Open With" actions
    // because they were children of the widget. We would then get a crash in the
    // destructor of fileItemActions because it tried to delete dangling pointers.
    delete widget;
}

QTEST_MAIN(KFileItemActionsTest)
