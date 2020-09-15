/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef OPENURLJOBTEST_H
#define OPENURLJOBTEST_H

#include <QObject>
#include <QStringList>
#include <QTemporaryDir>

class OpenUrlJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void startProcess_data();
    void startProcess();

    void noServiceNoHandler();
    void invalidUrl();
    void refuseRunningNativeExecutables_data();
    void refuseRunningNativeExecutables();
    void refuseRunningRemoteNativeExecutables_data();
    void refuseRunningRemoteNativeExecutables();
    void notAuthorized();
    void runScript_data();
    void runScript();
    void runNativeExecutable_data();
    void runNativeExecutable();
    void openOrExecuteScript_data();
    void openOrExecuteScript();
    void openOrExecuteDesktop_data();
    void openOrExecuteDesktop();
    void launchExternalBrowser_data();
    void launchExternalBrowser();
    void nonExistingFile();

    void httpUrlWithKIO();
    void ftpUrlWithKIO();

    void takeOverAfterMimeTypeFound();
    void runDesktopFileDirectly();

private:
    void writeApplicationDesktopFile(const QString &filePath, const QByteArray &cmd);

    QStringList m_filesToRemove;
    QTemporaryDir m_tempDir;
    QString m_fakeService;
};

#endif /* OPENURLJOBTEST_H */

