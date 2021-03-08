/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020-2021 David Redondo <kde@david-redondo.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef DBUSACTIVATIONRUNNER_P_H
#define DBUSACTIVATIONRUNNER_P_H

#include "kprocessrunner_p.h"

class DBusActivationRunner : public KProcessRunner
{
    Q_OBJECT
public:
    explicit DBusActivationRunner(const QString &action);
    void startProcess() override;
    bool waitForStarted(int timeout = 30000) override;
    static bool activationPossible(const KService::Ptr service, KIO::ApplicationLauncherJob::RunFlags flags, const QString &suggestedFileName);

private:
    QString m_actionName;
    bool m_finished = false;
};

#endif
