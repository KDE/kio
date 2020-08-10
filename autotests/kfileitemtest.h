/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KFILEITEMTEST_H
#define KFILEITEMTEST_H

#include <QObject>

class KFileItemTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testPermissionsString();
    void testNull();
    void testDoesNotExist();
    void testDetach();
    void testMove();
    void testBasic();
    void testRootDirectory();
    void testHiddenFile();
    void testMimeTypeOnDemand();
    void testCmp();
    void testCmpAndInit();
    void testCmpByUrl();
    void testRename();
    void testRefresh();
    void testDotDirectory();
    void testMimetypeForRemoteFolder();
    void testMimetypeForRemoteFolderWithFileType();
    void testCurrentMimetypeForRemoteFolder();
    void testCurrentMimetypeForRemoteFolderWithFileType();
    void testIconNameForCustomFolderIcons();
    void testIconNameForStandardPath();

#ifndef Q_OS_WIN
    void testIsReadable_data();
    void testIsReadable();
#endif

    void testDecodeFileName_data();
    void testDecodeFileName();
    void testEncodeFileName_data();
    void testEncodeFileName();

    // KFileItemListProperties tests
    void testListProperties_data();
    void testListProperties();
#ifndef Q_OS_WIN
    void testNonWritableDirectory();
#endif

    // KIO global tests
    void testIconNameForUrl_data();
    void testIconNameForUrl();
};

#endif
