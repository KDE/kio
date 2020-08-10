/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "commandlauncherjob.h"
#include "kprocessrunner_p.h"
#include "kiogui_debug.h"
#include <KShell>
#include <KLocalizedString>

class KIO::CommandLauncherJobPrivate
{
public:
    QString m_command;
    QString m_desktopName;
    QString m_executable;
    QString m_iconName;
    QString m_workingDirectory;
    QByteArray m_startupId;
    QPointer<KProcessRunner> m_processRunner;
    qint64 m_pid = 0;
};

KIO::CommandLauncherJob::CommandLauncherJob(const QString &command, QObject *parent)
    : KJob(parent), d(new CommandLauncherJobPrivate())
{
    d->m_command = command;
}

KIO::CommandLauncherJob::CommandLauncherJob(const QString &executable, const QStringList &args, QObject *parent)
    : KJob(parent), d(new CommandLauncherJobPrivate())
{
    d->m_executable = executable;
    d->m_command = KShell::quoteArg(executable) + QLatin1Char(' ') + KShell::joinArgs(args);
}

KIO::CommandLauncherJob::~CommandLauncherJob()
{
    // Do *NOT* delete the KProcessRunner instances here.
    // We need it to keep running so it can terminate startup notification on process exit.
}

void KIO::CommandLauncherJob::setExecutable(const QString &executable)
{
    d->m_executable = executable;
}

void KIO::CommandLauncherJob::setIcon(const QString &iconName)
{
    d->m_iconName = iconName;
}

void KIO::CommandLauncherJob::setDesktopName(const QString &desktopName)
{
    d->m_desktopName = desktopName;
}

void KIO::CommandLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KIO::CommandLauncherJob::setWorkingDirectory(const QString &workingDirectory)
{
    d->m_workingDirectory = workingDirectory;
}

void KIO::CommandLauncherJob::start()
{
    // Some fallback for lazy callers, not 100% accurate though
    if (d->m_executable.isEmpty()) {
        const QStringList args = KShell::splitArgs(d->m_command);
        if (!args.isEmpty()) {
            d->m_executable = args.first();
        }
    }

    QString displayName = d->m_executable;
    KService::Ptr service = KService::serviceByDesktopName(d->m_desktopName);
    if (service) {
        displayName = service->name();
    }
    emit description(this, i18nc("Launching application", "Launching %1", displayName), {}, {});

    if (d->m_iconName.isEmpty()) {
        d->m_iconName = d->m_executable;
    }
    d->m_processRunner = KProcessRunner::fromCommand(d->m_command, d->m_desktopName, d->m_executable,
                                                     d->m_iconName, d->m_startupId,
                                                     d->m_workingDirectory);
    connect(d->m_processRunner, &KProcessRunner::error, this, [this](const QString &errorText) {
        setError(KJob::UserDefinedError);
        setErrorText(errorText);
        emitResult();
    });
    connect(d->m_processRunner, &KProcessRunner::processStarted, this, [this](qint64 pid) {
        d->m_pid = pid;
        emitResult();
    });
}

bool KIO::CommandLauncherJob::waitForStarted()
{
    if (d->m_processRunner.isNull()) {
        return false;
    }
    const bool ret = d->m_processRunner->waitForStarted();
    if (!d->m_processRunner.isNull()) {
        qApp->sendPostedEvents(d->m_processRunner); // so slotStarted gets called
    }
    return ret;
}

qint64 KIO::CommandLauncherJob::pid() const
{
    return d->m_pid;
}
