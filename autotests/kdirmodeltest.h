/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KDIRMODELTEST_H
#define KDIRMODELTEST_H

#include <QObject>
#include <QTemporaryDir>
#include <kdirmodel.h>
#include <QEventLoop>
#include <QTest>
#include <QTestEventLoop>

// If you disable this, you need to change all exitLoop into quit in connect() statements...
#define USE_QTESTEVENTLOOP

class KDirModelTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void cleanup();
    void testRowCount();
    void testIndex();
    void testNames();
    void testItemForIndex();
    void testIndexForItem();
    void testData();
    void testReload();
    void testModifyFile();
    void testRenameFile();
    void testMoveDirectory();
    void testRenameDirectory();
    void testRenameDirectoryInCache();
    void testChmodDirectory();
    void testExpandToUrl_data();
    void testExpandToUrl();
    void testFilter();
    void testMimeFilter();
    void testShowHiddenFiles();
    void testMultipleSlashes();
    void testUrlWithRef();
    void testRemoteUrlWithHost();
    void testZipFile();
    void testBug196695();
    void testMimeData();
    void testDotHiddenFile_data();
    void testDotHiddenFile();
    void testShowRoot();
    void testShowRootWithTrailingSlash();
    void testShowRootAndExpandToUrl();
    void testHasChildren_data();
    void testHasChildren();

    // These tests must be done last
    void testDeleteFile();
    void testDeleteFileWhileListing();
    void testOverwriteFileWithDir();
    void testDeleteFiles();
    void testRenameFileToHidden();
    void testDeleteDirectory();
    void testDeleteCurrentDirectory();

    // Somewhat unrelated
    void testQUrlHash();

protected Q_SLOTS: // 'more private than private slots' - i.e. not seen by qtestlib
    void slotListingCompleted();
    void slotExpand(const QModelIndex &index);
    void slotRowsInserted(const QModelIndex &index, int, int);

private:
    void recreateTestData();
    void enterLoop();
    void fillModel(bool reload, bool expectAllIndexes = true);
    void collectKnownIndexes();
    void testMoveDirectory(const QString &srcdir);
    void testUpdateParentAfterExpand();

private:
#ifdef USE_QTESTEVENTLOOP
    QTestEventLoop m_eventLoop;
#else
    QEventLoop m_eventLoop;
#endif
    QTemporaryDir *m_tempDir;
    KDirModel *m_dirModel;
    QModelIndex m_fileIndex;
    QModelIndex m_specialFileIndex;
    QModelIndex m_secondFileIndex;
    QModelIndex m_dirIndex;
    QModelIndex m_fileInDirIndex;
    QModelIndex m_fileInSubdirIndex;
    QStringList m_topLevelFileNames; // files only

    // for slotExpand
    QStringList m_expectedExpandSignals;
    int m_nextExpectedExpandSignals; // index into m_expectedExpandSignals
    KDirModel *m_dirModelForExpand;
    QUrl m_urlToExpandTo;
    bool m_rowsInsertedEmitted;
    bool m_expectRowsInserted;
};

#endif
