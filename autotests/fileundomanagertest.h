/*
    This file is part of KDE
    SPDX-FileCopyrightText: 2006, 2008 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef FILEUNDOMANAGERTEST_H
#define FILEUNDOMANAGERTEST_H

#include <QObject>
class TestUiInterface;

class FileUndoManagerTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testCopyFiles();
    void testMoveFiles();
    void testCopyDirectory();
    void testCopyEmptyDirectory();
    void testMoveDirectory();
    void testRenameFile();
    void testRenameDir();
    void testTrashFiles();
    void testRestoreTrashedFiles();
    void testModifyFileBeforeUndo(); // #20532
    void testCreateSymlink();
    void testCreateDir();
    void testMkpath();
    void testPasteClipboardUndo(); // #318757
    void testBatchRename();
    void testUndoCopyOfDeletedFile();

    void testErrorDuringMoveUndo();

    // TODO test renaming during a CopyJob.
    // Doesn't seem possible though, requires user interaction...

    // TODO: add test for undoing after a partial move (http://bugs.kde.org/show_bug.cgi?id=91579)
    // Difficult too.

private:
    void doUndo();
    TestUiInterface *m_uiInterface;
};

#endif
