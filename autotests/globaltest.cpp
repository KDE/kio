/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "globaltest.h"
#include <QTest>

#include "global.h"
#include "kioglobal_p.h"

#include <QFile>

#include <sys/stat.h>

QTEST_MAIN(GlobalTest)

void GlobalTest::testUserPermissionConversion()
{
    const int permissions = S_IRUSR | S_IWUSR | S_IXUSR;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, perms);

    perms = (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
    QCOMPARE(qPermissions & perms, 0);
}

void GlobalTest::testGroupPermissionConversion()
{
    const int permissions = S_IRGRP | S_IWGRP | S_IXGRP;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, perms);

    perms = (QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
    QCOMPARE(qPermissions & perms, 0);
}

void GlobalTest::testOtherPermissionConversion()
{
    const int permissions = S_IROTH | S_IWOTH | S_IXOTH;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
    QCOMPARE(qPermissions & perms, perms);
}
