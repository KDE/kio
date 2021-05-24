/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kterminallauncherjob.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <QProcessEnvironment>

class KTerminalLauncherJobPrivate
{
public:
    QString m_workingDirectory;
    QString m_command; // "ls"
    QString m_fullCommand; // "xterm -e ls"
    QString m_desktopName;
    QByteArray m_startupId;
    QProcessEnvironment m_environment;
};

KTerminalLauncherJob::KTerminalLauncherJob(const QString &command, QObject *parent)
    : KJob(parent)
    , d(new KTerminalLauncherJobPrivate)
{
    d->m_command = command;
}

KTerminalLauncherJob::~KTerminalLauncherJob() = default;

void KTerminalLauncherJob::setWorkingDirectory(const QString &workingDirectory)
{
    d->m_workingDirectory = workingDirectory;
}

void KTerminalLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KTerminalLauncherJob::setProcessEnvironment(const QProcessEnvironment &environment)
{
    d->m_environment = environment;
}

void KTerminalLauncherJob::start()
{
    determineFullCommand();
    if (error()) {
        emitDelayedResult();
    } else {
        auto *subjob = new KIO::CommandLauncherJob(d->m_fullCommand, this);
        subjob->setDesktopName(d->m_desktopName);
        subjob->setWorkingDirectory(d->m_workingDirectory);
        subjob->setStartupId(d->m_startupId);
        subjob->setProcessEnvironment(d->m_environment);
        connect(subjob, &KJob::result, this, &KJob::result);
        subjob->start();
    }
}

void KTerminalLauncherJob::emitDelayedResult()
{
    // Use delayed invocation so the caller has time to connect to the signal
    QMetaObject::invokeMethod(this, &KTerminalLauncherJob::emitResult, Qt::QueuedConnection);
}

// This sets m_fullCommand, but also (when possible) m_desktopName
void KTerminalLauncherJob::determineFullCommand()
{
    const QString workingDir = d->m_workingDirectory;
#ifndef Q_OS_WIN
    const KConfigGroup confGroup(KSharedConfig::openConfig(), "General");
    const QString terminalExec = confGroup.readEntry("TerminalApplication");
    const QString terminalService = confGroup.readEntry("TerminalService");
    KServicePtr service;
    if (!terminalService.isEmpty()) {
        service = KService::serviceByStorageId(terminalService);
    } else if (!terminalExec.isEmpty()) {
        service = new KService(QStringLiteral("terminal"), terminalExec, QStringLiteral("utilities-terminal"));
    }
    if (!service) {
        service = KService::serviceByStorageId(QStringLiteral("org.kde.konsole"));
    }
    QString exec;
    if (service) {
        d->m_desktopName = service->desktopEntryName();
        exec = service->exec();
    } else {
        // konsole not found by desktop file, let's see what PATH has for us
        auto useIfAvailable = [&exec](const QString &terminalApp) {
            const bool found = !QStandardPaths::findExecutable(terminalApp).isEmpty();
            if (found) {
                exec = terminalApp;
            }
            return found;
        };
        if (!useIfAvailable(QStringLiteral("konsole")) && !useIfAvailable(QStringLiteral("xterm"))) {
            setError(KJob::UserDefinedError);
            setErrorText(i18n("No terminal emulator found"));
            return;
        }
    }
    if (!d->m_command.isEmpty()) {
        if (exec == QLatin1String("konsole")) {
            exec += QLatin1String(" --noclose");
        } else if (exec == QLatin1String("xterm")) {
            exec += QLatin1String(" -hold");
        }
    }
    if (service->exec() == QLatin1String("konsole") && !workingDir.isEmpty()) {
        exec += QLatin1String(" --workdir %1").arg(KShell::quoteArg(workingDir));
    }
    if (!d->m_command.isEmpty()) {
        exec += QLatin1String(" -e ") + d->m_command;
    }
#else
    const QString windowsTerminal = QStringLiteral("wt.exe");
    const QString pwsh = QStringLiteral("pwsh.exe");
    const QString powershell = QStringLiteral("powershell.exe"); // Powershell is used as fallback
    const bool hasWindowsTerminal = !QStandardPaths::findExecutable(windowsTerminal).isEmpty();
    const bool hasPwsh = !QStandardPaths::findExecutable(pwsh).isEmpty();

    QString exec;
    if (hasWindowsTerminal) {
        exec = windowsTerminal;
        if (!workingDir.isEmpty()) {
            exec += QLatin1String(" --startingDirectory %1").arg(KShell::quoteArg(workingDir));
        }
        if (!d->m_command.isEmpty()) {
            // Command and NoExit flag will be added later
            exec += QLatin1Char(' ') + (hasPwsh ? pwsh : powershell);
        }
    } else {
        exec = hasPwsh ? pwsh : powershell;
    }
    if (!d->m_command.isEmpty()) {
        exec += QLatin1String(" -NoExit -Command ") + d->m_command;
    }
#endif
    d->m_fullCommand = exec;
}

QString KTerminalLauncherJob::fullCommand() const
{
    return d->m_fullCommand;
}
