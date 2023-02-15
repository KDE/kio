/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2022 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "copyjob.h"
#include "kiotesthelper.h"
#include "mockcoredelegateextensions.h"

#include <kio/deletejob.h>
#include <kio/deleteortrashjob.h>

#include <KJobUiDelegate>

#include <QObject>

class DeleteOrTrashJobTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void deleteFileTest();
    void moveToTrashTest();
    void emptyTrashTest();
    void deleteTrashFileTest();
};

QTEST_MAIN(DeleteOrTrashJobTest)

using AskIface = KIO::AskUserActionInterface;

struct Info {
    KIO::DeleteOrTrashJob *job = nullptr;
    MockAskUserInterface *askUserHandler = nullptr;
};

static Info createJobWithUrl(AskIface::DeletionType deletionType, const QUrl &url)
{
    auto *job = new KIO::DeleteOrTrashJob({url}, //
                                          deletionType,
                                          AskIface::DefaultConfirmation,
                                          nullptr);

    auto uiDelegate = new KJobUiDelegate{};
    job->setUiDelegate(uiDelegate);
    auto askUserHandler = new MockAskUserInterface(uiDelegate);
    askUserHandler->m_deleteResult = true;
    return {job, askUserHandler};
}

static Info createJob(AskIface::DeletionType deletionType)
{
    const QString path = homeTmpDir() + "delete_or_trash_job_test_file";
    createTestFile(path);
    return createJobWithUrl(deletionType, QUrl::fromLocalFile(path));
}

void DeleteOrTrashJobTest::deleteFileTest()
{
    auto [job, askUserHandler] = createJob(AskIface::Delete);
    QVERIFY(job->exec());
    QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
    QCOMPARE(askUserHandler->m_delType, KIO::AskUserActionInterface::DeletionType::Delete);
}

void DeleteOrTrashJobTest::moveToTrashTest()
{
    auto [job, askUserHandler] = createJob(AskIface::Trash);
    QVERIFY(job->exec());
    QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
    QCOMPARE(askUserHandler->m_delType, KIO::AskUserActionInterface::DeletionType::Trash);
}

void DeleteOrTrashJobTest::emptyTrashTest()
{
    auto [job, askUserHandler] = createJob(AskIface::EmptyTrash);
    QVERIFY(job->exec());
    QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
    QCOMPARE(askUserHandler->m_delType, KIO::AskUserActionInterface::DeletionType::EmptyTrash);
}

void DeleteOrTrashJobTest::deleteTrashFileTest()
{
    QStandardPaths::setTestModeEnabled(true);

    const QString path = homeTmpDir() + "delete_or_trash_job_test_file";
    createTestFile(path);

    KIO::CopyJob *trashJob = KIO::move(QUrl::fromLocalFile(path), QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    trashJob->setUiDelegate(nullptr);
    QVERIFY(trashJob->exec());

    auto [job, askUserHandler] = createJobWithUrl(AskIface::Trash, QUrl("trash:/0-delete_or_trash_job_test_file"));
    bool res = job->exec();
    QVERIFY(res);
    QCOMPARE(askUserHandler->m_delType, KIO::AskUserActionInterface::DeletionType::Delete);
    QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
}

#include "deleteortrashjobtest.moc"
