/*
    This file is part of the KIO framework tests
    SPDX-FileCopyrightText: 2016 Albert Astals Cid <aacid@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kfilewidget.h"

#include <QLabel>
#include <QLoggingCategory>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "../utils_p.h"
#include "kiotesthelper.h" // createTestFile
#include <KDirLister>
#include <KFileFilterCombo>
#include <KUrlComboBox>
#include <kdiroperator.h>
#include <kurlnavigator.h>

#include <KLocalizedString>
#include <KWindowSystem>

#include <QAbstractItemView>
#include <QDialog>
#include <QDropEvent>
#include <QLineEdit>
#include <QList>
#include <QMimeData>
#include <QPushButton>
#include <QStringList>
#include <QStringLiteral>
#include <QUrl>

Q_DECLARE_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW)
Q_LOGGING_CATEGORY(KIO_KFILEWIDGETS_FW, "kf.kio.kfilewidgets.kfilewidget", QtInfoMsg)

static QWidget *findLocationLabel(QWidget *parent)
{
    const QList<QLabel *> labels = parent->findChildren<QLabel *>();
    for (QLabel *label : labels) {
        if (label->text() == i18n("&Name:") || label->text() == i18n("Name:")) {
            return label->buddy();
        }
    }
    Q_ASSERT(false);
    return nullptr;
}

/**
 * Unit test for KFileWidget
 */
class KFileWidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testFilterCombo();
    void testFocusOnLocationEdit();
    void testFocusOnLocationEditChangeDir();
    void testFocusOnLocationEditChangeDir2();
    void testFocusOnDirOps();
    void testGetStartUrl();
    void testSetSelection_data();

    void testSetSelectedUrl_data();
    void testSetSelectedUrl();
    void testPreserveFilenameWhileNavigating();
    void testEnterUrl_data();
    void testEnterUrl();
    void testSetFilterForSave_data();
    void testSetFilterForSave();
    void testExtensionForSave_data();
    void testExtensionForSave();
    void testFilterChange();
    void testDropFile_data();
    void testDropFile();
    void testCreateNestedNewFolders();
    void testTokenize_data();
    void testTokenize();
    void testTokenizeForSave_data();
    void testTokenizeForSave();
    void testThumbnailPreviewSetting();
    void testReplaceLocationEditFilename_data();
    void testReplaceLocationEditFilename();
};

void KFileWidgetTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(QDir::homePath() != QDir::tempPath());
}

