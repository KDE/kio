/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>
#include <kdiroperator.h>
#include <KDirLister>
#include <KConfigGroup>
#include <KSharedConfig>
#include <QTreeView>
#include <QSignalSpy>
#include <QDir>

/**
 * Unit test for KDirOperator
 */
class KDirOperatorTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        // To avoid a runtime dependency on klauncher
        qputenv("KDE_FORK_SLAVES", "yes");
    }

    void cleanupTestCase()
    {
    }

    void testNoViewConfig()
    {
        KDirOperator dirOp;

        // setIconsZoom/setIconSize try to write config.
        // Make sure it won't crash if setViewConfig() isn't called.

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 76)
        dirOp.setIconsZoom(40);
        QCOMPARE(dirOp.iconsZoom(), 40);
#endif

        // Now the same for setIconSize
        dirOp.setIconSize(50);
        QCOMPARE(dirOp.iconSize(), 50);
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
        QVERIFY(qobject_cast<QTreeView *>(dirOp->view()));
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
        QSignalSpy completedSpy(dirOp.dirLister(), QOverload<>::of(&KCoreDirLister::completed));
        dirOp.setView(KFile::DetailTree);
        completedSpy.wait(1000);
        dirOp.setCurrentItem(QUrl(QStringLiteral("file:///")));
        dirOp.setCurrentItem(QUrl::fromLocalFile(QFINDTESTDATA("kdiroperatortest.cpp")));
        //completedSpy.wait(1000);
        QTest::qWait(1000);
    }

    void testSetUrlPathAdjustment_data()
    {
        QTest::addColumn<QUrl>("url");
        QTest::addColumn<QUrl>("expectedUrl");

        QTest::newRow("with_host") << QUrl(QStringLiteral("ftp://foo.com/folder")) << QUrl(QStringLiteral("ftp://foo.com/folder/"));
        QTest::newRow("with_no_host") << QUrl(QStringLiteral("smb://")) << QUrl(QStringLiteral("smb://"));
        QTest::newRow("with_host_without_path") << QUrl(QStringLiteral("ftp://user@example.com")) << QUrl(QStringLiteral("ftp://user@example.com"));
        QTest::newRow("with_trailing_slashs") << QUrl::fromLocalFile(QDir::tempPath() + QLatin1String("////////")) << QUrl::fromLocalFile(QDir::tempPath() + QLatin1Char('/'));
    }

    void testSetUrlPathAdjustment()
    {
        QFETCH(QUrl, url);
        QFETCH(QUrl, expectedUrl);

        KDirOperator dirOp;
        QSignalSpy spy(&dirOp, &KDirOperator::urlEntered);
        dirOp.setUrl(url, true);
        QCOMPARE(spy.takeFirst().at(0).toUrl(), expectedUrl);
    }

    void testSupportedSchemes()
    {
        KDirOperator dirOp;
        QSignalSpy spy(&dirOp, &KDirOperator::urlEntered);
        QCOMPARE(dirOp.supportedSchemes(), QStringList());
        dirOp.setSupportedSchemes({"file"});
        QCOMPARE(dirOp.supportedSchemes(), QStringList("file"));
        dirOp.setUrl(QUrl("smb://foo/bar"), true);
        QCOMPARE(spy.count(), 0);
        const auto fileUrl = QUrl::fromLocalFile(QDir::homePath() + QLatin1Char('/'));
        dirOp.setUrl(fileUrl, true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toUrl(), fileUrl);
    }

    void testEnabledDirHighlighting()
    {
        const QString path = QFileInfo(QFINDTESTDATA("kdiroperatortest.cpp")).absolutePath() + QLatin1Char('/');
        // <src dir>/autotests/
        const QUrl dirC = QUrl::fromLocalFile(path);
        const QUrl dirB = dirC.resolved(QUrl(QStringLiteral("..")));
        const QUrl dirA = dirB.resolved(QUrl(QStringLiteral("..")));

        KDirOperator dirOp(dirC);

        dirOp.show();
        dirOp.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dirOp));

        dirOp.setView(KFile::Default);

        // first case, go up...
        dirOp.cdUp();
        QCOMPARE(dirOp.url(), dirB);

        // the selection will happen after the dirLister has completed listing
        QTRY_VERIFY(!dirOp.selectedItems().isEmpty());
        QCOMPARE(dirOp.selectedItems().at(0).url(), dirC.adjusted(QUrl::StripTrailingSlash));

        // same as above
        dirOp.cdUp();
        QCOMPARE(dirOp.url(), dirA);

        QTRY_VERIFY(!dirOp.selectedItems().isEmpty());
        QCOMPARE(dirOp.selectedItems().at(0).url(), dirB.adjusted(QUrl::StripTrailingSlash));

        // we were in A/B/C, went up twice, now in A/
        // going back, we are in B/ and C/ is highlighted
        dirOp.back();
        QCOMPARE(dirOp.url(), dirB);

        QTRY_VERIFY(!dirOp.selectedItems().isEmpty());
        QCOMPARE(dirOp.selectedItems().at(0).url(), dirC.adjusted(QUrl::StripTrailingSlash));

        dirOp.clearHistory();
        // we start in A/
        dirOp.setUrl(dirA, true);
        QCOMPARE(dirOp.url(), dirA);
        // go to B/
        dirOp.setUrl(dirB, true);
        QCOMPARE(dirOp.url(), dirB);
        // go to C/
        dirOp.setUrl(dirC, true);
        QCOMPARE(dirOp.url(), dirC);

        // go back, C/ is highlighted
        dirOp.back();
        QCOMPARE(dirOp.url(), dirB);

        QTRY_VERIFY(!dirOp.selectedItems().isEmpty());
        QCOMPARE(dirOp.selectedItems().at(0).url(), dirC.adjusted(QUrl::StripTrailingSlash));

        // go back, B/ is highlighted
        dirOp.back();
        QCOMPARE(dirOp.url(), dirA);

        QTRY_VERIFY(!dirOp.selectedItems().isEmpty());
        QCOMPARE(dirOp.selectedItems().at(0).url(), dirB.adjusted(QUrl::StripTrailingSlash));
    }

    void testDisabledDirHighlighting()
    {
        const QString path = QFileInfo(QFINDTESTDATA("kdiroperatortest.cpp")).absolutePath() + QLatin1Char('/');
        // <src dir>/autotests/
        const QUrl dirC = QUrl::fromLocalFile(path);
        const QUrl dirB = dirC.resolved(QUrl(QStringLiteral("..")));
        const QUrl dirA = dirB.resolved(QUrl(QStringLiteral("..")));

        KDirOperator dirOp(dirC);
        dirOp.setEnableDirHighlighting(false);

        dirOp.show();
        dirOp.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&dirOp));

        dirOp.setView(KFile::Default);

        // finishedLoading is emitted when the dirLister emits the completed signal
        QSignalSpy finishedSpy(&dirOp, &KDirOperator::finishedLoading);
        // first case, go up...
        dirOp.cdUp();
        QCOMPARE(dirOp.url(), dirB);

        QVERIFY(finishedSpy.wait(1000));
        // ... no items highlighted
        QVERIFY(dirOp.selectedItems().isEmpty());

        // same as above
        dirOp.cdUp();
        QCOMPARE(dirOp.url(), dirA);

        QVERIFY(finishedSpy.wait(1000));
        QVERIFY(dirOp.selectedItems().isEmpty());

        // we were in A/B/C, went up twice, now in A/
        dirOp.back();
        QCOMPARE(dirOp.url(), dirB);

        QVERIFY(finishedSpy.wait(1000));
        QVERIFY(dirOp.selectedItems().isEmpty());

        dirOp.clearHistory();
        // we start in A/
        dirOp.setUrl(dirA, true);
        QCOMPARE(dirOp.url(), dirA);
        // go to B/
        dirOp.setUrl(dirB, true);
        QCOMPARE(dirOp.url(), dirB);
        // go to C/
        dirOp.setUrl(dirC, true);
        QCOMPARE(dirOp.url(), dirC);

        dirOp.back();
        QCOMPARE(dirOp.url(), dirB);

        QVERIFY(finishedSpy.wait(1000));
        QVERIFY(dirOp.selectedItems().isEmpty());

        dirOp.back();
        QCOMPARE(dirOp.url(), dirA);

        QVERIFY(finishedSpy.wait(1000));
        QVERIFY(dirOp.selectedItems().isEmpty());
    }
};

QTEST_MAIN(KDirOperatorTest)

#include "kdiroperatortest.moc"
