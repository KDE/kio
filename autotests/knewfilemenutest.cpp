/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2012 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>

#include <KCollapsibleGroupBox>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KIO/StoredTransferJob>
#include <KMessageWidget>
#include <KShell>
#include <QDialog>
#include <QLineEdit>
#include <QMenu>
#include <knameandurlinputdialog.h>
#include <knewfilemenu.h>
#include <kpropertiesdialog.h>

#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include <algorithm>

class KNewFileMenuTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
#ifdef Q_OS_UNIX
        m_umask = ::umask(0);
        ::umask(m_umask);
#endif

        // These have to be created here before KNewFileMenuSingleton is created,
        // otherwise they wouldn't be picked up
        QStandardPaths::setTestModeEnabled(true);
        m_xdgConfigDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        // Must not stay in testMode, or user-dirs.dirs does not get parsed
        // See https://codebrowser.dev/qt6/qtbase/src/corelib/io/qstandardpaths_unix.cpp.html#268
        QStandardPaths::setTestModeEnabled(false);

        // must use a fake XDG_CONFIG_HOME to change QStandardPaths::TemplatesLocation
        qputenv("XDG_CONFIG_HOME", m_xdgConfigDir.toUtf8());

        const QString templatesLoc = m_xdgConfigDir + "/test-templates";

        QDir dir;
        QVERIFY(dir.mkpath(m_xdgConfigDir));
        QFile userDirs(m_xdgConfigDir + "/user-dirs.dirs");
        QVERIFY(userDirs.open(QIODevice::WriteOnly));
        const QString templDir = "XDG_TEMPLATES_DIR=\"" + templatesLoc + "\"\n";
        userDirs.write(templDir.toLocal8Bit());
        userDirs.close();

        // Different location than what KNewFileMenuPrivate::slotFillTemplates() checks by default

        QVERIFY(dir.mkpath(templatesLoc));

        // knewfilemenu keeps its data in static variable
        // The files in template dirs are traversed only once
        QFile templ(m_xdgConfigDir + "/test-templates/test-text.desktop");
        QVERIFY(templ.open(QIODevice::WriteOnly));
        const QByteArray contents =
            "[Desktop Entry]\n"
            "Name=Custom...\n"
            "Type=Link\n"
            "URL=TestTextFile.txt\n"
            "Icon=text-plain\n";

        templ.write(contents);
        templ.close();

        QDir folderTemplate(templatesLoc + "/my-folder");
        QVERIFY(folderTemplate.mkdir(folderTemplate.path()));

        QFile templateFile(templatesLoc + "/my-script.py");
        QVERIFY(templateFile.open(QIODevice::WriteOnly));
        templateFile.close();
    }

    void cleanupTestCase()
    {
        QFile::remove(m_xdgConfigDir + "/user-dirs.dirs");
        QDir(m_xdgConfigDir + "/test-templates").removeRecursively();
    }

    void openActionText(KNewFileMenu *menu, const QString actionText)
    {
        QAction *action = menu;
        QVERIFY(action);
        QAction *textAct = nullptr;
        const QList<QAction *> actionsList = action->menu()->actions();
        for (QAction *act : actionsList) {
            if (act->text().contains(actionText)) {
                textAct = act;
            }
        }
        QVERIFY(textAct != nullptr);
        if (!textAct) {
            for (const QAction *act : actionsList) {
                qDebug() << act << act->text() << act->data();
            }
            const QString err = QLatin1String("action with text \"") + actionText + QLatin1String("\" not found.");
            QVERIFY2(textAct, qPrintable(err));
        }
        textAct->trigger();
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

        QTest::newRow("text file") << "Text File"
                                   << "Text File.txt"
                                   << "tmp_knewfilemenutest.txt"
                                   << "tmp_knewfilemenutest.txt";
        QTest::newRow("text file with jpeg extension") << "Text File"
                                                       << "Text File.txt"
                                                       << "foo.jpg"
                                                       << "foo.jpg"; // You get what you typed
        QTest::newRow("html file") << "HTML File"
                                   << "HTML File.html"
                                   << "foo.html"
                                   << "foo.html";
        QTest::newRow("url desktop file") << "Link to Location "
                                          << ""
                                          << "tmp_link.desktop"
                                          << "tmp_link.desktop";
        QTest::newRow("url desktop file no extension") << "Link to Location "
                                                       << ""
                                                       << "tmp_link1"
                                                       << "tmp_link1.desktop";
        QTest::newRow("url desktop file .pl extension") << "Link to Location "
                                                        << ""
                                                        << "tmp_link.pl"
                                                        << "tmp_link.pl.desktop";
        QTest::newRow("symlink") << "Link to File"
                                 << ""
                                 << "thelink"
                                 << "thelink";
        QTest::newRow("folder") << "Folder..."
                                << "New Folder"
                                << "folder1"
                                << "folder1";
        QTest::newRow("folder_named_tilde") << "Folder..."
                                            << "New Folder"
                                            << "~"
                                            << "~";

        // ~/.qttest/share/folderTildeExpanded
        const QString tildeDirPath =
            KShell::tildeCollapse(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/folderTildeExpanded"));
        QVERIFY(tildeDirPath.startsWith(QLatin1Char('~')));
        QTest::newRow("folder_tilde_expanded") << "Folder..."
                                               << "New Folder" << tildeDirPath << "folderTildeExpanded";

        QTest::newRow("folder_default_name") << "Folder..."
                                             << "New Folder"
                                             << "New Folder"
                                             << "New Folder";
        QTest::newRow("folder_with_suggested_name") << "Folder..."
                                                    << "New Folder (1)"
                                                    << "New Folder (1)"
                                                    << "New Folder (1)";
        QTest::newRow("folder_with_suggested_name_but_user_overrides") << "Folder..."
                                                                       << "New Folder (2)"
                                                                       << "New Folder"
                                                                       << "";
        QTest::newRow("application") << "Link to Application..."
                                     << "Link to Application"
                                     << "app1"
                                     << "app1.desktop";
    }

    void test()
    {
        QFETCH(QString, actionText);
        QFETCH(QString, expectedDefaultFilename);
        QFETCH(QString, typedFilename);
        QFETCH(QString, expectedFilename);

        QWidget parentWidget;
        KNewFileMenu menu(this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        menu.setSelectDirWhenAlreadyExist(true);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();
        openActionText(&menu, actionText);

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
            QLineEdit *lineEdit = propsDialog->findChild<QLineEdit *>(QStringLiteral("fileNameLineEdit"));
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
        QSignalSpy fileCreatedSpy(&menu, &KNewFileMenu::fileCreated);
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
            path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1Char('/') + QLatin1String("folderTildeExpanded");
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
            if (fileCreatedSpy.isEmpty()) {
                QVERIFY(fileCreatedSpy.wait(2000));
            }
            emittedUrl = fileCreatedSpy.at(0).at(0).toUrl();
            QVERIFY(QFile::exists(path));
            if (actionText != QLatin1String("Link to File")) {
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

    void testParsingUserDirs()
    {
        KNewFileMenu menu(this);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();
        const auto list = menu.menu()->actions();
        auto it = std::find_if(list.cbegin(), list.cend(), [](QAction *act) {
            return act->text() == QStringLiteral("Custom...");
        });
        QVERIFY(it != list.cend());
        // There is a separator between system-wide templates and the ones
        // from the user's home
        QVERIFY((*--it)->isSeparator());
    }

    void testParsingSimpleTemplates()
    {
        KNewFileMenu menu(this);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();
        const auto list = menu.menu()->actions();
        auto itFolder = std::find_if(list.cbegin(), list.cend(), [](QAction *act) {
            return act->text() == QStringLiteral("my-folder");
        });
        QVERIFY(itFolder != list.cend());

        auto itScript = std::find_if(list.cbegin(), list.cend(), [](QAction *act) {
            return act->text() == QStringLiteral("my-script");
        });
        QVERIFY(itScript != list.cend());
    }

    void testForbidTildeUsername_data()
    {
        QString tildeUsername = QLatin1Char('~') + QDir::home().dirName();
        QTest::addColumn<QString>("actionText"); // the action we're clicking on
        QTest::addColumn<QString>("typedFilename"); // what the user is typing
        QTest::addColumn<bool>("filenameAllowed"); // is the filename allowed, compared to OK button status

        QTest::newRow("text file is ~username.txt") << "Text File" << tildeUsername + ".txt" << true;
        QTest::newRow("text file is ~username") << "Text File" << tildeUsername << false;

        QTest::newRow("html file is ~username.html") << "HTML File" << tildeUsername + ".html" << true;
        QTest::newRow("html file is ~username") << "HTML File" << tildeUsername << false;

        QTest::newRow("folder starts with ~") << "Folder..."
                                              << "~folder1" << true;
        QTest::newRow("folder name is ~username") << "Folder..." << tildeUsername << false;
    }

    void testForbidTildeUsername()
    {
        QFETCH(QString, actionText);
        QFETCH(QString, typedFilename);
        QFETCH(bool, filenameAllowed);

        QWidget parentWidget;
        KNewFileMenu menu(this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        menu.setSelectDirWhenAlreadyExist(false);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();
        openActionText(&menu, actionText);

        QDialog *dialog;
        // QTRY_ because a NameFinderJob could be running and the dialog will be shown when
        // it finishes.
        QTRY_VERIFY(dialog = parentWidget.findChild<QDialog *>());

        QLineEdit *lineEdit = dialog->findChild<QLineEdit *>();
        KMessageWidget *msgWidget = dialog->findChild<KMessageWidget *>();
        QSignalSpy msgSpy(msgWidget, &KMessageWidget::showAnimationFinished);
        QVERIFY(lineEdit);
        lineEdit->setText(typedFilename);
        QVERIFY(msgSpy.wait(1000));

        QDialogButtonBox *buttonsList = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttonsList);
        auto okButton = buttonsList->button(QDialogButtonBox::Ok);
        QVERIFY(okButton);
        auto cancelButton = buttonsList->button(QDialogButtonBox::Cancel);
        QVERIFY(cancelButton);

        // No need to create new file, we just want to see if the OK button is enabled or not
        QCOMPARE(okButton->isEnabled(), filenameAllowed);
        cancelButton->click();
    }

    void testNoCustomIconOnFile()
    {
        QWidget parentWidget;
        KNewFileMenu menu(this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        menu.setSelectDirWhenAlreadyExist(false);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();

        openActionText(&menu, QStringLiteral("Text File"));

        QDialog *dialog;
        // QTRY_ because a NameFinderJob could be running and the dialog will be shown when
        // it finishes.
        QTRY_VERIFY(dialog = parentWidget.findChild<QDialog *>());

        KCollapsibleGroupBox *chooseIconBox = dialog->findChild<KCollapsibleGroupBox *>();
        QVERIFY(chooseIconBox);
        QVERIFY(!chooseIconBox->isVisibleTo(dialog));
        QVERIFY(!chooseIconBox->isExpanded());
    }

    void testFolderIconCollection_data()
    {
        QTest::addColumn<int>("buttonIndex");
        QTest::addColumn<QString>("iconName");
        QTest::addColumn<bool>("expectsIcon");

        QTest::newRow("default") << 0 << QStringLiteral("inode-directory") << false;
        QTest::newRow("red") << 1 << QStringLiteral("folder-red") << true;
        QTest::newRow("yellow") << 2 << QStringLiteral("folder-yellow") << true;
        QTest::newRow("orange") << 3 << QStringLiteral("folder-orange") << true;
        QTest::newRow("green") << 4 << QStringLiteral("folder-green") << true;
        QTest::newRow("cyan") << 5 << QStringLiteral("folder-cyan") << true;
        QTest::newRow("blue") << 6 << QStringLiteral("folder-blue") << true;
        QTest::newRow("violet") << 7 << QStringLiteral("folder-violet") << true;
        QTest::newRow("brown") << 8 << QStringLiteral("folder-brown") << true;
        QTest::newRow("grey") << 9 << QStringLiteral("folder-grey") << true;

        QTest::newRow("bookmark") << 10 << QStringLiteral("folder-bookmark") << true;
        QTest::newRow("cloud") << 11 << QStringLiteral("folder-cloud") << true;
        QTest::newRow("development") << 12 << QStringLiteral("folder-development") << true;
        QTest::newRow("games") << 13 << QStringLiteral("folder-games") << true;
        QTest::newRow("mail") << 14 << QStringLiteral("folder-mail") << true;
        QTest::newRow("music") << 15 << QStringLiteral("folder-music") << true;
        QTest::newRow("print") << 16 << QStringLiteral("folder-print") << true;
        QTest::newRow("tar") << 17 << QStringLiteral("folder-tar") << true;
        QTest::newRow("temp") << 18 << QStringLiteral("folder-temp") << true;
        QTest::newRow("important") << 19 << QStringLiteral("folder-important") << true;
    }

    void testFolderIconCollection()
    {
        QFETCH(int, buttonIndex);
        QFETCH(QString, iconName);
        QFETCH(bool, expectsIcon);

        QWidget parentWidget;
        KNewFileMenu menu(this);
        menu.setModal(false);
        menu.setParentWidget(&parentWidget);
        menu.setSelectDirWhenAlreadyExist(false);
        menu.setWorkingDirectory(QUrl::fromLocalFile(m_tmpDir.path()));
        menu.checkUpToDate();

        openActionText(&menu, QStringLiteral("Folder..."));

        QDialog *dialog;
        // QTRY_ because a NameFinderJob could be running and the dialog will be shown when
        // it finishes.
        QTRY_VERIFY(dialog = parentWidget.findChild<QDialog *>());

        KCollapsibleGroupBox *chooseIconBox = dialog->findChild<KCollapsibleGroupBox *>();
        QVERIFY(chooseIconBox);
        QVERIFY(chooseIconBox->isVisibleTo(dialog));

        // It should remember that it was expanded.
        if (buttonIndex == 0) {
            QVERIFY(!chooseIconBox->isExpanded());
        } else {
            QVERIFY(chooseIconBox->isExpanded());
        }

        chooseIconBox->setExpanded(true);

        QGridLayout *folderIconGrid = chooseIconBox->findChild<QGridLayout *>();
        QVERIFY(folderIconGrid);
        QCOMPARE(folderIconGrid->count(), 20);

        QLabel *iconLabel = dialog->findChild<QLabel *>("iconLabel");
        QVERIFY(iconLabel);

        const QString defaultFolderIconName = QStringLiteral("inode-directory");
        QCOMPARE(iconLabel->property("iconName").toString(), defaultFolderIconName);

        QToolButton *firstIconButton = qobject_cast<QToolButton *>(folderIconGrid->itemAt(0)->widget());
        QVERIFY(firstIconButton);
        QCOMPARE(firstIconButton->icon().name(), defaultFolderIconName);
        QVERIFY(firstIconButton->isChecked());

        QToolButton *button = qobject_cast<QToolButton *>(folderIconGrid->itemAt(buttonIndex)->widget());
        QVERIFY(button);
        QCOMPARE(button->icon().name(), iconName);

        button->click();
        QVERIFY(button->isChecked());
        QCOMPARE(iconLabel->property("iconName").toString(), iconName);

        QDialogButtonBox *buttonsList = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttonsList);
        auto okButton = buttonsList->button(QDialogButtonBox::Ok);
        QVERIFY(okButton);

        QSignalSpy folderSpy(&menu, &KNewFileMenu::directoryCreated);
        okButton->click();

        QVERIFY(folderSpy.wait(1000));
        QUrl emittedUrl = folderSpy.at(0).at(0).toUrl();

        const QString desktopPath = emittedUrl.toLocalFile() + QLatin1String("/.directory");
        QCOMPARE(QFileInfo::exists(desktopPath), expectsIcon);

        if (expectsIcon) {
            KDesktopFile desktopFile{desktopPath};
            QCOMPARE(desktopFile.readIcon(), iconName);
        }
    }

    // TODO test custom folder icon and that it remembers it.
    // But that needs access to KNewFileMenuPrivate and therefore a bit of shuffling of files and classes.

private:
    QTemporaryDir m_tmpDir;
    QString m_xdgConfigDir;
#ifdef Q_OS_UNIX
    mode_t m_umask;
#endif
};

QTEST_MAIN(KNewFileMenuTest)

#include "knewfilemenutest.moc"
