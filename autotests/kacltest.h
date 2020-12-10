/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2005 Till Adam <adam@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KACLTEST_H
#define KACLTEST_H

#include <QObject>
#include <kacl.h>

class KACLTest : public QObject
{
    Q_OBJECT
public:
    KACLTest();

private Q_SLOTS:
    void initTestCase();
    void testAsString();
    void testSetACL();
    void testGetOwnerPermissions();
    void testGetOwningGroupPermissions();
    void testGetOthersPermissions();
    void testGetMaskPermissions();
    void testGetAllUserPermissions();
    void testGetAllGroupsPermissions();
    void testIsExtended();
    void testOperators();
    void testSettingBasic();
    void testSettingExtended();
    void testSettingErrorHandling();
    void testNewMask();

private:
    KACL m_acl;
    KACL m_acl2;

    QString m_testACL;
    QString m_testACL2;
    QString m_testACLEffective;
    int m_audioGid;
    int m_usersGid;
};

#endif
