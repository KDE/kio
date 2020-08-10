/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2010 Rolf Eike Beer <kde@opensource.sf-tec.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTPHEADERDISPOSITIONTEST_H
#define HTTPHEADERDISPOSITIONTEST_H

#include <QObject>

class HeaderDispositionTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void runAllTests();
    void runAllTests_data();
};

#endif //HTTPHEADERDISPOSITIONTEST_H
