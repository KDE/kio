/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2026 Meven Car <meven.car@collabora.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILEPREVIEWJOBTEST_H
#define FILEPREVIEWJOBTEST_H

#include <QObject>

class FilePreviewJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testTimeoutTimerStoppedOnFinish();
};

#endif
