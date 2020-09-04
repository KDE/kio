/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef SYSTEMDPROCESSRUNNER_H
#define SYSTEMDPROCESSRUNNER_H
#include "kprocessrunner_p.h"

class OrgFreedesktopSystemd1ManagerInterface;
class OrgFreedesktopDBusPropertiesInterface;
class QDBusObjectPath;
class QDBusPendingCallWatcher;

const auto systemdService = QStringLiteral("org.freedesktop.systemd1");
const auto systemdPath = QStringLiteral("/org/freedesktop/systemd1");

class SystemdProcessRunner : public KProcessRunner
{
    Q_OBJECT

public:
    explicit SystemdProcessRunner();
    void startProcess() override;
    bool waitForStarted(int timeout) override;
    static bool isAvailable();
    static QString escapeUnitName(const QString &input);

private:
    void handleProperties(QDBusPendingCallWatcher *watcher);
    void handleUnitNew(const QString &newName, const QDBusObjectPath &newPath);
    void systemdError(const QString &error);

    bool m_exited = false;
    QString m_serviceName;
    QString m_servicePath;
    QString m_jobPath;
    OrgFreedesktopSystemd1ManagerInterface *m_manager = nullptr;
    OrgFreedesktopDBusPropertiesInterface *m_serviceProperties = nullptr;
};
#endif // SYSTEMDPROCESSRUNNER_H
