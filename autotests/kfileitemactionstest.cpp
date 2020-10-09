/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
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
    const QUrl homeUrl = QUrl::fromLocalFile(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).constFirst());
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
