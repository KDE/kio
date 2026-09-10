/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>
    SPDX-FileCopyrightText: 2026 Wellington Dias <wdiasjunior@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <KConfigGroup>
#include <KFileItem>
#include <KSharedConfig>

#include <renamefiledialog.h>

class RenameFileDialogTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        for (const QString &name : {QStringLiteral("one.txt"), QStringLiteral("two.txt")}) {
            QFile file(m_dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            m_items.append(KFileItem(QUrl::fromLocalFile(file.fileName())));
        }
    }

    void init()
    {
        stateConfig().deleteGroup();
    }

    void offersEnumerateWhenNothingWasRememberedYet()
    {
        KIO::RenameFileDialog dialog(m_items, nullptr);
        QCOMPARE(currentOperation(dialog), int(Enumerate));
    }

    void offersTheOperationOfTheLastRename()
    {
        renameWith(AddText);

        KIO::RenameFileDialog dialog(m_items, nullptr);
        QCOMPARE(currentOperation(dialog), int(AddText));
    }

    void keepsTheOperationWhenAnotherOneIsOnlyTriedOut()
    {
        renameWith(AddText);

        {
            // Looking at Replace text and closing the dialog is not renaming with it.
            KIO::RenameFileDialog dialog(m_items, nullptr);
            QComboBox *operation = renameOperation(dialog);
            QVERIFY(operation);
            operation->setCurrentIndex(Replace);
        }

        KIO::RenameFileDialog dialog(m_items, nullptr);
        QCOMPARE(currentOperation(dialog), int(AddText));
    }

    void enumerateCanKeepTheOldNameBeforeOrAfterTheNewText()
    {
        KIO::RenameFileDialog dialog(m_items, nullptr);
        QCOMPARE(currentOperation(dialog), int(Enumerate));

        auto *nameTemplate = dialog.findChild<QLineEdit *>(QStringLiteral("enumerateTemplate"));
        auto *position = dialog.findChild<QComboBox *>(QStringLiteral("enumeratePosition"));
        auto *preview = dialog.findChild<QLineEdit *>(QStringLiteral("preview"));
        QVERIFY(nameTemplate);
        QVERIFY(position);
        QVERIFY(preview);

        nameTemplate->setText(QStringLiteral("copy #"));

        position->setCurrentIndex(ReplaceName);
        QCOMPARE(preview->text(), QStringLiteral("copy 1.txt"));

        position->setCurrentIndex(BeforeName);
        QCOMPARE(preview->text(), QStringLiteral("copy 1one.txt"));

        position->setCurrentIndex(AfterName);
        QCOMPARE(preview->text(), QStringLiteral("onecopy 1.txt"));
    }

    void enumerateTemplateFollowsTheChoiceWhileItIsUntouched()
    {
        KIO::RenameFileDialog dialog(m_items, nullptr);

        auto *nameTemplate = dialog.findChild<QLineEdit *>(QStringLiteral("enumerateTemplate"));
        auto *position = dialog.findChild<QComboBox *>(QStringLiteral("enumeratePosition"));
        auto *preview = dialog.findChild<QLineEdit *>(QStringLiteral("preview"));
        QVERIFY(nameTemplate);
        QVERIFY(position);
        QVERIFY(preview);

        const QString replaceDefault = nameTemplate->text();

        position->setCurrentIndex(AfterName);
        QCOMPARE(nameTemplate->text(), QStringLiteral("_#"));
        QCOMPARE(preview->text(), QStringLiteral("one_1.txt"));

        position->setCurrentIndex(BeforeName);
        QCOMPARE(nameTemplate->text(), QStringLiteral("#_"));
        QCOMPARE(preview->text(), QStringLiteral("1_one.txt"));

        position->setCurrentIndex(ReplaceName);
        QCOMPARE(nameTemplate->text(), replaceDefault);
    }

    void enumerateKeepsATemplateTheUserWrote()
    {
        KIO::RenameFileDialog dialog(m_items, nullptr);

        auto *nameTemplate = dialog.findChild<QLineEdit *>(QStringLiteral("enumerateTemplate"));
        auto *position = dialog.findChild<QComboBox *>(QStringLiteral("enumeratePosition"));
        QVERIFY(nameTemplate);
        QVERIFY(position);

        nameTemplate->setText(QStringLiteral("holiday #"));

        position->setCurrentIndex(AfterName);
        QCOMPARE(nameTemplate->text(), QStringLiteral("holiday #"));

        position->setCurrentIndex(ReplaceName);
        QCOMPARE(nameTemplate->text(), QStringLiteral("holiday #"));
    }

    void enumerateTakesATemplateWithoutANumberWhenItKeepsTheOldName()
    {
        // Two files share the extension, so a template on its own gives both the same name and is
        // refused. It is enough once each result carries the old name as well.
        KIO::RenameFileDialog dialog(m_items, nullptr);

        auto *nameTemplate = dialog.findChild<QLineEdit *>(QStringLiteral("enumerateTemplate"));
        auto *position = dialog.findChild<QComboBox *>(QStringLiteral("enumeratePosition"));
        auto *preview = dialog.findChild<QLineEdit *>(QStringLiteral("preview"));
        QVERIFY(nameTemplate);
        QVERIFY(position);
        QVERIFY(preview);

        nameTemplate->setText(QStringLiteral("copy of "));

        position->setCurrentIndex(ReplaceName);
        QVERIFY(!acceptButton(dialog)->isEnabled());

        position->setCurrentIndex(BeforeName);
        QVERIFY(acceptButton(dialog)->isEnabled());
        QCOMPARE(preview->text(), QStringLiteral("copy of one.txt"));
    }

    void offersEnumerateWhenTheRememberedOperationIsNoLongerKnown()
    {
        KConfigGroup group = stateConfig();
        group.writeEntry("RenameOperation", QStringLiteral("Regex"));
        group.sync();

        KIO::RenameFileDialog dialog(m_items, nullptr);
        QCOMPARE(currentOperation(dialog), int(Enumerate));
    }

    /**
     * The preview must show exactly the name the rename job goes on to create. These used
     * to disagree: the preview passed the whole file name to the rename function while
     * BatchRenameJob passed it with the extension split off. See BUG: 492660
     */
    void previewShowsTheNameTheRenameProduces_data()
    {
        QTest::addColumn<QStringList>("fileNames");
        QTest::addColumn<bool>("asDirs");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<QString>("replacement");
        QTest::addColumn<QString>("expected");

        /* clang-format off */
        // Extension unknown to QMimeDatabase: the trailing ".1" must survive.
        QTest::newRow("unknown-extension") << (QStringList{"Title_Vol.1 Ch.3.1", "Title_Vol.1 Ch.3.2"})
                                           << false
                                           << "Title_Vol.1"
                                           << QString()
                                           << "Ch.3.1";

        // The originally reported case: the same names as directories.
        QTest::newRow("unknown-extension-directories") << (QStringList{"Title_Vol.1 Ch.3.1", "Title_Vol.1 Ch.3.2"})
                                                       << true
                                                       << "Title_Vol.1"
                                                       << QString()
                                                       << "Ch.3.1";

        // A directory keeps its whole name even when it ends in a known suffix.
        QTest::newRow("directory-with-known-suffix") << (QStringList{"old_backup.tar.gz", "old_data.tar.gz"})
                                                     << true
                                                     << "old_"
                                                     << "new_"
                                                     << "new_backup.tar.gz";

        QTest::newRow("known-extension") << (QStringList{"old_photo.jpg", "old_notes.txt"})
                                         << false
                                         << "old_"
                                         << "new_"
                                         << "new_photo.jpg";

        QTest::newRow("multi-part-extension") << (QStringList{"old_archive.tar.gz", "old_backup.tar.gz"})
                                              << false
                                              << "old_"
                                              << "new_"
                                              << "new_archive.tar.gz";

        // Numeric trailing segments from the review of MR !1846.
        QTest::newRow("numeric-segments") << (QStringList{"old_result.10", "old_result.11"})
                                          << false
                                          << "old_"
                                          << "new_"
                                          << "new_result.10";
        /* clang-format on */
    }

    void previewShowsTheNameTheRenameProduces()
    {
        QFETCH(QStringList, fileNames);
        QFETCH(bool, asDirs);
        QFETCH(QString, pattern);
        QFETCH(QString, replacement);
        QFETCH(QString, expected);

        const KFileItemList items = createItems(fileNames, asDirs);
        QCOMPARE(items.count(), fileNames.count());

        // Heap-allocated: the batch rename job finishes asynchronously after the dialog
        // has been accepted, and renamingFinished is waited for below.
        auto *dialog = new KIO::RenameFileDialog(items, nullptr);

        QComboBox *combo = renameOperation(*dialog);
        QVERIFY(combo);
        combo->setCurrentIndex(Replace);

        QLineEdit *patternEdit = editWithPlaceholder(dialog, QStringLiteral("Pattern"));
        QLineEdit *replacementEdit = editWithPlaceholder(dialog, QStringLiteral("Replacement"));
        QVERIFY(patternEdit);
        QVERIFY(replacementEdit);

        patternEdit->setText(pattern);
        replacementEdit->setText(replacement);

        QLineEdit *preview = previewEdit(dialog);
        QVERIFY(preview);
        QCOMPARE(preview->text(), expected);

        // Now actually perform the rename and confirm reality agrees with the preview.
        QSignalSpy finishedSpy(dialog, &KIO::RenameFileDialog::renamingFinished);
        QSignalSpy errorSpy(dialog, &KIO::RenameFileDialog::error);

        auto *buttonBox = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttonBox);
        QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
        QVERIFY(okButton);
        QVERIFY(okButton->isEnabled());
        okButton->click();

        QVERIFY(finishedSpy.wait());
        QCOMPARE(errorSpy.count(), 0);

        const QList<QUrl> renamed = finishedSpy.first().first().value<QList<QUrl>>();
        // Every item must have been renamed: a name collision caused by a dropped
        // extension used to abort the batch after the first item.
        QCOMPARE(renamed.count(), fileNames.count());
        QCOMPARE(renamed.first().fileName(), expected);
        QVERIFY(QFile::exists(m_dir.filePath(expected)));

        for (const QUrl &url : renamed) {
            if (asDirs) {
                QDir(url.toLocalFile()).removeRecursively();
            } else {
                QFile::remove(url.toLocalFile());
            }
        }
        dialog->deleteLater();
    }

    /**
     * Add text used to split the extension off a second time inside its own rename
     * function. Once the job and the preview split it off before calling that function,
     * the second split ate a suffix of its own: "photo.jpg.jpg" lost a ".jpg".
     */
    void addTextKeepsEveryPartOfTheName_data()
    {
        QTest::addColumn<QStringList>("fileNames");
        QTest::addColumn<bool>("afterName");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QStringList>("expected");

        /* clang-format off */
        QTest::newRow("after-name-double-suffix") << (QStringList{"photo.jpg.jpg", "notes.txt"})
                                                  << true
                                                  << "_edited"
                                                  << (QStringList{"photo.jpg_edited.jpg", "notes_edited.txt"});

        QTest::newRow("before-name") << (QStringList{"photo.jpg", "notes.txt"})
                                     << false
                                     << "old_"
                                     << (QStringList{"old_photo.jpg", "old_notes.txt"});

        QTest::newRow("after-name-unknown-extension") << (QStringList{"Title_Vol.1 Ch.3.1", "Title_Vol.1 Ch.3.2"})
                                                      << true
                                                      << " (read)"
                                                      << (QStringList{"Title_Vol.1 Ch.3.1 (read)", "Title_Vol.1 Ch.3.2 (read)"});
        /* clang-format on */
    }

    void addTextKeepsEveryPartOfTheName()
    {
        QFETCH(QStringList, fileNames);
        QFETCH(bool, afterName);
        QFETCH(QString, text);
        QFETCH(QStringList, expected);

        const KFileItemList items = createItems(fileNames, false);
        QCOMPARE(items.count(), fileNames.count());

        auto *dialog = new KIO::RenameFileDialog(items, nullptr);

        QComboBox *combo = renameOperation(*dialog);
        QVERIFY(combo);
        combo->setCurrentIndex(AddText);

        QLineEdit *textEdit = editWithPlaceholder(dialog, QStringLiteral("Text to add"));
        QVERIFY(textEdit);
        QComboBox *beforeAfter = nullptr;
        const auto combos = dialog->findChildren<QComboBox *>();
        for (QComboBox *candidate : combos) {
            if (candidate != combo) {
                beforeAfter = candidate;
            }
        }
        QVERIFY(beforeAfter);
        beforeAfter->setCurrentIndex(afterName ? 1 : 0);
        textEdit->setText(text);

        QLineEdit *preview = previewEdit(dialog);
        QVERIFY(preview);
        QCOMPARE(preview->text(), expected.first());

        QSignalSpy finishedSpy(dialog, &KIO::RenameFileDialog::renamingFinished);
        QSignalSpy errorSpy(dialog, &KIO::RenameFileDialog::error);

        auto *buttonBox = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttonBox);
        QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
        QVERIFY(okButton);
        QVERIFY(okButton->isEnabled());
        okButton->click();

        QVERIFY(finishedSpy.wait());
        QCOMPARE(errorSpy.count(), 0);

        const QList<QUrl> renamed = finishedSpy.first().first().value<QList<QUrl>>();
        QStringList renamedNames;
        for (const QUrl &url : renamed) {
            renamedNames << url.fileName();
        }
        QCOMPARE(renamedNames, expected);
        for (const QString &name : expected) {
            QVERIFY2(QFile::exists(m_dir.filePath(name)), qPrintable(name));
            QFile::remove(m_dir.filePath(name));
        }
        dialog->deleteLater();
    }

    /**
     * Enumerate with the old name kept also split the extension off inside its own rename
     * function, after the caller had already done so, which lost a suffix of "photo.jpg.jpg".
     */
    void enumerateKeepsEveryPartOfTheName()
    {
        const QStringList fileNames{QStringLiteral("photo.jpg.jpg"), QStringLiteral("notes.txt")};
        const KFileItemList items = createItems(fileNames, false);
        QCOMPARE(items.count(), fileNames.count());

        auto *dialog = new KIO::RenameFileDialog(items, nullptr);
        QComboBox *combo = renameOperation(*dialog);
        QVERIFY(combo);
        combo->setCurrentIndex(Enumerate);

        auto *nameTemplate = dialog->findChild<QLineEdit *>(QStringLiteral("enumerateTemplate"));
        auto *position = dialog->findChild<QComboBox *>(QStringLiteral("enumeratePosition"));
        auto *preview = dialog->findChild<QLineEdit *>(QStringLiteral("preview"));
        QVERIFY(nameTemplate);
        QVERIFY(position);
        QVERIFY(preview);

        position->setCurrentIndex(AfterName);
        nameTemplate->setText(QStringLiteral("_#"));
        QCOMPARE(preview->text(), QStringLiteral("photo.jpg_1.jpg"));

        QSignalSpy finishedSpy(dialog, &KIO::RenameFileDialog::renamingFinished);
        QSignalSpy errorSpy(dialog, &KIO::RenameFileDialog::error);
        QVERIFY(acceptButton(*dialog)->isEnabled());
        acceptButton(*dialog)->click();

        QVERIFY(finishedSpy.wait());
        QCOMPARE(errorSpy.count(), 0);

        const QList<QUrl> renamed = finishedSpy.first().first().value<QList<QUrl>>();
        QStringList renamedNames;
        for (const QUrl &url : renamed) {
            renamedNames << url.fileName();
        }
        const QStringList expected{QStringLiteral("photo.jpg_1.jpg"), QStringLiteral("notes_2.txt")};
        QCOMPARE(renamedNames, expected);
        for (const QString &name : expected) {
            QVERIFY2(QFile::exists(m_dir.filePath(name)), qPrintable(name));
            QFile::remove(m_dir.filePath(name));
        }
        dialog->deleteLater();
    }

