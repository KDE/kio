/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kterminallauncherjob.h"

#include "desktopexecparser.h"

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
        connect(subjob, &KJob::result, this, [this, subjob] {
            // NB: must go through emitResult otherwise we don't get correctly finished!
            // TODO KF6: maybe change the base to KCompositeJob so we can get rid of this nonesense
            if (subjob->error()) {
                setError(subjob->error());
                setErrorText(subjob->errorText());
            }
            emitResult();
        });
        subjob->start();
    }
}

void KTerminalLauncherJob::emitDelayedResult()
{
    // Use delayed invocation so the caller has time to connect to the signal
    QMetaObject::invokeMethod(this, &KTerminalLauncherJob::emitResult, Qt::QueuedConnection);
}

// This sets m_fullCommand, but also (when possible) m_desktopName
void KTerminalLauncherJob::determineFullCommand(bool fallbackToKonsoleService /* allow synthesizing no konsole */)
{
    const std::optional<KIO::DesktopExecParser::CommandResult> result =
        KIO::DesktopExecParser::determineFullCommand(d->m_workingDirectory, d->m_command, fallbackToKonsoleService);
    if (result) {
        d->m_fullCommand = result->exec;
        d->m_desktopName = result->desktopName;
    } else {
        setError(KJob::UserDefinedError);
        setErrorText(i18n("No terminal emulator found"));
    }
}

QString KTerminalLauncherJob::fullCommand() const
{
    return d->m_fullCommand;
}
