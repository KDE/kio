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
