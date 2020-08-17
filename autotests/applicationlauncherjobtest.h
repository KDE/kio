/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014, 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef APPLICATIONLAUNCHERJOBTEST_H
#define APPLICATIONLAUNCHERJOBTEST_H

#include <QObject>
#include <QStringList>

class ApplicationLauncherJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void startProcess_data();
    void startProcess();

    void shouldFailOnNonExecutableDesktopFile_data();
    void shouldFailOnNonExecutableDesktopFile();

    void shouldFailOnNonExistingExecutable_data();
    void shouldFailOnNonExistingExecutable();

    void shouldFailOnInvalidService();
    void shouldFailOnServiceWithNoExec();
    void shouldFailOnExecutableWithoutPermissions();

    void showOpenWithDialog_data();
    void showOpenWithDialog();

private:
    QString createTempService();
    void writeTempServiceDesktopFile(const QString &filePath);

    QStringList m_filesToRemove;
    QString m_tempService;
};

#endif /* APPLICATIONLAUNCHERJOBTEST_H */