void KFileWidgetTest::testFilterCombo()
{
    KFileWidget fw(QUrl(QStringLiteral("kfiledialog:///SaveDialog")), nullptr);
    fw.setOperationMode(KFileWidget::Saving);
    fw.setMode(KFile::File);

    const KFileFilter wordFilter = KFileFilter::fromFilterString("*.xml *.a|Word 2003 XML (.xml)").first();
    const KFileFilter odtFilter = KFileFilter::fromFilterString("*.odt|ODF Text Document (.odt)").first();
    const KFileFilter docBookFilter = KFileFilter::fromFilterString("*.xml *.b|DocBook (.xml)").first();
    const KFileFilter rawFilter = KFileFilter::fromFilterString("*|Raw (*)").first();

    fw.setFilters({wordFilter, odtFilter, docBookFilter, rawFilter});

    // default filter is selected
    QCOMPARE(fw.currentFilter(), wordFilter);

    // setUrl runs with blocked signals, so use setUrls.
    // auto-select ODT filter via filename
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
    QCOMPARE(fw.currentFilter(), odtFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

    // select 2nd duplicate XML filter (see bug 407642)
    fw.filterWidget()->setCurrentFilter(docBookFilter);
    QCOMPARE(fw.currentFilter(), docBookFilter);
    // when editing the filter, there is delay to avoid refreshing the KDirOperator after each keypress
    QTest::qWait(350);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

    // keep filter after file change with same extension
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test2.xml")));
    QCOMPARE(fw.currentFilter(), docBookFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test2.xml"));

    // back to the non-xml / ODT filter
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
    QCOMPARE(fw.currentFilter(), odtFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

    // auto-select 1st XML filter
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.xml")));
    QCOMPARE(fw.currentFilter(), wordFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

    // select Raw '*' filter
    fw.filterWidget()->setCurrentFilter(rawFilter);
    QCOMPARE(fw.currentFilter(), rawFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));

    // keep Raw '*' filter with matching file extension
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.odt")));
    QCOMPARE(fw.currentFilter(), rawFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.odt"));

    // keep Raw '*' filter with non-matching file extension
    fw.locationEdit()->setUrls(QStringList(QStringLiteral("test.core")));
    QCOMPARE(fw.currentFilter(), rawFilter);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.core"));

    // select 2nd XML filter
    fw.filterWidget()->setCurrentFilter(docBookFilter);
    QCOMPARE(fw.currentFilter(), docBookFilter);
    // when editing the filter, there is delay to avoid refreshing the KDirOperator after each keypress
    QTest::qWait(350);
    QCOMPARE(fw.locationEdit()->urls()[0], QStringLiteral("test.xml"));
}

void KFileWidgetTest::testFocusOnLocationEdit()
{
    if (KWindowSystem::isPlatformWayland()) {
        QSKIP("X11 only, activation issue");
        return;
    }
    KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
    fw.show();
    fw.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&fw));

    QWidget *label = findLocationLabel(&fw);
    QVERIFY(label);
    QVERIFY(label->hasFocus());
}

void KFileWidgetTest::testFocusOnLocationEditChangeDir()
{
    if (KWindowSystem::isPlatformWayland()) {
        QSKIP("X11 only, activation issue");
        return;
    }
    KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
    fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));
    fw.show();
    fw.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&fw));

    QWidget *label = findLocationLabel(&fw);
    QVERIFY(label);
    QVERIFY(label->hasFocus());
}

void KFileWidgetTest::testFocusOnLocationEditChangeDir2()
{
    if (KWindowSystem::isPlatformWayland()) {
        QSKIP("X11 only, activation issue");
        return;
    }
    KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
    fw.show();
    fw.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&fw));

    fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

    QWidget *label = findLocationLabel(&fw);
    QVERIFY(label);
    QVERIFY(label->hasFocus());
}

void KFileWidgetTest::testFocusOnDirOps()
{
    if (KWindowSystem::isPlatformWayland()) {
        QSKIP("X11 only, activation issue");
        return;
    }
    KFileWidget fw(QUrl::fromLocalFile(QDir::homePath()));
    fw.show();
    fw.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&fw));

    const QList<KUrlNavigator *> nav = fw.findChildren<KUrlNavigator *>();
    QCOMPARE(nav.count(), 1);
    nav[0]->setFocus();

    fw.setUrl(QUrl::fromLocalFile(QDir::tempPath()));

    const QList<KDirOperator *> ops = fw.findChildren<KDirOperator *>();
    QCOMPARE(ops.count(), 1);
    QVERIFY(ops[0]->hasFocus());
}

