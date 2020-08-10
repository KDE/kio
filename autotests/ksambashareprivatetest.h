/*
    SPDX-FileCopyrightText: 2018 Stefan Brüns <stefan.bruens@rwth-aachen.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef KSAMBASHAREPRIVATETEST_H
#define KSAMBASHAREPRIVATETEST_H

#include <QObject>

class KSambaSharePrivateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testParser();
    void testParser_data();
};

#endif // KSAMBASHAREPRIVATETEST_H

