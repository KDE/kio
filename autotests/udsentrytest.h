/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef UDSENTRYTEST_H
#define UDSENTRYTEST_H

#include <QObject>

class UDSEntryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSaveLoad();
    void testMove();
    void testEquality();
};

#endif
