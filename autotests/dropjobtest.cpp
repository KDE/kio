/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QMenu>
#include <QMimeData>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "kiotesthelper.h"

#include <KIO/DropJob>
#include <KIO/StatJob>
#include <KIO/CopyJob>
#include <KIO/DeleteJob>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KFileItemListProperties>
#include <KJobUiDelegate>
#include "mockcoredelegateextensions.h"

Q_DECLARE_METATYPE(Qt::KeyboardModifiers)
Q_DECLARE_METATYPE(Qt::DropAction)
Q_DECLARE_METATYPE(Qt::DropActions)
Q_DECLARE_METATYPE(KFileItemListProperties)

#ifndef Q_OS_WIN
void initLocale()
{
    setenv("LC_ALL", "en_US.utf-8", 1);
}
Q_CONSTRUCTOR_FUNCTION(initLocale)
#endif


class JobSpy : public QObject
{
    Q_OBJECT
public:
    JobSpy(KIO::Job *job)
        : QObject(nullptr),
          m_spy(job, &KJob::result),
          m_error(0)
    {
        connect(job, &KJob::result, this, [this](KJob * job) {
            m_error = job->error();
        });
    }
    // like job->exec(), but with a timeout (to avoid being stuck with a popup grabbing mouse and keyboard...)
    bool waitForResult()
    {
        // implementation taken from QTRY_COMPARE, to move the QVERIFY to the caller
        if (m_spy.isEmpty()) {
            QTest::qWait(0);
        }
        for (int i = 0; i < 5000 && m_spy.isEmpty(); i += 50) {
            QTest::qWait(50);
        }
        return !m_spy.isEmpty();
    }
    int error() const
    {
        return m_error;
    }

private:
    QSignalSpy m_spy;
    int m_error;
};

class DropJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qputenv("KIOSLAVE_ENABLE_TESTMODE", "1"); // ensure the ioslaves call QStandardPaths::setTestModeEnabled too

        // To avoid a runtime dependency on klauncher
        qputenv("KDE_FORK_SLAVES", "yes");

        const QString trashDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/Trash");
        QDir(trashDir).removeRecursively();

        QVERIFY(m_tempDir.isValid());
        QVERIFY(m_nonWritableTempDir.isValid());
        QVERIFY(QFile(m_nonWritableTempDir.path()).setPermissions(QFile::ReadOwner | QFile::ReadUser | QFile::ExeOwner | QFile::ExeUser));
        m_srcDir = m_tempDir.path();

        m_srcFile = m_srcDir + "/srcfile";
        m_srcLink = m_srcDir + "/link";

        qRegisterMetaType<KIO::CopyJob*>();
    }

    void cleanupTestCase()
    {
        QVERIFY(QFile(m_nonWritableTempDir.path()).setPermissions(QFile::ReadOwner | QFile::ReadUser | QFile::WriteOwner | QFile::WriteUser | QFile::ExeOwner | QFile::ExeUser));
    }

    // Before every test method, ensure the test file m_srcFile exists
    void init()
    {
        if (QFile::exists(m_srcFile)) {
            QVERIFY(QFileInfo(m_srcFile).isWritable());
        } else {
            QFile srcFile(m_srcFile);
            QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
            srcFile.write("Hello world\n");
        }
#ifndef Q_OS_WIN
        if (!QFile::exists(m_srcLink)) {
            QVERIFY(QFile(m_srcFile).link(m_srcLink));
            QVERIFY(QFileInfo(m_srcLink).isSymLink());
        }
#endif
        QVERIFY(QFileInfo(m_srcFile).isWritable());
        m_mimeData.setUrls(QList<QUrl>{QUrl::fromLocalFile(m_srcFile)});
    }

    void shouldDropToDesktopFile()
    {
        // Given an executable application desktop file and a source file
        const QString desktopPath = m_srcDir + "/target.desktop";
        KDesktopFile desktopFile(desktopPath);
        KConfigGroup desktopGroup = desktopFile.desktopGroup();
        desktopGroup.writeEntry("Type", "Application");
        desktopGroup.writeEntry("StartupNotify", "false");
#ifdef Q_OS_WIN
        desktopGroup.writeEntry("Exec", "copy.exe %f %d/dest");
#else
        desktopGroup.writeEntry("Exec", "cp %f %d/dest");
#endif
        desktopFile.sync();
        QFile file(desktopPath);
        file.setPermissions(file.permissions() | QFile::ExeOwner | QFile::ExeUser);

        // When dropping the source file onto the desktop file
        QUrl destUrl = QUrl::fromLocalFile(desktopPath);
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, destUrl, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::DropJob::itemCreated);

        // Then the application is run with the source file as argument
        // (in this example, it copies the source file to "dest")
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 0);
        const QString dest = m_srcDir + "/dest";
        QTRY_VERIFY(QFile::exists(dest));

        QVERIFY(QFile::remove(desktopPath));
        QVERIFY(QFile::remove(dest));
    }

    void shouldDropToDirectory_data()
    {
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
        QTest::addColumn<Qt::DropAction>("dropAction"); // Qt's dnd support sets it from the modifiers, we fake it here
        QTest::addColumn<QString>("srcFile");
        QTest::addColumn<QString>("dest"); // empty for a temp dir
        QTest::addColumn<int>("expectedError");
        QTest::addColumn<bool>("shouldSourceStillExist");

        QTest::newRow("Ctrl") << Qt::KeyboardModifiers(Qt::ControlModifier) << Qt::CopyAction << m_srcFile << QString()
                              << 0 << true;
        QTest::newRow("Shift") << Qt::KeyboardModifiers(Qt::ShiftModifier) << Qt::MoveAction << m_srcFile << QString()
                               << 0 << false;
        QTest::newRow("Ctrl_Shift") << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier) << Qt::LinkAction << m_srcFile << QString()
                                    << 0 << true;
        QTest::newRow("DropOnItself") << Qt::KeyboardModifiers() << Qt::CopyAction << m_srcDir << m_srcDir
                                      << int(KIO::ERR_DROP_ON_ITSELF) << true;
        QTest::newRow("DropDirOnFile") << Qt::KeyboardModifiers(Qt::ControlModifier) << Qt::CopyAction << m_srcDir << m_srcFile
                                       << int(KIO::ERR_ACCESS_DENIED) << true;
        QTest::newRow("NonWritableDest") << Qt::KeyboardModifiers() << Qt::CopyAction << m_srcFile << m_nonWritableTempDir.path()
                                         << int(KIO::ERR_WRITE_ACCESS_DENIED) << true;
    }

    void shouldDropToDirectory()
    {
        QFETCH(Qt::KeyboardModifiers, modifiers);
        QFETCH(Qt::DropAction, dropAction);
        QFETCH(QString, srcFile);
        QFETCH(QString, dest);
        QFETCH(int, expectedError);
        QFETCH(bool, shouldSourceStillExist);

        // Given a directory and a source file
        QTemporaryDir tempDestDir;
        QVERIFY(tempDestDir.isValid());
        if (dest.isEmpty()) {
            dest = tempDestDir.path();
        }

        // When dropping the source file onto the directory
        const QUrl destUrl = QUrl::fromLocalFile(dest);
        m_mimeData.setUrls(QList<QUrl>{QUrl::fromLocalFile(srcFile)});
        QDropEvent dropEvent(QPoint(10, 10), dropAction, &m_mimeData, Qt::LeftButton, modifiers);
        KIO::DropJob *job = KIO::drop(&dropEvent, destUrl, KIO::HideProgressInfo | KIO::NoPrivilegeExecution);
        job->setUiDelegate(nullptr);
        job->setUiDelegateExtension(nullptr);
        JobSpy jobSpy(job);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QSignalSpy itemCreatedSpy(job, &KIO::DropJob::itemCreated);

        // Then the file is copied
        QVERIFY(jobSpy.waitForResult());
        QCOMPARE(jobSpy.error(), expectedError);
        if (expectedError == 0) {
            QCOMPARE(copyJobSpy.count(), 1);
            const QString destFile = dest + "/srcfile";
            QCOMPARE(itemCreatedSpy.count(), 1);
            QCOMPARE(itemCreatedSpy.at(0).at(0).value<QUrl>(), QUrl::fromLocalFile(destFile));
            QVERIFY(QFile::exists(destFile));
            QCOMPARE(QFile::exists(m_srcFile), shouldSourceStillExist);
            if (dropAction == Qt::LinkAction) {
                QVERIFY(QFileInfo(destFile).isSymLink());
            }
        }
    }

    void shouldDropToTrash_data()
    {
        QTest::addColumn<Qt::KeyboardModifiers>("modifiers");
        QTest::addColumn<Qt::DropAction>("dropAction"); // Qt's dnd support sets it from the modifiers, we fake it here
        QTest::addColumn<QString>("srcFile");

        QTest::newRow("Ctrl") << Qt::KeyboardModifiers(Qt::ControlModifier) << Qt::CopyAction << m_srcFile;
        QTest::newRow("Shift") << Qt::KeyboardModifiers(Qt::ShiftModifier) << Qt::MoveAction << m_srcFile;
        QTest::newRow("Ctrl_Shift") << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier) << Qt::LinkAction << m_srcFile;
        QTest::newRow("NoModifiers") << Qt::KeyboardModifiers() << Qt::CopyAction << m_srcFile;