void KFileWidgetTest::testGetStartUrl()
{
    QString recentDirClass;
    QString outFileName;
    QUrl localUrl = KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///attachmentDir")), recentDirClass, outFileName);
    QCOMPARE(recentDirClass, QStringLiteral(":attachmentDir"));
    QCOMPARE(localUrl.toLocalFile(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    QVERIFY(outFileName.isEmpty());

    localUrl = KFileWidget::getStartUrl(QUrl(QStringLiteral("kfiledialog:///attachments/foo.txt")), recentDirClass, outFileName);
    QCOMPARE(recentDirClass, QStringLiteral(":attachments"));
    QCOMPARE(localUrl.toLocalFile(), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    QCOMPARE(outFileName, QStringLiteral("foo.txt"));
}

void KFileWidgetTest::testSetSelection_data()
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

void KFileWidgetTest::testSetSelectedUrl_data()
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

void KFileWidgetTest::testSetSelectedUrl()
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

void KFileWidgetTest::testPreserveFilenameWhileNavigating() // bug 418711
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

void KFileWidgetTest::testEnterUrl_data()
{
    QTest::addColumn<QUrl>("expectedUrl");

    // Check if the root urls are well transformed into themself, otherwise
    // when going up from file:///home/ it will become file:///home/user
    QTest::newRow("file") << QUrl::fromLocalFile("/");
    QTest::newRow("trash") << QUrl("trash:/");
    QTest::newRow("sftp") << QUrl("sftp://127.0.0.1/");
}

void KFileWidgetTest::testEnterUrl()
{
    // GIVEN
    QFETCH(QUrl, expectedUrl);

    // WHEN
    // These lines are copied from src/filewidgets/kfilewidget.cpp
    // void KFileWidgetPrivate::_k_enterUrl(const QUrl &url)
    QUrl u(expectedUrl);
    Utils::appendSlashToPath(u);
    // THEN
    QVERIFY(u.isValid());
    QCOMPARE(u, expectedUrl);
}

void KFileWidgetTest::testSetFilterForSave_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("filter");
    QTest::addColumn<QString>("expectedCurrentText");
    QTest::addColumn<QString>("expectedSelectedFileName");

    const QString filter = QStringLiteral("*.txt|Text files\n*.HTML|HTML files");

    QTest::newRow("some.txt") << "some.txt" << filter << QStringLiteral("some.txt") << QStringLiteral("some.txt");

    // If an application provides a name without extension, then the
    // displayed name will not receive an extension. It will however be
    // appended when the dialog is closed.
    QTest::newRow("extensionless name") << "some" << filter << QStringLiteral("some") << QStringLiteral("some.txt");

    // If the file literally exists, then no new extension will be appended.
    QTest::newRow("existing file") << "README" << filter << QStringLiteral("README") << QStringLiteral("README");

    // XXX perhaps the "extension" should not be modified when it does not
    // match any of the existing types? Should "some.2019.txt" be expected?
    QTest::newRow("some.2019") << "some.2019" << filter << QStringLiteral("some.txt") << QStringLiteral("some.txt");

    // XXX be smarter and do not change the extension if one of the other
    // filters match. Should "some.html" be expected?
    QTest::newRow("some.html") << "some.html" << filter << QStringLiteral("some.txt") << QStringLiteral("some.txt");
}

void KFileWidgetTest::testSetFilterForSave()
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
    fw.setFilters(KFileFilter::fromFilterString(filter));

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

void KFileWidgetTest::testExtensionForSave_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("filter");
    QTest::addColumn<QString>("expectedCurrentText");
    QTest::addColumn<QString>("expectedSelectedFileName");

    const QString filter = QStringLiteral("*.txt *.text|Text files\n*.HTML|HTML files");

    QTest::newRow("some.txt") << "some.txt" << filter << QStringLiteral("some.txt") << QStringLiteral("some.txt");

    // If an application provides a name without extension, then the
    // displayed name will not receive an extension. It will however be
    // appended when the dialog is closed.
    QTest::newRow("extensionless name") << "some" << filter << QStringLiteral("some") << QStringLiteral("some.txt");
    QTest::newRow("extensionless name") << "some.with_dot" << filter << QStringLiteral("some.with_dot") << QStringLiteral("some.with_dot.txt");
    QTest::newRow("extensionless name") << "some.with.dots" << filter << QStringLiteral("some.with.dots") << QStringLiteral("some.with.dots.txt");

    // If the file literally exists, then no new extension will be appended.
    QTest::newRow("existing file") << "README" << filter << QStringLiteral("README") << QStringLiteral("README");

    // test bug 382437
    const QString octetStreamfilter = QStringLiteral("application/octet-stream");
    QTest::newRow("octetstream.noext") << "some" << octetStreamfilter << QStringLiteral("some") << QStringLiteral("some");
    QTest::newRow("octetstream.ext") << "some.txt" << octetStreamfilter << QStringLiteral("some.txt") << QStringLiteral("some.txt");
}

void KFileWidgetTest::testExtensionForSave()
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
    // Calling setFilter has side-effects and changes the file name.
    // The difference to testSetFilterForSave is that the filter is already set before the fileUrl
    // is set, and will not be changed after.
    fw.setFilters(KFileFilter::fromFilterString(filter));
    fw.setSelectedUrl(fileUrl);

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

void KFileWidgetTest::testFilterChange()
{
    QTemporaryDir tempDir;
    createTestFile(tempDir.filePath("some.c"));
    bool created = QDir(tempDir.path()).mkdir("directory");
    Q_ASSERT(created);

    KFileWidget fw(QUrl::fromLocalFile(tempDir.path()));
    fw.setOperationMode(KFileWidget::Saving);
    fw.setSelectedUrl(QUrl::fromLocalFile(tempDir.filePath("some.txt")));
    const QList<KFileFilter> filters = KFileFilter::fromFilterString("*.txt|Txt\n*.c|C");
    fw.setFilters(filters);

    // Initial filename.
    QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("some.txt"));
    QCOMPARE(fw.filterWidget()->currentFilter(), filters[0]);

    // Select type with an existing file.
    fw.filterWidget()->setCurrentFilter(filters[1]);
    // when editing the filter, there is delay to avoid refreshing the KDirOperator after each keypress
    QTest::qWait(350);
    QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("some.c"));
    QCOMPARE(fw.filterWidget()->currentFilter(), filters[1]);

    // Do not update extension if the current selection is a directory.
    fw.setSelectedUrl(QUrl::fromLocalFile(tempDir.filePath("directory")));
    fw.filterWidget()->setCurrentFilter(filters[0]);
    QCOMPARE(fw.locationEdit()->currentText(), QStringLiteral("directory"));
    QCOMPARE(fw.filterWidget()->currentFilter(), filters[0]);

    // The user types something into the combobox.
    fw.filterWidget()->setCurrentText("qml");

    QSignalSpy filterChangedSpy(&fw, &KFileWidget::filterChanged);
    filterChangedSpy.wait();
    QVERIFY(filterChangedSpy.count());

    // Plain text is automatically upgraded to wildcard syntax
    QCOMPARE(fw.dirOperator()->nameFilter(), "*qml*");

    // But existing wildcards are left intact
    fw.filterWidget()->setCurrentText("*.md");
    filterChangedSpy.wait();
    QVERIFY(filterChangedSpy.count());
    QCOMPARE(fw.dirOperator()->nameFilter(), "*.md");

    fw.filterWidget()->setCurrentText("[ab]c");
    filterChangedSpy.wait();
    QVERIFY(filterChangedSpy.count());
    QCOMPARE(fw.dirOperator()->nameFilter(), "[ab]c");

    fw.filterWidget()->setCurrentText("b?c");
    filterChangedSpy.wait();
    QVERIFY(filterChangedSpy.count());
    QCOMPARE(fw.dirOperator()->nameFilter(), "b?c");
}

