/*
    This file is part of the KIO framework tests
    SPDX-FileCopyrightText: 2016 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kfilewidget.h"

#include <QLabel>
#include <QLoggingCategory>
#include <QTemporaryDir>
#include <QTest>
#include <QStandardPaths>
#include <QSignalSpy>
#include <QUrl>

#include <KDirLister>
#include <kdiroperator.h>
#include <KFileFilterCombo>
#include <KLocalizedString>
#include <kurlnavigator.h>
#include <KUrlComboBox>
#include "kiotesthelper.h" // createTestFile

#include <QAbstractItemView>
#include <QDialog>
#include <QDropEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QStringList>
#include <QStringLiteral>
#include <QList>
#include <QUrl>


Q_DECLARE_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW)
Q_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW, "kf.kio.kfilewidgets.kfilewidget", QtInfoMsg)


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

        QWidget *label = findLocationLabel(&fw);
        QVERIFY(label);
        QVERIFY(label->hasFocus());
    }

    void testFocusOnLocationEditChangeDir()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        QWidget *label = findLocationLabel(&fw);
        QVERIFY(label);
        QVERIFY(label->hasFocus());
    }

    void testFocusOnLocationEditChangeDir2()
    {
        KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
        fw.show();
        fw.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&fw));

        fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

        QWidget *label = findLocationLabel(&fw);
        QVERIFY(label);
        QVERIFY(label->hasFocus());
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
        QCOMPARE(localUrl.toLocalFile(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        QVERIFY(outFileName.isEmpty());

        localUrl = KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///attachments/foo.txt?global")), recentDirClass, outFileName);
        QCOMPARE(recentDirClass, QStringLiteral("::attachments"));
        QCOMPARE(localUrl.toLocalFile(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
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

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 33)
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

        // WHEN
        fw.setSelection(selection); // now deprecated, this test shows why ;)

        // THEN
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), expectedBaseUrl);
        //if (QByteArray(QTest::currentDataTag()) == "filename") {
            QEXPECT_FAIL("filename", "setSelection cannot work with filenames, bad API", Continue);
        //}
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);
    }
#endif

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

        // WHEN
        fw.setSelectedUrl(selectionUrl);

        // THEN
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), expectedBaseUrl);
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);
    }

    void testPreserveFilenameWhileNavigating() // bug 418711
    {
        // GIVEN
        const QUrl url = QUrl::fromLocalFile(QDir::homePath());
        KFileWidget fw(url);
        fw.setOperationMode(KFileWidget::Saving);
        fw.setMode(KFile::File);
        QString baseDir = QDir::homePath();
        if (baseDir.endsWith('/')) {
            baseDir.chop(1);
        }
        const QString fileName = QStringLiteral("somefi#le");
        const QUrl fileUrl = QUrl::fromLocalFile(baseDir + QLatin1Char('/') + fileName);
        fw.setSelectedUrl(fileUrl);
        const QUrl baseUrl = QUrl::fromLocalFile(baseDir);
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), baseUrl);
        QCOMPARE(fw.locationEdit()->currentText(), fileName);

        // WHEN
        fw.dirOperator()->cdUp();

        // THEN
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), baseUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
        QCOMPARE(fw.locationEdit()->currentText(), fileName); // unchanged
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

        QMimeData *mimeData = new QMimeData();
        mimeData->setUrls(QList<QUrl>() << fileUrl);

        KDirLister *dirLister = fileWidget.dirOperator()->dirLister();
        QSignalSpy spy(dirLister, QOverload<>::of(&KCoreDirLister::completed));

        QAbstractItemView *view = fileWidget.dirOperator()->view();
        QVERIFY(view);

        QDragEnterEvent event1(QPoint(), Qt::DropAction::MoveAction, mimeData, Qt::MouseButton::LeftButton, Qt::KeyboardModifier::NoModifier);
        QVERIFY(qApp->sendEvent(view->viewport(), &event1));

        // Fake drop
        QDropEvent event(QPoint(), Qt::DropAction::MoveAction, mimeData, Qt::MouseButton::LeftButton, Qt::KeyboardModifier::NoModifier);
        QVERIFY(qApp->sendEvent(view->viewport(), &event));

        if (!dir.isEmpty()) {
            // once we drop a file the dirlister scans the dir
            // wait for the completed signal from the dirlister
            QVERIFY(spy.wait());
        }

        // Verify the expected populated name.
        QCOMPARE(fileWidget.baseUrl().adjusted(QUrl::StripTrailingSlash), dirUrl);
        QCOMPARE(fileWidget.locationEdit()->currentText(), expectedCurrentText);

        // QFileDialog ends up calling KDEPlatformFileDialog::selectedFiles()
        // which calls KFileWidget::selectedUrls().
        // Accept the filename to ensure that a filename is selected.
        connect(&fileWidget, &KFileWidget::accepted, &fileWidget, &KFileWidget::accept);
        QTest::keyClick(fileWidget.locationEdit(), Qt::Key_Return);
        const QList<QUrl> urls = fileWidget.selectedUrls();
        QCOMPARE(urls.size(), 1);
        QCOMPARE(urls[0], fileUrl);
    }

    void testCreateNestedNewFolders()
    {
        // when creating multiple nested new folders in the "save as" dialog, where folders are
        //created and entered, kdirlister would hit an assert (in reinsert()), bug 408801
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString dir = tempDir.path();
        const QUrl url = QUrl::fromLocalFile(dir);
        KFileWidget fw(url);
        fw.setOperationMode(KFileWidget::Saving);
        fw.setMode(KFile::File);

        QString currentPath = dir;
        // create the nested folders
        for (int i = 1; i < 6; ++i) {
            fw.dirOperator()->mkdir();
            QDialog *dialog;
            // QTRY_ because a NameFinderJob could be running and the dialog will be shown when
            // it finishes.
            QTRY_VERIFY(dialog = fw.findChild<QDialog *>());
            QLineEdit *lineEdit = dialog->findChild<QLineEdit *>();
            QVERIFY(lineEdit);
            const QString name = QStringLiteral("folder%1").arg(i);
            lineEdit->setText(name);
            // simulate the time the user will take to type the new folder name
            QTest::qWait(1000);

            dialog->accept();

            currentPath += QLatin1Char('/') + name;
            // Wait till the filewidget changes to the new folder
            QTRY_COMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash).toLocalFile(), currentPath);
        }
    }

    void testTokenize_data()
    {
        // Real filename (as in how they are stored in the fs)
        QTest::addColumn<QStringList>("fileNames");
        // Escaped value of the text-box in the dialog
        QTest::addColumn<QString>("expectedCurrentText");

        QTest::newRow("simple") << QStringList{"test2"} << QString("test2");

        // When a single file with space is selected, it is _not_ quoted ...
        QTest::newRow("space-single-file")
            << QStringList{"test space"}
            << QString("test space");

        // However, when multiple files are selected, they are quoted
        QTest::newRow("space-multi-file")
            << QStringList{"test space", "test2"}
            << QString("\"test space\" \"test2\"");

        // All quotes in names should be escaped, however since this is a single
        // file, the whole name will not be escaped.
        QTest::newRow("quote-single-file")
            << QStringList{"test\"quote"}
            << QString("test\\\"quote");

        // Escape multiple files. Files should also be wrapped in ""
        // Note that we are also testing quote at the end of the name
        QTest::newRow("quote-multi-file")
            << QStringList{"test\"quote", "test2-quote\"", "test"}
            << QString("\"test\\\"quote\" \"test2-quote\\\"\" \"test\"");

        // Ok, enough with quotes... lets do some backslashes
        // Backslash literals in file names - Unix only case
        QTest::newRow("backslash-single-file")
            << QStringList{"test\\backslash"}
            << QString("test\\\\backslash");

        QTest::newRow("backslash-multi-file")
            << QStringList{"test\\back\\slash", "test"}
            << QString("\"test\\\\back\\\\slash\" \"test\"");

        QTest::newRow("double-backslash-multi-file")
            << QStringList{"test\\\\back\\slash", "test"}
            << QString("\"test\\\\\\\\back\\\\slash\" \"test\"");

        QTest::newRow("double-backslash-end")
            << QStringList{"test\\\\"}
            << QString("test\\\\\\\\");

        QTest::newRow("single-backslash-end")
            << QStringList{"some thing", "test\\"}
            << QString("\"some thing\" \"test\\\\\"");

        QTest::newRow("sharp")
            << QStringList{"some#thing"}
            << QString("some#thing");

        // Filenames beginning with ':'; QDir::isAbsolutePath() always returns true
        // in that case, #322837
        QTest::newRow("file-beginning-with-colon") << QStringList{":test2"} << QString{":test2"};

        QTest::newRow("multiple-files-beginning-with-colon") << QStringList{":test space", ":test2"}
                                                             << QString{"\":test space\" \":test2\""};
    }

    void testTokenize()
    {
        // We will use setSelectedUrls([QUrl]) here in order to check correct
        // filename escaping. Afterwards we will accept() the dialog to confirm
        // correct result
        QFETCH(QStringList, fileNames);
        QFETCH(QString, expectedCurrentText);

        QTemporaryDir tempDir;
        const QString tempDirPath = tempDir.path();
        const QUrl tempDirUrl = QUrl::fromLocalFile(tempDirPath);
        QList<QUrl> fileUrls;
        for (const auto &fileName : fileNames) {
            const QString filePath = tempDirPath + QLatin1Char('/') + fileName;
            const QUrl localUrl = QUrl::fromLocalFile(filePath);
            fileUrls.append(localUrl);
            qCDebug(KIO_KFILEWIDGETS_FW) << fileName << " => " << localUrl;
        }

        KFileWidget fw(tempDirUrl);
        fw.setOperationMode(KFileWidget::Opening);
        fw.setMode(KFile::Files);
        fw.setSelectedUrls(fileUrls);

        // Verify the expected populated name.
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), tempDirUrl);
        QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);

        // QFileDialog ends up calling KDEPlatformFileDialog::selectedFiles()
        // which calls KFileWidget::selectedUrls().
        // Accept the filename to ensure that a filename is selected.
        connect(&fw, &KFileWidget::accepted, &fw, &KFileWidget::accept);
        QTest::keyClick(fw.locationEdit(), Qt::Key_Return);
        const QList<QUrl> urls = fw.selectedUrls();

        // We must have the same size as requested files
        QCOMPARE(urls.size(), fileNames.size());
        QCOMPARE(urls, fileUrls);
    }

private:
    static QWidget *findLocationLabel(QWidget *parent)
    {
        const QList<QLabel*> labels = parent->findChildren<QLabel*>();
        for (QLabel *label : labels) {
            if (label->text() == i18n("&Name:") || label->text() == i18n("Name:"))
                return label->buddy();
        }
        Q_ASSERT(false);
        return nullptr;
    }

};

QTEST_MAIN(KFileWidgetTest)

#include "kfilewidgettest.moc"
