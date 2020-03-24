/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

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
    srcFile.write("Hello world\n");
}

void CommandLauncherJobTest::startProcess_data()
{
    QTest::addColumn<bool>("useExec");

    QTest::newRow("exec") << true;
    QTest::newRow("waitForStarted") << false;
}

void CommandLauncherJobTest::startProcess()
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
    QVERIFY(QFile::exists(srcDir + "/srcfile"));
    QVERIFY(QFile::remove(dest)); // cleanup

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