void KFileWidgetTest::testDropFile_data()
{
    QTest::addColumn<QString>("dir");
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("expectedCurrentText");

    QTest::newRow("some.txt") << ""
                              << "some.txt"
                              << "some.txt";

    QTest::newRow("subdir/some.txt") << "subdir"
                                     << "subdir/some.txt"
                                     << "some.txt";
}

void KFileWidgetTest::testDropFile()
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
    QSignalSpy spy(dirLister, qOverload<>(&KCoreDirLister::completed));

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

void KFileWidgetTest::testCreateNestedNewFolders()
{
    // when creating multiple nested new folders in the "save as" dialog, where folders are
    // created and entered, kdirlister would hit an assert (in reinsert()), bug 408801
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

void KFileWidgetTest::testTokenize_data()
{
    // Real filename (as in how they are stored in the fs)
    QTest::addColumn<QStringList>("fileNames");
    // Escaped value of the text-box in the dialog
    QTest::addColumn<QString>("expectedCurrentText");

    QTest::newRow("simple") << QStringList{"test2"} << QString("test2");

    // When a single file with space is selected, it is _not_ quoted ...
    QTest::newRow("space-single-file") << QStringList{"test space"} << QString("test space");

    // However, when multiple files are selected, they are quoted
    QTest::newRow("space-multi-file") << QStringList{"test space", "test2"} << QString("\"test space\" \"test2\"");

    // All quotes in names should be escaped, however since this is a single
    // file, the whole name will not be escaped.
    QTest::newRow("quote-single-file") << QStringList{"test\"quote"} << QString("test\\\"quote");

    QTest::newRow("single-file-with-two-quotes") << QStringList{"\"test\".txt"} << QString{"\\\"test\\\".txt"};

    // Escape multiple files. Files should also be wrapped in ""
    // Note that we are also testing quote at the end of the name
    QTest::newRow("quote-multi-file") << QStringList{"test\"quote", "test2-quote\"", "test"} << QString("\"test\\\"quote\" \"test2-quote\\\"\" \"test\"");

    // Ok, enough with quotes... lets do some backslashes
    // Backslash literals in file names - Unix only case
    QTest::newRow("backslash-single-file") << QStringList{"test\\backslash"} << QString("test\\\\backslash");

    QTest::newRow("backslash-multi-file") << QStringList{"test\\back\\slash", "test"} << QString("\"test\\\\back\\\\slash\" \"test\"");

    QTest::newRow("double-backslash-multi-file") << QStringList{"test\\\\back\\slash", "test"} << QString("\"test\\\\\\\\back\\\\slash\" \"test\"");

    QTest::newRow("double-backslash-end") << QStringList{"test\\\\"} << QString("test\\\\\\\\");

    QTest::newRow("single-backslash-end") << QStringList{"some thing", "test\\"} << QString("\"some thing\" \"test\\\\\"");

    QTest::newRow("sharp") << QStringList{"some#thing"} << QString("some#thing");

    // Filenames beginning with ':'; QDir::isAbsolutePath() always returns true
    // in that case, #322837
    QTest::newRow("file-beginning-with-colon") << QStringList{":test2"} << QString{":test2"};

    QTest::newRow("multiple-files-beginning-with-colon") << QStringList{":test space", ":test2"} << QString{"\":test space\" \":test2\""};

    // # 473228
    QTest::newRow("file-beginning-with-something-that-looks-like-a-url-scheme") << QStringList{"Hello: foo.txt"} << QString{"Hello: foo.txt"};
    QTest::newRow("file-beginning-with-something-that-looks-like-a-file-url-scheme") << QStringList{"file: /foo.txt"} << QString{"file: /foo.txt"};

    QTemporaryDir otherTempDir;
    otherTempDir.setAutoRemove(false);
    const auto testFile1Path = otherTempDir.filePath("test-1");
    createTestFile(testFile1Path);
    const auto testFile2Path = otherTempDir.filePath("test-2");
    createTestFile(testFile2Path);

    QTest::newRow("absolute-url-not-in-dir") << QStringList{"file://" + testFile1Path} << QString{"file://" + testFile1Path};
    QTest::newRow("absolute-urls-not-in-dir") << QStringList{"file://" + testFile1Path, "file://" + testFile2Path}
                                              << QString{"\"file://" + testFile1Path + "\" \"file://" + testFile2Path + "\""};

    auto expectedtestFile1Path = testFile1Path;
    expectedtestFile1Path = expectedtestFile1Path.remove(0, 1);
    auto expectedtestFile2Path = testFile2Path;
    expectedtestFile2Path = expectedtestFile2Path.remove(0, 1);

    QTest::newRow("absolute-url-not-in-dir-no-scheme") << QStringList{testFile1Path} << QString{testFile1Path};
    QTest::newRow("absolute-urls-not-in-dir-no-scheme")
        << QStringList{testFile1Path, testFile2Path} << QString{"\"" + testFile1Path + "\" \"" + testFile2Path + "\""};

    QTest::newRow("absolute-urls-not-in-dir-scheme-mixed")
        << QStringList{testFile1Path, "file://" + testFile2Path} << QString{"\"" + testFile1Path + "\" \"file://" + testFile2Path + "\""};
}

void KFileWidgetTest::testTokenize()
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
        auto localUrl = QUrl(fileName);
        if (!localUrl.path().startsWith(QLatin1Char('/'))) {
            const QString filePath = tempDirPath + QLatin1Char('/') + fileName;
            localUrl = QUrl::fromLocalFile(filePath);
        }
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

    for (auto &localUrl : fileUrls) {
        if (localUrl.scheme().isEmpty()) {
            localUrl.setScheme("file");
        }
    }
    QCOMPARE(urls, fileUrls);
}

