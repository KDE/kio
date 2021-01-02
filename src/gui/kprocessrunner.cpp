/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kprocessrunner_p.h"

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#include "systemd/systemdprocessrunner_p.h"
#include "systemd/scopedprocessrunner_p.h"
#endif

#include "kiogui_debug.h"
#include "config-kiogui.h"

#include "desktopexecparser.h"
#include "krecentdocument.h"
#include <KDesktopFile>
#include <KLocalizedString>
#include <KWindowSystem>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QString>
#include <QUuid>

static int s_instanceCount = 0; // for the unittest

KProcessRunner::KProcessRunner()
    : m_process {new KProcess}
{
    ++s_instanceCount;
}

static KProcessRunner *makeInstance()
{
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
    if (SystemdProcessRunner::isAvailable()) {
        if (qEnvironmentVariableIntValue("KDE_APPLICATIONS_AS_SERVICE")) {
            return new SystemdProcessRunner();
        }
        if (qEnvironmentVariableIntValue("KDE_APPLICATIONS_AS_SCOPE")) {
            return new ScopedProcessRunner();
        }
    }
#endif
    return new ForkingProcessRunner();
}

KProcessRunner *KProcessRunner::fromApplication(const KService::Ptr &service,
                                                const QList<QUrl> &urls,
                                                KIO::ApplicationLauncherJob::RunFlags flags,
                                                const QString &suggestedFileName,
                                                const QByteArray &asn)
{
    auto instance = makeInstance();

    if (!service->isValid()) {
        instance->emitDelayedError(i18n("The desktop entry file\n%1\nis not valid.", service->entryPath()));
        return instance;
    }

    instance->m_executable = KIO::DesktopExecParser::executablePath(service->exec());

    KIO::DesktopExecParser execParser(*service, urls);
    execParser.setUrlsAreTempFiles(flags & KIO::ApplicationLauncherJob::DeleteTemporaryFiles);
    execParser.setSuggestedFileName(suggestedFileName);
    const QStringList args = execParser.resultingArguments();
    if (args.isEmpty()) {
        instance->emitDelayedError(execParser.errorMessage());
        return instance;
    }

    qCDebug(KIO_GUI) << "Starting process:" << args;
    *instance->m_process << args;

    enum DiscreteGpuCheck { NotChecked, Present, Absent };
    static DiscreteGpuCheck s_gpuCheck = NotChecked;

    if (service->runOnDiscreteGpu() && s_gpuCheck == NotChecked) {
        // Check whether we have a discrete gpu
        bool hasDiscreteGpu = false;
        QDBusInterface iface(QStringLiteral("org.kde.Solid.PowerManagement"),
                             QStringLiteral("/org/kde/Solid/PowerManagement"),
                             QStringLiteral("org.kde.Solid.PowerManagement"),
                             QDBusConnection::sessionBus());
        if (iface.isValid()) {
            QDBusReply<bool> reply = iface.call(QStringLiteral("hasDualGpu"));
            if (reply.isValid()) {
                hasDiscreteGpu = reply.value();
            }
        }

        s_gpuCheck = hasDiscreteGpu ? Present : Absent;
    }

    if (service->runOnDiscreteGpu() && s_gpuCheck == Present) {
        instance->m_process->setEnv(QStringLiteral("DRI_PRIME"), QStringLiteral("1"));
    }

    QString workingDir(service->workingDirectory());
    if (workingDir.isEmpty() && !urls.isEmpty() && urls.first().isLocalFile()) {
        workingDir = urls.first().adjusted(QUrl::RemoveFilename).toLocalFile();
    }
    instance->m_process->setWorkingDirectory(workingDir);

    if ((flags & KIO::ApplicationLauncherJob::DeleteTemporaryFiles) == 0) {
        // Remember we opened those urls, for the "recent documents" menu in kicker
        for (const QUrl &url : urls) {
            KRecentDocument::add(url, service->desktopEntryName());
        }
    }

    instance->init(service, service->name(), service->icon(), asn);
    return instance;
}

