/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>

#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <knameandurlinputdialog.h>
#include <kpropertiesdialog.h>
#include <KActionCollection>
#include <knewfilemenu.h>
#include <KIO/StoredTransferJob>
#include <KShell>

#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include <algorithm>

class KNewFileMenuTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
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
        const QString src = QStringLiteral(":/kio5/newfile-templates/.source/HTMLFile.html");
        QVERIFY(QFile::exists(src));
        QFile srcFile(src);
        QVERIFY(srcFile.open(QIODevice::ReadOnly));
        const QString dest = m_tmpDir.path() + QStringLiteral("/dest");
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

        QTest::newRow("text file") << "Text File" << "Text File.txt" << "tmp_knewfilemenutest.txt" << "tmp_knewfilemenutest.txt";
        QTest::newRow("text file with jpeg extension") << "Text File" << "Text File.txt" << "foo.jpg" << "foo.jpg.txt";
        QTest::newRow("html file") << "HTML File" << "HTML File.html" << "foo.html" << "foo.html";
        QTest::newRow("url desktop file") << "Link to Location " << "" << "tmp_link.desktop" << "tmp_link.desktop";
        QTest::newRow("url desktop file no extension") << "Link to Location " << "" << "tmp_link" << "tmp_link";
        QTest::newRow("url desktop file .pl extension") << "Link to Location " << "" << "tmp_link.pl" << "tmp_link.pl.desktop";
        QTest::newRow("symlink") << "Basic Link" << "" << "thelink" << "thelink";
        QTest::newRow("folder") << "Folder..." << "New Folder" << "folder1" << "folder1";
        QTest::newRow("folder_named_tilde") << "Folder..." << "New Folder" << "~" << "~";

        // ~/.qttest/share/folderTildeExpanded
        const QString tildeDirPath = KShell::tildeCollapse(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                                           + QLatin1String("/folderTildeExpanded"));
        QVERIFY(tildeDirPath.startsWith(QLatin1Char('~')));
        QTest::newRow("folder_tilde_expanded") << "Folder..." << "New Folder" << tildeDirPath << "folderTildeExpanded";

        QTest::newRow("folder_default_name") << "Folder..." << "New Folder" << "New Folder" << "New Folder";
        QTest::newRow("folder_with_suggested_name") << "Folder..." << "New Folder (1)" << "New Folder (1)" << "New Folder (1)";
        QTest::newRow("folder_with_suggested_name_but_user_overrides") << "Folder..." << "New Folder (2)" << "New Folder" << "";
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
        menu.setSelectDirWhenAlreadyExist(true);
        QList<QUrl> lst;
        lst << QUrl::fromLocalFile(m_tmpDir.path());
        menu.setPopupFiles(lst);
        menu.checkUpToDate();
        QAction *action = coll.action(QStringLiteral("the_action"));
        QVERIFY(action);
        QAction *textAct = nullptr;
        const QList<QAction *> actionsList = action->menu()->actions();
        for (QAction *act : actionsList) {
            if (act->text().contains(actionText)) {
                textAct = act;
            }
        }
        if (!textAct) {
            for (const QAction *act : actionsList) {
                qDebug() << act << act->text() << act->data();
            }
            const QString err = QLatin1String("action with text \"") + actionText + QLatin1String("\" not found.");
            QVERIFY2(textAct, qPrintable(err));
        }
        textAct->trigger();

        QDialog *dialog;
        // QTRY_ because a NameFinderJob could be running and the dialog will be shown when
        // it finishes.
        QTRY_VERIFY(dialog = parentWidget.findChild<QDialog *>());

        const auto buttonsList = dialog->findChildren<QPushButton *>();
        auto it = std::find_if(buttonsList.cbegin(), buttonsList.cend(), [](const QPushButton *button) {
            return button->text().contains("OK");
        });
        QVERIFY(it != buttonsList.cend());
        QPushButton *okButton = *it;

        if (KNameAndUrlInputDialog *nauiDialog = qobject_cast<KNameAndUrlInputDialog *>(dialog)) {
            QCOMPARE(nauiDialog->name(), expectedDefaultFilename);
            nauiDialog->setSuggestedName(typedFilename);
            nauiDialog->setSuggestedUrl(QUrl(QStringLiteral("file:///etc")));
        } else if (KPropertiesDialog *propsDialog = qobject_cast<KPropertiesDialog *>(dialog)) {
            QLineEdit *lineEdit = propsDialog->findChild<QLineEdit *>(QStringLiteral("KFilePropsPlugin::nameLineEdit"));
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
        QSignalSpy spy(&menu, &KNewFileMenu::fileCreated);
        QSignalSpy folderSpy(&menu, &KNewFileMenu::directoryCreated);

        // expectedFilename is empty in the "Folder already exists" case, the button won't
        // become enabled.
        if (!expectedFilename.isEmpty()) {
            // For all other cases, QTRY_ because we may be waiting for the StatJob in
            // KNewFileMenuPrivate::_k_delayedSlotTextChanged() to finish, the OK button
            // is disabled while it's checking if a folder/file with that name already exists.
            QTRY_VERIFY(okButton->isEnabled());
        }

        okButton->click();
        QString path = m_tmpDir.path() + QLatin1Char('/') + expectedFilename;
        if (typedFilename.contains(QLatin1String("folderTildeExpanded"))) {
            path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                   + QLatin1Char('/') + QLatin1String("folderTildeExpanded");
        }
        if (actionText == QLatin1String("Folder...")) {
            if (expectedFilename.isEmpty()) {
                // This is the "Folder already exists" case; expect an error dialog
                okButton->click();
                path.clear();
            } else {
                QVERIFY(folderSpy.wait(1000));
                emittedUrl = folderSpy.at(0).at(0).toUrl();
                QVERIFY(QFileInfo(path).isDir());
            }
        } else {
            if (spy.isEmpty()) {
                QVERIFY(spy.wait(1000));
            }
            emittedUrl = spy.at(0).at(0).toUrl();
            QVERIFY(QFile::exists(path));
            if (actionText != QLatin1String("Basic Link")) {
                QFile file(path);
                QVERIFY(file.open(QIODevice::ReadOnly));
                const QByteArray contents = file.readAll();
                if (actionText.startsWith(QLatin1String("HTML"))) {
                    QCOMPARE(QString::fromLatin1(contents.left(6)), QStringLiteral("<!DOCT"));
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
