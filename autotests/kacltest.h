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

#ifndef KACLTEST_H
#define KACLTEST_H

#include <QtCore/QObject>
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
};

#endif
