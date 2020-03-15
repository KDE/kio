/*
    This file is part of the KDE libraries
    Copyright (c) 2014, 2020 David Faure <faure@kde.org>

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

#include "processlauncherjobtest.h"
#include "processlauncherjob.h"

#include "kiotesthelper.h" // createTestFile etc.

#include <kservice.h>
#include <kconfiggroup.h>
#include <KDesktopFile>

#ifdef Q_OS_UNIX
#include <signal.h> // kill
#endif

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <kprocessrunner_p.h>

QTEST_GUILESS_MAIN(ProcessLauncherJobTest)

void ProcessLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void ProcessLauncherJobTest::cleanupTestCase()
{
    std::for_each(m_filesToRemove.begin(), m_filesToRemove.end(), [](const QString & f) {
        QFile::remove(f);
    });
}

static const char s_tempServiceName[] = "processlauncherjobtest_service.desktop";

static void createSrcFile(const QString path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    srcFile.write("Hello world\n");
}

void ProcessLauncherJobTest::startProcess_data()
{
    QTest::addColumn<bool>("tempFile");
    QTest::addColumn<bool>("useExec");
    QTest::addColumn<int>("numFiles");

    QTest::newRow("1_file_exec") << false << true << 1;
    QTest::newRow("1_file_waitForStarted") << false << false << 1;
    QTest::newRow("1_tempfile_exec") << true << true << 1;
    QTest::newRow("1_tempfile_waitForStarted") << true << false << 1;

    QTest::newRow("2_files_exec") << false << true << 2;
    QTest::newRow("2_files_waitForStarted") << false << false << 2;
    QTest::newRow("2_tempfiles_exec") << true << true << 2;
    QTest::newRow("2_tempfiles_waitForStarted") << true << false << 2;
}

void ProcessLauncherJobTest::startProcess()
{
    QFETCH(bool, tempFile);
    QFETCH(bool, useExec);
    QFETCH(int, numFiles);

    // Given a service desktop file and a number of source files
    const QString path = createTempService();
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    QList<QUrl> urls;
    for (int i = 0; i < numFiles; ++i) {
        const QString srcFile = srcDir + "/srcfile" + QString::number(i + 1);
        createSrcFile(srcFile);
        QVERIFY(QFile::exists(srcFile));
        urls.append(QUrl::fromLocalFile(srcFile));
    }

    // When running a ProcessLauncherJob
    KService::Ptr servicePtr(new KService(path));
    KIO::ProcessLauncherJob *job = new KIO::ProcessLauncherJob(servicePtr, WId{}, this);
    job->setUrls(urls);
    if (tempFile) {
        job->setRunFlags(KIO::ProcessLauncherJob::DeleteTemporaryFiles);
    }
    if (useExec) {
        QVERIFY(job->exec());
    } else {
        job->start();
        QVERIFY(job->waitForStarted());
    }
    const QVector<qint64> pids = job->pids();

    // Then the service should be executed (which copies the source file to "dest")
    QCOMPARE(pids.count(), numFiles);
    QVERIFY(!pids.contains(0));
    for (int i = 0; i < numFiles; ++i) {
        const QString dest = srcDir + "/dest_srcfile" + QString::number(i + 1);
        QTRY_VERIFY2(QFile::exists(dest), qPrintable(dest));
        QVERIFY(QFile::exists(srcDir + "/srcfile" + QString::number(i + 1))); // if tempfile is true, kioexec will delete it... in 3 minutes.
        QVERIFY(QFile::remove(dest)); // cleanup
    }

#ifdef Q_OS_UNIX
    // Kill the running kioexec processes
    for (qint64 pid : pids) {
        ::kill(pid, SIGTERM);
    }
#endif

    // The kioexec processes that are waiting for 3 minutes and got killed above,
    // will now trigger KProcessRunner::slotProcessError, KProcessRunner::slotProcessExited and delete the KProcessRunner.
    // We wait for that to happen otherwise it gets confusing to see that output from later tests.
    QTRY_COMPARE(KProcessRunner::instanceCount(), 0);
}

void ProcessLauncherJobTest::shouldFailOnNonExecutableDesktopFile()
{
    // Given a .desktop file in a temporary directory (outside the trusted paths)
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString desktopFilePath = srcDir + "/shouldfail.desktop";
    writeTempServiceDesktopFile(desktopFilePath);
    m_filesToRemove.append(desktopFilePath);

    const QString srcFile = srcDir + "/srcfile";
    createSrcFile(srcFile);
    const QList<QUrl> urls{QUrl::fromLocalFile(srcFile)};
    KService::Ptr servicePtr(new KService(desktopFilePath));
    KIO::ProcessLauncherJob *job = new KIO::ProcessLauncherJob(servicePtr, WId{}, this);
    job->setUrls(urls);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KJob::UserDefinedError);
    QCOMPARE(job->errorString(), QStringLiteral("You are not authorized to execute this file."));
}

void ProcessLauncherJobTest::shouldFailOnNonExistingExecutable_data()
{
    QTest::addColumn<bool>("tempFile");

    QTest::newRow("file") << false;
    QTest::newRow("tempFile") << true;
}

void ProcessLauncherJobTest::shouldFailOnNonExistingExecutable()
{
    QFETCH(bool, tempFile);

    const QString desktopFilePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/non_existing_executable.desktop");
    KDesktopFile file(desktopFilePath);
    KConfigGroup group = file.desktopGroup();
    group.writeEntry("Name", "KRunUnittestService");
    group.writeEntry("Type", "Service");
    group.writeEntry("Exec", "does_not_exist %f %d/dest_%n");
    file.sync();

    KService::Ptr servicePtr(new KService(desktopFilePath));
    KIO::ProcessLauncherJob *job = new KIO::ProcessLauncherJob(servicePtr, WId{}, this);
    job->setUrls({QUrl::fromLocalFile(desktopFilePath)}); // just to have one URL as argument, as the desktop file expects
    if (tempFile) {
        job->setRunFlags(KIO::ProcessLauncherJob::DeleteTemporaryFiles);
    }
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KJob::UserDefinedError);
    QCOMPARE(job->errorString(), QStringLiteral("Could not find the program 'does_not_exist'"));

    QFile::remove(desktopFilePath);
}

void ProcessLauncherJobTest::writeTempServiceDesktopFile(const QString &filePath)
{
    if (!QFile::exists(filePath)) {
        KDesktopFile file(filePath);
        KConfigGroup group = file.desktopGroup();
        group.writeEntry("Name", "KRunUnittestService");
        group.writeEntry("Type", "Service");
#ifdef Q_OS_WIN
        group.writeEntry("Exec", "copy.exe %f %d/dest_%n");
#else
        group.writeEntry("Exec", "cd %d ; cp %f %d/dest_%n"); // cd is just to show that we can't do QFile::exists(binary)
#endif
        file.sync();
    }
}

QString ProcessLauncherJobTest::createTempService()
{
    const QString fileName = s_tempServiceName;
    const QString fakeService = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/kservices5/") + fileName;
    writeTempServiceDesktopFile(fakeService);
    m_filesToRemove.append(fakeService);
    return fakeService;
}
