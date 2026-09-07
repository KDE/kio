/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
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

    QTemporaryDir m_dir;
    KFileItemList m_items;
};

QTEST_MAIN(RenameFileDialogTest)

#include "renamefiledialogtest.moc"