KProcessRunner *KProcessRunner::fromCommand(const QString &cmd,
                                            const QString &desktopName,
                                            const QString &execName,
                                            const QString &iconName,
                                            const QByteArray &asn,
                                            const QString &workingDirectory)
{
    auto instance = makeInstance();

    instance->m_executable = KIO::DesktopExecParser::executablePath(execName);
    instance->m_process->setShellCommand(cmd);
    if (!workingDirectory.isEmpty()) {
        instance->m_process->setWorkingDirectory(workingDirectory);
    }
    if (!desktopName.isEmpty()) {
        KService::Ptr service = KService::serviceByDesktopName(desktopName);
        if (service) {
            if (instance->m_executable.isEmpty()) {
                instance->m_executable = KIO::DesktopExecParser::executablePath(service->exec());
            }
            instance->init(service, service->name(), service->icon(), asn);
            return instance;
        }
    }
    instance->init(KService::Ptr(), execName /*user-visible name*/, iconName, asn);
    return instance;
}


void KProcessRunner::init(const KService::Ptr &service, const QString &userVisibleName, const QString &iconName, const QByteArray &asn)
{
    if (service && !service->entryPath().isEmpty()
            && !KDesktopFile::isAuthorizedDesktopFile(service->entryPath())) {
        qCWarning(KIO_GUI) << "No authorization to execute" << service->entryPath();
        emitDelayedError(i18n("You are not authorized to execute this file."));
        return;
    }

#if HAVE_X11
    static bool isX11 = QGuiApplication::platformName() == QLatin1String("xcb");
    if (isX11) {
        bool silent;
        QByteArray wmclass;
        const bool startup_notify = (asn != "0" && KIOGuiPrivate::checkStartupNotify(service.data(), &silent, &wmclass));
        if (startup_notify) {
            m_startupId.initId(asn);
            m_startupId.setupStartupEnv();
            KStartupInfoData data;
            data.setHostname();
            // When it comes from a desktop file, m_executable can be a full shell command, so <bin> here is not 100% reliable.
            // E.g. it could be "cd", which isn't an existing binary. It's just a heuristic anyway.
            const QString bin = KIO::DesktopExecParser::executableName(m_executable);
            data.setBin(bin);
            if (!userVisibleName.isEmpty()) {
                data.setName(userVisibleName);
            } else if (service && !service->name().isEmpty()) {
                data.setName(service->name());
            }
            data.setDescription(i18n("Launching %1", data.name()));
            if (!iconName.isEmpty()) {
                data.setIcon(iconName);
            } else if (service && !service->icon().isEmpty()) {
                data.setIcon(service->icon());
            }
            if (!wmclass.isEmpty()) {
                data.setWMClass(wmclass);
            }
            if (silent) {
                data.setSilent(KStartupInfoData::Yes);
            }
            data.setDesktop(KWindowSystem::currentDesktop());
            if (service && !service->entryPath().isEmpty()) {
                data.setApplicationId(service->entryPath());
            }
            KStartupInfo::sendStartup(m_startupId, data);
        }
    }
#else
    Q_UNUSED(bin);
    Q_UNUSED(userVisibleName);
    Q_UNUSED(iconName);
#endif
    if (service) {
        // Store the desktop name, used by debug output and for the systemd unit name
        m_desktopName = service->menuId();
        if (m_desktopName.endsWith(QLatin1String(".desktop"))) { // always true, in theory
            m_desktopName.chop(strlen(".desktop"));
        }
        if (m_desktopName.isEmpty()) { // desktop files not in the menu
            // desktopEntryName is lowercase so this is only a fallback
            m_desktopName = service->desktopEntryName();
        }
        m_desktopFilePath = QFileInfo(service->entryPath()).absoluteFilePath();
        m_description = service->name();
        if (!service->genericName().isEmpty()) {
            m_description.append(QStringLiteral(" - %1").arg(service->genericName()));
        }
    } else {
        m_description = userVisibleName;
    }
    startProcess();
}

void ForkingProcessRunner::startProcess()
{
    connect(m_process.get(), QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ForkingProcessRunner::slotProcessExited);
    connect(m_process.get(), &QProcess::started,
            this, &ForkingProcessRunner::slotProcessStarted, Qt::QueuedConnection);
    connect(m_process.get(), &QProcess::errorOccurred,
            this, &ForkingProcessRunner::slotProcessError);

    m_process->start();
}

bool ForkingProcessRunner::waitForStarted(int timeout)
{
    return m_process->waitForStarted(timeout);
}