void KFileWidgetTest::testTokenizeForSave_data()
{
    // Real filename (as in how they are stored in the fs)
    QTest::addColumn<QString>("fileName");
    // Escaped value of the text-box in the dialog
    // Escaped cwd: Because setSelectedUrl is called in this
    // test, it actually sets the CWD to the dirname and only
    // keeps the filename displayed in the test
    QTest::addColumn<QString>("expectedSubFolder");
    QTest::addColumn<QString>("expectedCurrentText");

    QTest::newRow("save-simple") << QString{"test2"} << QString() << QString("test2");

    // When a single file with space is selected, it is _not_ quoted ...
    QTest::newRow("save-space") << QString{"test space"} << QString() << QString("test space");

    // All quotes in names should be escaped, however since this is a single
    // file, the whole name will not be escaped.
    QTest::newRow("save-quote") << QString{"test\"quote"} << QString() << QString("test\\\"quote");
    QTest::newRow("save-file-with-quotes") << QString{"\"test\".txt"} << QString() << QString{"\\\"test\\\".txt"};

    // Ok, enough with quotes... lets do some backslashes
    // Backslash literals in file names - Unix only case
    QTest::newRow("save-backslash") << QString{"test\\backslash"} << QString() << QString("test\\\\backslash");

    QTest::newRow("save-double-backslash") << QString{"test\\\\back\\slash"} << QString() << QString("test\\\\\\\\back\\\\slash");

    QTest::newRow("save-double-backslash-end") << QString{"test\\\\"} << QString() << QString("test\\\\\\\\");

    QTest::newRow("save-single-backslash-end") << QString{"test\\"} << QString() << QString("test\\\\");

    QTest::newRow("save-sharp") << QString{"some#thing"} << QString() << QString("some#thing");

    // Filenames beginning with ':'; QDir::isAbsolutePath() always returns true
    // in that case, #322837
    QTest::newRow("save-file-beginning-with-colon") << QString{":test2"} << QString() << QString{":test2"};

    // # 473228
    QTest::newRow("save-save-file-beginning-with-something-that-looks-like-a-url-scheme")
        << QString{"Hello: foo.txt"} << QString() << QString{"Hello: foo.txt"};

    QTemporaryDir otherTempDir;
    otherTempDir.setAutoRemove(false);
    const auto testFile1Path = otherTempDir.filePath("test-1");
    createTestFile(testFile1Path);

    QTest::newRow("save-absolute-url-not-in-dir") << QString{"file://" + testFile1Path} << otherTempDir.path() << QString{"test-1"};

    auto expectedtestFile1Path = testFile1Path;
    expectedtestFile1Path = expectedtestFile1Path.remove(0, 1);

    QTest::newRow("save-absolute-url-not-in-dir-no-scheme") << QString{testFile1Path} << otherTempDir.path() << QString{"test-1"};
}

