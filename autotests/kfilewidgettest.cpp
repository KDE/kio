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
#include <KFileFilterCombo>
#include <klocalizedstring.h>
#include <kurlnavigator.h>
#include <KUrlComboBox>
#include <KFileFilterCombo>
#include "kiotesthelper.h" // createTestFile

#include <QAbstractItemView>
#include <QDropEvent>
#include <QMimeData>

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
        for (QLabel *label : labels) {
            if (label->text() == i18n("&Name:"))
                return label->buddy();
        }
        Q_ASSERT(false);
        return nullptr;
    }

    void testFilterCombo()
    {
        KFileWidget fw(QUrl(QStringLiteral("kfiledialog:///SaveDialog")), nullptr);
        fw.setOperationMode(KFileWidget::Saving);
        fw.setMode(KFile::File);

        fw.setFilter(QStringLiteral(
            "*.xml *.a|Word 2003 XML (.xml)\n"
            "*.odt|ODF Text Document (.odt)\n"
            "*.xml *.b|DocBook (.xml)\n"
            "*|Raw (*)"));

        // default filter is selected
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.xml *.a"));

        // setUrl runs with blocked signals, so use setUrls.
        // auto-select ODT filter via filename
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.odt"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

        // select 2nd duplicate XML filter (see bug 407642)
        fw.filterWidget()->setCurrentFilter("*.xml *.b|DocBook (.xml)");
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.xml *.b"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

        // keep filter after file change with same extension
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test2.xml")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.xml *.b"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test2.xml"));

        // back to the non-xml / ODT filter
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.odt"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

        // auto-select 1st XML filter
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.xml")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.xml *.a"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

        // select Raw '*' filter
        fw.filterWidget()->setCurrentFilter("*|Raw (*)");
        QCOMPARE(fw.currentFilter(), QStringLiteral("*"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

        // keep Raw '*' filter with matching file extension
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

        // keep Raw '*' filter with non-matching file extension
        fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.core")));
        QCOMPARE(fw.currentFilter(), QStringLiteral("*"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.core"));

        // select 2nd XML filter
        fw.filterWidget()->setCurrentFilter("*.xml *.b|DocBook (.xml)");
        QCOMPARE(fw.currentFilter(), QStringLiteral("*.xml *.b"));
        QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));
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

    void testSetFilterForSave_data()
    {
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("filter");
        QTest::addColumn<QString>("expectedCurrentText");
        QTest::addColumn<QString>("expectedSelectedFileName");

        const QString filter = QStringLiteral("*.txt|Text files\n*.HTML|HTML files");

        QTest::newRow("some.txt")
            << "some.txt"
            << filter
            << QStringLiteral("some.txt")
            << QStringLiteral("some.txt");

        // If an application provides a name without extension, then the
        // displayed name will not receive an extension. It will however be
        // appended when the dialog is closed.
        QTest::newRow("extensionless name")
            << "some"
            << filter
            << QStringLiteral("some")
            << QStringLiteral("some.txt");

        // If the file literally exists, then no new extension will be appended.
        QTest::newRow("existing file")
            << "README"
            << filter
            << QStringLiteral("README")
            << QStringLiteral("README");

        // XXX perhaps the "extension" should not be modified when it does not
        // match any of the existing types? Should "some.2019.txt" be expected?
        QTest::newRow("some.2019")
            << "some.2019"
            << filter
            << QStringLiteral("some.txt")
            << QStringLiteral("some.txt");

        // XXX be smarter and do not change the extension if one of the other
        // filters match. Should "some.html" be expected?
        QTest::newRow("some.html")
            << "some.html"
            << filter
            << QStringLiteral("some.txt")
            << QStringLiteral("some.txt");
    }

    void testSetFilterForSave()
    {
        QFETCH(QString, fileName);
        QFETCH(QString, filter);
        QFETCH(QString, expectedCurrentText);
        QFETCH(QString, expectedSelectedFileName);

        // Use a temporary directory since the presence of existing files
        // influences whether an extension is automatically appended.
        QTemporaryDir tempDir;
        const QUrl dirUrl = QUrl::fromLocalFile(tempDir.path());
        const QUrl fileUrl = QUrl::fromLocalFile(tempDir.filePath(fileName));
        const QUrl expectedFileUrl = QUrl::fromLocalFile(tempDir.filePath(expectedSelectedFileName));
        createTestFile(tempDir.filePath("README"));

        KFileWidget fw(dirUrl);
        fw.setOperationMode(KFileWidget::Saving);
        fw.setSelectedUrl(fileUrl);
        // Calling setFilter has side-effects and changes the file name.
        fw.setFilter(filter);
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        // Verify the expected populated name.
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), dirUrl);
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);

        // QFileDialog ends up calling KDEPlatformFileDialog::selectedFiles()
        // which calls KFileWidget::selectedUrls().
        // Accept the filename to ensure that a filename is selected.
        connect(&fw, &KFileWidget::accepted, &fw, &KFileWidget::accept);
        QTest::keyClick(fw.locationEdit(), Qt::Key_Return);
        QList<QUrl> urls = fw.selectedUrls();
        QCOMPARE(urls.size(), 1);
        QCOMPARE(urls[0], expectedFileUrl);
    }

    void testFilterChange()
    {
        QTemporaryDir tempDir;
        createTestFile(tempDir.filePath("some.c"));
        bool created = QDir(tempDir.path()).mkdir("directory");
        Q_ASSERT(created);

        KFileWidget fw(QUrl::fromLocalFile(tempDir.path()));
        fw.setOperationMode(KFileWidget::Saving);
        fw.setSelectedUrl(QUrl::fromLocalFile(tempDir.filePath("some.txt")));
        fw.setFilter("*.txt|Txt\n*.c|C");
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        // Initial filename.
        QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("some.txt"));
        QCOMPARE(fw.filterWidget()->currentFilter(), QStringLiteral("*.txt"));

        // Select type with an existing file.
        fw.filterWidget()->setCurrentFilter("*.c|C");
        QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("some.c"));
        QCOMPARE(fw.filterWidget()->currentFilter(), QStringLiteral("*.c"));

        // Do not update extension if the current selection is a directory.
        fw.setSelectedUrl(QUrl::fromLocalFile(tempDir.filePath("directory")));
        fw.filterWidget()->setCurrentFilter("*.txt|Txt");
        QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("directory"));
        QCOMPARE(fw.filterWidget()->currentFilter(), QStringLiteral("*.txt"));
    }

    void testDropFile_data()
    {
        QTest::addColumn<QString>("dir");
        QTest::addColumn<QString>("fileName");
        QTest::addColumn<QString>("expectedCurrentText");

        QTest::newRow("some.txt")
            << ""
            << "some.txt"
            << "some.txt";

        QTest::newRow("subdir/some.txt")
            << "subdir"
            << "subdir/some.txt"
            << "some.txt";
    }

    void testDropFile()
    {
        QFETCH(QString, dir);
        QFETCH(QString, fileName);
        QFETCH(QString, expectedCurrentText);

        // Use a temporary directory since the presence of existing files
        // influences whether an extension is automatically appended.
        QTemporaryDir tempDir;
        QUrl dirUrl = QUrl::fromLocalFile(tempDir.path());
        const QUrl fileUrl = QUrl::fromLocalFile(tempDir.filePath(fileName));
        if (!dir.isEmpty()) {
            createTestDirectory(tempDir.filePath(dir));
            dirUrl = QUrl::fromLocalFile(tempDir.filePath(dir));
        }
        createTestFile(tempDir.filePath(fileName));

        KFileWidget fileWidget(QUrl::fromLocalFile(tempDir.path()));
        fileWidget.setOperationMode(KFileWidget::Saving);
        fileWidget.setMode(KFile::File);
        fileWidget.show();
        fileWidget.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fileWidget));

        QMimeData *mimeData = new QMimeData();
        mimeData->setUrls(QList<QUrl>() << fileUrl);

        QDragEnterEvent event1(QPoint(), Qt::DropAction::MoveAction, mimeData, Qt::MouseButton::LeftButton, Qt::KeyboardModifier::NoModifier);

        QVERIFY(qApp->sendEvent(fileWidget.dirOperator()->view()->viewport(), &event1));

        // Fake drop
        QDropEvent event(QPoint(), Qt::DropAction::MoveAction, mimeData, Qt::MouseButton::LeftButton, Qt::KeyboardModifier::NoModifier);

        QVERIFY(qApp->sendEvent(fileWidget.dirOperator()->view()->viewport(), &event));

        // QVERIFY(QTest::qWaitForWindowActive(&fileWidget));

        // once we drop a file the dirlister scans the dir
        // wait a bit to the dirlister time to finish
        QTest::qWait(100);

        // Verify the expected populated name.
        QCOMPARE(fileWidget.baseUrl().adjusted(QUrl::StripTrailingSlash), dirUrl);
        QCOMPARE(fileWidget.locationEdit()->currentText(), expectedCurrentText);

        // QFileDialog ends up calling KDEPlatformFileDialog::selectedFiles()
        // which calls KFileWidget::selectedUrls().
        // Accept the filename to ensure that a filename is selected.
        connect(&fileWidget, &KFileWidget::accepted, &fileWidget, &KFileWidget::accept);
        QTest::keyClick(fileWidget.locationEdit(), Qt::Key_Return);
        QList<QUrl> urls = fileWidget.selectedUrls();
        QCOMPARE(urls.size(), 1);
        QCOMPARE(urls[0], fileUrl);
    }
};

QTEST_MAIN(KFileWidgetTest)

#include "kfilewidgettest.moc"
