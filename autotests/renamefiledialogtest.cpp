/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QComboBox>
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

    static KConfigGroup stateConfig()
    {
        return KConfigGroup(KSharedConfig::openStateConfig(QStringLiteral("kiostaterc")), QStringLiteral("Rename dialog"));
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
