/* This file is part of the KDE Frameworks
    Copyright (c) 2008, 2016 David Faure <faure@kde.org>

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

#include <kurlrequester.h>
#include <kfilewidget.h>
#include <kdiroperator.h>

#include <QDebug>
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
    void initTestCase();
    void testUrlRequester();
    void testComboRequester();

private:
    bool createTestFile(const QString &fileName) {
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
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        KFileWidget *fw = widget->findChild<KFileWidget *>();
        if (fw) {
            widgets.append(fw);
        }
    }
    return (widgets.count() == 1) ? widgets.first() : nullptr;
}


void KUrlRequesterTest::initTestCase()
{
    qputenv("KDE_FORK_SLAVES", "yes");
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
    QCOMPARE(fileDialog->selectedFiles(), QStringList() << filePath);
    QCOMPARE(req.url().toLocalFile(), filePath);

    // Check there is no longer any file dialog visible
    QVERIFY(fileDialog->isHidden());

    // Click KUrlRequester button again. This time the filedialog is initialized with a file URL
    req.button()->click();
    fileDialog = req.findChild<QFileDialog *>();
    QVERIFY(fileDialog);
    fw = findFileWidget();
    if (fw) { // no need to wait for dir listing again, but we need it to be visible at least (for Key_Return to accept)
        //QVERIFY(QTest::qWaitForWindowExposed(fw->window())); // doesn't seem to be enough
        QTRY_VERIFY(fw->isVisible());
    }

    // Select file 2
    fileDialog->selectFile(filePath2);

    // Click OK, check URLRequester shows and returns selected file
    qApp->sendEvent(fw ? static_cast<QWidget *>(fw) : static_cast<QWidget *>(fileDialog), &keyPressEv);
    QCOMPARE(fileDialog->result(), static_cast<int>(QDialog::Accepted));
    QCOMPARE(fileDialog->selectedFiles(), QStringList() << filePath2);
    QCOMPARE(req.url().toLocalFile(), filePath2);
}

void KUrlRequesterTest::testComboRequester()
{
    KUrlComboRequester req;
    QList<QLineEdit *> lineEdits = req.findChildren<QLineEdit *>();
    QVERIFY(lineEdits.isEmpty()); // no lineedits, only a readonly combo
}

QTEST_MAIN(KUrlRequesterTest)
#include "kurlrequestertest.moc"
