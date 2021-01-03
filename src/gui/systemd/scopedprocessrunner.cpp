#include "kiogui_debug.h"
#include "managerinterface.h"
#include "scopedprocessrunner_p.h"
#include "systemdprocessrunner_p.h"

using namespace org::freedesktop;

ScopedProcessRunner::ScopedProcessRunner()
    : ForkingProcessRunner()
{
}

void ScopedProcessRunner::slotProcessStarted()
{
    ForkingProcessRunner::slotProcessStarted();
    // As specified in "XDG standardization for applications" in https://systemd.io/DESKTOP_ENVIRONMENTS/
    QString serviceName = QStringLiteral("app-%1-%2.scope")
                              .arg(SystemdProcessRunner::escapeUnitName(name()), QUuid::createUuid().toString(QUuid::Id128));

    const auto manager = new systemd1::Manager(systemdService, systemdPath, QDBusConnection::sessionBus(), this);

    // Ask systemd for a new transient service
    const auto startReply = manager->StartTransientUnit(
        serviceName,
        QStringLiteral("fail"), // mode defines what to do in the case of a name conflict, in this case, just do nothing
        { // Properties of the transient service unit
          { QStringLiteral("Slice"), QStringLiteral("app.slice") },
          { QStringLiteral("Description"), m_description },
          { QStringLiteral("SourcePath"), m_desktopFilePath },
          { QStringLiteral("PIDs"), QVariant::fromValue(QList<uint> { static_cast<uint>(m_process->processId()) }) } },
        {} // aux is currently unused and should be passed as empty array.
    );

    connect(new QDBusPendingCallWatcher(startReply, this),
            &QDBusPendingCallWatcher::finished,
            [serviceName](QDBusPendingCallWatcher *watcher) {
                QDBusPendingReply<QDBusObjectPath> reply = *watcher;
                watcher->deleteLater();
                if (reply.isError()) {
                    qCWarning(KIO_GUI) << "Failed to register new cgroup:" << serviceName << reply.error().name()
                                       << reply.error().message();
                } else {
                    qCDebug(KIO_GUI) << "Successfully registered new cgroup:" << serviceName;
                }
            });
}
