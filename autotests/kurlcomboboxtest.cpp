/***************************************************************************
 *   Copyright (C) 2015 by Alejandro Fiestas Olivares <afiestas@kde.org    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/

#include "kurlcomboboxtest.h"
#include "kurlcombobox.h"

#include <QtTestWidgets>

QTEST_MAIN(KUrlComboBoxTest)

void KUrlComboBoxTest::testTextForItem_data()
{
    QTest::addColumn<QString>("url");
    QTest::addColumn<QString>("expectedText");

    QTest::newRow("with_host") << "ftp://foo.com/folder" << "ftp://foo.com/folder/";
    QTest::newRow("with_no_host") << "smb://" << "smb://";
    QTest::newRow("with_host_without_path") << "ftp://user@example.com" << "ftp://user@example.com";
}

void KUrlComboBoxTest::testTextForItem()
{
    QFETCH(QString, url);
    QFETCH(QString, expectedText);

    KUrlComboBox combo(KUrlComboBox::Directories);
    combo.setUrl(QUrl(url));

    QCOMPARE(combo.itemText(0), expectedText);

}