#ifndef Q_OS_WIN
        QTest::newRow("Link_Ctrl") << Qt::KeyboardModifiers(Qt::ControlModifier) << Qt::CopyAction << m_srcLink;
        QTest::newRow("Link_Shift") << Qt::KeyboardModifiers(Qt::ShiftModifier) << Qt::MoveAction << m_srcLink;
        QTest::newRow("Link_Ctrl_Shift") << Qt::KeyboardModifiers(Qt::ControlModifier | Qt::ShiftModifier) << Qt::LinkAction << m_srcLink;
        QTest::newRow("Link_NoModifiers") << Qt::KeyboardModifiers() << Qt::CopyAction << m_srcLink;
#endif
    }

    void shouldDropToTrash()
    {
        // Given a source file
        QFETCH(Qt::KeyboardModifiers, modifiers);
        QFETCH(Qt::DropAction, dropAction);
        QFETCH(QString, srcFile);
        const bool isLink = QFileInfo(srcFile).isSymLink();

        // When dropping it into the trash, with <modifiers> pressed
        m_mimeData.setUrls(QList<QUrl>{QUrl::fromLocalFile(srcFile)});
        QDropEvent dropEvent(QPoint(10, 10), dropAction, &m_mimeData, Qt::LeftButton, modifiers);
        KIO::DropJob *job = KIO::drop(&dropEvent, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QSignalSpy itemCreatedSpy(job, &KIO::DropJob::itemCreated);

        // Then a confirmation dialog should appear
        auto *uiDelegate = new KJobUiDelegate;
        job->setUiDelegate(uiDelegate);
        auto *askUserHandler = new MockAskUserInterface(uiDelegate);
        askUserHandler->m_deleteResult = true;

        // And the file should be moved to the trash, no matter what the modifiers are
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
        QCOMPARE(copyJobSpy.count(), 1);
        QCOMPARE(itemCreatedSpy.count(), 1);
        const QUrl trashUrl = itemCreatedSpy.at(0).at(0).value<QUrl>();
        QCOMPARE(trashUrl.scheme(), QString("trash"));
        KIO::StatJob *statJob = KIO::stat(trashUrl, KIO::HideProgressInfo);
        QVERIFY(statJob->exec());
        if (isLink) {
            QVERIFY(statJob->statResult().isLink());
        }

        // clean up
        KIO::DeleteJob *delJob = KIO::del(trashUrl, KIO::HideProgressInfo);
        QVERIFY2(delJob->exec(), qPrintable(delJob->errorString()));
    }

    void shouldDropFromTrash()
    {
        // Given a file in the trash
        const QFile::Permissions origPerms = QFileInfo(m_srcFile).permissions();
        QVERIFY(QFileInfo(m_srcFile).isWritable());
        KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(m_srcFile), QUrl(QStringLiteral("trash:/")));
        QSignalSpy copyingDoneSpy(copyJob, &KIO::CopyJob::copyingDone);
        QVERIFY(copyJob->exec());
        const QUrl trashUrl = copyingDoneSpy.at(0).at(2).value<QUrl>();
        QVERIFY(trashUrl.isValid());
        QVERIFY(!QFile::exists(m_srcFile));

        // When dropping the trashed file into a local dir, without modifiers
        m_mimeData.setUrls(QList<QUrl>{trashUrl});
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, QUrl::fromLocalFile(m_srcDir), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QSignalSpy spy(job, &KIO::DropJob::itemCreated);

        // Then the file should be moved, without a popup. No point in copying out of the trash, or linking to it.
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(copyJobSpy.count(), 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<QUrl>(), QUrl::fromLocalFile(m_srcFile));
        QVERIFY(QFile::exists(m_srcFile));
        QCOMPARE(int(QFileInfo(m_srcFile).permissions()), int(origPerms));
        QVERIFY(QFileInfo(m_srcFile).isWritable());
        KIO::StatJob *statJob = KIO::stat(trashUrl, KIO::HideProgressInfo);
        QVERIFY(!statJob->exec());
        QVERIFY(QFileInfo(m_srcFile).isWritable());
    }

    void shouldDropTrashRootWithoutMovingAllTrashedFiles() // #319660
    {
        // Given some stuff in the trash
        const QUrl trashUrl(QStringLiteral("trash:/"));
        KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(m_srcFile), trashUrl);
        QVERIFY(copyJob->exec());
        // and an empty destination directory
        QTemporaryDir tempDestDir;
        QVERIFY(tempDestDir.isValid());
        const QUrl destUrl = QUrl::fromLocalFile(tempDestDir.path());

        // When dropping a link / icon of the trash...
        m_mimeData.setUrls(QList<QUrl>{trashUrl});
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, destUrl, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        // Then a full move shouldn't happen, just a link
        QCOMPARE(copyJobSpy.count(), 1);
        const QStringList items = QDir(tempDestDir.path()).entryList();
        QVERIFY2(!items.contains("srcfile"), qPrintable(items.join(',')));
        QVERIFY2(items.contains("trash:" + QChar(0x2044) + ".desktop"), qPrintable(items.join(',')));
    }

    void shouldDropFromTrashToTrash() // #378051
    {
        // Given a file in the trash
        QVERIFY(QFileInfo(m_srcFile).isWritable());
        KIO::CopyJob *copyJob = KIO::move(QUrl::fromLocalFile(m_srcFile), QUrl(QStringLiteral("trash:/")));
        QSignalSpy copyingDoneSpy(copyJob, &KIO::CopyJob::copyingDone);
        QVERIFY(copyJob->exec());
        const QUrl trashUrl = copyingDoneSpy.at(0).at(2).value<QUrl>();
        QVERIFY(trashUrl.isValid());
        QVERIFY(!QFile::exists(m_srcFile));

        // When dropping the trashed file in the trash
        m_mimeData.setUrls(QList<QUrl>{trashUrl});
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QSignalSpy spy(job, &KIO::DropJob::itemCreated);

        // Then an error should be reported and no files action should occur
        QVERIFY(!job->exec());
        QCOMPARE(job->error(), KIO::ERR_DROP_ON_ITSELF);
    }


    void shouldDropToDirectoryWithPopup_data()
    {
        QTest::addColumn<QString>("dest"); // empty for a temp dir
        QTest::addColumn<Qt::DropActions>("offeredActions");
        QTest::addColumn<int>("triggerActionNumber");
        QTest::addColumn<int>("expectedError");
        QTest::addColumn<Qt::DropAction>("expectedDropAction");
        QTest::addColumn<bool>("shouldSourceStillExist");

        const Qt::DropActions threeActions = Qt::MoveAction | Qt::CopyAction | Qt::LinkAction;
        const Qt::DropActions copyAndLink = Qt::CopyAction | Qt::LinkAction;
        QTest::newRow("Move") << QString() << threeActions << 0 << 0 << Qt::MoveAction << false;
        QTest::newRow("Copy") << QString() << threeActions << 1 << 0 << Qt::CopyAction << true;
        QTest::newRow("Link") << QString() << threeActions << 2 << 0 << Qt::LinkAction << true;
        QTest::newRow("SameDestCopy") << m_srcDir << copyAndLink << 0 << int(KIO::ERR_IDENTICAL_FILES) << Qt::CopyAction << true;
        QTest::newRow("SameDestLink") << m_srcDir << copyAndLink << 1 << int(KIO::ERR_FILE_ALREADY_EXIST) << Qt::LinkAction << true;
    }

    void shouldDropToDirectoryWithPopup()
    {
        QFETCH(QString, dest);
        QFETCH(Qt::DropActions, offeredActions);
        QFETCH(int, triggerActionNumber);
        QFETCH(int, expectedError);
        QFETCH(Qt::DropAction, expectedDropAction);
        QFETCH(bool, shouldSourceStillExist);

        // Given a directory and a source file
        QTemporaryDir tempDestDir;
        QVERIFY(tempDestDir.isValid());
        if (dest.isEmpty()) {
            dest = tempDestDir.path();
        }
        QVERIFY(!findPopup());

        // When dropping the source file onto the directory
        QUrl destUrl = QUrl::fromLocalFile(dest);
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction /*unused*/, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, destUrl, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->setUiDelegateExtension(nullptr); // no rename dialog
        JobSpy jobSpy(job);
        qRegisterMetaType<KFileItemListProperties>();
        QSignalSpy spyShow(job, &KIO::DropJob::popupMenuAboutToShow);
        QSignalSpy copyJobSpy(job, &KIO::DropJob::copyJobStarted);
        QVERIFY(spyShow.isValid());

        // Then a popup should appear, with the expected available actions
        QVERIFY(spyShow.wait());
        QTRY_VERIFY(findPopup());
        QMenu *popup = findPopup();
        QCOMPARE(int(popupDropActions(popup)), int(offeredActions));

        // And when selecting action number <triggerActionNumber>
        QAction *action = popup->actions().at(triggerActionNumber);
        QVERIFY(action);
        QCOMPARE(int(action->data().value<Qt::DropAction>()), int(expectedDropAction));
        const QRect actionGeom = popup->actionGeometry(action);
        QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, actionGeom.center());

        // Then the job should finish, and the chosen action should happen.
        QVERIFY(jobSpy.waitForResult());
        QCOMPARE(jobSpy.error(), expectedError);
        if (expectedError == 0) {
            QCOMPARE(copyJobSpy.count(), 1);
            const QString destFile = dest + "/srcfile";
            QVERIFY(QFile::exists(destFile));
            QCOMPARE(QFile::exists(m_srcFile), shouldSourceStillExist);
            if (expectedDropAction == Qt::LinkAction) {
                QVERIFY(QFileInfo(destFile).isSymLink());
            }
        }
        QTRY_VERIFY(!findPopup()); // flush deferred delete, so we don't get this popup again in findPopup
    }

    void shouldAddApplicationActionsToPopup()
    {
        // Given a directory and a source file
        QTemporaryDir tempDestDir;
        QVERIFY(tempDestDir.isValid());
        const QUrl destUrl = QUrl::fromLocalFile(tempDestDir.path());

        // When dropping the source file onto the directory
        QDropEvent dropEvent(QPoint(10, 10), Qt::CopyAction /*unused*/, &m_mimeData, Qt::LeftButton, Qt::NoModifier);
        KIO::DropJob *job = KIO::drop(&dropEvent, destUrl, KIO::HideProgressInfo);
        QAction appAction1(QStringLiteral("action1"), this);
        QAction appAction2(QStringLiteral("action2"), this);
        QList<QAction *> appActions; appActions << &appAction1 << &appAction2;
        job->setUiDelegate(nullptr);
        job->setApplicationActions(appActions);
        JobSpy jobSpy(job);

        // Then a popup should appear, with the expected available actions
        QTRY_VERIFY(findPopup());
        QMenu *popup = findPopup();
        const QList<QAction *> actions = popup->actions();
        QVERIFY(actions.contains(&appAction1));
        QVERIFY(actions.contains(&appAction2));
        QVERIFY(actions.at(actions.indexOf(&appAction1) - 1)->isSeparator());
        QVERIFY(actions.at(actions.indexOf(&appAction2) + 1)->isSeparator());

        // And when selecting action appAction1
        const QRect actionGeom = popup->actionGeometry(&appAction1);
        QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, actionGeom.center());

        // Then the menu should hide and the job terminate (without doing any copying)
        QVERIFY(jobSpy.waitForResult());
        QCOMPARE(jobSpy.error(), 0);
        const QString destFile = tempDestDir.path() + "/srcfile";
        QVERIFY(!QFile::exists(destFile));
    }

private:
    static QMenu *findPopup()
    {
        const QList<QWidget *> widgetsList = qApp->topLevelWidgets();
        for (QWidget *widget : widgetsList) {
            if (QMenu *menu = qobject_cast<QMenu *>(widget)) {
                return menu;
            }
        }
        return nullptr;
    }
    static Qt::DropActions popupDropActions(QMenu *menu)
    {
        Qt::DropActions actions;
        const QList<QAction *> actionsList = menu->actions();
        for (const QAction *action : actionsList) {
            const QVariant userData = action->data();
            if (userData.isValid()) {
                actions |= userData.value<Qt::DropAction>();
            }
        }
        return actions;
    }
    QMimeData m_mimeData; // contains m_srcFile
    QTemporaryDir m_tempDir;
    QString m_srcDir;
    QString m_srcFile;
    QString m_srcLink;
    QTemporaryDir m_nonWritableTempDir;
};

QTEST_MAIN(DropJobTest)

#include "dropjobtest.moc"

