/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KTERMINALLAUNCHERJOBTEST_H
#define KTERMINALLAUNCHERJOBTEST_H

#include <QObject>
#include <QTemporaryDir>

class KTerminalLauncherJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

#ifndef Q_OS_WIN
    void startKonsole_data();
    void startKonsole();
    void startXterm();
    void startFallbackToPath();
#else
    void startTerminal_data();
    void startTerminal();
#endif

private:
    QTemporaryDir m_tempDir;
};

#endif
