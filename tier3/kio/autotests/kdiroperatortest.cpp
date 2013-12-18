/* This file is part of the KDE libraries
    Copyright (c) 2009 David Faure <faure@kde.org>

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
#include <kdiroperator.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>
#include <qtreeview.h>

/**
 * Unit test for KDirOperator
 */
class KDirOperatorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
    }

    void cleanupTestCase()
    {
    }

    void testNoViewConfig()
    {
        KDirOperator dirOp;
        // setIconsZoom tries to write config.
        // Make sure it won't crash if setViewConfig() isn't called
        dirOp.setIconsZoom(5);
        QCOMPARE(dirOp.iconsZoom(), 5);
    }

    void testReadConfig()
    {
        // Test: Make sure readConfig() and then setView() restores
        // the correct kind of view.
        KDirOperator *dirOp = new KDirOperator;
        dirOp->setView(KFile::DetailTree);
        dirOp->setShowHiddenFiles(true);
        KConfigGroup cg(KSharedConfig::openConfig(), "diroperator");
        dirOp->writeConfig(cg);
        delete dirOp;

        dirOp = new KDirOperator;
        dirOp->readConfig(cg);
        dirOp->setView(KFile::Default);
        QVERIFY(dirOp->showHiddenFiles());
        // KDirOperatorDetail inherits QTreeView, so this test should work
        QVERIFY(qobject_cast<QTreeView*>(dirOp->view()));
        delete dirOp;
    }

    /**
     * testBug187066 does the following:
     *
     * 1. Open a KDirOperator in kdelibs/kfile
     * 2. Set the current item to "file:///"
     * 3. Set the current item to "file:///.../kdelibs/kfile/tests/kdiroperatortest.cpp"
     *
     * This may result in a crash, see https://bugs.kde.org/show_bug.cgi?id=187066
     */

    void testBug187066()
    {
        const QString dir = QFileInfo(QFINDTESTDATA("kdiroperatortest.cpp")).absolutePath();
        const QUrl kFileDirUrl(QUrl::fromLocalFile(dir).adjusted(QUrl::RemoveFilename));

        KDirOperator dirOp(kFileDirUrl);
        QSignalSpy completedSpy(dirOp.dirLister(), SIGNAL(completed()));
        dirOp.setView(KFile::DetailTree);
        completedSpy.wait(1000);
        dirOp.setCurrentItem(QUrl("file:///"));
        dirOp.setCurrentItem(QUrl::fromLocalFile(QFINDTESTDATA("kdiroperatortest.cpp")));
        //completedSpy.wait(1000);
        QTest::qWait(1000);
    }
};

QTEST_MAIN(KDirOperatorTest)

#include "kdiroperatortest.moc"
