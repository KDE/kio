/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfileitemactionstest.h"

#include <kfileitemactions.h>
#include <kfileitemlistproperties.h>

#include <QMenu>
#include <QStandardPaths>
#include <QTest>

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
    fileItemActions.addActionsTo(&menu);

    // Delete the widget. In KDE 4.x, this would also delete the "Open With" actions
    // because they were children of the widget. We would then get a crash in the
    // destructor of fileItemActions because it tried to delete dangling pointers.
    delete widget;
}

void KFileItemActionsTest::testTopLevelServiceMenuActions()
{
#ifdef Q_OS_WIN
    QSKIP("Test skipped on Windows");
#endif
    QStandardPaths::setTestModeEnabled(true);

    qputenv("XDG_DATA_DIRS", QFINDTESTDATA("servicemenu_protocol_mime_test_data").toLocal8Bit());
    KFileItemActions actions;

    {
        // only one menu should show up for the inode/directory mime type
        actions.setItemListProperties(KFileItemList({KFileItem(QUrl::fromLocalFile(QFINDTESTDATA("servicemenu_protocol_mime_test_data")))}));
        QMenu menu;
        actions.addActionsTo(&menu, KFileItemActions::MenuActionSource::Services);
        QCOMPARE(menu.actions().count(), 1);
        QCOMPARE(menu.actions().constFirst()->text(), "dir_service_menu");
    }
    {
        // The two actions should show up
        actions.setItemListProperties(KFileItemList({KFileItem(QUrl(QStringLiteral("smb://somefile.txt")))}));
        QMenu menu;
        actions.addActionsTo(&menu, KFileItemActions::MenuActionSource::Services);
        const auto resultingActions = menu.actions();
        QCOMPARE(resultingActions.count(), 2);
        QCOMPARE(resultingActions.at(0)->text(), "no_file");
        QCOMPARE(resultingActions.at(1)->text(), "smb");
    }
    {
        // Only the menu which handles URLs
        actions.setItemListProperties(KFileItemList({KFileItem(QUrl(QStringLiteral("someweirdscheme://somefile.txt")))}));
        QMenu menu;
        actions.addActionsTo(&menu, KFileItemActions::MenuActionSource::Services);
        const auto resultingActions = menu.actions();
        QCOMPARE(resultingActions.count(), 1);
        QCOMPARE(resultingActions.at(0)->text(), "no_file");
    }
}

QTEST_MAIN(KFileItemActionsTest)
