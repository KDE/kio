/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef CLIPBOARDUPDATERTEST_H
#define CLIPBOARDUPDATERTEST_H

#include <QObject>

class ClipboardUpdaterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void testPasteAfterRenameFiles();
    void testPasteAfterMoveFile();
    void testPasteAfterMoveFiles();
    void testPasteAfterDeleteFile();
    void testPasteAfterDeleteFiles();
};

#endif
