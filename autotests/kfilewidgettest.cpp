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

#include "kfilewidget.h"

#include <QLabel>
#include <QTest>
#include <QStandardPaths>

#include <kdiroperator.h>
#include <klocalizedstring.h>
#include <kurlnavigator.h>
#include <KUrlComboBox>

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

        QStandardPaths::setTestModeEnabled(true);

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
        return nullptr;
    }

    void testFocusOnLocationEdit()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnLocationEditChangeDir()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnLocationEditChangeDir2()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

        QVERIFY(findLocationLabel(&fw)->hasFocus());
    }

    void testFocusOnDirOps()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        const QList<KUrlNavigator*> nav = fw.findChildren<KUrlNavigator*>();
        QCOMPARE(nav.count(), 1);
        nav[0]->setFocus();

        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

        const QList<KDirOperator*> ops = fw.findChildren<KDirOperator*>();
        QCOMPARE(ops.count(), 1);
        QVERIFY(ops[0]->hasFocus());
    }

    void testGetStartUrl()
    {
        QString recentDirClass;
        QString outFileName;
        QUrl localUrl = KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///attachmentDir")), recentDirClass, outFileName);
        QCOMPARE(recentDirClass, QStringLiteral(":attachmentDir"));
        QCOMPARE(localUrl.path(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        QVERIFY(outFileName.isEmpty());

        localUrl = KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///attachments/foo.txt?global")), recentDirClass, outFileName);
        QCOMPARE(recentDirClass, QStringLiteral("::attachments"));
        QCOMPARE(localUrl.path(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        QCOMPARE(outFileName, QStringLiteral("foo.txt"));
    }

    void testSetSelection_data()
    {
        QTest::addColumn<QString>("baseDir");
        QTest::addColumn<QString>("selection");
        QTest::addColumn<QString>("expectedBaseDir");
        QTest::addColumn<QString>("expectedCurrentText");

        const QString baseDir = QDir::homePath();
        // A nice filename to detect URL encoding issues
        const QString fileName = QStringLiteral("some:fi#le");

        // Bug 369216, kdialog calls setSelection(path)
        QTest::newRow("path") << baseDir << baseDir + QLatin1Char('/') + fileName << baseDir << fileName;
        QTest::newRow("differentPath") << QDir::rootPath() << baseDir + QLatin1Char('/') + fileName << baseDir << fileName;
        // kdeplatformfiledialoghelper.cpp calls setSelection(URL as string)
        QTest::newRow("url") << baseDir << QUrl::fromLocalFile(baseDir + QLatin1Char('/') + fileName).toString() << baseDir << fileName;
        // What if someone calls setSelection(fileName)? That breaks, hence e70f8134a2b in plasma-integration.git
        QTest::newRow("filename") << baseDir << fileName << baseDir << fileName;
    }

    void testSetSelection()
    {
        // GIVEN
        QFETCH(QString, baseDir);
        QFETCH(QString, selection);
        QFETCH(QString, expectedBaseDir);
        QFETCH(QString, expectedCurrentText);
        const QUrl baseUrl = QUrl::fromLocalFile(baseDir).adjusted(QUrl::StripTrailingSlash);
        const QUrl expectedBaseUrl = QUrl::fromLocalFile(expectedBaseDir);

        KFileWidget fw(baseUrl);
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        // WHEN
        fw.setSelection(selection); // now deprecated, this test shows why ;)

        // THEN
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), expectedBaseUrl);
        //if (QByteArray(QTest::currentDataTag()) == "filename") {
            QEXPECT_FAIL("filename", "setSelection cannot work with filenames, bad API", Continue);
        //}
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);
    }

    void testSetSelectedUrl_data()
    {
        QTest::addColumn<QString>("baseDir");
        QTest::addColumn<QUrl>("selectionUrl");
        QTest::addColumn<QString>("expectedBaseDir");
        QTest::addColumn<QString>("expectedCurrentText");

        const QString baseDir = QDir::homePath();
        // A nice filename to detect URL encoding issues
        const QString fileName = QStringLiteral("some:fi#le");
        const QUrl fileUrl = QUrl::fromLocalFile(baseDir + QLatin1Char('/') + fileName);

        QTest::newRow("path") << baseDir << fileUrl << baseDir << fileName;
        QTest::newRow("differentPath") << QDir::rootPath() << fileUrl << baseDir << fileName;
        QTest::newRow("url") << baseDir << QUrl::fromLocalFile(baseDir + QLatin1Char('/') + fileName) << baseDir << fileName;

        QUrl relativeUrl;
        relativeUrl.setPath(fileName);
        QTest::newRow("filename") << baseDir << relativeUrl << baseDir << fileName;
    }

    void testSetSelectedUrl()
    {
        // GIVEN
        QFETCH(QString, baseDir);
        QFETCH(QUrl, selectionUrl);
        QFETCH(QString, expectedBaseDir);
        QFETCH(QString, expectedCurrentText);

        const QUrl baseUrl = QUrl::fromLocalFile(baseDir).adjusted(QUrl::StripTrailingSlash);
        const QUrl expectedBaseUrl = QUrl::fromLocalFile(expectedBaseDir);
        KFileWidget fw(baseUrl);
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        // WHEN
        fw.setSelectedUrl(selectionUrl);

        // THEN
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), expectedBaseUrl);
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);
    }

    void testEnterUrl_data()
    {
        QTest::addColumn<QUrl>("expectedUrl");

        // Check if the root urls are well transformed into themself, otherwise
        // when going up from file:///home/ it will become file:///home/user
        QTest::newRow("file") << QUrl::fromLocalFile("/");
        QTest::newRow("trash") << QUrl("trash:/");
        QTest::newRow("sftp") << QUrl("sftp://127.0.0.1/");
    }

    void testEnterUrl()
    {
        // GIVEN
        QFETCH(QUrl, expectedUrl);

        // WHEN
        // These lines are copied from src/filewidgets/kfilewidget.cpp
        // void KFileWidgetPrivate::_k_enterUrl(const QUrl &url)
        QUrl u(expectedUrl);
        if (!u.path().isEmpty() && !u.path().endsWith(QLatin1Char('/'))) {
            u.setPath(u.path() + QLatin1Char('/'));
        }
        // THEN
        QVERIFY(u.isValid());
        QCOMPARE(u, expectedUrl);
    }
};

QTEST_MAIN(KFileWidgetTest)

#include "kfilewidgettest.moc"
