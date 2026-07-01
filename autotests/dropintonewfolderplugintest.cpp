/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWidget>

#include <KFileItem>
#include <KFileItemListProperties>
#include <KPluginFactory>
#include <KPluginMetaData>

#include <KIO/DndPopupMenuPlugin>

#ifndef DROPINTONEWFOLDER_PLUGIN_PATH
#error "DROPINTONEWFOLDER_PLUGIN_PATH must be defined by the build system"
#endif

class DropIntoNewFolderPluginTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testSkipsNonLocalDestination()
    {
        auto plugin = loadPlugin();
        QVERIFY(plugin);
        const auto actions = plugin->setup(propsFor({QUrl(QStringLiteral("file:///tmp/some/file"))}), QUrl(QStringLiteral("sftp://host/dir")));
        QVERIFY2(actions.isEmpty(), "the plugin must not offer the action for a non-local destination");
        delete plugin;
    }

    void testOffersActionForLocalDir()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("a.txt"));
        QVERIFY(createFile(file));

        auto plugin = loadPlugin();
        QVERIFY(plugin);
        const auto actions = plugin->setup(propsFor({QUrl::fromLocalFile(file)}), QUrl::fromLocalFile(dir.path()));
        QCOMPARE(actions.size(), 1);
        QVERIFY(actions.first()->isEnabled());
        delete plugin;
    }

    // Regression test for the lifetime bug: the DropJob owns and destroys the plugin as soon as the
    // action is triggered, so the (asynchronous) folder creation and move must complete even after
    // the plugin is gone. Deleting the plugin here reproduces what DropJob does.
    void testMoveSurvivesPluginDeletion()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        // Two "dragged" source files and a drop destination directory.
        const QString srcA = dir.filePath(QStringLiteral("a.txt"));
        const QString srcB = dir.filePath(QStringLiteral("b.txt"));
        QVERIFY(createFile(srcA));
        QVERIFY(createFile(srcB));
        const QString destDir = dir.filePath(QStringLiteral("dest"));
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("dest")));

        // Give the plugin an active window to parent its dialog to (as it does via
        // QApplication::activeWindow()).
        QWidget window;
        window.show();
        window.activateWindow();

        auto plugin = loadPlugin();
        QVERIFY(plugin);
        const auto actions = plugin->setup(propsFor({QUrl::fromLocalFile(srcA), QUrl::fromLocalFile(srcB)}), QUrl::fromLocalFile(destDir));
        QCOMPARE(actions.size(), 1);

        actions.first()->trigger();
        // Simulate DropJob deleting the plugin right after the action fired.
        delete plugin;

        // The New Folder name dialog must still appear (it would not if it had been destroyed with
        // the plugin).
        QDialog *dialog = nullptr;
        QTRY_VERIFY(dialog = findDialog());

        auto *lineEdit = dialog->findChild<QLineEdit *>();
        QVERIFY(lineEdit);
        lineEdit->setText(QStringLiteral("moved-here"));

        auto *buttons = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        auto *ok = buttons->button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        QTRY_VERIFY(ok->isEnabled()); // enabled once the name-availability StatJob finishes
        ok->click();

        // The folder is created and the two files moved into it - all after the plugin was deleted.
        const QString newFolder = destDir + QStringLiteral("/moved-here");
        QTRY_VERIFY(QFileInfo::exists(newFolder + QStringLiteral("/a.txt")));
        QTRY_VERIFY(QFileInfo::exists(newFolder + QStringLiteral("/b.txt")));
        QVERIFY(!QFileInfo::exists(srcA));
        QVERIFY(!QFileInfo::exists(srcB));
    }

private:
    static KIO::DndPopupMenuPlugin *loadPlugin()
    {
        KPluginMetaData md(QString::fromUtf8(DROPINTONEWFOLDER_PLUGIN_PATH));
        if (!md.isValid()) {
            return nullptr;
        }
        return KPluginFactory::instantiatePlugin<KIO::DndPopupMenuPlugin>(md, nullptr, {}).plugin;
    }

    static KFileItemListProperties propsFor(const QList<QUrl> &urls)
    {
        KFileItemList items;
        items.reserve(urls.size());
        for (const QUrl &url : urls) {
            items << KFileItem(url);
        }
        return KFileItemListProperties(items);
    }

    static bool createFile(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write("x") == 1;
    }

    static QDialog *findDialog()
    {
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *w : widgets) {
            if (auto *d = qobject_cast<QDialog *>(w)) {
                return d;
            }
            if (auto *d = w->findChild<QDialog *>()) {
                return d;
            }
        }
        return nullptr;
    }
};

QTEST_MAIN(DropIntoNewFolderPluginTest)

#include "dropintonewfolderplugintest.moc"