void ForkingProcessRunner::slotProcessError(QProcess::ProcessError errorCode)
{
    // E.g. the process crashed.
    // This is unlikely to happen while the ApplicationLauncherJob is still connected to the KProcessRunner.
    // So the emit does nothing, this is really just for debugging.
    qCDebug(KIO_GUI) << name() << "error=" << errorCode << m_process->errorString();
    Q_EMIT error(m_process->errorString());
}

void ForkingProcessRunner::slotProcessStarted()
{
    setPid(m_process->processId());
}

void KProcessRunner::setPid(qint64 pid)
{
    if (!m_pid && pid) {
        qCDebug(KIO_GUI) << "Setting PID" << pid << "for:" << name();
        m_pid = pid;
#if HAVE_X11
        if (!m_startupId.isNull()) {
            KStartupInfoData data;
            data.addPid(static_cast<int>(m_pid));
            KStartupInfo::sendChange(m_startupId, data);
            KStartupInfo::resetStartupEnv();
        }
#endif
    }
    Q_EMIT processStarted(pid);
}

KProcessRunner::~KProcessRunner()
{
    // This destructor deletes m_process, since it's a unique_ptr.
    --s_instanceCount;
}

int KProcessRunner::instanceCount()
{
    return s_instanceCount;
}

void KProcessRunner::terminateStartupNotification()
{
#if HAVE_X11
    if (!m_startupId.isNull()) {
        KStartupInfoData data;
        data.addPid(static_cast<int>(m_pid)); // announce this pid for the startup notification has finished
        data.setHostname();
        KStartupInfo::sendFinish(m_startupId, data);
    }
#endif
}

QString KProcessRunner::name() const
{
    return !m_desktopName.isEmpty() ? m_desktopName : m_executable;
}

void KProcessRunner::emitDelayedError(const QString &errorMsg)
{
    qCWarning(KIO_GUI) << errorMsg;
    terminateStartupNotification();
    // Use delayed invocation so the caller has time to connect to the signal
    QMetaObject::invokeMethod(this, [this, errorMsg]() {
        emit error(errorMsg);
        deleteLater();
    }, Qt::QueuedConnection);
}

void ForkingProcessRunner::slotProcessExited(int exitCode, QProcess::ExitStatus exitStatus)
{
    qCDebug(KIO_GUI) << name() << "exitCode=" << exitCode << "exitStatus=" << exitStatus;
    terminateStartupNotification();
    deleteLater();
}

// This code is also used in klauncher (and KRun).
bool KIOGuiPrivate::checkStartupNotify(const KService *service, bool *silent_arg, QByteArray *wmclass_arg)
{
    bool silent = false;
    QByteArray wmclass;
    if (service && service->property(QStringLiteral("StartupNotify")).isValid()) {
        silent = !service->property(QStringLiteral("StartupNotify")).toBool();
        wmclass = service->property(QStringLiteral("StartupWMClass")).toString().toLatin1();
    } else if (service && service->property(QStringLiteral("X-KDE-StartupNotify")).isValid()) {
        silent = !service->property(QStringLiteral("X-KDE-StartupNotify")).toBool();
        wmclass = service->property(QStringLiteral("X-KDE-WMClass")).toString().toLatin1();
    } else { // non-compliant app
        if (service) {
            if (service->isApplication()) { // doesn't have .desktop entries needed, start as non-compliant
                wmclass = "0"; // krazy:exclude=doublequote_chars
            } else {
                return false; // no startup notification at all
            }
        } else {
#if 0
            // Create startup notification even for apps for which there shouldn't be any,
            // just without any visual feedback. This will ensure they'll be positioned on the proper
            // virtual desktop, and will get user timestamp from the ASN ID.
            wmclass = '0';
            silent = true;
#else   // That unfortunately doesn't work, when the launched non-compliant application
            // launches another one that is compliant and there is any delay inbetween (bnc:#343359)
            return false;
#endif
        }
    }
    if (silent_arg) {
        *silent_arg = silent;
    }
    if (wmclass_arg) {
        *wmclass_arg = wmclass;
    }
    return true;
}

ForkingProcessRunner::ForkingProcessRunner()
    : KProcessRunner()
{
}
