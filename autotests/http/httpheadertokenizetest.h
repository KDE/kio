/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2008 Andreas Hartmetz <ahartmetz@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef HTTPHEADERTOKENIZETEST_H
#define HTTPHEADERTOKENIZETEST_H

#include <QObject>

class HeaderTokenizeTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMessyHeader();
    void testRedirectHeader();
};

#endif //HTTPHEADERTOKENIZETEST_H
