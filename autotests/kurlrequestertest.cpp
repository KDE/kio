/*
    This file is part of the KDE Frameworks
    SPDX-FileCopyrightText: 2008, 2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <KComboBox>
#include <kdiroperator.h>
#include <kfilewidget.h>
#include <kurlrequester.h>

#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QTest>

/*
IMPORTANT:
  Because this unittest interacts with the file dialog,
  remember to run it both with plugins/platformthemes/KDEPlasmaPlatformTheme.so (to use KFileWidget)
  and without it (to use the builtin QFileDialog code)
*/

class KUrlRequesterTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testUrlRequester();
    void testComboRequester();
    void testComboRequester_data();
    void testNameFilters();
    void testNameFilters_data();

private:
    bool createTestFile(const QString &fileName)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write("Hello world\n");
        return true;
    }
};

// Same as in kfiledialog_unittest.cpp
static KFileWidget *findFileWidget()
{
    QList<KFileWidget *> widgets;
    const QList<QWidget *> widgetsList = QApplication::topLevelWidgets();
    for (QWidget *widget : widgetsList) {
        KFileWidget *fw = widget->findChild<KFileWidget *>();
        if (fw) {
            widgets.append(fw);
        }
    }
    return (widgets.count() == 1) ? widgets.first() : nullptr;
}

void KUrlRequesterTest::testUrlRequester()
{
    KUrlRequester req;
    req.setFileDialogModality(Qt::NonModal);
    const QString fileName = QStringLiteral("some_test_file");
    QVERIFY(createTestFile(fileName));
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    const QString filePath2 = tempFile.fileName();
    QVERIFY(QFile::exists(filePath2));

    // Set start dir
    const QUrl dirUrl = QUrl::fromLocalFile(QDir::currentPath());
    req.setStartDir(dirUrl);
    QCOMPARE(req.startDir().toString(), dirUrl.toString());

    // Click the button
    req.button()->click();
    QFileDialog *fileDialog = req.findChild<QFileDialog *>();
    QVERIFY(fileDialog);

    // Find out if we're using KFileDialog or QFileDialog
    KFileWidget *fw = findFileWidget();

    // Wait for directory listing
    if (fw) {
        QSignalSpy spy(fw->dirOperator(), &KDirOperator::finishedLoading);
        QVERIFY(spy.wait());
    }

    // Select file
    const QString filePath = dirUrl.toLocalFile() + '/' + fileName;
    fileDialog->selectFile(fileName);

    // Click OK, check URLRequester shows and returns selected file
    QKeyEvent keyPressEv(QKeyEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    qApp->sendEvent(fw ? static_cast<QWidget *>(fw) : static_cast<QWidget *>(fileDialog), &keyPressEv);
    QCOMPARE(fileDialog->result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(fileDialog->selectedFiles(), QStringList{filePath});
    QCOMPARE(req.url().toLocalFile(), filePath);

    // Check there is no longer any file dialog visible
    QVERIFY(fileDialog->isHidden());

    // Click KUrlRequester button again. This time the filedialog is initialized with a file URL
    req.button()->click();
    fileDialog = req.findChild<QFileDialog *>();
    QVERIFY(fileDialog);
    fw = findFileWidget();
    if (fw) { // no need to wait for dir listing again, but we need it to be visible at least (for Key_Return to accept)
        // QVERIFY(QTest::qWaitForWindowExposed(fw->window())); // doesn't seem to be enough
        QTRY_VERIFY(fw->isVisible());
    }

    // Select file 2
    fileDialog->selectFile(filePath2);

    // Click OK, check URLRequester shows and returns selected file
    qApp->sendEvent(fw ? static_cast<QWidget *>(fw) : static_cast<QWidget *>(fileDialog), &keyPressEv);
    QCOMPARE(fileDialog->result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(fileDialog->selectedFiles(), QStringList{filePath2});
    QCOMPARE(req.url().toLocalFile(), filePath2);
}

void KUrlRequesterTest::testComboRequester()
{
    QFETCH(bool, editable);

    KUrlComboRequester req;
    req.show();

    QList<QLineEdit *> lineEdits = req.findChildren<QLineEdit *>();
    QVERIFY(lineEdits.isEmpty()); // no lineedits, only a readonly combo

    QSignalSpy textSpy(&req, &KUrlComboRequester::textChanged);
    QSignalSpy editSpy(&req, &KUrlComboRequester::textEdited);

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 80)
    QSignalSpy returnSpy(&req, qOverload<>(&KUrlComboRequester::returnPressed));
#endif
    QSignalSpy returnWithTextSpy(&req, qOverload<const QString &>(&KUrlComboRequester::returnPressed));

    QVERIFY(!req.comboBox()->isEditable());
    if (editable) {
        req.comboBox()->setEditable(true);

        const auto text = QStringLiteral("foobar");
        QTest::keyClicks(req.comboBox(), text, Qt::NoModifier);
        QCOMPARE(textSpy.size(), text.size());
        QCOMPARE(editSpy.size(), text.size());
        QCOMPARE(textSpy.last().first().toString(), text);
        QCOMPARE(editSpy.last().first().toString(), text);

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 80)
        QCOMPARE(returnSpy.size(), 0);
#endif
        QCOMPARE(returnWithTextSpy.size(), 0);

        QTest::keyEvent(QTest::Click, req.comboBox(), Qt::Key_Return);

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 80)
        QCOMPARE(returnSpy.size(), 1);
#endif
        QCOMPARE(returnWithTextSpy.size(), 1);
        QCOMPARE(returnWithTextSpy.last().first().toString(), text);
    } else {
        const auto url1 = QUrl("file:///foo/bar/1");
        const auto url2 = QUrl("file:///foo/bar/2");
        req.comboBox()->addUrl(url1);
        QCOMPARE(textSpy.size(), 1);
        QCOMPARE(textSpy.last().first().toUrl(), url1);

        req.comboBox()->addUrl(url2);
        QCOMPARE(textSpy.size(), 1);

        QTest::keyEvent(QTest::Click, req.comboBox(), Qt::Key_Down);
        QCOMPARE(textSpy.size(), 2);
        QCOMPARE(textSpy.last().first().toUrl(), url2);

        // only editable combo boxes get the edit and return signals emitted
        QCOMPARE(editSpy.size(), 0);

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 80)
        QCOMPARE(returnSpy.size(), 0);
#endif
        QCOMPARE(returnWithTextSpy.size(), 0);
    }
}

