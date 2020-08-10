/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef COMMANDLAUNCHERJOBTEST_H
#define COMMANDLAUNCHERJOBTEST_H

#include <QObject>
#include <QStringList>

class CommandLauncherJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void startProcessAsCommand_data();
    void startProcessAsCommand();

    void startProcessWithArgs_data();
    void startProcessWithArgs();

    void startProcessWithSpacesInExecutablePath_data();
    void startProcessWithSpacesInExecutablePath();

    void doesNotFailOnNonExistingExecutable();
    void shouldDoNothingOnEmptyCommand();
};

#endif /* COMMANDLAUNCHERJOBTEST_H */

