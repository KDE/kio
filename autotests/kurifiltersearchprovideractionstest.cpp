/*
    Copyright (c) 2015 Montel Laurent <montel@kde.org>

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

#include "kurifiltersearchprovideractionstest.h"
#include "kurifiltersearchprovideractions.h"
#include <KUriFilter>
#include <QMenu>
#include <qtest.h>

void KUriFilterSearchProviderActionsTest::initTestCase()
{
    QString searchProvidersDir = QFINDTESTDATA("../src/urifilters/ikws/searchproviders/google.desktop").section('/', 0, -2);
    QVERIFY(!searchProvidersDir.isEmpty());
    qputenv("KIO_SEARCHPROVIDERS_DIR", QFile::encodeName(searchProvidersDir));
}

void KUriFilterSearchProviderActionsTest::shouldHaveDefaultValue()
{
    KIO::KUriFilterSearchProviderActions shortcutManager;
    QVERIFY(shortcutManager.selectedText().isEmpty());
}

void KUriFilterSearchProviderActionsTest::shouldAssignSelectedText()
{
    KIO::KUriFilterSearchProviderActions shortcutManager;
    const QString selectText = QStringLiteral("foo");
    shortcutManager.setSelectedText(selectText);
    QCOMPARE(shortcutManager.selectedText(), selectText);
}

void KUriFilterSearchProviderActionsTest::shouldAddActionToMenu()
{
    KIO::KUriFilterSearchProviderActions shortcutManager;
    QMenu *menu = new QMenu(nullptr);
    shortcutManager.addWebShortcutsToMenu(menu);
    //Empty when we don't have selected text
    QVERIFY(menu->actions().isEmpty());

    const QString selectText = QStringLiteral("foo");

    KUriFilterData filterData(selectText);

    filterData.setSearchFilteringOptions(KUriFilterData::RetrievePreferredSearchProvidersOnly);

    QStringList searchProviders;
    if (KUriFilter::self()->filterSearchUri(filterData, KUriFilter::NormalTextFilter)) {
        searchProviders = filterData.preferredSearchProviders();
    }

    shortcutManager.setSelectedText(selectText);
    shortcutManager.addWebShortcutsToMenu(menu);
    QVERIFY(!menu->actions().isEmpty());

    // Verify that there is a submenu
    QVERIFY(menu->actions().at(0)->menu());
    QVERIFY(!menu->actions().at(0)->menu()->actions().isEmpty());

    QStringList actionData;
    for (const QString &str : qAsConst(searchProviders)) {
        actionData.append(filterData.queryForPreferredSearchProvider(str));
    }

    int count = 0;
    const QList<QAction *> actionsList = menu->actions().at(0)->menu()->actions();
    for (const QAction *act : actionsList) {
        if (!act->data().isNull()) {
            QVERIFY(actionData.contains(act->data().toString()));
            count++;
        }
    }
    QCOMPARE(count, actionData.count());

    delete menu;
}

QTEST_MAIN(KUriFilterSearchProviderActionsTest)
