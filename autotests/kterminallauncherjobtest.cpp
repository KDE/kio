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
#include <kapplicationtrader.h>
#include <kdesktopfile.h>
#include <qdbusconnection.h>
#include <qdbusmetatype.h>
#include <qdbusreply.h>
#include <qprocess.h>
#include <qsignalspy.h>
#include <qtestcase.h>

using namespace Qt::StringLiterals;

QTEST_GUILESS_MAIN(KTerminalLauncherJobTest)

void KTerminalLauncherJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    confGroup.writeEntry("TerminalApplication", "");
    confGroup.writeEntry("TerminalService", "");
}

void KTerminalLauncherJobTest::cleanup()
{
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    confGroup.writeEntry("TerminalApplication", "");
    confGroup.writeEntry("TerminalService", "");
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

    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
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
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
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
    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
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

class IntentHandler : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Terminal1")
public:
    struct LaunchRequest {
        const QList<QVariantMap> commands;
        const QByteArray desktop_entry;
        const QVariantMap options;
        const QVariantMap platformData;
    };
    std::vector<LaunchRequest> launchrequests;
public Q_SLOTS:
    void LaunchCommand(const QList<QVariantMap> &commands, const QByteArray &desktop_entry, const QVariantMap &options, const QVariantMap &platformData)
    {
        launchrequests.push_back({commands, desktop_entry, options, platformData});
    }
};

#if WITH_QTDBUS
void KTerminalLauncherJobTest::testLaunchingIntent()
{
    const QString desktopFile =
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + QLatin1Char('/') + u"org.kde.terminallaunchertest.desktop"_s;
    KDesktopFile file(desktopFile);
    file.desktopGroup().writeEntry("Name", "KTerminalLauncherJobTest");
    file.desktopGroup().writeEntry("DBusActivatable", true);
    file.desktopGroup().writeEntry("Implements", "org.freedesktop.Terminal1");

    KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
    confGroup.writeEntry("TerminalService", "org.kde.terminallaunchertest.desktop");

    KApplicationTrader::setPreferredServiceForIntent(u"org.freedesktop.Terminal1"_s, KService::serviceByDesktopName(u"org.kde.terminallaunchertest"_s));

    IntentHandler handler;
    QVERIFY(QDBusConnection::sessionBus().registerObject(u"/org/kde/terminallaunchertest"_s, &handler, QDBusConnection::ExportAllContents));
    QVERIFY(QDBusConnection::sessionBus().registerService(u"org.kde.terminallaunchertest"_s));

    KTerminalLauncherJob job(u"make cheese"_s);
    job.setWorkingDirectory(u"/work"_s);
    job.setStartupId("moo");
    QProcessEnvironment env;
    env.insert(u"environment"_s, u"hilly"_s);
    job.setProcessEnvironment(env);

    // QSignalSpy finishedSpy(&job, &KTerminalLauncherJob::finished);
    // QVERIFY(finishedSpy.wait());
    QVERIFY(job.exec());
    QCOMPARE(job.errorString(), QString());

    QCOMPARE(handler.launchrequests.size(), 1);
    const auto &request = handler.launchrequests.front();
    QCOMPARE(request.commands.size(), 1);
    const auto command = request.commands.front();
    auto fetchaay = [](const QVariant &v) {
        return qdbus_cast<QList<QByteArray>>(v.value<QDBusArgument>());
    };
    QCOMPARE(fetchaay(command.value(u"exec"_s)), QList<QByteArray>({"make", "cheese"}));
    QCOMPARE(fetchaay(command.value(u"env"_s)), QList<QByteArray>{"environment=hilly"});
    QCOMPARE(command.value(u"working_directory"_s).toByteArray(), "/work");
    QCOMPARE(request.desktop_entry, QByteArray());
    QCOMPARE(request.options, QVariantMap({{u"keep-terminal-open"_s, false}}));
}
#endif

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

#include "kterminallauncherjobtest.moc"
#include "moc_kterminallauncherjobtest.cpp"
