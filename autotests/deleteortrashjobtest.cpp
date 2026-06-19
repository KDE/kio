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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#ifdef Q_OS_WIN
#include <kio/listjob.h>
#include <kio/udsentry.h>

#include <qt_windows.h>

#include <shellapi.h>

// On Windows the trash worker drives the real recycle bin, so the test refuses to
// run when the bin is not empty (see initTestCase) and empties only what it added.
static qint64 recycleBinItemCount()
{
    SHQUERYRBINFO info;
    info.cbSize = sizeof(info);
    // Query only the drive the test operates on. SHQueryRecycleBin(nullptr) enumerates
    // every drive's recycle bin, which can block on a volume that is not a ready local disk.
    const QString root = homeTmpDir().left(2) + QLatin1Char('\\');
    return SHQueryRecycleBin(reinterpret_cast<LPCWSTR>(root.utf16()), &info) == S_OK ? info.i64NumItems : -1;
}

static void emptyRecycleBin()
{
    // Remove the $I (metadata) and $R (content) entries the shell keeps under
    // <drive>\$Recycle.Bin\<SID>\ for the test's drive. SHEmptyRecycleBin walks every
    // volume and can block on one that is not a ready local disk.
    QDir bin(homeTmpDir().left(2) + QLatin1String("\\$Recycle.Bin"));
    if (!bin.exists()) {
        return;
    }
    const QFileInfoList userBins = bin.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &userBin : userBins) {
        QDir dir(userBin.absoluteFilePath());
        const QFileInfoList items =
            dir.entryInfoList({QStringLiteral("$I*"), QStringLiteral("$R*")}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &item : items) {
            if (item.isDir()) {
                QDir(item.absoluteFilePath()).removeRecursively();
            } else {
                QFile::remove(item.absoluteFilePath());
            }
        }
    }
}

// The recycle-bin item URL is the shell parse name the worker reports, not the Unix
// trash id, so it has to be looked up by listing the trash.
static QUrl firstTrashItemUrl()
{
    KIO::ListJob *job = KIO::listDir(QUrl(QStringLiteral("trash:/")),
                                     KIO::HideProgressInfo,
                                     KIO::ListJob::ListFlag::IncludeHidden | KIO::ListJob::ListFlag::ExcludeDotAndDotDot);
    QUrl url;
    QObject::connect(job, &KIO::ListJob::entries, job, [&url](KIO::Job *, const KIO::UDSEntryList &list) {
        if (url.isEmpty() && !list.isEmpty()) {
            url.setScheme(QStringLiteral("trash"));
            url.setPath(QLatin1Char('/') + list.first().stringValue(KIO::UDSEntry::UDS_NAME));
        }
    });
    job->exec();
    return url;
}
#endif

class DeleteOrTrashJobTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanup();

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

void DeleteOrTrashJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);

#ifdef Q_OS_WIN
    // The trash tests below operate on the real recycle bin, so stop the whole suite
    // rather than risk mixing with, or emptying, a bin that holds the user's files.
    if (recycleBinItemCount() != 0) {
        QSKIP("The recycle bin is not empty (or could not be queried); refusing to run.");
    }

    // Probe whether trashing actually works; restricted environments deny the recycle
    // bin, in which case the whole suite skips instead of failing or hanging.
    const QString probe = homeTmpDir() + "delete_or_trash_probe";
    createTestFile(probe);
    KIO::CopyJob *probeJob = KIO::move(QUrl::fromLocalFile(probe), QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    probeJob->setUiDelegate(nullptr);
    const bool canTrash = probeJob->exec() && !QFile::exists(probe);
    QFile::remove(probe);
    emptyRecycleBin();
    if (!canTrash) {
        QSKIP("Trashing is not available in this environment (the recycle bin denied the operation).");
    }
#endif
}

void DeleteOrTrashJobTest::cleanup()
{
#ifdef Q_OS_WIN
    // The bin started empty, so whatever a test left behind is ours to drop.
    emptyRecycleBin();
#endif
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
    const QString path = homeTmpDir() + "delete_or_trash_job_test_file";
    createTestFile(path);

    KIO::CopyJob *trashJob = KIO::move(QUrl::fromLocalFile(path), QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    trashJob->setUiDelegate(nullptr);
    QVERIFY(trashJob->exec());

#ifdef Q_OS_WIN
    const QUrl trashUrl = firstTrashItemUrl();
    QVERIFY(!trashUrl.isEmpty());
#else
    const QUrl trashUrl("trash:/0-delete_or_trash_job_test_file");
#endif

    auto [job, askUserHandler] = createJobWithUrl(AskIface::Trash, trashUrl);
    bool res = job->exec();
    QVERIFY(res);
    QCOMPARE(askUserHandler->m_delType, KIO::AskUserActionInterface::DeletionType::Delete);
    QCOMPARE(askUserHandler->m_askUserDeleteCalled, 1);
}

#include "deleteortrashjobtest.moc"
