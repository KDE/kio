/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "systemdprocessrunner_p.h"
#include "kiogui_debug.h"

#include "managerinterface.h"
#include "propertiesinterface.h"
#include "unitinterface.h"

#include <QTimer>

#include <mutex>
#include <signal.h>

using namespace org::freedesktop;

bool SystemdProcessRunner::isAvailable()
{
    static std::once_flag dbusRegistered;
    static bool runAsService = false;
    std::call_once(dbusRegistered, []() {
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(systemdService)) {
            runAsService = true;
            qDBusRegisterMetaType<QVariantMultiItem>();
            qDBusRegisterMetaType<QVariantMultiMap>();
            qDBusRegisterMetaType<TransientAux>();
            qDBusRegisterMetaType<TransientAuxList>();
            qDBusRegisterMetaType<ExecCommand>();
            qDBusRegisterMetaType<ExecCommandList>();
        }
    });
    return runAsService;
}

SystemdProcessRunner::SystemdProcessRunner()
    : KProcessRunner()
{
}

// Only alphanum, ':' and '_' allowed in systemd unit names
QString SystemdProcessRunner::escapeUnitName(const QString &input)
{
    QString res;
    const auto bytes = input.toUtf8();
    for (const auto &c : bytes) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ':' || c == '_' || c == '.') {
            res += QLatin1Char(c);
        } else {
            res += QStringLiteral("\\x%1").arg(c, 2, 16, QLatin1Char('0'));
        }
    }
    return res;
}

bool SystemdProcessRunner::waitForStarted(int timeout)
{
    if (m_pid || m_exited) {
        return true;
    }
    QEventLoop loop;
    bool success = false;
    loop.connect(this, &KProcessRunner::processStarted, [&loop, &success]() {
        loop.quit();
        success = true;
    });
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    QObject::connect(this, &KProcessRunner::error, &loop, &QEventLoop::quit);
    loop.exec();
    return success;
}

void SystemdProcessRunner::startProcess()
{
    m_serviceName = QStringLiteral("app-%1@%2.service")
                        .arg(escapeUnitName(name()),
                            QUuid::createUuid().toString(QUuid::Id128));

    // Watch for new services
    m_manager = new systemd1::Manager(systemdService, systemdPath, QDBusConnection::sessionBus(), this);
    m_manager->Subscribe();
    connect(m_manager, &systemd1::Manager::UnitNew, this, &SystemdProcessRunner::handleUnitNew);

    // Watch for service creation job error
    connect(m_manager,
        &systemd1::Manager::JobRemoved,
        this,
        [this](uint jobId, const QDBusObjectPath &jobPath, const QString &unitName, const QString &result) {
            Q_UNUSED(jobId)
            if (jobPath.path() == m_jobPath && unitName == m_serviceName && result != QLatin1String("done")) {
                qCWarning(KIO_GUI) << "Failed to launch process as service:" << m_serviceName << ", result " << result;
                // result=failed is not a fatal error, service is actually created in this case
                if (result != QLatin1String("failed")) {
                    systemdError(result);
                }
            }
        });

    // Ask systemd for a new transient service
    const auto startReply = m_manager->StartTransientUnit(m_serviceName,
        QStringLiteral("fail"), // mode defines what to do in the case of a name conflict, in this case, just do nothing
        { // Properties of the transient service unit
            { QStringLiteral("Type"), QStringLiteral("oneshot") },
            { QStringLiteral("Slice"), QStringLiteral("app.slice") },
            { QStringLiteral("Description"), m_description },
            { QStringLiteral("SourcePath"), m_desktopFilePath },
            { QStringLiteral("AddRef"), true }, // Asks systemd to avoid garbage collecting the service if it immediately crashes,
                                                // so we can be notified (see https://github.com/systemd/systemd/pull/3984)
            { QStringLiteral("Environment"), m_process->environment() },
            { QStringLiteral("WorkingDirectory"), m_process->workingDirectory() },
            { QStringLiteral("ExecStart"), QVariant::fromValue(ExecCommandList { { m_process->program().first(), m_process->program(), false } }) },
            { QStringLiteral("KillMode"), QStringLiteral("process")}
        },
        {} // aux is currently unused and should be passed as empty array.
    );
    connect(new QDBusPendingCallWatcher(startReply, this), &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qCWarning(KIO_GUI) << "Failed to launch process as service:" << m_serviceName << reply.error().name() << reply.error().message();
            return systemdError(reply.error().name());
        }
        qCDebug(KIO_GUI) << "Successfully asked systemd to launch process as service:" << m_serviceName;
        m_jobPath = reply.argumentAt<0>().path();
    });
}