void KFileWidgetTest::testTokenizeForSave()
{
    // We will use setSelectedUrls([QUrl]) here in order to check correct
    // filename escaping. Afterwards we will accept() the dialog to confirm
    // correct result

    // This test is similar to testTokenize but focuses on single-file
    // "save" operation. This follows a different code-path internally
    // and calls setSelectedUrl instead of setSelectedUrls.

    QFETCH(QString, fileName);
    QFETCH(QString, expectedSubFolder);
    QFETCH(QString, expectedCurrentText);

    QTemporaryDir tempDir;
    const QString tempDirPath = tempDir.path();
    const QUrl tempDirUrl = QUrl::fromLocalFile(tempDirPath);
    auto fileUrl = QUrl(fileName);
    if (!fileUrl.path().startsWith(QLatin1Char('/'))) {
        const QString filePath = tempDirPath + QLatin1Char('/') + fileName;
        fileUrl = QUrl::fromLocalFile(filePath);
    }
    if (fileUrl.scheme().isEmpty()) {
        fileUrl.setScheme("file");
    }
    qCDebug(KIO_KFILEWIDGETS_FW) << fileName << " => " << fileUrl;

    KFileWidget fw(tempDirUrl);
    fw.setOperationMode(KFileWidget::Saving);
    fw.setMode(KFile::File);
    fw.setSelectedUrl(fileUrl);

    // Verify the expected populated name.
    if (expectedSubFolder != "") {
        const QUrl expectedBaseUrl = tempDirUrl.resolved(QUrl::fromLocalFile(expectedSubFolder));
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), expectedBaseUrl);
    } else {
        QCOMPARE(fw.baseUrl().adjusted(QUrl::StripTrailingSlash), tempDirUrl);
    }
    QCOMPARE(fw.locationEdit()->currentText(), expectedCurrentText);

    // QFileDialog ends up calling KDEPlatformFileDialog::selectedFiles()
    // which calls KFileWidget::selectedUrls().
    // Accept the filename to ensure that a filename is selected.
    connect(&fw, &KFileWidget::accepted, &fw, &KFileWidget::accept);
    QTest::keyClick(fw.locationEdit(), Qt::Key_Return);
    const QList<QUrl> urls = fw.selectedUrls();

    // We always only have one URL here
    QCOMPARE(urls.size(), 1);
    QCOMPARE(urls[0], fileUrl);
}

