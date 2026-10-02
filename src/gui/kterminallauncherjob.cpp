/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kterminallauncherjob.h"

#include "kio_version.h"
#include "kprocessrunner_p.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <KWaylandExtras>
#include <KWindowSystem>

#if WITH_QTDBUS
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QTimer>
#endif
#include <QProcessEnvironment>

using namespace Qt::StringLiterals;

constexpr auto terminalInterface = "org.freedesktop.Terminal1"_L1;

class KTerminalLauncherJobPrivate
{
public:
    QString m_workingDirectory;
    QString m_command; // "ls"
    QString m_fullCommand; // "xterm -e ls"
    QString m_desktopName;
    QByteArray m_startupId;
    KServicePtr m_terminalIntentService;
    QProcessEnvironment m_environment{QProcessEnvironment::InheritFromParent};
};

#if WITH_QTDBUS
class TerminalLauncher : public QObject
{
    Q_OBJECT
public:
    TerminalLauncher(const KTerminalLauncherJobPrivate *d)
        : d(d)
    {
    }
    void start()
    {
    }
#endif
} void sendMessage(const QByteArray &startupId)
{
    const QString appId = d->m_terminalIntentService->desktopEntryName();
    const QString objectPath = u"/%1"_s.arg(appId).replace(u'.', u'/').replace(u'-', u'_');
    auto message = QDBusMessage::createMethodCall(appId, objectPath, terminalInterface, u"LaunchCommand"_s);
    const auto args = KShell::splitArgs(d->m_command) | std::views::transform([](const QString &arg) {
                          return arg.toUtf8();
                      });
    const auto environment = d->m_environment.inheritsFromParent() ? d->m_environment : QProcessEnvironment::systemEnvironment();
    const auto env = environment.toStringList() | std::views::transform([](const QString &arg) {
                         return arg.toUtf8();
                     });
    const QVariantMap command{{u"exec"_s, QVariant::fromValue(QList<QByteArray>(args.begin(), args.end()))},
                              {u"env"_s, QVariant::fromValue(QList<QByteArray>(env.begin(), env.end()))},
                              {u"working_directoy"_s, d->m_workingDirectory.toUtf8()}};
    const QByteArray desktop_entry; // TODO
    const QVariantMap options{
        // {u"keep-terminal-open"_s, true},
    };
    message << command << desktop_entry << options;
}
const KTerminalLauncherJobPrivate *const d;
Q_SIGNALS:
void started();
void error(const QString &message);
}
;
#endif

KTerminalLauncherJob::KTerminalLauncherJob(const QString &command, QObject *parent)
    : KJob(parent)
    , d(new KTerminalLauncherJobPrivate)
{
#if WITH_QTDBUS
    [[maybe_unused]] static bool once = []() {
        qDBusRegisterMetaType<QList<QVariantMap>>();
        qDBusRegisterMetaType<QList<QByteArray>>();
        return true;
    }();
#endif
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

// prepares for launching but doesn't actually launch yet. sets error on error
bool KTerminalLauncherJob::prepare()
{
    determineFullCommand(); // checks and sets m_fullCommand
    return error() == KJob::NoError;
}

void KTerminalLauncherJob::start()
{
    if (!prepare()) {
        emitDelayedResult();
        return;
    }
#if WITH_QTDBUS
    if (d->m_terminalIntentService) {
        auto launcher = new TerminalLauncher{d.get()};
        launcher->start();
        connect(launcher, &KProcessRunner::error, this, [this](const QString &errorText) {
            setError(KJob::UserDefinedError);
            setErrorText(errorText);
            emitResult();
        });
        connect(launcher, &KProcessRunner::processStarted, this, [this] {
            emitResult();
        });
        return;
    }
#endif
    auto *subjob = new KIO::CommandLauncherJob(d->m_fullCommand, this);
    subjob->setDesktopName(d->m_desktopName);
    subjob->setWorkingDirectory(d->m_workingDirectory);
    subjob->setStartupId(d->m_startupId);
    subjob->setProcessEnvironment(d->m_environment);
    connect(subjob, &KJob::result, this, [this, subjob] {
        // NB: must go through emitResult otherwise we don't get correctly finished!
        // TODO KF6: maybe change the base to KCompositeJob so we can get rid of this nonsense
        if (subjob->error()) {
            setError(subjob->error());
            setErrorText(subjob->errorText());
        }
        emitResult();
    });
    subjob->start();
}

void KTerminalLauncherJob::emitDelayedResult()
{
    // Use delayed invocation so the caller has time to connect to the signal
    QMetaObject::invokeMethod(this, &KTerminalLauncherJob::emitResult, Qt::QueuedConnection);
}

#ifndef Q_OS_WIN
// helper function to help scope service so that not the entire determineFullCommand has access to it (it is not
// always not null!)
static KServicePtr serviceFromConfig(bool fallbackToKonsoleService)
{
    KServicePtr service;
    // On Plasma we want to use what the user has configured in systemsettings,
    // otherwise always use what is configured for the intent
    if (qgetenv("XDG_CURRENT_DESKTOP") == "KDE") {
        const KConfigGroup confGroup(KSharedConfig::openConfig(), QStringLiteral("General"));
        const QString terminalExec = confGroup.readEntry("TerminalApplication");
        const QString terminalService = confGroup.readEntry("TerminalService");
        if (!terminalService.isEmpty()) {
            service = KService::serviceByStorageId(terminalService);
        } else if (!terminalExec.isEmpty()) {
            service = new KService(QStringLiteral("terminal"), terminalExec, QStringLiteral("utilities-terminal"));
        }
    }
    if (!service) {
        // service = KApplicationTrader::preferredIntent(u"org.freedesktop.Terminal1"_s);
    }
    if (!service && fallbackToKonsoleService) {
        service = KService::serviceByStorageId(QStringLiteral("org.kde.konsole"));
    }
    return service;
}

#endif

// This sets m_fullCommand, but also (when possible) m_desktopName
void KTerminalLauncherJob::determineFullCommand(bool fallbackToKonsoleService /* allow synthesizing no konsole */)
{
    const QString workingDir = d->m_workingDirectory;
#ifndef Q_OS_WIN

    auto service = serviceFromConfig(fallbackToKonsoleService);
    if (service->property<QStringList>(u"Implements"_s).contains(terminalInterface)) {
        d->terminalIntentService = service;
        return;
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
    if (exec.startsWith(QLatin1String("konsole")) && !workingDir.isEmpty()) {
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

#include "moc_kterminallauncherjob.cpp"
