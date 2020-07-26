/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

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
    void refuseRunningNativeExecutables();
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

