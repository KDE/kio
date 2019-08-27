/* This file is part of the KDE project
   Copyright (C) 2014 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2 of the License or ( at
   your option ) version 3 or, at the discretion of KDE e.V. ( which shall
   act as a proxy as in section 14 of the GPLv3 ), any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <qtest.h>
#include <QSignalSpy>
#include <QMenu>
#include <QTemporaryDir>

#include <KConfigGroup>
#include <KSharedConfig>
#include <KFileCopyToMenu>
#include "kiotesthelper.h"
#include "jobuidelegatefactory.h"

#include <KIO/CopyJob>

class KFileCopyToMenuTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qputenv("KIOSLAVE_ENABLE_TESTMODE", "1"); // ensure the ioslaves call QStandardPaths::setTestModeEnabled too
        qputenv("KDE_FORK_SLAVES", "yes"); // to avoid a runtime dependency on klauncher

        QVERIFY(m_tempDir.isValid());
        QVERIFY(m_tempDestDir.isValid());
        QVERIFY(m_nonWritableTempDir.isValid());
        QVERIFY(QFile(m_nonWritableTempDir.path()).setPermissions(QFile::ReadOwner | QFile::ReadUser | QFile::ExeOwner | QFile::ExeUser));
        m_srcDir = m_tempDir.path();
        m_destDir = m_tempDestDir.path();

        m_srcFile = m_srcDir + QStringLiteral("/srcfile");

        KIO::setDefaultJobUiDelegateExtension(nullptr); // no "skip" dialogs

        // Set a recent dir
        KConfigGroup recentDirsGroup(KSharedConfig::openConfig(), "kuick-copy");
        m_recentDirs
                << m_destDir + QStringLiteral("/nonexistentsubdir") // will be action number count-3
                << m_nonWritableTempDir.path() // will be action number count-2
                << m_destDir; // will be action number count-1
        recentDirsGroup.writeEntry("Paths", m_recentDirs);

        m_lastActionCount = 0;
    }

    void cleanupTestCase()
    {
        QVERIFY(QFile(m_nonWritableTempDir.path()).setPermissions(QFile::ReadOwner | QFile::ReadUser | QFile::WriteOwner | QFile::WriteUser | QFile::ExeOwner | QFile::ExeUser));
    }

    // Before every test method, ensure the test file m_srcFile exists
    void init()
    {
        if (QFile::exists(m_srcFile)) {
            QVERIFY(QFileInfo(m_srcFile).isWritable());
        } else {
            QFile srcFile(m_srcFile);
            QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
            srcFile.write("Hello world\n");
        }
        QVERIFY(QFileInfo(m_srcFile).isWritable());
    }

    void shouldHaveParentWidget()
    {
        KFileCopyToMenu generator(&m_parentWidget);
        QCOMPARE(generator.parent(), &m_parentWidget);
    }

    void shouldAddActions()
    {
        KFileCopyToMenu generator(&m_parentWidget);
        QMenu menu;
        generator.addActionsTo(&menu);
        QList<QUrl> urls; urls << QUrl::fromLocalFile(m_srcFile);
        generator.setUrls(urls);
        QCOMPARE(extractActionNames(menu), QStringList() << QStringLiteral("copyTo_submenu") << QStringLiteral("moveTo_submenu"));
        //menu.popup(QPoint(-50, -50));
        QMenu *copyMenu = menu.actions().at(0)->menu(); // "copy" submenu
        QVERIFY(copyMenu);

        // When
        copyMenu->popup(QPoint(-100, -100));

        // Then
        const QStringList actionNames = extractActionNames(*copyMenu);
        QCOMPARE(actionNames.first(), QStringLiteral("home"));
        QVERIFY(actionNames.contains(QLatin1String("browse")));
        QCOMPARE(actionNames.at(actionNames.count() - 2), m_nonWritableTempDir.path());
        QCOMPARE(actionNames.last(), m_destDir);
    }

    void shouldTryCopyingToRecentPath_data()
    {
        QTest::addColumn<int>("actionNumber"); // from the bottom of the menu, starting at 1; see the recentDirs list in initTestCase
        QTest::addColumn<int>("expectedErrorCode");

        QTest::newRow("working") << 1 << 0; // no error
        QTest::newRow("non_writable") << 2 << int(KIO::ERR_WRITE_ACCESS_DENIED);
        QTest::newRow("non_existing") << 3 << int(KIO::ERR_CANNOT_OPEN_FOR_WRITING);
    }

    void shouldTryCopyingToRecentPath()
    {
        QFETCH(int, actionNumber);
        QFETCH(int, expectedErrorCode);

        KFileCopyToMenu generator(&m_parentWidget);
        QMenu menu;
        QList<QUrl> urls; urls << QUrl::fromLocalFile(m_srcFile);
        generator.setUrls(urls);
        generator.addActionsTo(&menu);
        QMenu *copyMenu = menu.actions().at(0)->menu();
        copyMenu->popup(QPoint(-100, -100));
        const QList<QAction *> actions = copyMenu->actions();
        if (m_lastActionCount == 0) {
            m_lastActionCount = actions.count();
        } else {
            QCOMPARE(actions.count(), m_lastActionCount); // should be stable, i.e. selecting a recent dir shouldn't duplicate it
        }
        QAction *copyAction = actions.at(actions.count() - actionNumber);
        QSignalSpy spy(&generator, SIGNAL(error(int,QString)));

        // When
        copyAction->trigger();

        // Then
        QTRY_COMPARE(spy.count(), expectedErrorCode ? 1 : 0);
        if (expectedErrorCode) {
            QCOMPARE(spy.at(0).at(0).toInt(), expectedErrorCode);
        } else {
            QTRY_VERIFY(QFile::exists(m_destDir + QStringLiteral("/srcfile")));
        }
    }

private:

    static QStringList extractActionNames(const QMenu &menu)
    {
        QStringList ret;
        foreach (const QAction *action, menu.actions()) {
            ret.append(action->objectName());
        }
        return ret;
    }

    QTemporaryDir m_tempDir;
    QString m_srcDir;
    QString m_srcFile;
    QTemporaryDir m_tempDestDir;
    QString m_destDir;
    QTemporaryDir m_nonWritableTempDir;
    QWidget m_parentWidget;
    QStringList m_recentDirs;
    int m_lastActionCount;
};

QTEST_MAIN(KFileCopyToMenuTest)

#include "kfilecopytomenutest.moc"

