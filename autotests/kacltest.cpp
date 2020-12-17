/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2005 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kacltest.h"

#include <QTest>
#include <config-kiocore.h>

#if HAVE_POSIX_ACL
#include <sys/types.h>
#include <grp.h> // getgrnam()
#endif

// The code comes partly from kdebase/kioslave/trash/testtrash.cpp

QTEST_MAIN(KACLTest)

KACLTest::KACLTest()
{
}

void KACLTest::initTestCase()
{
#if !HAVE_POSIX_ACL
    QSKIP("ACL support not compiled");
#endif

    m_testACL = QStringLiteral("user::rw-\n"
                               "user:bin:rwx\n"
                               "group::rw-\n"
                               "mask::rwx\n"
                               "other::r--\n");


    m_acl = KACL(m_testACL);

    // setACL call acl_from_text(), which seems to order the groups in the resulting ACL
    // according to group numeric Id, in ascending order. Find which group comes first
    // so that the tests pass regardless of which distro they're run on.
    auto *grpStruct = getgrnam("audio");
    m_audioGid = static_cast<int>(grpStruct->gr_gid);

    grpStruct = getgrnam("users");
    m_usersGid = static_cast<int>(grpStruct->gr_gid);

    QLatin1String orderedGroups;
    if (m_audioGid < m_usersGid) {
        orderedGroups = QLatin1String{"group:audio:--x\n"
                                      "group:users:r--\n"};
    } else {
        orderedGroups = QLatin1String{"group:users:r--\n"
                                      "group:audio:--x\n"};
    }

    m_testACL2 = QLatin1String{"user::rwx\n"
                               "user:bin:rwx\n"
                               "group::rw-\n"}
                + orderedGroups
                + QLatin1String{"mask::r-x\n"
                                "other::r--\n"};

    m_testACLEffective = QLatin1String{"user::rwx\n"
                                       "user:bin:rwx    #effective:r-x\n"
                                       "group::rw-      #effective:r--\n"}
                         + orderedGroups
                         + QLatin1String{"mask::r-x\n"
                                         "other::r--\n"};

    QVERIFY(m_acl2.setACL(m_testACL2));
}

void KACLTest::testAsString()
{
    QCOMPARE(m_acl.asString(), m_testACL);
}

void KACLTest::testSetACL()
{
    QCOMPARE(m_acl2.asString().simplified(), m_testACLEffective.simplified());
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
    QCOMPARE(name, QStringLiteral("bin"));
    QCOMPARE(permissions, 7);
}

void KACLTest::testGetAllGroupsPermissions()
{
    const ACLGroupPermissionsList list = m_acl2.allGroupPermissions();
    QCOMPARE(list.size(), 2);

    const bool isAudioFirst = m_audioGid < m_usersGid;

    QString name;
    int permissions;

    const auto acl1 = list.at(0);
    name = acl1.first;
    permissions = acl1.second;
    if (isAudioFirst) {
        QCOMPARE(name, QStringLiteral("audio"));
        QCOMPARE(permissions, 1);
    } else {
        QCOMPARE(name, QStringLiteral("users"));
        QCOMPARE(permissions, 4);
    }

    const auto acl2 = list.at(1);
    name = acl2.first;
    permissions = acl2.second;
    if (isAudioFirst) {
        QCOMPARE(name, QStringLiteral("users"));
        QCOMPARE(permissions, 4);
    } else {
        QCOMPARE(name, QStringLiteral("audio"));
        QCOMPARE(permissions, 1);
    }
}

void KACLTest::testIsExtended()
{
    KACL dukeOfMonmoth(m_testACL);
    QVERIFY(dukeOfMonmoth.isExtended());
    KACL earlOfUpnor(QStringLiteral("user::r--\ngroup::r--\nother::r--\n"));
    QVERIFY(!earlOfUpnor.isExtended());
}

void KACLTest::testOperators()
{
    KACL dukeOfMonmoth(m_testACL);
    KACL JamesScott(m_testACL);
    KACL earlOfUpnor(m_testACL2);
    QVERIFY(!(dukeOfMonmoth == earlOfUpnor));
    QVERIFY(dukeOfMonmoth != earlOfUpnor);
    QVERIFY(dukeOfMonmoth != earlOfUpnor);
    QVERIFY(!(dukeOfMonmoth != JamesScott));
}