private:
    // The operations the dialog offers, in the order it lists them.
    enum Operation {
        Enumerate,
        Replace,
        AddText,
    };

    // Where Enumerate puts its text, in the order the choice lists them.
    enum Position {
        ReplaceName,
        BeforeName,
        AfterName,
    };

    static KConfigGroup stateConfig()
    {
        return KConfigGroup(KSharedConfig::openStateConfig(QStringLiteral("kiostaterc")), QStringLiteral("Rename dialog"));
    }

    static QPushButton *acceptButton(const KIO::RenameFileDialog &dialog)
    {
        return dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    }

    static QComboBox *renameOperation(const KIO::RenameFileDialog &dialog)
    {
        return dialog.findChild<QComboBox *>(QStringLiteral("renameOperation"));
    }

    int currentOperation(const KIO::RenameFileDialog &dialog)
    {
        QComboBox *operation = renameOperation(dialog);
        if (!operation) {
            return -1;
        }
        return operation->currentIndex();
    }

    void renameWith(Operation operation)
    {
        KIO::RenameFileDialog dialog(m_items, nullptr);
        QComboBox *combo = renameOperation(dialog);
        QVERIFY(combo);
        combo->setCurrentIndex(operation);
        dialog.accept();
    }

    KFileItemList createItems(const QStringList &fileNames, bool asDirs)
    {
        KFileItemList items;
        for (const QString &fileName : fileNames) {
            const QString path = m_dir.filePath(fileName);
            if (asDirs) {
                if (!QDir().mkpath(path)) {
                    qWarning("Could not create directory %s", qPrintable(path));
                    return {};
                }
            } else {
                QFile file(path);
                if (!file.open(QIODevice::WriteOnly)) {
                    qWarning("Could not create %s", qPrintable(path));
                    return {};
                }
                file.close();
            }
            items << KFileItem(QUrl::fromLocalFile(path));
        }
        return items;
    }

    /// The read-only line edit showing the resulting file name.
    QLineEdit *previewEdit(KIO::RenameFileDialog *dialog)
    {
        const auto edits = dialog->findChildren<QLineEdit *>();
        for (QLineEdit *edit : edits) {
            if (edit->isReadOnly()) {
                return edit;
            }
        }
        return nullptr;
    }

    QLineEdit *editWithPlaceholder(KIO::RenameFileDialog *dialog, const QString &placeholder)
    {
        const auto edits = dialog->findChildren<QLineEdit *>();
        for (QLineEdit *edit : edits) {
            if (edit->placeholderText() == placeholder) {
                return edit;
            }
        }
        return nullptr;
    }

    QTemporaryDir m_dir;
    KFileItemList m_items;
};

QTEST_MAIN(RenameFileDialogTest)

#include "renamefiledialogtest.moc"
