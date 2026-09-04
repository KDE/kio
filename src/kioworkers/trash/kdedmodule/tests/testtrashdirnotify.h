/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef TESTTRASHDIRNOTIFY_H
#define TESTTRASHDIRNOTIFY_H

#include <QObject>

class TrashDirNotify;

class TestTrashDirNotify : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testFileCreatedInPhysicalTrash();
    void testFileRemovedFromPhysicalTrash();

private:
    QString filesDir() const;

    TrashDirNotify *m_notifier = nullptr;
};

#endif
