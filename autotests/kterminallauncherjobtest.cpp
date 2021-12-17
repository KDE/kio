/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kterminallauncherjobtest.h"
#include "kterminallauncherjob.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QStandardPaths>
#include <QTest>

QTEST_GUILESS_MAIN(KTerminalLauncherJobTest)

void KTerminalLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

#ifndef Q_OS_WIN

void KTerminalLauncherJobTest::startKonsole_data()
{
    QTest::addColumn<QString>("command");
    QTest::addColumn<QString>("workdir");
    QTest::addColumn<QString>("expectedCommand");

    QTest::newRow("no_command_no_workdir") << ""
                                           << ""
                                           << "konsole";
    QTest::newRow("no_command_but_with_workdir") << ""
                                                 << "/tmp"
                                                 << "konsole --workdir /tmp";
    QTest::newRow("with_command") << "make cheese"
                                  << ""
                                  << "konsole --noclose -e make cheese";
    QTest::newRow("with_command_and_workdir") << "make cheese"
                                              << "/tmp"
                                              << "konsole --noclose --workdir /tmp -e make cheese";
}

void KTerminalLauncherJobTest::startKonsole()
{
    // Given
    QFETCH(QString, command);
    QFETCH(QString, workdir);
    QFETCH(QString, expectedCommand);

    KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
    confGroup.writeEntry("TerminalApplication", "konsole");

    // When
    auto *job = new KTerminalLauncherJob(command, this);
    job->setWorkingDirectory(workdir);

    // Then
    job->determineFullCommand(); // internal API
    QCOMPARE(job->fullCommand(), expectedCommand);
}

void KTerminalLauncherJobTest::startXterm()
{
    // Given
    KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
    confGroup.writeEntry("TerminalApplication", "xterm");

    const QString command = "play golf";

    // When
    auto *job = new KTerminalLauncherJob(command, this);
    job->setWorkingDirectory("/tmp"); // doesn't show in the command, but actually works due to QProcess::setWorkingDirectory

    // Then
    job->determineFullCommand(); // internal API
    QCOMPARE(job->fullCommand(), QLatin1String("xterm -hold -e play golf"));
}

void KTerminalLauncherJobTest::startFallbackToPath()
{
    // Given
    KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
    confGroup.writeEntry("TerminalApplication", "");
    confGroup.writeEntry("TerminalService", "");

    const QString command = "play golf";

    // When
    // Mock binaries so we know konsole is available in PATH. Otherwise the expectations may not be true.
    const QString pathEnv = QFINDTESTDATA("kterminallauncherjobtest") + QLatin1Char(':') + qEnvironmentVariable("PATH");
    qputenv("PATH", pathEnv.toUtf8());
    auto *job = new KTerminalLauncherJob(command, this);
    job->setWorkingDirectory("/tmp"); // doesn't show in the command, but actually works due to QProcess::setWorkingDirectory

    // Then
    job->determineFullCommand(false); // internal API
    // We do not particularly care about what was produced so long as there was no crash
    // https://bugs.kde.org/show_bug.cgi?id=446539
    // and it's not empty.
    QVERIFY(!job->fullCommand().isEmpty());
}

#else

void KTerminalLauncherJobTest::startTerminal_data()
{
    QTest::addColumn<bool>("useWindowsTerminal");
    QTest::addColumn<QString>("command");
    QTest::addColumn<QString>("workdir");
    QTest::addColumn<QString>("expectedCommand");

    QTest::newRow("no_command") << false << ""
                                << "not_part_of_command"
                                << "powershell.exe";
    QTest::newRow("with_command") << false << "make cheese"
                                  << "not_part_of_command"
                                  << "powershell.exe -NoExit -Command make cheese";
    QTest::newRow("wt_no_command_no_workdir") << true << ""
                                              << ""
                                              << "wt.exe";
    QTest::newRow("wt_no_command_with_workdir") << true << ""
                                                << "C:\\"
                                                << "wt.exe --startingDirectory 'C:\\'";
    QTest::newRow("wt_with_command_no_workdir") << true << "make cheese"
                                                << ""
                                                << "wt.exe powershell.exe -NoExit -Command make cheese";
    QTest::newRow("wt_with_command_with_workdir") << true << "make cheese"
                                                  << "C:\\"
                                                  << "wt.exe --startingDirectory 'C:\\' powershell.exe -NoExit -Command make cheese";
}

void KTerminalLauncherJobTest::startTerminal()
{
    // Given
    QFETCH(bool, useWindowsTerminal);
    QFETCH(QString, command);
    QFETCH(QString, workdir);
    QFETCH(QString, expectedCommand);

    // Control the presence of wt.exe in %PATH%, by clearing it and setting our own dir
    const QString binDir = m_tempDir.path();
    qputenv("PATH", binDir.toLocal8Bit().constData());
    QString exe = binDir + QLatin1String("/wt.exe");
    if (useWindowsTerminal) {
        QFile fakeExe(exe);
        QVERIFY2(fakeExe.open(QIODevice::WriteOnly), qPrintable(fakeExe.errorString()));
        QVERIFY(fakeExe.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser));
    } else {
        QFile::remove(exe);
    }

    // When
    auto *job = new KTerminalLauncherJob(command, this);
    job->setWorkingDirectory(workdir);

    // Then
    QCOMPARE(job->fullCommand(), expectedCommand);
}

#endif