void KUrlRequesterTest::testComboRequester_data()
{
    QTest::addColumn<bool>("editable");

    QTest::newRow("read-only") << false;
    QTest::newRow("editable") << true;
}

void KUrlRequesterTest::testNameFilters()
{
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QFETCH(QString, filter);
    QFETCH(QStringList, dialogNameFilters);
#endif
    QFETCH(QString, nameFilter);
    QFETCH(QStringList, nameFilters);

    KUrlRequester req;
    req.setFileDialogModality(Qt::NonModal);

    // Click the button to get the filedialog
    req.button()->click();
    QFileDialog *fileDialog = req.findChild<QFileDialog *>();
    QVERIFY(fileDialog);

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    // set filter
    req.setFilter(filter);

    // check all
    QCOMPARE(req.filter(), filter);
    QCOMPARE(req.nameFilters(), nameFilters);
    QCOMPARE(fileDialog->nameFilters(), dialogNameFilters);
#endif

    // set name filter
    req.setNameFilter(nameFilter);

    // check all
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QCOMPARE(req.filter(), filter);
#endif
    QCOMPARE(req.nameFilters(), nameFilters);
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QCOMPARE(fileDialog->nameFilters(), dialogNameFilters);
#else
    QCOMPARE(fileDialog->nameFilters(), nameFilters);
#endif

    // set name filters
    req.setNameFilters(nameFilters);

    // check all
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QCOMPARE(req.filter(), filter);
    QCOMPARE(req.filter(), filter);
#endif
    QCOMPARE(req.nameFilters(), nameFilters);
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QCOMPARE(fileDialog->nameFilters(), dialogNameFilters);
#else
    QCOMPARE(fileDialog->nameFilters(), nameFilters);
#endif
}

void KUrlRequesterTest::testNameFilters_data()
{
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
    QTest::addColumn<QString>("filter");
    // result of backward compat to older Plasma Integration plugins:
    // which expect a name with each filter entry
    // and which has been always fabricated by KUrlRequester,
    // so != nameFilters property
    QTest::addColumn<QStringList>("dialogNameFilters");
#endif
    QTest::addColumn<QString>("nameFilter");
    QTest::addColumn<QStringList>("nameFilters");

    /* clang-format off */
    QTest::newRow("singleglob-comment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo|Comment")
        << QStringList{QStringLiteral("Comment (*.foo)")}
#endif
        << QStringLiteral("Comment (*.foo)")
        << QStringList{QStringLiteral("Comment (*.foo)")};

    QTest::newRow("singleglob-nocomment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo")
        << QStringList{QStringLiteral("*.foo (*.foo)")}
#endif
        << QStringLiteral("*.foo")
        << QStringList{QStringLiteral("*.foo")};

    QTest::newRow("multiglob-comment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo *.bar|Comment")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)")}
#endif
        << QStringLiteral("Comment (*.foo *.bar)")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)")};

    QTest::newRow("multiglob-nocomment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo *.bar")
        << QStringList{QStringLiteral("*.foo *.bar (*.foo *.bar)")}
#endif
        << QStringLiteral("*.foo *.bar")
        << QStringList{QStringLiteral("*.foo *.bar")};

    QTest::newRow("multilines-comment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo *.bar|Comment\n*.kde|Comment2")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)"), QStringLiteral("Comment2 (*.kde)")}
#endif
        << QStringLiteral("Comment (*.foo *.bar);;Comment2 (*.kde)")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)"), QStringLiteral("Comment2 (*.kde)")};

    QTest::newRow("multilines-nocomment")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo *.bar\n*.kde")
        << QStringList{QStringLiteral("*.foo *.bar (*.foo *.bar)"), QStringLiteral("*.kde (*.kde)")}
#endif
        << QStringLiteral("*.foo *.bar;;*.kde")
        << QStringList{QStringLiteral("*.foo *.bar"), QStringLiteral("*.kde")};

    QTest::newRow("multilines-commentmixed")
#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 108)
        << QStringLiteral("*.foo *.bar|Comment\n*.kde")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)"), QStringLiteral("*.kde (*.kde)")}
#endif
        << QStringLiteral("Comment (*.foo *.bar);;*.kde")
        << QStringList{QStringLiteral("Comment (*.foo *.bar)"), QStringLiteral("*.kde")};
    /* clang-format on */
}

QTEST_MAIN(KUrlRequesterTest)
#include "kurlrequestertest.moc"