void KACLTest::testSettingBasic()
{
    KACL CharlesII(m_testACL);
    CharlesII.setOwnerPermissions(7); // clearly
    CharlesII.setOwningGroupPermissions(0);
    CharlesII.setOthersPermissions(0);
    QCOMPARE(int(CharlesII.ownerPermissions()), 7);
    QCOMPARE(int(CharlesII.owningGroupPermissions()), 0);
    QCOMPARE(int(CharlesII.othersPermissions()), 0);
}

void KACLTest::testSettingExtended()
{
    KACL CharlesII(m_testACL);
    CharlesII.setMaskPermissions(7); // clearly
    bool dummy = false;
    QCOMPARE(int(CharlesII.maskPermissions(dummy)), 7);

    const QString expected(QStringLiteral("user::rw-\nuser:root:rwx\nuser:bin:r--\ngroup::rw-\nmask::rwx\nother::r--\n"));

    ACLUserPermissionsList users;
    ACLUserPermissions user = qMakePair(QStringLiteral("root"), (unsigned short)7);
    users.append(user);
    user = qMakePair(QStringLiteral("bin"), (unsigned short)4);
    users.append(user);
    CharlesII.setAllUserPermissions(users);
    QCOMPARE(CharlesII.asString(), expected);

    CharlesII.setACL(m_testACL); // reset
    // it already has an entry for bin, let's change it
    CharlesII.setNamedUserPermissions(QStringLiteral("bin"), 4);
    CharlesII.setNamedUserPermissions(QStringLiteral("root"), 7);
    QCOMPARE(CharlesII.asString(), expected);

    // groups, all and named

    QLatin1String orderedGroups;
    if (m_audioGid < m_usersGid) {
        orderedGroups = QLatin1String{"group:audio:-wx\n"
                                      "group:users:r--\n"};
    } else {
        orderedGroups = QLatin1String{"group:users:r--\n"
                                      "group:audio:-wx\n"};
    }

    const QString expected2 = QLatin1String{"user::rw-\n"
                                            "user:bin:rwx\n"
                                            "group::rw-\n"}
                              + orderedGroups
                              + QLatin1String{"mask::rwx\n"
                                              "other::r--\n"};

    CharlesII.setACL(m_testACL); // reset
    ACLGroupPermissionsList groups;
    ACLGroupPermissions group = qMakePair(QStringLiteral("audio"), (unsigned short)3);
    groups.append(group);
    group = qMakePair(QStringLiteral("users"), (unsigned short)4);
    groups.append(group);
    CharlesII.setAllGroupPermissions(groups);
    QCOMPARE(CharlesII.asString(), expected2);

    CharlesII.setACL(m_testACL); // reset
    CharlesII.setNamedGroupPermissions(QStringLiteral("audio"), 3);
    CharlesII.setNamedGroupPermissions(QStringLiteral("users"), 4);
    QCOMPARE(CharlesII.asString(), expected2);
}

void KACLTest::testSettingErrorHandling()
{
    KACL foo(m_testACL);
    bool v = foo.setNamedGroupPermissions(QStringLiteral("audio"), 7); // existing group
    QVERIFY(v);
    v = foo.setNamedGroupPermissions(QStringLiteral("jongel"), 7); // non-existing group
    QVERIFY(!v);

    v = foo.setNamedUserPermissions(QStringLiteral("bin"), 7); // existing user
    QVERIFY(v);
    v = foo.setNamedUserPermissions(QStringLiteral("jongel"), 7); // non-existing user
    QVERIFY(!v);
}

void KACLTest::testNewMask()
{
    KACL CharlesII(QStringLiteral("user::rw-\ngroup::rw-\nother::rw\n"));
    bool dummy = false;
    CharlesII.maskPermissions(dummy);
    QVERIFY(!dummy);

    CharlesII.setMaskPermissions(6);
    QCOMPARE(int(CharlesII.maskPermissions(dummy)), 6);
    QVERIFY(dummy); // mask exists now
}
