/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kfileitemactionstest.h"

#include <kfileitemactions.h>
#include <kfileitemlistproperties.h>

#include <KServiceTypeTrader>
#include <KSycoca>
#include <QMenu>
#include <QStandardPaths>
#include <QTest>

void KFileItemActionsTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    const QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    QString appsPath = dataLocation + QDir::separator() + "kservices5";
    QString servicesPath = dataLocation + QDir::separator() + "kservicetypes5/";
    qputenv("XDG_DATA_DIRS", QFile::encodeName(dataLocation));
    QDir(appsPath).removeRecursively();
    QVERIFY(QDir().mkpath(appsPath));
    QVERIFY(QDir().mkpath(servicesPath));
    QFile::copy(QStringLiteral(SERVICE_FILE_PATH), servicesPath + "konqpopupmenuplugin.desktop");
}

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

void KFileItemActionsTest::testTopLevelServiceMenuActions()
{
    copyTestFile("servicemenu1.desktop");
    KFileItemActions actions;
    actions.setItemListProperties(KFileItemList({KFileItem(QUrl::fromLocalFile(QFINDTESTDATA("data")))}));

    QMenu *menu = new QMenu();
    actions.addActionsTo(menu, KFileItemActions::MenuActionSource::Services);
    QCOMPARE(menu->actions().count(), 1);
}

void KFileItemActionsTest::cleanup()
{
    const QString servicesPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "kservices5";
    const QStringList files = QDir(servicesPath).entryList(QDir::NoDotAndDotDot | QDir::Files);
    for (const QString &file : files) {
        QVERIFY(QFile::remove(servicesPath + QDir::separator() + file));
    }
}

void KFileItemActionsTest::copyTestFile(const QString &fileName)
{
    QString source = QFINDTESTDATA("data") + QDir::separator() + fileName;
    QString target = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QDir::separator() + "kservices5" + QDir::separator() + fileName;
    QVERIFY2(QFile::copy(source, target), qPrintable(QStringLiteral("can't copy %1 => %2").arg(source, target)));
    KSycoca::self()->ensureCacheValid();
}

QTEST_MAIN(KFileItemActionsTest)
