/*
    This file is part of the KDE libraries
    Copyright (c) 2014, 2020 David Faure <faure@kde.org>

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

private:
    QString createTempService();
    void writeTempServiceDesktopFile(const QString &filePath);

    QStringList m_filesToRemove;

};

#endif /* APPLICATIONLAUNCHERJOBTEST_H */

