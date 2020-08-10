/*
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef PRIVILEGEJOBTEST_H
#define PRIVILEGEJOBTEST_H

#include <QObject>

class PrivilegeJobTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void privilegeChmod();
    void privilegeCopy();
    void privilegeDelete();
    void privilegeMkpath();
    void privilegePut();
    void privilegeRename();
    void privileSymlink();

private:
    QString m_testFilePath;
};

#endif
