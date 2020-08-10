/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "commandlauncherjobtest.h"
#include "commandlauncherjob.h"

#include "kiotesthelper.h" // createTestFile etc.

#ifdef Q_OS_UNIX
#include <signal.h> // kill
#endif

#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <kprocessrunner_p.h>

QTEST_GUILESS_MAIN(CommandLauncherJobTest)

void CommandLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

static void createSrcFile(const QString path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    QVERIFY(srcFile.write("Hello world\n") > 0);
}

void CommandLauncherJobTest::startProcessAsCommand_data()
{
    QTest::addColumn<bool>("useExec");

    QTest::newRow("exec") << true;
    QTest::newRow("waitForStarted") << false;
}

void CommandLauncherJobTest::startProcessAsCommand()
{
    QFETCH(bool, useExec);

    // Given a command
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + "/srcfile";
    createSrcFile(srcFile);
    QVERIFY(QFile::exists(srcFile));

#ifdef Q_OS_WIN
    QString command = "copy.exe";
#else
    QString command = "cp";
#endif

    command += QStringLiteral(" %1 destfile").arg(srcFile);

    // When running a CommandLauncherJob
    KIO::CommandLauncherJob *job = new KIO::CommandLauncherJob(command, this);
    job->setWorkingDirectory(srcDir);
    if (useExec) {
        QVERIFY(job->exec());
    } else {
        job->start();
        QVERIFY(job->waitForStarted());
    }
    const qint64 pid = job->pid();

    // Then the service should be executed (which copies the source file to "dest")
    QVERIFY(pid != 0);
    const QString dest = srcDir + "/destfile";
    QTRY_VERIFY2(QFile::exists(dest), qPrintable(dest));
    QVERIFY(QFile::remove(srcFile)); // cleanup
    QVERIFY(QFile::remove(dest));

    // Just to make sure
    QTRY_COMPARE(KProcessRunner::instanceCount(), 0);
}

void CommandLauncherJobTest::startProcessWithArgs_data()
{
    QTest::addColumn<QString>("srcName");
    QTest::addColumn<QString>("destName");

    QTest::newRow("path without spaces") << QStringLiteral("srcfile") << QStringLiteral("destfile");
    QTest::newRow("path with spaces") << QStringLiteral("Source File") << QStringLiteral("Destination File");
}

void CommandLauncherJobTest::startProcessWithArgs()
{
    QFETCH(QString, srcName);
    QFETCH(QString, destName);

    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcPath = srcDir + '/' + srcName;
    const QString destPath = srcDir + '/' + destName;

    createSrcFile(srcPath);
    QVERIFY(QFileInfo::exists(srcPath));

#ifdef Q_OS_WIN
    const QString executable = "copy.exe";
#else
    const QString executable = "cp";
#endif

    auto *job = new KIO::CommandLauncherJob(executable, {
        srcPath, destName
    }, this);
    job->setWorkingDirectory(srcDir);

    job->start();
    QVERIFY(job->waitForStarted());

    const qint64 pid = job->pid();

    // Then the service should be executed (which copies the source file to "dest")
    QVERIFY(pid != 0);
    QTRY_VERIFY2(QFileInfo::exists(destPath), qPrintable(destPath));
    QVERIFY(QFile::remove(srcPath));
    QVERIFY(QFile::remove(destPath)); // cleanup

    // Just to make sure
    QTRY_COMPARE(KProcessRunner::instanceCount(), 0);
}

void CommandLauncherJobTest::startProcessWithSpacesInExecutablePath_data()
{
    QTest::addColumn<QString>("srcName");
    QTest::addColumn<QString>("destName");

    QTest::newRow("path without spaces") << QStringLiteral("srcfile") << QStringLiteral("destfile");
    QTest::newRow("path with spaces") << QStringLiteral("Source File") << QStringLiteral("Destination File");
}

void CommandLauncherJobTest::startProcessWithSpacesInExecutablePath()
{
    QFETCH(QString, srcName);
    QFETCH(QString, destName);

    QTemporaryDir tempDir;

    const QString srcDir = tempDir.filePath("folder with spaces");
    QVERIFY(QDir().mkpath(srcDir));

    const QString srcPath = srcDir + '/' + srcName;
    const QString destPath = srcDir + '/' + destName;

    createSrcFile(srcPath);
    QVERIFY(QFileInfo::exists(srcPath));

    // Copy the executable into the folder with spaces in its path
#ifdef Q_OS_WIN
    const QString executableName = "copy"; // QStandardPaths appends extension as necessary
#else
    const QString executableName = "cp";
#endif

    const QString executablePath = QStandardPaths::findExecutable(executableName);
    QVERIFY(!executablePath.isEmpty());
    QFileInfo fi(executablePath);
    // Needed since it could be .exe or .bat on Windows
    const QString executableFileName = fi.fileName();

    const QString executable = srcDir + '/' + executableFileName;
    QVERIFY(QFile::copy(executablePath, executable));

    auto *job = new KIO::CommandLauncherJob(executable, {
        srcPath, destName
    }, this);
    job->setWorkingDirectory(srcDir);

    job->start();
    QVERIFY(job->waitForStarted());

    const qint64 pid = job->pid();

    // Then the service should be executed (which copies the source file to "dest")
    QVERIFY(pid != 0);
    QTRY_VERIFY2(QFileInfo::exists(destPath), qPrintable(destPath));

    // cleanup
    QVERIFY(QFile::remove(destPath));
    QVERIFY(QFile::remove(srcPath));
    QVERIFY(QFile::remove(executable));

    // Just to make sure
    QTRY_COMPARE(KProcessRunner::instanceCount(), 0);
}

void CommandLauncherJobTest::doesNotFailOnNonExistingExecutable()
{
    // Given a command that uses an executable that doesn't exist
    const QString command = "does_not_exist foo bar";

    // When running a CommandLauncherJob
    KIO::CommandLauncherJob *job = new KIO::CommandLauncherJob(command, this);
    job->setExecutable("really_does_not_exist");

    // Then it doesn't actually fail. QProcess is starting /bin/sh, which works...
    QVERIFY(job->exec());
}

void CommandLauncherJobTest::shouldDoNothingOnEmptyCommand()
{
    // When running an empty command
    KIO::CommandLauncherJob *job = new KIO::CommandLauncherJob(QString(), this);

    // THEN it should do nothing
    // at least not crash (old bug 186036)
    QVERIFY(job->exec());
}
