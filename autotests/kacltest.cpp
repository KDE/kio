/* This file is part of the KDE project
   Copyright (C) 2005 Till Adam <adam@kde.org>

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

#include "kacltest.h"

#include <QtTest/QtTest>
#include <config-kiocore.h>

#include <kacl.h>
#include <QDebug>

#include <QDir>

// The code comes partly from kdebase/kioslave/trash/testtrash.cpp

QTEST_MAIN(KACLTest)

static const QString s_testACL("user::rw-\nuser:bin:rwx\ngroup::rw-\nmask::rwx\nother::r--\n");
static const QString s_testACL2("user::rwx\nuser:bin:rwx\ngroup::rw-\ngroup:users:r--\ngroup:audio:--x\nmask::r-x\nother::r--\n");
static const QString s_testACLEffective("user::rwx\nuser:bin:rwx    #effective:r-x\ngroup::rw-      #effective:r--\ngroup:audio:--x\ngroup:users:r--\nmask::r-x\nother::r--\n");

KACLTest::KACLTest()
  : m_acl(s_testACL)
{
}

void KACLTest::initTestCase()
{
    m_acl2.setACL(s_testACL2);
}

void KACLTest::testAsString()
{
    QCOMPARE(m_acl.asString(), s_testACL);
}

void KACLTest::testSetACL()
{
    QCOMPARE(m_acl2.asString().simplified(), s_testACLEffective.simplified());
}

void KACLTest::testGetOwnerPermissions()
{
    QCOMPARE(int(m_acl.ownerPermissions()), 6);
}

void KACLTest::testGetOwningGroupPermissions()
{
    QCOMPARE(int(m_acl.owningGroupPermissions()), 6);
}

void KACLTest::testGetOthersPermissions()
{
    QCOMPARE(int(m_acl.othersPermissions()), 4);
}

void KACLTest::testGetMaskPermissions()
{
    bool exists = false;
    int mask = m_acl.maskPermissions(exists);
    QVERIFY(exists);
    QCOMPARE(mask, 7);
}

void KACLTest::testGetAllUserPermissions()
{
    ACLUserPermissionsList list = m_acl.allUserPermissions();
    ACLUserPermissionsConstIterator it = list.constBegin();
    QString name;
    int permissions = 0;
    int count = 0;
    while (it != list.constEnd()) {
      name = (*it).first;
      permissions = (*it).second;
      ++it;
      ++count;
    }
    QCOMPARE(count, 1);
    QCOMPARE(name, QString("bin"));
    QCOMPARE(permissions, 7);
}

void KACLTest::testGetAllGroupsPermissions()
{
  ACLGroupPermissionsList list = m_acl2.allGroupPermissions();
  ACLGroupPermissionsConstIterator it = list.constBegin();
  QString name;
  int permissions;
  int count = 0;
  while (it != list.constEnd()) {
    name = (*it).first;
    permissions = (*it).second;
    // setACL sorts them alphabetically ...
    if (count == 0) {
        QCOMPARE(name, QString("audio"));
        QCOMPARE(permissions, 1);
    } else if (count == 1) {
        QCOMPARE(name, QString("users"));
        QCOMPARE(permissions, 4);
    }
    ++it;
    ++count;
  }
  QCOMPARE(count, 2);
}

void KACLTest::testIsExtended()
{
    KACL dukeOfMonmoth(s_testACL);
    QVERIFY(dukeOfMonmoth.isExtended());
    KACL earlOfUpnor("user::r--\ngroup::r--\nother::r--\n");
    QVERIFY(!earlOfUpnor.isExtended());
}

void KACLTest::testOperators()
{
    KACL dukeOfMonmoth(s_testACL);
    KACL JamesScott(s_testACL);
    KACL earlOfUpnor(s_testACL2);
    QVERIFY(!(dukeOfMonmoth == earlOfUpnor));
    QVERIFY(dukeOfMonmoth != earlOfUpnor);
    QVERIFY(dukeOfMonmoth != earlOfUpnor);
    QVERIFY(!(dukeOfMonmoth != JamesScott));
}

void KACLTest::testSettingBasic()
{
    KACL CharlesII(s_testACL);
    CharlesII.setOwnerPermissions(7); // clearly
    CharlesII.setOwningGroupPermissions(0);
    CharlesII.setOthersPermissions(0);
    QCOMPARE(int(CharlesII.ownerPermissions()), 7);
    QCOMPARE(int(CharlesII.owningGroupPermissions()), 0);
    QCOMPARE(int(CharlesII.othersPermissions()), 0);
}

void KACLTest::testSettingExtended()
{
    KACL CharlesII(s_testACL);
    CharlesII.setMaskPermissions(7); // clearly
    bool dummy = false;
    QCOMPARE(int(CharlesII.maskPermissions(dummy)), 7);

    const QString expected("user::rw-\nuser:root:rwx\nuser:bin:r--\ngroup::rw-\nmask::rwx\nother::r--\n");

    ACLUserPermissionsList users;
    ACLUserPermissions user = qMakePair(QString("root"), (unsigned short)7);
    users.append(user);
    user = qMakePair(QString("bin"), (unsigned short)4);
    users.append(user);
    CharlesII.setAllUserPermissions(users);
    QCOMPARE(CharlesII.asString(), expected);

    CharlesII.setACL(s_testACL); // reset
    // it already has an entry for bin, let's change it
    CharlesII.setNamedUserPermissions(QString("bin"), 4);
    CharlesII.setNamedUserPermissions(QString("root"), 7);
    QCOMPARE(CharlesII.asString(), expected);

    // groups, all and named

    const QString expected2("user::rw-\nuser:bin:rwx\ngroup::rw-\ngroup:audio:-wx\ngroup:users:r--\nmask::rwx\nother::r--\n");
    CharlesII.setACL(s_testACL); // reset
    ACLGroupPermissionsList groups;
    ACLGroupPermissions group = qMakePair(QString("audio"), (unsigned short)3);
    groups.append(group);
    group = qMakePair(QString("users"), (unsigned short)4);
    groups.append(group);
    CharlesII.setAllGroupPermissions(groups);
    QCOMPARE(CharlesII.asString(), expected2);

    CharlesII.setACL(s_testACL); // reset
    CharlesII.setNamedGroupPermissions(QString("audio"), 3);
    CharlesII.setNamedGroupPermissions(QString("users"), 4);
    QCOMPARE(CharlesII.asString(), expected2);
}

void KACLTest::testSettingErrorHandling()
{
    KACL foo(s_testACL);
    bool v = foo.setNamedGroupPermissions("audio", 7); // existing group
    QVERIFY(v);
    v = foo.setNamedGroupPermissions("jongel", 7); // non-existing group
    QVERIFY(!v);

    v = foo.setNamedUserPermissions("bin", 7); // existing user
    QVERIFY(v);
    v = foo.setNamedUserPermissions("jongel", 7); // non-existing user
    QVERIFY(!v);
}

void KACLTest::testNewMask()
{
    KACL CharlesII("user::rw-\ngroup::rw-\nother::rw\n");
    bool dummy = false;
    CharlesII.maskPermissions(dummy);
    QVERIFY(!dummy);

    CharlesII.setMaskPermissions(6);
    QCOMPARE(int(CharlesII.maskPermissions(dummy)), 6);
    QVERIFY(dummy); // mask exists now
}
