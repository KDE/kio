/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_GLOBALTEST_H
#define KIO_GLOBALTEST_H

#include <QObject>

class GlobalTest : public QObject
{
    Q_OBJECT

public:
    GlobalTest() {}

private Q_SLOTS:
    void testUserPermissionConversion();
    void testGroupPermissionConversion();
    void testOtherPermissionConversion();
};

#endif
