/* This file is part of the KDE libraries
    Copyright (c) 2012 David Faure <faure@kde.org>

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

#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <knameandurlinputdialog.h>
#include <kpropertiesdialog.h>
#include <kactioncollection.h>
#include <knewfilemenu.h>
#include <KIO/StoredTransferJob>

#include <qtemporarydir.h>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif

class KNewFileMenuTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        qputenv("KDE_FORK_SLAVES", "yes"); // to avoid a runtime dependency on klauncher
#ifdef Q_OS_UNIX
        m_umask = ::umask(0);
        ::umask(m_umask);
#endif
        QVERIFY(m_tmpDir.isValid());
    }

    void cleanupTestCase()
    {
    }

    // Ensure that we can use storedPut() with a qrc file as input
    // similar to JobTest::storedPutIODeviceFile, but with a qrc file as input
    // (and here because jobtest doesn't link to KIO::FileWidgets, which has the qrc)
    void storedPutIODeviceQrcFile()
    {
        // Given a source (in a Qt resource file) and a destination file
        const QString src = ":/kio5/newfile-templates/.source/HTMLFile.html";
        QVERIFY(QFile::exists(src));
        QFile srcFile(src);
        QVERIFY(srcFile.open(QIODevice::ReadOnly));
        const QString dest = m_tmpDir.path() + "/dest";
        QFile::remove(dest);
        const QUrl destUrl = QUrl::fromLocalFile(dest);

        // When using storedPut with the QFile as argument
        KIO::StoredTransferJob *job = KIO::storedPut(&srcFile, destUrl, -1, KIO::Overwrite | KIO::HideProgressInfo);

        // Then the copy should succeed and the dest file exist
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QVERIFY(QFile::exists(dest));
        QCOMPARE(QFileInfo(src).size(), QFileInfo(dest).size());
        // And the permissions should respect the umask (#359581)
#ifdef Q_OS_UNIX
        if (m_umask & S_IWOTH) {
            QVERIFY2(!(QFileInfo(dest).permissions() & QFileDevice::WriteOther), qPrintable(dest));
        }
        if (m_umask & S_IWGRP) {
            QVERIFY(!(QFileInfo(dest).permissions() & QFileDevice::WriteGroup));
        }
#endif
        QFile::remove(dest);
    }

    void test_data()
    {
        QTest::addColumn<QString>("actionText"); // the action we're clicking on
        QTest::addColumn<QString>("expectedDefaultFilename"); // the initial filename in the dialog
        QTest::addColumn<QString>("typedFilename"); // what the user is typing
        QTest::addColumn<QString>("expectedFilename"); // the final file name

        QTest::newRow("text file") << "Text File" << "Text File" << "tmp_knewfilemenutest.txt" << "tmp_knewfilemenutest.txt";
        QTest::newRow("text file with jpeg extension") << "Text File" << "Text File" << "foo.jpg" << "foo.jpg.txt";
        QTest::newRow("html file") << "HTML File" << "HTML File" << "foo.html" << "foo.html";
        QTest::newRow("url desktop file") << "Link to Location " << "" << "tmp_link.desktop" << "tmp_link.desktop";
        QTest::newRow("url desktop file no extension") << "Link to Location " << "" << "tmp_link" << "tmp_link";
        QTest::newRow("url desktop file .pl extension") << "Link to Location " << "" << "tmp_link.pl" << "tmp_link.pl.desktop";
        QTest::newRow("symlink") << "Basic link" << "" << "thelink" << "thelink";
        QTest::newRow("folder") << "Folder..." << "New Folder" << "folder1" << "folder1";
        QTest::newRow("folder_default_name") << "Folder..." << "New Folder" << "New Folder" << "New Folder";
        QTest::newRow("folder_with_suggested_name") << "Folder..." << "New Folder (1)" << "New Folder" << "New Folder";
        QTest::newRow("application") << "Link to Application..." << "Link to Application" << "app1" << "app1.desktop";
    }

    void test()
    {
        QFETCH(QString, actionText);
        QFETCH(QString, expectedDefaultFilename);
        QFETCH(QString, typedFilename);
        QFETCH(QString, expectedFilename);

        QWidget parentWidget;
        KActionCollection coll(this, QStringLiteral("foo"));
        KNewFileMenu menu(&coll, QStringLiteral("the_action"), this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        QList<QUrl> lst;
        lst << QUrl::fromLocalFile(m_tmpDir.path());
        menu.setPopupFiles(lst);
        menu.checkUpToDate();
        QAction *action = coll.action(QStringLiteral("the_action"));
        QVERIFY(action);
        QAction *textAct = 0;
        Q_FOREACH (QAction *act, action->menu()->actions()) {
            if (act->text().contains(actionText)) {
                textAct = act;
            }
        }
        if (!textAct) {
            Q_FOREACH (QAction *act, action->menu()->actions()) {
                qDebug() << act << act->text() << act->data();
            }
            const QString err = "action with text \"" + actionText + "\" not found.";
            QVERIFY2(textAct, qPrintable(err));
        }
        textAct->trigger();
        QDialog *dialog = parentWidget.findChild<QDialog *>();
        QVERIFY(dialog);
        if (KNameAndUrlInputDialog *nauiDialog = qobject_cast<KNameAndUrlInputDialog *>(dialog)) {
            QCOMPARE(nauiDialog->name(), expectedDefaultFilename);
            nauiDialog->setSuggestedName(typedFilename);
            nauiDialog->setSuggestedUrl(QUrl(QStringLiteral("file:///etc")));
        } else if (KPropertiesDialog *propsDialog = qobject_cast<KPropertiesDialog *>(dialog)) {
            QLineEdit *lineEdit = propsDialog->findChild<QLineEdit *>("KFilePropsPlugin::nameLineEdit");
            QVERIFY(lineEdit);
            QCOMPARE(lineEdit->text(), expectedDefaultFilename);
            lineEdit->setText(typedFilename);
        } else {
            QLineEdit *lineEdit = dialog->findChild<QLineEdit *>();
            QVERIFY(lineEdit);
            QCOMPARE(lineEdit->text(), expectedDefaultFilename);
            lineEdit->setText(typedFilename);
        }
        QUrl emittedUrl;
        QSignalSpy spy(&menu, SIGNAL(fileCreated(QUrl)));
        QSignalSpy folderSpy(&menu, SIGNAL(directoryCreated(QUrl)));
        dialog->accept();
        const QString path = m_tmpDir.path() + '/' + expectedFilename;
        if (actionText == QLatin1String("Folder...")) {
            QVERIFY(folderSpy.wait(1000));
            emittedUrl = folderSpy.at(0).at(0).toUrl();
            QVERIFY(QFileInfo(path).isDir());
        } else {
            if (spy.isEmpty()) {
                QVERIFY(spy.wait(1000));
            }
            emittedUrl = spy.at(0).at(0).toUrl();
            QVERIFY(QFile::exists(path));
            if (actionText != QLatin1String("Basic link")) {
                QFile file(path);
                QVERIFY(file.open(QIODevice::ReadOnly));
                const QByteArray contents = file.readAll();
                if (actionText.startsWith("HTML")) {
                    QCOMPARE(QString::fromLatin1(contents.left(6)), QStringLiteral("<html>"));
                }
            }
        }
        QCOMPARE(emittedUrl.toLocalFile(), path);
    }
private:
    QTemporaryDir m_tmpDir;
#ifdef Q_OS_UNIX
    mode_t m_umask;
#endif
};

QTEST_MAIN(KNewFileMenuTest)

#include "knewfilemenutest.moc"
