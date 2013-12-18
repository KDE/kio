/* This file is part of the KDE project
   Copyright (C) 2013 Dawit Alemayehu <adawit@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "globaltest.h"
#include <QTest>

#include <kio/global.h>

#include <QFile>

#include <sys/stat.h>

QTEST_MAIN(GlobalTest)


void GlobalTest::testUserPermissionConversion()
{
    const int permissions = S_IRUSR | S_IWUSR | S_IXUSR;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, perms);

    perms = (QFile::ReadGroup | QFile::WriteGroup |QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadOther | QFile::WriteOther |QFile::ExeOther);
    QCOMPARE(qPermissions & perms, 0);
}

void GlobalTest::testGroupPermissionConversion()
{
    const int permissions = S_IRGRP | S_IWGRP | S_IXGRP;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadGroup | QFile::WriteGroup |QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, perms);

    perms = (QFile::ReadOther | QFile::WriteOther |QFile::ExeOther);
    QCOMPARE(qPermissions & perms, 0);
}

void GlobalTest::testOtherPermissionConversion()
{
    const int permissions = S_IROTH | S_IWOTH | S_IXOTH;
    QFile::Permissions qPermissions = KIO::convertPermissions(permissions);

    QFile::Permissions perms = (QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadGroup | QFile::WriteGroup |QFile::ExeGroup);
    QCOMPARE(qPermissions & perms, 0);

    perms = (QFile::ReadOther | QFile::WriteOther |QFile::ExeOther);
    QCOMPARE(qPermissions & perms, perms);
}

#include "globaltest.moc"
