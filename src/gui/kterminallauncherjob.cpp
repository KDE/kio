/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kterminallauncherjob.h"

#include "desktopexecparser.h"
#include "kprocessrunner_p.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <KWaylandExtras>
#include <KWindowSystem>
#include <qcontainerfwd.h>
#include <qdbuspendingcall.h>
#if HAVE_WAYLAND
#include <KWaylandExtras>
#endif
#if HAVE_X11
#include <KStartupInfo>
#endif

#if WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QGuiApplication>
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
    TerminalLauncher(KTerminalLauncherJob *parent, const KTerminalLauncherJobPrivate *d)
        : QObject(parent)
        , d(d)
    {
    }
    void start()
    {
#if HAVE_WAYLAND
        if (KWindowSystem::isPlatformWayland() && d->m_startupId.isEmpty()) {
            auto window = qGuiApp->focusWindow();
            if (!window && !qGuiApp->allWindows().isEmpty()) {
                window = qGuiApp->allWindows().constFirst();
            }
            if (window) {
                KWaylandExtras::xdgActivationToken(window, d->m_terminalIntentService->menuId()).then(this, std::bind_front(&TerminalLauncher::launch, this));
            }
        }
#endif
#if HAVE_X11
        if (KWindowSystem::isPlatformX11() && !d->m_startupId.isEmpty()) {
            doX11StartupNotification();
        }
#endif
        launch(QString::fromUtf8(m_x11startupId.id()));
    }

private:
#if HAVE_X11
    void doX11StartupNotification()
    {
        bool silent = false;
        QByteArray wmclass;
        const bool startup_notify = (d->m_startupId != "0" && KIOGuiPrivate::checkStartupNotify(d->m_terminalIntentService.data(), &silent, &wmclass));
        if (startup_notify) {
            m_x11startupId.initId(d->m_startupId);
            m_x11startupId.setupStartupEnv();
            KStartupInfoData data;
            data.setHostname();
            // When it comes from a desktop file, m_executable can be a full shell command, so <bin> here is not 100% reliable.
            // E.g. it could be "cd", which isn't an existing binary. It's just a heuristic anyway.
            const QString bin = KIO::DesktopExecParser::executableName(d->m_terminalIntentService->exec());
            data.setBin(bin);
            if (const auto name = d->m_terminalIntentService->name(); !name.isEmpty()) {
                data.setName(name);
            }
            data.setDescription(i18n("Launching %1", data.name()));
            if (const auto icon = d->m_terminalIntentService->icon(); !icon.isEmpty()) {
                data.setIcon(icon);
            }
            if (!wmclass.isEmpty()) {
                data.setWMClass(wmclass);
            }
            if (silent) {
                data.setSilent(KStartupInfoData::Yes);
            }
            if (const auto entryPath = d->m_terminalIntentService->entryPath(); !entryPath.isEmpty()) {
                data.setApplicationId(entryPath);
            }
            KStartupInfo::sendStartup(m_x11startupId, data);
        }
    }
#endif
    void launch(const QString &activationToken)
    {
        const QString appId = d->m_terminalIntentService->desktopEntryName();
        const QString objectPath = u"/%1"_s.arg(appId).replace(u'.', u'/').replace(u'-', u'_');
        auto message = QDBusMessage::createMethodCall(appId, objectPath, terminalInterface, u"LaunchCommand"_s);
        const auto args = KShell::splitArgs(d->m_command) | std::views::transform([](const QString &arg) {
                              return arg.toUtf8();
                          });
        const auto environment = d->m_environment.inheritsFromParent() ? QProcessEnvironment::systemEnvironment() : d->m_environment;
        const auto env = environment.toStringList() | std::views::transform([](const QString &arg) {
                             return arg.toUtf8();
                         });
        const QList<QVariantMap> command{{{u"exec"_s, QVariant::fromValue(QList<QByteArray>(args.begin(), args.end()))},
                                          {u"env"_s, QVariant::fromValue(QList<QByteArray>(env.begin(), env.end()))},
                                          {u"working_directory"_s, d->m_workingDirectory.toUtf8()}}};
        const QByteArray desktop_entry; // TODO ????
        const QVariantMap options{
            {u"keep-terminal-open"_s, false}, // TODO Api for that ?
        };
        QVariantMap platformData;
        if (KWindowSystem::isPlatformX11()) {
            platformData = {{u"desktop-starup-id"_s, activationToken}};
        } else if (KWindowSystem::isPlatformWayland()) {
            platformData = {{u"activation-token"_s, activationToken}};
        }
        message << QVariant::fromValue(command) << desktop_entry << options << platformData;
        auto pendingCall = QDBusConnection::sessionBus().asyncCall(message);
        connect(new QDBusPendingCallWatcher(pendingCall, this), &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *call) {
            call->deleteLater();
            if (call->isError()) {
                Q_EMIT error(call->error().message());
#if HAVE_X11
                if (!m_x11startupId.id().isNull()) {
                    KStartupInfo::sendFinish(m_x11startupId, {});
                }
#endif
            } else {
                Q_EMIT started();
            }
            deleteLater();
        });
    }
Q_SIGNALS:
void started();
void error(const QString &message);

private:
const KTerminalLauncherJobPrivate *const d;
#if HAVE_X11
KStartupInfoId m_x11startupId;
#endif
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
        auto launcher = new TerminalLauncher(this, d.get());
        connect(launcher, &TerminalLauncher::error, this, [this](const QString &errorText) {
            setError(KJob::UserDefinedError);
            setErrorText(errorText);
            emitResult();
        });
        connect(launcher, &TerminalLauncher::started, this, [this] {
            emitResult();
        });
        launcher->start();
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
    } else if (!service) {
        service = KApplicationTrader::preferredServiceForIntent(terminalInterface);
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
    if (service && service->supportedIntents().contains(terminalInterface)) {
        d->m_terminalIntentService = service;
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

#include "kterminallauncherjob.moc"
#include "moc_kterminallauncherjob.cpp"
