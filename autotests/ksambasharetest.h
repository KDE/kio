/*
    SPDX-FileCopyrightText: 2018 Kai Uwe Broulik <kde@broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KSAMBASHARETEST_H
#define KSAMBASHARETEST_H

#include <QObject>

class KSambaShareTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testAcl();
    void testAcl_data();
    void testOwnAcl();
};

#endif // KSAMBASHARETEST_H