// BUG: 501743
void KFileWidgetTest::testThumbnailPreviewSetting()
{
    QTemporaryDir tempDir;
    QUrl path = QUrl::fromLocalFile(tempDir.path());
    auto getAction = [](KFileWidget *fw) {
        QAction *previewAction = nullptr;
        for (auto action : fw->actions()) {
            if (action->text() == "Show Preview") {
                previewAction = action;
                break;
            }
        }
        return previewAction;
    };

    // Set up
    KFileWidget fwSetup(path);
    fwSetup.setOperationMode(KFileWidget::Saving);
    fwSetup.setMode(KFile::File);
    QAction *previewAction = getAction(&fwSetup);
    QVERIFY(previewAction);
    previewAction->setChecked(true);
    QCOMPARE(previewAction->isChecked(), true);
    fwSetup.cancelButton()->click();

    // Check preview settings are true, then save them as false
    KFileWidget fwPreviewTrue(path);
    fwPreviewTrue.setOperationMode(KFileWidget::Saving);
    fwPreviewTrue.setMode(KFile::File);
    previewAction = getAction(&fwPreviewTrue);
    QVERIFY(previewAction);
    QCOMPARE(previewAction->isChecked(), true);
    previewAction->setChecked(false);
    QCOMPARE(previewAction->isChecked(), false);
    fwPreviewTrue.cancelButton()->click();

    // Check preview settings are saved as false
    KFileWidget fwPreviewFalse(path);
    fwPreviewFalse.setOperationMode(KFileWidget::Saving);
    fwPreviewFalse.setMode(KFile::File);
    previewAction = getAction(&fwPreviewFalse);
    QVERIFY(previewAction);
    QCOMPARE(previewAction->isChecked(), true);
    fwPreviewFalse.cancelButton()->click();
}

