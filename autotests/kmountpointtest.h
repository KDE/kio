/*
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef KMOUNTPOINTTEST_H
#define KMOUNTPOINTTEST_H

#include <QObject>

class KMountPointTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();

    void testCurrentMountPoints();
    void testPossibleMountPoints();

private:
};

#endif
