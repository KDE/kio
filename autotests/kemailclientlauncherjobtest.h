/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KMAILCLIENTLAUNCHERJOBTEST_H
#define KMAILCLIENTLAUNCHERJOBTEST_H

#include <QObject>

class KEMailClientLauncherJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();

    void testEmpty();
    void testTo();
    void testManyFields();
    void testAttachments();
};

#endif