struct LocationTestItem {
    bool dir;
    QString name;
};

void KFileWidgetTest::testReplaceLocationEditFilename_data()
{
    QTest::addColumn<LocationTestItem>("initialItem");
    QTest::addColumn<LocationTestItem>("selectedItem");
    QTest::addColumn<QString>("lineEditTextResult");
    QTest::addColumn<bool>("overrideModifiedText");

    QTest::newRow("replace-dir-with-dir") << LocationTestItem(true, "folder1") << LocationTestItem(true, "folder2") << "" << false;
    QTest::newRow("replace-dir-with-file") << LocationTestItem(true, "folder1") << LocationTestItem(false, "file1") << "file1" << true;
    QTest::newRow("replace-file-with-file") << LocationTestItem(false, "file1") << LocationTestItem(false, "file2") << "file2" << true;
    QTest::newRow("replace-file-with-dir") << LocationTestItem(false, "file1") << LocationTestItem(true, "folder1") << "file1" << false;
}

// BUG: 502794
// Test that we don't override file names with folder names
void KFileWidgetTest::testReplaceLocationEditFilename()
{
    QFETCH(LocationTestItem, initialItem);
    QFETCH(LocationTestItem, selectedItem);
    QFETCH(QString, lineEditTextResult);
    QFETCH(bool, overrideModifiedText);

    // Setup - Create folders/files in temp dir
    QTemporaryDir tempDir;
    const QString tempDirPath = tempDir.path();
    QUrl tempDirUrl = QUrl::fromLocalFile(tempDirPath);
    QUrl replacedUrl = QUrl::fromLocalFile(tempDirPath + QLatin1Char('/') + initialItem.name);
    QUrl selectedUrl = QUrl::fromLocalFile(tempDirPath + QLatin1Char('/') + selectedItem.name);

    auto createTestItem = [tempDirUrl](LocationTestItem item, const QUrl &url) {
        if (item.dir) {
            QDir(tempDirUrl.toLocalFile()).mkdir(url.toLocalFile());
            QVERIFY(QDir(url.toLocalFile()).exists());
        } else {
            QFile file(url.toLocalFile());
            if (!file.open(QIODevice::WriteOnly)) {
                qFatal("Couldn't create %s", qPrintable(url.toLocalFile()));
            }
            file.write(QByteArray("Test file"));
            file.close();
            QVERIFY(file.exists());
        }
    };

    createTestItem(initialItem, replacedUrl);
    createTestItem(selectedItem, selectedUrl);

    // Open the filewidget in tempdir
    KFileWidget fw(tempDirUrl);
    fw.setOperationMode(KFileWidget::Saving);

    // Highlight the item, then another
    auto highlightItem = [&fw](QUrl url) {
        KFileItem fileItem(url);
        QSignalSpy fileHighlightedSpy(fw.dirOperator(), &KDirOperator::fileHighlighted);
        fw.dirOperator()->highlightFile(fileItem);
        fileHighlightedSpy.wait(500);
        QVERIFY(fileHighlightedSpy.count());
    };

    highlightItem(replacedUrl);
    highlightItem(selectedUrl);

    // Compare that we have the wanted result when selecting items
    QCOMPARE(fw.locationEdit()->lineEdit()->text(), lineEditTextResult);

    // Make sure we don't overwrite any text user has modified in some cases
    const QString modifiedText("New Filename.txt");
    fw.locationEdit()->setEditText(modifiedText);
    highlightItem(selectedUrl);

    if (overrideModifiedText) {
        QCOMPARE(fw.locationEdit()->lineEdit()->text(), lineEditTextResult);
    } else {
        QCOMPARE(fw.locationEdit()->lineEdit()->text(), modifiedText);
    }
}

QTEST_MAIN(KFileWidgetTest)

#include "kfilewidgettest.moc"
