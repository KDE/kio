/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020-2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dbusactivationrunner_p.h"

#include "kiogui_debug.h"
#include <KWindowSystem>

#ifndef Q_OS_ANDROID
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#endif
#include <QTimer>

bool DBusActivationRunner::activationPossible(const KService::Ptr service, KIO::ApplicationLauncherJob::RunFlags flags, const QString &suggestedFileName)
{
#if defined Q_OS_UNIX && !defined Q_OS_ANDROID
    if (!service->isApplication()) {
        return false;
    }
    if (service->property(QStringLiteral("DBusActivatable"), QVariant::Bool).toBool()) {
        if (!suggestedFileName.isEmpty()) {
            qCDebug(KIO_GUI) << "Cannot activate" << service->desktopEntryName() << "because suggestedFileName is set";
            return false;
        }
        if (flags & KIO::ApplicationLauncherJob::DeleteTemporaryFiles) {
            qCDebug(KIO_GUI) << "Cannot activate" << service->desktopEntryName() << "because DeleteTemporaryFiles is set";
            return false;
        }
        return true;
    }
#endif
    return false;
}

DBusActivationRunner::DBusActivationRunner(const QString &action)
    : KProcessRunner()
    , m_actionName(action)
{
}

void DBusActivationRunner::startProcess()
{
#ifndef Q_OS_ANDROID
    // DBusActivatable as per https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#dbus
    const QString objectPath = QStringLiteral("/%1").arg(m_desktopName).replace(QLatin1Char('.'), QLatin1Char('/'));
    const QString interface = QStringLiteral("org.freedesktop.Application");
    QDBusMessage message;
    if (m_urls.isEmpty()) {
        if (m_actionName.isEmpty()) {
            message = QDBusMessage::createMethodCall(m_desktopName, objectPath, interface, QStringLiteral("Activate"));
        } else {
            message = QDBusMessage::createMethodCall(m_desktopName, objectPath, interface, QStringLiteral("ActivateAction"));
            message << m_actionName << QVariantList();
        }
    } else {
        message = QDBusMessage::createMethodCall(m_desktopName, objectPath, interface, QStringLiteral("Open"));
        message << QUrl::toStringList(m_urls);
    }
    if (KWindowSystem::isPlatformX11()) {
        message << QVariantMap{{QStringLiteral("desktop-startup-id"), m_startupId.id()}};
    } else if (KWindowSystem::isPlatformWayland()) {
        message << QVariantMap{{QStringLiteral("activation-token"), m_process->processEnvironment().value(QStringLiteral("XDG_ACTIVATION_TOKEN"))}};
    }
    auto call = QDBusConnection::sessionBus().asyncCall(message);
    auto activationWatcher = new QDBusPendingCallWatcher(call, this);
    connect(activationWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (watcher->isError()) {
            Q_EMIT error(watcher->error().message());
            terminateStartupNotification();
            m_finished = true;
            deleteLater();
            return;
        }
        auto call = QDBusConnection::sessionBus().interface()->asyncCall(QStringLiteral("GetConnectionUnixProcessID"), m_desktopName);
        auto pidWatcher = new QDBusPendingCallWatcher(call, this);
        connect(pidWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
            m_finished = true;
            QDBusPendingReply<uint> reply = *watcher;
            if (reply.isError()) {
                Q_EMIT error(watcher->error().message());
                terminateStartupNotification();
            } else {
                Q_EMIT processStarted(reply.value());
            }
            deleteLater();
        });
    });
#endif
}

bool DBusActivationRunner::waitForStarted(int timeout)
{
#ifndef Q_OS_ANDROID
    if (m_finished) {
        return m_pid != 0;
    }

    QEventLoop loop;
    bool success = false;
    connect(this, &KProcessRunner::processStarted, [&loop, &success]() {
        loop.quit();
        success = true;
    });
    connect(this, &KProcessRunner::error, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
    loop.exec();
    return success;
#else
    return false;
#endif
}
