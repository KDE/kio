/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2013 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEITEMACTIONSTEST_H
#define KFILEITEMACTIONSTEST_H

#include <QObject>

class KFileItemActionsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSetParentWidget();
};

#endif
