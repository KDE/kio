/*
    This file is part of the KDE libraries
    Copyright (c) 2020 Henri Chain <henri.chain@enioka.com>

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

#ifndef SYSTEMDPROCESSRUNNER_H
#define SYSTEMDPROCESSRUNNER_H
#include "kprocessrunner_p.h"

class OrgFreedesktopSystemd1ManagerInterface;
class OrgFreedesktopDBusPropertiesInterface;
class QDBusObjectPath;
class QDBusPendingCallWatcher;

class SystemdProcessRunner : public KProcessRunner
{
    Q_OBJECT

public:
    explicit SystemdProcessRunner(const QString &executable);
    void startProcess() override;
    bool waitForStarted(int timeout) override;
    static bool isAvailable();

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
