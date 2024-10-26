#include "kiogui_debug.h"
#include "managerinterface.h"
#include "scopedprocessrunner_p.h"
#include "systemdprocessrunner_p.h"

#include <sys/eventfd.h>
#include <unistd.h>

using namespace org::freedesktop;

ScopedProcessRunner::ScopedProcessRunner()
    : ForkingProcessRunner()
{
}

void ScopedProcessRunner::startProcess()
{
    std::function oldModifier = m_process->childProcessModifier();
    int efd = eventfd(0, EFD_CLOEXEC);
    m_process->setChildProcessModifier([efd, oldModifier]() {
        // wait for the parent process to be done registering the transient unit
        eventfd_read(efd, nullptr);
        if (oldModifier)
            oldModifier();
    });

    // actually start
    ForkingProcessRunner::startProcess();
    m_process->setChildProcessModifier(oldModifier);

    Q_ASSERT(m_process->processId());

    // As specified in "XDG standardization for applications" in https://systemd.io/DESKTOP_ENVIRONMENTS/
    const QString serviceName = QStringLiteral("app-%1-%2.scope").arg(escapeUnitName(resolveServiceAlias()), QUuid::createUuid().toString(QUuid::Id128));

    const auto manager = new systemd1::Manager(systemdService, systemdPath, QDBusConnection::sessionBus(), this);

    // Ask systemd for a new transient service
    const auto startReply =
        manager->StartTransientUnit(serviceName,
                                    QStringLiteral("fail"), // mode defines what to do in the case of a name conflict, in this case, just do nothing
                                    {// Properties of the transient service unit
                                     {QStringLiteral("Slice"), QStringLiteral("app.slice")},
                                     {QStringLiteral("Description"), m_description},
                                     {QStringLiteral("SourcePath"), m_desktopFilePath},
                                     {QStringLiteral("PIDs"), QVariant::fromValue(QList<uint>{static_cast<uint>(m_process->processId())})}},
                                    {} // aux is currently unused and should be passed as empty array.
        );

    m_transientUnitStartupwatcher = new QDBusPendingCallWatcher(startReply, this);
    connect(m_transientUnitStartupwatcher, &QDBusPendingCallWatcher::finished, [serviceName, efd](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (reply.isError()) {
            qCWarning(KIO_GUI) << "Failed to register new cgroup:" << serviceName << reply.error().name() << reply.error().message();
        } else {
            qCDebug(KIO_GUI) << "Successfully registered new cgroup:" << serviceName;
        }

        // release child and close the eventfd
        eventfd_write(efd, 1);
        close(efd);
    });
}

bool ScopedProcessRunner::waitForStarted(int timeout)
{
    if (m_process->state() == QProcess::NotRunning || m_waitingForXdgToken || !m_transientUnitStartupwatcher->isFinished()) {
        QEventLoop loop;
        QObject::connect(m_process.get(), &QProcess::stateChanged, &loop, &QEventLoop::quit);
        QObject::connect(m_transientUnitStartupwatcher, &QDBusPendingCallWatcher::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        loop.exec();
    }
    return m_process->waitForStarted(timeout);
}

#include "moc_scopedprocessrunner_p.cpp"
