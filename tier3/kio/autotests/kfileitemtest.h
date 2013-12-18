/* This file is part of the KDE project
   Copyright (C) 2006 David Faure <faure@kde.org>

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
#ifndef KFILEITEMTEST_H
#define KFILEITEMTEST_H

#include <QtCore/QObject>

class KFileItemTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testPermissionsString();
    void testNull();
    void testDoesNotExist();
    void testDetach();
    void testBasic();
    void testRootDirectory();
    void testHiddenFile();
    void testMimeTypeOnDemand();
    void testCmp();
    void testRename();
    void testDotDirectory();
    void testIsReadable_data();
    void testIsReadable();

    void testDecodeFileName_data();
    void testDecodeFileName();
    void testEncodeFileName_data();
    void testEncodeFileName();

    // KFileItemListProperties tests
    void testListProperties_data();
    void testListProperties();

    // KIO global tests
    void testIconNameForUrl_data();
    void testIconNameForUrl();
};


#endif
