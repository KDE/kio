/* This file is part of the KIO framework tests

   Copyright (c) 2016 Albert Astals Cid <aacid@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QtTest/QtTest>

#include "kfilewidget.h"

#include <QLabel>

#include <kdiroperator.h>
#include <klocalizedstring.h>
#include <kurlnavigator.h>

/**
 * Unit test for KFileWidget
 */
class KFileWidgetTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        // To avoid a runtime dependency on klauncher
        qputenv("KDE_FORK_SLAVES", "yes");

        QVERIFY(QDir::homePath() != QDir::tempPath());
    }

    void cleanupTestCase()
    {
    }

    QWidget *findLocationLabel(QWidget *parent)
    {
        const QList<QLabel*> labels = parent->findChildren<QLabel*>();
        foreach(QLabel *label, labels) {
            if (label->text() == i18n("&Name:"))
                return label->buddy();
        }
        Q_ASSERT(false);
        return 0;
    }

    void testFocusOnLocationEdit()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        QTest::qWaitForWindowActive(&fw);

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnLocationEditChangeDir()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));
        fw.show();
        QTest::qWaitForWindowActive(&fw);

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnLocationEditChangeDir2()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        QTest::qWaitForWindowActive(&fw);

        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnDirOps()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        QTest::qWaitForWindowActive(&fw);

        const QList<KUrlNavigator*> nav = fw.findChildren<KUrlNavigator*>();
        QCOMPARE(nav.count(), 1);
        nav[0]->setFocus();

        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

        const QList<KDirOperator*> ops = fw.findChildren<KDirOperator*>();
        QCOMPARE(ops.count(), 1);
        QVERIFY(ops[0]->hasFocus());
    }
};

QTEST_MAIN(KFileWidgetTest)

#include "kfilewidgettest.moc"