void SystemdProcessRunner::handleProperties(QDBusPendingCallWatcher *watcher)
{
    const QDBusPendingReply<QVariantMap> reply = *watcher;
    watcher->deleteLater();
    if (reply.isError()) {
        qCWarning(KIO_GUI) << "Failed to get properties for service:" << m_serviceName << reply.error().name()
                           << reply.error().message();
        return systemdError(reply.error().name());
    }
    qCDebug(KIO_GUI) << "Successfully retrieved properties for service:" << m_serviceName;
    if (m_exited) {
        return;
    }
    const auto properties = reply.argumentAt<0>();
    if (!m_pid) {
        setPid(properties[QStringLiteral("ExecMainPID")].value<quint32>());
        return;
    }
    const auto activeState = properties[QStringLiteral("ActiveState")].toString();
    if (activeState != QLatin1String("inactive") && activeState != QLatin1String("failed")) {
        return;
    }
    m_exited = true;

    // ExecMainCode/Status correspond to si_code/si_status in the siginfo_t structure
    // ExecMainCode is the signal code: CLD_EXITED (1) means normal exit
    // ExecMainStatus is the process exit code in case of normal exit, otherwise it is the signal number
    const auto signalCode = properties[QStringLiteral("ExecMainCode")].value<qint32>();
    const auto exitCodeOrSignalNumber = properties[QStringLiteral("ExecMainStatus")].value<qint32>();
    const auto exitStatus = signalCode == CLD_EXITED ? QProcess::ExitStatus::NormalExit : QProcess::ExitStatus::CrashExit;

    qCDebug(KIO_GUI) << m_serviceName << "pid=" << m_pid << "exitCode=" << exitCodeOrSignalNumber << "exitStatus=" << exitStatus;
    terminateStartupNotification();
    deleteLater();

    systemd1::Unit unitInterface(systemdService, m_servicePath, QDBusConnection::sessionBus(), this);
    connect(new QDBusPendingCallWatcher(unitInterface.Unref(), this), &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qCWarning(KIO_GUI) << "Failed to unref service:" << m_serviceName << reply.error().name() << reply.error().message();
            return systemdError(reply.error().name());
        }
        qCDebug(KIO_GUI) << "Successfully unref'd service:" << m_serviceName;
    });
}

void SystemdProcessRunner::handleUnitNew(const QString &newName, const QDBusObjectPath &newPath)
{
    if (newName != m_serviceName) {
        return;
    }
    qCDebug(KIO_GUI) << "Successfully launched process as service:" << m_serviceName;

    // Get PID (and possibly exit code) from systemd service properties
    m_servicePath = newPath.path();
    m_serviceProperties = new DBus::Properties(systemdService, m_servicePath, QDBusConnection::sessionBus(), this);
    auto propReply = m_serviceProperties->GetAll(QString());
    connect(new QDBusPendingCallWatcher(propReply, this),
        &QDBusPendingCallWatcher::finished,
        this,
        &SystemdProcessRunner::handleProperties);

    // Watch for status change
    connect(m_serviceProperties, &DBus::Properties::PropertiesChanged, this, [this]() {
        if (m_exited) {
            return;
        }
        qCDebug(KIO_GUI) << "Got PropertiesChanged signal:" << m_serviceName;
        // We need to look at the full list of properties rather than only those which changed
        auto reply = m_serviceProperties->GetAll(QString());
        connect(new QDBusPendingCallWatcher(reply, this),
            &QDBusPendingCallWatcher::finished,
            this,
            &SystemdProcessRunner::handleProperties);
    });
}

void SystemdProcessRunner::systemdError(const QString &message)
{
    Q_EMIT error(message);
    deleteLater();
}
