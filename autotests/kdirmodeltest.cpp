/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2006-2007 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirmodeltest.h"
#include <kdirnotify.h>
#include <kio/copyjob.h>
#include <kio/chmodjob.h>
#include "jobuidelegatefactory.h"
#include <kprotocolinfo.h>
#include <kdirlister.h>
#include <kio/deletejob.h>
#include <kio/job.h>
#include <KDirWatch>

#include <QTest>
#include <QMimeData>
#include <QSignalSpy>
#include <QDebug>
#include <QUrl>

#ifdef Q_OS_UNIX
#include <utime.h>
#endif
#include "kiotesthelper.h"
#include "mockcoredelegateextensions.h"

QTEST_MAIN(KDirModelTest)

#ifndef USE_QTESTEVENTLOOP
#define exitLoop quit
#endif

#ifndef Q_OS_WIN
#define SPECIALCHARS " special chars%:.pdf"
#else
#define SPECIALCHARS " special chars%.pdf"
#endif

Q_DECLARE_METATYPE(KFileItemList)

void KDirModelTest::initTestCase()
{
    qputenv("LC_ALL", "en_US.UTF-8");
    // To avoid a runtime dependency on klauncher
    qputenv("KDE_FORK_SLAVES", "yes");

    qRegisterMetaType<KFileItemList>("KFileItemList");

    m_dirModelForExpand = nullptr;
    m_dirModel = nullptr;
    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs(-30);   // 30 seconds ago
    m_tempDir = nullptr;
    m_topLevelFileNames << QStringLiteral("toplevelfile_1")
                        << QStringLiteral("toplevelfile_2")
                        << QStringLiteral("toplevelfile_3")
                        << SPECIALCHARS
                        ;
    recreateTestData();

    fillModel(false);
}

void KDirModelTest::recreateTestData()
{
    if (m_tempDir) {
        qDebug() << "Deleting old tempdir" << m_tempDir->path();
        delete m_tempDir;
        qApp->processEvents(); // process inotify events so they don't pollute us later on
    }

    m_tempDir = new QTemporaryDir;
    qDebug() << "new tmp dir:" << m_tempDir->path();
    // Create test data:
    /*
     * PATH/toplevelfile_1
     * PATH/toplevelfile_2
     * PATH/toplevelfile_3
     * PATH/special chars%:.pdf
     * PATH/.hiddenfile
     * PATH/.hiddenfile2
     * PATH/subdir
     * PATH/subdir/testfile
     * PATH/subdir/testsymlink
     * PATH/subdir/subsubdir
     * PATH/subdir/subsubdir/testfile
     * PATH/subdir/hasChildren
     * PATH/subdir/hasChildren/emptyDir
     * PATH/subdir/hasChildren/hiddenfileDir
     * PATH/subdir/hasChildren/hiddenfileDir/.hidden
     * PATH/subdir/hasChildren/hiddenDirDir
     * PATH/subdir/hasChildren/hiddenDirDir/.hidden
     * PATH/subdir/hasChildren/symlinkDir
     * PATH/subdir/hasChildren/symlinkDir/link
     * PATH/subdir/hasChildren/pipeDir
     * PATH/subdir/hasChildren/pipeDir/pipe
     */
    const QString path = m_tempDir->path() + '/';
    for (const QString &f : qAsConst(m_topLevelFileNames)) {
        createTestFile(path + f);
    }
    createTestFile(path + ".hiddenfile");
    createTestFile(path + ".hiddenfile2");
    createTestDirectory(path + "subdir");
    createTestDirectory(path + "subdir/subsubdir", NoSymlink);
    createTestDirectory(path + "subdir/hasChildren", Empty);
    createTestDirectory(path + "subdir/hasChildren/emptyDir", Empty);
    createTestDirectory(path + "subdir/hasChildren/hiddenfileDir", Empty);
    createTestFile(path + "subdir/hasChildren/hiddenfileDir/.hidden");
    createTestDirectory(path + "subdir/hasChildren/hiddenDirDir", Empty);
    createTestDirectory(path + "subdir/hasChildren/hiddenDirDir/.hidden", Empty);
    createTestDirectory(path + "subdir/hasChildren/symlinkDir", Empty);
    createTestSymlink(path + "subdir/hasChildren/symlinkDir/link", QString(path + "toplevelfile_1").toUtf8());
    createTestDirectory(path + "subdir/hasChildren/pipeDir", Empty);
    createTestPipe(path + "subdir/hasChildren/pipeDir/pipe");

    m_dirIndex = QModelIndex();
    m_fileIndex = QModelIndex();
    m_secondFileIndex = QModelIndex();
}

void KDirModelTest::cleanupTestCase()
{
    delete m_tempDir;
    m_tempDir = nullptr;
    delete m_dirModel;
    m_dirModel = nullptr;
    delete m_dirModelForExpand;
    m_dirModelForExpand = nullptr;
}

void KDirModelTest::fillModel(bool reload, bool expectAllIndexes)
{
    if (!m_dirModel) {
        m_dirModel = new KDirModel;
    }
    m_dirModel->dirLister()->setAutoErrorHandlingEnabled(false, nullptr);
    const QString path = m_tempDir->path() + '/';
    KDirLister *dirLister = m_dirModel->dirLister();
    qDebug() << "Calling openUrl";
    m_dirModel->openUrl(QUrl::fromLocalFile(path), reload ? KDirModel::Reload : KDirModel::NoFlags);
    connect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    qDebug() << "enterLoop, waiting for completed()";
    enterLoop();

    if (expectAllIndexes) {
        collectKnownIndexes();
    }
    disconnect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
}

// Called after test function
void KDirModelTest::cleanup()
{
    if (m_dirModel) {
        disconnect(m_dirModel, nullptr, &m_eventLoop, nullptr);
        disconnect(m_dirModel->dirLister(), nullptr, this, nullptr);
        m_dirModel->dirLister()->setNameFilter(QString());
        m_dirModel->dirLister()->setMimeFilter(QStringList());
        m_dirModel->dirLister()->emitChanges();
    }
}

void KDirModelTest::collectKnownIndexes()
{
    m_dirIndex = QModelIndex();
    m_fileIndex = QModelIndex();
    m_secondFileIndex = QModelIndex();
    // Create the indexes once and for all
    // The trouble is that the order of listing is undefined, one can get 1/2/3/subdir or subdir/3/2/1 for instance.
    for (int row = 0; row < m_topLevelFileNames.count() + 1 /*subdir*/; ++row) {
        QModelIndex idx = m_dirModel->index(row, 0, QModelIndex());
        QVERIFY(idx.isValid());
        KFileItem item = m_dirModel->itemForIndex(idx);
        qDebug() << item.url() << "isDir=" << item.isDir();
        QString fileName = item.url().fileName();
        if (item.isDir()) {
            m_dirIndex = idx;
        } else if (fileName == QLatin1String("toplevelfile_1")) {
            m_fileIndex = idx;
        } else if (fileName == QLatin1String("toplevelfile_2")) {
            m_secondFileIndex = idx;
        } else if (fileName.startsWith(QLatin1String(" special"))) {
            m_specialFileIndex = idx;
        }
    }
    QVERIFY(m_dirIndex.isValid());
    QVERIFY(m_fileIndex.isValid());
    QVERIFY(m_secondFileIndex.isValid());
    QVERIFY(m_specialFileIndex.isValid());

    // Now list subdir/
    QVERIFY(m_dirModel->canFetchMore(m_dirIndex));
    m_dirModel->fetchMore(m_dirIndex);
    qDebug() << "Listing subdir/";
    enterLoop();

    // Index of a file inside a directory (subdir/testfile)
    QModelIndex subdirIndex;
    m_fileInDirIndex = QModelIndex();
    for (int row = 0; row < 4; ++row) {
        QModelIndex idx = m_dirModel->index(row, 0, m_dirIndex);
        const KFileItem item = m_dirModel->itemForIndex(idx);
        if (item.isDir() && item.name() == QLatin1String("subsubdir")) {
            subdirIndex = idx;
        } else if (item.name() == QLatin1String("testfile")) {
            m_fileInDirIndex = idx;
        }
    }

    // List subdir/subsubdir
    QVERIFY(m_dirModel->canFetchMore(subdirIndex));
    qDebug() << "Listing subdir/subsubdir";
    m_dirModel->fetchMore(subdirIndex);
    enterLoop();

    // Index of ... well, subdir/subsubdir/testfile
    m_fileInSubdirIndex = m_dirModel->index(0, 0, subdirIndex);
}

void KDirModelTest::enterLoop()
{
#ifdef USE_QTESTEVENTLOOP
    m_eventLoop.enterLoop(10 /*seconds max*/);
    QVERIFY(!m_eventLoop.timeout());
#else
    m_eventLoop.exec();
#endif
}

void KDirModelTest::slotListingCompleted()
{
    qDebug();
#ifdef USE_QTESTEVENTLOOP
    m_eventLoop.exitLoop();
#else
    m_eventLoop.quit();
#endif
}

void KDirModelTest::testRowCount()
{
    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, m_topLevelFileNames.count() + 1 /*subdir*/);
    const int subdirRowCount = m_dirModel->rowCount(m_dirIndex);
    QCOMPARE(subdirRowCount, 4);

    QVERIFY(m_fileIndex.isValid());
    const int fileRowCount = m_dirModel->rowCount(m_fileIndex); // #176555
    QCOMPARE(fileRowCount, 0);
}

void KDirModelTest::testIndex()
{
    QVERIFY(m_dirModel->hasChildren());

    // Index of the first file
    QVERIFY(m_fileIndex.isValid());
    QCOMPARE(m_fileIndex.model(), static_cast<const QAbstractItemModel *>(m_dirModel));
    //QCOMPARE(m_fileIndex.row(), 0);
    QCOMPARE(m_fileIndex.column(), 0);
    QVERIFY(!m_fileIndex.parent().isValid());
    QVERIFY(!m_dirModel->hasChildren(m_fileIndex));

    // Index of a directory
    QVERIFY(m_dirIndex.isValid());
    QCOMPARE(m_dirIndex.model(), static_cast<const QAbstractItemModel *>(m_dirModel));
    //QCOMPARE(m_dirIndex.row(), 3); // ordering isn't guaranteed
    QCOMPARE(m_dirIndex.column(), 0);
    QVERIFY(!m_dirIndex.parent().isValid());
    QVERIFY(m_dirModel->hasChildren(m_dirIndex));

    // Index of a file inside a directory (subdir/testfile)
    QVERIFY(m_fileInDirIndex.isValid());
    QCOMPARE(m_fileInDirIndex.model(), static_cast<const QAbstractItemModel *>(m_dirModel));
    //QCOMPARE(m_fileInDirIndex.row(), 0); // ordering isn't guaranteed
    QCOMPARE(m_fileInDirIndex.column(), 0);
    QVERIFY(m_fileInDirIndex.parent() == m_dirIndex);
    QVERIFY(!m_dirModel->hasChildren(m_fileInDirIndex));

    // Index of subdir/subsubdir/testfile
    QVERIFY(m_fileInSubdirIndex.isValid());
    QCOMPARE(m_fileInSubdirIndex.model(), static_cast<const QAbstractItemModel *>(m_dirModel));
    QCOMPARE(m_fileInSubdirIndex.row(), 0); // we can check it because it's the only file there
    QCOMPARE(m_fileInSubdirIndex.column(), 0);
    QVERIFY(m_fileInSubdirIndex.parent().parent() == m_dirIndex);
    QVERIFY(!m_dirModel->hasChildren(m_fileInSubdirIndex));

    // Test sibling() by going from subdir/testfile to subdir/subsubdir
    const QModelIndex subsubdirIndex = m_fileInSubdirIndex.parent();
    QVERIFY(subsubdirIndex.isValid());
    QModelIndex sibling1 = m_dirModel->sibling(subsubdirIndex.row(), 0, m_fileInDirIndex);
    QVERIFY(sibling1.isValid());
    QVERIFY(sibling1 == subsubdirIndex);
    QVERIFY(m_dirModel->hasChildren(subsubdirIndex));

    // Invalid sibling call
    QVERIFY(!m_dirModel->sibling(2, 0, m_fileInSubdirIndex).isValid());

    // Test index() with a valid parent (dir).
    QModelIndex index2 = m_dirModel->index(m_fileInSubdirIndex.row(), m_fileInSubdirIndex.column(), subsubdirIndex);
    QVERIFY(index2.isValid());
    QVERIFY(index2 == m_fileInSubdirIndex);

    // Test index() with a non-parent (file).
    QModelIndex index3 = m_dirModel->index(m_fileInSubdirIndex.row(), m_fileInSubdirIndex.column(), m_fileIndex);
    QVERIFY(!index3.isValid());
}

void KDirModelTest::testNames()
{
    QString fileName = m_dirModel->data(m_fileIndex, Qt::DisplayRole).toString();
    QCOMPARE(fileName, QString("toplevelfile_1"));

    QString specialFileName = m_dirModel->data(m_specialFileIndex, Qt::DisplayRole).toString();
    QCOMPARE(specialFileName, QString(SPECIALCHARS));

    QString dirName = m_dirModel->data(m_dirIndex, Qt::DisplayRole).toString();
    QCOMPARE(dirName, QString("subdir"));

    QString fileInDirName = m_dirModel->data(m_fileInDirIndex, Qt::DisplayRole).toString();
    QCOMPARE(fileInDirName, QString("testfile"));

    QString fileInSubdirName = m_dirModel->data(m_fileInSubdirIndex, Qt::DisplayRole).toString();
    QCOMPARE(fileInSubdirName, QString("testfile"));
}

void KDirModelTest::testItemForIndex()
{
    // root item
    KFileItem rootItem = m_dirModel->itemForIndex(QModelIndex());
    QVERIFY(!rootItem.isNull());
    QCOMPARE(rootItem.name(), QString("."));

    KFileItem fileItem = m_dirModel->itemForIndex(m_fileIndex);
    QVERIFY(!fileItem.isNull());
    QCOMPARE(fileItem.name(), QString("toplevelfile_1"));
    QVERIFY(!fileItem.isDir());
    QCOMPARE(fileItem.url().toLocalFile(), QString(m_tempDir->path() + "/toplevelfile_1"));

    KFileItem dirItem = m_dirModel->itemForIndex(m_dirIndex);
    QVERIFY(!dirItem.isNull());
    QCOMPARE(dirItem.name(), QString("subdir"));
    QVERIFY(dirItem.isDir());
    QCOMPARE(dirItem.url().toLocalFile(), QString(m_tempDir->path() + "/subdir"));

    KFileItem fileInDirItem = m_dirModel->itemForIndex(m_fileInDirIndex);
    QVERIFY(!fileInDirItem.isNull());
    QCOMPARE(fileInDirItem.name(), QString("testfile"));
    QVERIFY(!fileInDirItem.isDir());
    QCOMPARE(fileInDirItem.url().toLocalFile(), QString(m_tempDir->path() + "/subdir/testfile"));

    KFileItem fileInSubdirItem = m_dirModel->itemForIndex(m_fileInSubdirIndex);
    QVERIFY(!fileInSubdirItem.isNull());
    QCOMPARE(fileInSubdirItem.name(), QString("testfile"));
    QVERIFY(!fileInSubdirItem.isDir());
    QCOMPARE(fileInSubdirItem.url().toLocalFile(), QString(m_tempDir->path() + "/subdir/subsubdir/testfile"));
}

void KDirModelTest::testIndexForItem()
{
    KFileItem rootItem = m_dirModel->itemForIndex(QModelIndex());
    QModelIndex rootIndex = m_dirModel->indexForItem(rootItem);
    QVERIFY(!rootIndex.isValid());

    KFileItem fileItem = m_dirModel->itemForIndex(m_fileIndex);
    QModelIndex fileIndex = m_dirModel->indexForItem(fileItem);
    QCOMPARE(fileIndex, m_fileIndex);

    KFileItem dirItem = m_dirModel->itemForIndex(m_dirIndex);
    QModelIndex dirIndex = m_dirModel->indexForItem(dirItem);
    QCOMPARE(dirIndex, m_dirIndex);

    KFileItem fileInDirItem = m_dirModel->itemForIndex(m_fileInDirIndex);
    QModelIndex fileInDirIndex = m_dirModel->indexForItem(fileInDirItem);
    QCOMPARE(fileInDirIndex, m_fileInDirIndex);

    KFileItem fileInSubdirItem = m_dirModel->itemForIndex(m_fileInSubdirIndex);
    QModelIndex fileInSubdirIndex = m_dirModel->indexForItem(fileInSubdirItem);
    QCOMPARE(fileInSubdirIndex, m_fileInSubdirIndex);
}

void KDirModelTest::testData()
{
    // First file
    QModelIndex idx1col0 = m_dirModel->index(m_fileIndex.row(), 0, QModelIndex());
    QCOMPARE(idx1col0.data().toString(), QString("toplevelfile_1"));
    QModelIndex idx1col1 = m_dirModel->index(m_fileIndex.row(), 1, QModelIndex());
    QString size1 = m_dirModel->data(idx1col1, Qt::DisplayRole).toString();
    QCOMPARE(size1, QString("11 B"));

    KFileItem item = m_dirModel->data(m_fileIndex, KDirModel::FileItemRole).value<KFileItem>();
    KFileItem fileItem = m_dirModel->itemForIndex(m_fileIndex);
    QCOMPARE(item, fileItem);

    QCOMPARE(m_dirModel->data(m_fileIndex, KDirModel::ChildCountRole).toInt(), (int)KDirModel::ChildCountUnknown);

    // Second file
    QModelIndex idx2col0 = m_dirModel->index(m_secondFileIndex.row(), 0, QModelIndex());
    QString display2 = m_dirModel->data(idx2col0, Qt::DisplayRole).toString();
    QCOMPARE(display2, QString("toplevelfile_2"));

    // Subdir: check child count
    QCOMPARE(m_dirModel->data(m_dirIndex, KDirModel::ChildCountRole).toInt(), 4);

    // Subsubdir: check child count
    QCOMPARE(m_dirModel->data(m_fileInSubdirIndex.parent(), KDirModel::ChildCountRole).toInt(), 1);
}

void KDirModelTest::testReload()
{
    fillModel(true);
    testItemForIndex();
}

// We want more info than just "the values differ", if they do.
#define COMPARE_INDEXES(a, b) \
    QCOMPARE(a.row(), b.row()); \
    QCOMPARE(a.column(), b.column()); \
    QCOMPARE(a.model(), b.model()); \
    QCOMPARE(a.parent().isValid(), b.parent().isValid()); \
    QCOMPARE(a, b);

void KDirModelTest::testModifyFile()
{
    const QString file = m_tempDir->path() + "/toplevelfile_2";

#if 1
    QSignalSpy spyDataChanged(m_dirModel, &QAbstractItemModel::dataChanged);
#else
    ModelSpy modelSpy(m_dirModel);
#endif
    connect(m_dirModel, &QAbstractItemModel::dataChanged,
            &m_eventLoop, &QTestEventLoop::exitLoop);

    // "Touch" the file
    setTimeStamp(file, s_referenceTimeStamp.addSecs(20));

    // In stat mode, kdirwatch doesn't notice file changes; we need to trigger it
    // by creating a file.
    //createTestFile(m_tempDir->path() + "/toplevelfile_5");
    KDirWatch::self()->setDirty(m_tempDir->path());

    // Wait for KDirWatch to notify the change (especially when using Stat)
    enterLoop();

    // If we come here, then dataChanged() was emitted - all good.
    const QVariantList dataChanged = spyDataChanged[0];
    QModelIndex receivedIndex = dataChanged[0].value<QModelIndex>();
    COMPARE_INDEXES(receivedIndex, m_secondFileIndex);
    receivedIndex = dataChanged[1].value<QModelIndex>();
    QCOMPARE(receivedIndex.row(), m_secondFileIndex.row()); // only compare row; column is count-1

    disconnect(m_dirModel, &QAbstractItemModel::dataChanged,
               &m_eventLoop, &QTestEventLoop::exitLoop);
}

void KDirModelTest::testRenameFile()
{
    const QUrl url = QUrl::fromLocalFile(m_tempDir->path() + "/toplevelfile_2");
    const QUrl newUrl = QUrl::fromLocalFile(m_tempDir->path() + "/toplevelfile_2_renamed");

    QSignalSpy spyDataChanged(m_dirModel, &QAbstractItemModel::dataChanged);
    connect(m_dirModel, &QAbstractItemModel::dataChanged,
            &m_eventLoop, &QTestEventLoop::exitLoop);

    KIO::SimpleJob *job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers dataChanged
    enterLoop();

    // If we come here, then dataChanged() was emitted - all good.
    QCOMPARE(spyDataChanged.count(), 1);
    COMPARE_INDEXES(spyDataChanged[0][0].value<QModelIndex>(), m_secondFileIndex);
    QModelIndex receivedIndex = spyDataChanged[0][1].value<QModelIndex>();
    QCOMPARE(receivedIndex.row(), m_secondFileIndex.row()); // only compare row; column is count-1

    // check renaming happened
    QCOMPARE(m_dirModel->itemForIndex(m_secondFileIndex).url().toString(), newUrl.toString());

    // check that KDirLister::cachedItemForUrl won't give a bad name if copying that item (#195385)
    KFileItem cachedItem = KDirLister::cachedItemForUrl(newUrl);
    QVERIFY(!cachedItem.isNull());
    QCOMPARE(cachedItem.name(), QString("toplevelfile_2_renamed"));
    QCOMPARE(cachedItem.entry().stringValue(KIO::UDSEntry::UDS_NAME), QString("toplevelfile_2_renamed"));

    // Put things back to normal
    job = KIO::rename(newUrl, url, KIO::HideProgressInfo);
    QVERIFY(job->exec());
    // Wait for the DBUS signal from KDirNotify, it's the one the triggers dataChanged
    enterLoop();
    QCOMPARE(m_dirModel->itemForIndex(m_secondFileIndex).url().toString(), url.toString());

    disconnect(m_dirModel, &QAbstractItemModel::dataChanged,
               &m_eventLoop, &QTestEventLoop::exitLoop);
}

void KDirModelTest::testMoveDirectory()
{
    testMoveDirectory(QStringLiteral("subdir"));
}

void KDirModelTest::testMoveDirectory(const QString &dir /*just a dir name, no slash*/)
{
    const QString path = m_tempDir->path() + '/';
    const QString srcdir = path + dir;
    QVERIFY(QDir(srcdir).exists());
    QTemporaryDir destDir;
    const QString dest = destDir.path() + '/';
    QVERIFY(QDir(dest).exists());

    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    // Move
    qDebug() << "Moving" << srcdir << "to" << dest;
    KIO::CopyJob *job = KIO::move(QUrl::fromLocalFile(srcdir), QUrl::fromLocalFile(dest), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->exec());

    // wait for kdirnotify
    enterLoop();

    disconnect(m_dirModel, &QAbstractItemModel::rowsRemoved,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir")).isValid());
    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed")).isValid());

    connect(m_dirModel, &QAbstractItemModel::rowsInserted,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    // Move back
    qDebug() << "Moving" << dest + dir << "back to" << srcdir;
    job = KIO::move(QUrl::fromLocalFile(dest + dir), QUrl::fromLocalFile(srcdir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    job->setUiDelegateExtension(nullptr);
    QVERIFY(job->exec());

    enterLoop();

    QVERIFY(QDir(srcdir).exists());
    disconnect(m_dirModel, &QAbstractItemModel::rowsInserted,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    // m_dirIndex is invalid after the above...
    fillModel(true);
}

void KDirModelTest::testRenameDirectory() // #172945, #174703, (and #180156)
{
    const QString path = m_tempDir->path() + '/';
    const QUrl url = QUrl::fromLocalFile(path + "subdir");
    const QUrl newUrl = QUrl::fromLocalFile(path + "subdir_renamed");

    // For #180156 we need a second kdirmodel, viewing the subdir being renamed.
    // I'm abusing m_dirModelForExpand for that purpose.
    delete m_dirModelForExpand;
    m_dirModelForExpand = new KDirModel;
    KDirLister *dirListerForExpand = m_dirModelForExpand->dirLister();
    connect(dirListerForExpand, QOverload<>::of(&KDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    dirListerForExpand->openUrl(url); // async
    enterLoop();

    // Now do the renaming
    QSignalSpy spyDataChanged(m_dirModel, &QAbstractItemModel::dataChanged);
    connect(m_dirModel, &QAbstractItemModel::dataChanged,
                   &m_eventLoop, &QTestEventLoop::exitLoop);
    KIO::SimpleJob *job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers dataChanged
    enterLoop();

    // If we come here, then dataChanged() was emitted - all good.
    //QCOMPARE(spyDataChanged.count(), 1); // it was in fact emitted 5 times...
    //COMPARE_INDEXES(spyDataChanged[0][0].value<QModelIndex>(), m_dirIndex);
    //QModelIndex receivedIndex = spyDataChanged[0][1].value<QModelIndex>();
    //QCOMPARE(receivedIndex.row(), m_dirIndex.row()); // only compare row; column is count-1

    // check renaming happened
    QCOMPARE(m_dirModel->itemForIndex(m_dirIndex).url().toString(), newUrl.toString());
    qDebug() << newUrl << "indexForUrl=" << m_dirModel->indexForUrl(newUrl) << "m_dirIndex=" << m_dirIndex;
    QCOMPARE(m_dirModel->indexForUrl(newUrl), m_dirIndex);
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/testfile")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/subsubdir")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/subsubdir/testfile")).isValid());

    // Check the other kdirmodel got redirected
    QCOMPARE(dirListerForExpand->url().toLocalFile(), QString(path + "subdir_renamed"));

    qDebug() << "calling testMoveDirectory(subdir_renamed)";

    // Test moving the renamed directory; if something inside KDirModel
    // wasn't properly updated by the renaming, this would detect it and crash (#180673)
    testMoveDirectory(QStringLiteral("subdir_renamed"));

    // Put things back to normal
    job = KIO::rename(newUrl, url, KIO::HideProgressInfo);
    QVERIFY(job->exec());
    // Wait for the DBUS signal from KDirNotify, it's the one the triggers dataChanged
    enterLoop();
    QCOMPARE(m_dirModel->itemForIndex(m_dirIndex).url().toString(), url.toString());

    disconnect(m_dirModel, &QAbstractItemModel::dataChanged,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    QCOMPARE(m_dirModel->itemForIndex(m_dirIndex).url().toString(), url.toString());
    QCOMPARE(m_dirModel->indexForUrl(url), m_dirIndex);
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir/testfile")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir/subsubdir")).isValid());
    QVERIFY(m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir/subsubdir/testfile")).isValid());
    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed")).isValid());
    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/testfile")).isValid());
    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/subsubdir")).isValid());
    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir_renamed/subsubdir/testfile")).isValid());

    // TODO INVESTIGATE
    // QCOMPARE(dirListerForExpand->url().toLocalFile(), path+"subdir");

    delete m_dirModelForExpand;
    m_dirModelForExpand = nullptr;
}

void KDirModelTest::testRenameDirectoryInCache() // #188807
{
    // Ensure the stuff is in cache.
    fillModel(true);
    const QString path = m_tempDir->path() + '/';
    QVERIFY(!m_dirModel->dirLister()->findByUrl(QUrl::fromLocalFile(path)).isNull());

    // No more dirmodel nor dirlister.
    delete m_dirModel;
    m_dirModel = nullptr;

    // Now let's rename a directory that is in KCoreDirListerCache
    const QUrl url = QUrl::fromLocalFile(path);
    QUrl newUrl = url.adjusted(QUrl::StripTrailingSlash);
    newUrl.setPath(newUrl.path() + "_renamed");
    qDebug() << newUrl;
    KIO::SimpleJob *job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Put things back to normal
    job = KIO::rename(newUrl, url, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // KDirNotify emits FileRenamed for each rename() above, which in turn
    // re-lists the directory. We need to wait for both signals to be emitted
    // otherwise the dirlister will not be in the state we expect.
    QTest::qWait(200);

    fillModel(true);

    QVERIFY(m_dirIndex.isValid());
    KFileItem rootItem = m_dirModel->dirLister()->findByUrl(QUrl::fromLocalFile(path));
    QVERIFY(!rootItem.isNull());
}

void KDirModelTest::testChmodDirectory() // #53397
{
    QSignalSpy spyDataChanged(m_dirModel, &QAbstractItemModel::dataChanged);
    connect(m_dirModel, &QAbstractItemModel::dataChanged,
                   &m_eventLoop, &QTestEventLoop::exitLoop);
    const QString path = m_tempDir->path() + '/';
    KFileItem rootItem = m_dirModel->itemForIndex(QModelIndex());
    const mode_t origPerm = rootItem.permissions();
    mode_t newPerm = origPerm ^ S_IWGRP;
    //const QFile::Permissions origPerm = rootItem.filePermissions();
    //QVERIFY(origPerm & QFile::ReadOwner);
    //const QFile::Permissions newPerm = origPerm ^ QFile::WriteGroup;
    QVERIFY(newPerm != origPerm);
    KFileItemList items; items << rootItem;
    KIO::Job *job = KIO::chmod(items, newPerm, S_IWGRP /*TODO: QFile::WriteGroup*/, QString(), QString(), false, KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY(job->exec());
    // ChmodJob doesn't talk to KDirNotify, kpropertiesdialog does.
    // [this allows to group notifications after all the changes one can make in the dialog]
    org::kde::KDirNotify::emitFilesChanged(QList<QUrl>{QUrl::fromLocalFile(path)});
    // Wait for the DBUS signal from KDirNotify, it's the one the triggers rowsRemoved
    enterLoop();

    // If we come here, then dataChanged() was emitted - all good.
    QCOMPARE(spyDataChanged.count(), 1);
    QModelIndex receivedIndex = spyDataChanged[0][0].value<QModelIndex>();
    qDebug() << receivedIndex;
    QVERIFY(!receivedIndex.isValid());

    const KFileItem newRootItem = m_dirModel->itemForIndex(QModelIndex());
    QVERIFY(!newRootItem.isNull());
    QCOMPARE(QString::number(newRootItem.permissions(), 16), QString::number(newPerm, 16));

    disconnect(m_dirModel, &QAbstractItemModel::dataChanged,
               &m_eventLoop, &QTestEventLoop::exitLoop);
}

enum {
    NoFlag = 0,
    NewDir = 1, // whether to re-create a new QTemporaryDir completely, to avoid cached fileitems
    ListFinalDir = 2, // whether to list the target dir at the same time, like k3b, for #193364
    Recreate = 4,
    CacheSubdir = 8, // put subdir in the cache before expandToUrl
    // flags, next item is 16!
};

void KDirModelTest::testExpandToUrl_data()
{
    QTest::addColumn<int>("flags"); // see enum above
    QTest::addColumn<QString>("expandToPath"); // relative path
    QTest::addColumn<QStringList>("expectedExpandSignals");

    QTest::newRow("the root, nothing to do")
            << int(NoFlag) << QString() << QStringList();
    QTest::newRow(".")
            << int(NoFlag) << "." << (QStringList());
    QTest::newRow("subdir") << int(NoFlag) << "subdir" << QStringList{QStringLiteral("subdir")};
    QTest::newRow("subdir/.") << int(NoFlag) << "subdir/." << QStringList{QStringLiteral("subdir")};

    const QString subsubdir = QStringLiteral("subdir/subsubdir");
    // Must list root, emit expand for subdir, list subdir, emit expand for subsubdir.
    QTest::newRow("subdir/subsubdir")
            << int(NoFlag) << subsubdir << QStringList{QStringLiteral("subdir"), subsubdir};

    // Must list root, emit expand for subdir, list subdir, emit expand for subsubdir, list subsubdir.
    const QString subsubdirfile = subsubdir + "/testfile";
    QTest::newRow("subdir/subsubdir/testfile sync")
            << int(NoFlag) << subsubdirfile << QStringList{QStringLiteral("subdir"), subsubdir, subsubdirfile};

#ifndef Q_OS_WIN
    // Expand a symlink to a directory (#219547)
    const QString dirlink = m_tempDir->path() + "/dirlink";
    createTestSymlink(dirlink, "subdir"); // dirlink -> subdir
    QVERIFY(QFileInfo(dirlink).isSymLink());
    // If this test fails, your first move should be to enable all debug output and see if KDirWatch says inotify failed
    QTest::newRow("dirlink")
            << int(NoFlag) << "dirlink/subsubdir" << QStringList{QStringLiteral("dirlink"), QStringLiteral("dirlink/subsubdir")};
#endif

    // Do a cold-cache test too, but nowadays it doesn't change anything anymore,
    // apart from testing different code paths inside KDirLister.
    QTest::newRow("subdir/subsubdir/testfile with reload")
            << int(NewDir) << subsubdirfile << QStringList{QStringLiteral("subdir"), subsubdir, subsubdirfile};

    QTest::newRow("hold dest dir") // #193364
            << int(NewDir | ListFinalDir) << subsubdirfile << QStringList{QStringLiteral("subdir"), subsubdir, subsubdirfile};

    // Put subdir in cache too (#175035)
    QTest::newRow("hold subdir and dest dir")
            << int(NewDir | CacheSubdir | ListFinalDir | Recreate) << subsubdirfile
            << QStringList{QStringLiteral("subdir"), subsubdir, subsubdirfile};

    // Make sure the last test has the Recreate option set, for the subsequent test methods.
}

void KDirModelTest::testExpandToUrl()
{
    QFETCH(int, flags);
    QFETCH(QString, expandToPath); // relative
    QFETCH(QStringList, expectedExpandSignals);

    if (flags & NewDir) {
        recreateTestData();
        // WARNING! m_dirIndex, m_fileIndex, m_secondFileIndex etc. are not valid anymore after this point!

    }

    const QString path = m_tempDir->path() + '/';
    if (flags & CacheSubdir) {
        // This way, the listDir for subdir will find items in cache, and will schedule a CachedItemsJob
        m_dirModel->dirLister()->openUrl(QUrl::fromLocalFile(path + "subdir"));
        QSignalSpy completedSpy(m_dirModel->dirLister(), QOverload<>::of(&KCoreDirLister::completed));
        QVERIFY(completedSpy.wait(2000));
    }
    if (flags & ListFinalDir) {
        // This way, the last listDir will find items in cache, and will schedule a CachedItemsJob
        m_dirModel->dirLister()->openUrl(QUrl::fromLocalFile(path + "subdir/subsubdir"));
        QSignalSpy completedSpy(m_dirModel->dirLister(), QOverload<>::of(&KCoreDirLister::completed));
        QVERIFY(completedSpy.wait(2000));
    }

    if (!m_dirModelForExpand || (flags & NewDir)) {
        delete m_dirModelForExpand;
        m_dirModelForExpand = new KDirModel;
        connect(m_dirModelForExpand, &KDirModel::expand,
                       this, &KDirModelTest::slotExpand);
        connect(m_dirModelForExpand, &QAbstractItemModel::rowsInserted,
                       this, &KDirModelTest::slotRowsInserted);
        KDirLister *dirListerForExpand = m_dirModelForExpand->dirLister();
        dirListerForExpand->openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags); // async
    }
    m_rowsInsertedEmitted = false;
    m_expectedExpandSignals = expectedExpandSignals;
    m_nextExpectedExpandSignals = 0;
    QSignalSpy spyExpand(m_dirModelForExpand, &KDirModel::expand);
    m_urlToExpandTo = QUrl::fromLocalFile(path + expandToPath);
    // If KDirModel doesn't know this URL yet, then we want to see rowsInserted signals
    // being emitted, so that the slots can get the index to that url then.
    m_expectRowsInserted = !expandToPath.isEmpty() && !m_dirModelForExpand->indexForUrl(m_urlToExpandTo).isValid();
    QVERIFY(QFileInfo::exists(m_urlToExpandTo.toLocalFile()));
    m_dirModelForExpand->expandToUrl(m_urlToExpandTo);
    if (expectedExpandSignals.isEmpty()) {
        QTest::qWait(20); // to make sure we process queued connection calls, otherwise spyExpand.count() is always 0 even if there's a bug...
        QCOMPARE(spyExpand.count(), 0);
    } else {
        if (spyExpand.count() < expectedExpandSignals.count()) {
            enterLoop();
            QCOMPARE(spyExpand.count(), expectedExpandSignals.count());
        }
        if (m_expectRowsInserted) {
            QVERIFY(m_rowsInsertedEmitted);
        }
    }

    // Now it should exist
    if (!expandToPath.isEmpty() && expandToPath != QLatin1String(".")) {
        qDebug() << "Do I know" << m_urlToExpandTo << "?";
        QVERIFY(m_dirModelForExpand->indexForUrl(m_urlToExpandTo).isValid());
    }

    if (flags & ListFinalDir) {
        testUpdateParentAfterExpand();
    }

    if (flags & Recreate) {
        // Clean up, for the next tests
        recreateTestData();
        fillModel(false);
    }
}

void KDirModelTest::slotExpand(const QModelIndex &index)
{
    QVERIFY(index.isValid());
    const QString path = m_tempDir->path() + '/';
    KFileItem item = m_dirModelForExpand->itemForIndex(index);
    QVERIFY(!item.isNull());
    qDebug() << item.url().toLocalFile();
    QCOMPARE(item.url().toLocalFile(), QString(path + m_expectedExpandSignals[m_nextExpectedExpandSignals++]));

    // if rowsInserted wasn't emitted yet, then any proxy model would be unable to do anything with index at this point
    if (item.url() == m_urlToExpandTo) {
        QVERIFY(m_dirModelForExpand->indexForUrl(m_urlToExpandTo).isValid());
        if (m_expectRowsInserted) {
            QVERIFY(m_rowsInsertedEmitted);
        }
    }

    if (m_nextExpectedExpandSignals == m_expectedExpandSignals.count()) {
        m_eventLoop.exitLoop();    // done
    }
}

void KDirModelTest::slotRowsInserted(const QModelIndex &, int, int)
{
    m_rowsInsertedEmitted = true;
}

// This code is called by testExpandToUrl
void KDirModelTest::testUpdateParentAfterExpand() // #193364
{
    const QString path = m_tempDir->path() + '/';
    const QString file = path + "subdir/aNewFile";
    qDebug() << "Creating" << file;
    QVERIFY(!QFile::exists(file));
    createTestFile(file);
    QSignalSpy spyRowsInserted(m_dirModelForExpand, &QAbstractItemModel::rowsInserted);
    QVERIFY(spyRowsInserted.wait(1000));
}

void KDirModelTest::testFilter()
{
    QVERIFY(m_dirIndex.isValid());
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const int oldSubdirRowCount = m_dirModel->rowCount(m_dirIndex);
    QSignalSpy spyItemsFilteredByMime(m_dirModel->dirLister(), &KCoreDirLister::itemsFilteredByMime);
    QSignalSpy spyItemsDeleted(m_dirModel->dirLister(), &KCoreDirLister::itemsDeleted);
    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    m_dirModel->dirLister()->setNameFilter(QStringLiteral("toplevel*"));
    QCOMPARE(m_dirModel->rowCount(), oldTopLevelRowCount); // no change yet
    QCOMPARE(m_dirModel->rowCount(m_dirIndex), oldSubdirRowCount); // no change yet
    m_dirModel->dirLister()->emitChanges();

    QCOMPARE(m_dirModel->rowCount(), 4); // 3 toplevel* files, one subdir
    QCOMPARE(m_dirModel->rowCount(m_dirIndex), 2); // the files get filtered out, subsubdir and hasChildren are remaining

    // In the subdir, we can get rowsRemoved signals like (1,2) or (0,0)+(2,2),
    // depending on the order of the files in the model.
    // So QCOMPARE(spyRowsRemoved.count(), 3) is fragile, we rather need
    // to sum up the removed rows per parent directory.
    QMap<QString, int> rowsRemovedPerDir;
    for (int i = 0; i < spyRowsRemoved.count(); ++i) {
        const QVariantList args = spyRowsRemoved[i];
        const QModelIndex parentIdx = args[0].value<QModelIndex>();
        QString dirName;
        if (parentIdx.isValid()) {
            const KFileItem item = m_dirModel->itemForIndex(parentIdx);
            dirName = item.name();
        } else {
            dirName = QStringLiteral("root");
        }
        rowsRemovedPerDir[dirName] += args[2].toInt() - args[1].toInt() + 1;
        //qDebug() << parentIdx << args[1].toInt() << args[2].toInt();
    }
    QCOMPARE(rowsRemovedPerDir.count(), 3); // once for every dir
    QCOMPARE(rowsRemovedPerDir.value("root"), 1);      // one from toplevel ('special chars')
    QCOMPARE(rowsRemovedPerDir.value("subdir"), 2);    // two from subdir
    QCOMPARE(rowsRemovedPerDir.value("subsubdir"), 1); // one from subsubdir
    QCOMPARE(spyItemsDeleted.count(), 3); // once for every dir
    QCOMPARE(spyItemsDeleted[0][0].value<KFileItemList>().count(), 1); // one from toplevel ('special chars')
    QCOMPARE(spyItemsDeleted[1][0].value<KFileItemList>().count(), 2); // two from subdir
    QCOMPARE(spyItemsDeleted[2][0].value<KFileItemList>().count(), 1); // one from subsubdir
    QCOMPARE(spyItemsFilteredByMime.count(), 0);
    spyItemsDeleted.clear();
    spyItemsFilteredByMime.clear();

    // Reset the filter
    qDebug() << "reset to no filter";
    m_dirModel->dirLister()->setNameFilter(QString());
    m_dirModel->dirLister()->emitChanges();

    QCOMPARE(m_dirModel->rowCount(), oldTopLevelRowCount);
    QCOMPARE(m_dirModel->rowCount(m_dirIndex), oldSubdirRowCount);
    QCOMPARE(spyItemsDeleted.count(), 0);
    QCOMPARE(spyItemsFilteredByMime.count(), 0);

    // The order of things changed because of filtering.
    // Fill again, so that m_fileIndex etc. are correct again.
    fillModel(true);
}

void KDirModelTest::testMimeFilter()
{
    QVERIFY(m_dirIndex.isValid());
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const int oldSubdirRowCount = m_dirModel->rowCount(m_dirIndex);
    QSignalSpy spyItemsFilteredByMime(m_dirModel->dirLister(), &KCoreDirLister::itemsFilteredByMime);
    QSignalSpy spyItemsDeleted(m_dirModel->dirLister(), &KCoreDirLister::itemsDeleted);
    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    m_dirModel->dirLister()->setMimeFilter(QStringList{QStringLiteral("application/pdf")});
    QCOMPARE(m_dirModel->rowCount(), oldTopLevelRowCount); // no change yet
    QCOMPARE(m_dirModel->rowCount(m_dirIndex), oldSubdirRowCount); // no change yet
    m_dirModel->dirLister()->emitChanges();

    QCOMPARE(m_dirModel->rowCount(), 1); // 1 pdf files, no subdir anymore

    QVERIFY(spyRowsRemoved.count() >= 1); // depends on contiguity...
    QVERIFY(spyItemsDeleted.count() >= 1); // once for every dir
    // Maybe it would make sense to have those items in itemsFilteredByMime,
    // but well, for the only existing use of that signal (MIME type filter plugin),
    // it's not really necessary, the plugin has seen those files before anyway.
    // The signal is mostly useful for the case of listing a dir with a MIME type filter set.
    //QCOMPARE(spyItemsFilteredByMime.count(), 1);
    //QCOMPARE(spyItemsFilteredByMime[0][0].value<KFileItemList>().count(), 4);
    spyItemsDeleted.clear();
    spyItemsFilteredByMime.clear();

    // Reset the filter
    qDebug() << "reset to no filter";
    m_dirModel->dirLister()->setMimeFilter(QStringList());
    m_dirModel->dirLister()->emitChanges();

    QCOMPARE(m_dirModel->rowCount(), oldTopLevelRowCount);
    QCOMPARE(spyItemsDeleted.count(), 0);
    QCOMPARE(spyItemsFilteredByMime.count(), 0);

    // The order of things changed because of filtering.
    // Fill again, so that m_fileIndex etc. are correct again.
    fillModel(true);
}

void KDirModelTest::testShowHiddenFiles() // #174788
{
    KDirLister *dirLister = m_dirModel->dirLister();

    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    QSignalSpy spyNewItems(dirLister, &KCoreDirLister::newItems);
    QSignalSpy spyRowsInserted(m_dirModel, &QAbstractItemModel::rowsInserted);
    dirLister->setShowingDotFiles(true);
    dirLister->emitChanges();
    const int numberOfDotFiles = 2;
    QCOMPARE(spyNewItems.count(), 1);
    QCOMPARE(spyNewItems[0][0].value<KFileItemList>().count(), numberOfDotFiles);
    QCOMPARE(spyRowsInserted.count(), 1);
    QCOMPARE(spyRowsRemoved.count(), 0);
    spyNewItems.clear();
    spyRowsInserted.clear();

    dirLister->setShowingDotFiles(false);
    dirLister->emitChanges();
    QCOMPARE(spyNewItems.count(), 0);
    QCOMPARE(spyRowsInserted.count(), 0);
    QCOMPARE(spyRowsRemoved.count(), 1);
}

void KDirModelTest::testMultipleSlashes()
{
    const QString path = m_tempDir->path() + '/';

    QModelIndex index = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir//testfile"));
    QVERIFY(index.isValid());

    index = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir//subsubdir//"));
    QVERIFY(index.isValid());

    index = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir///subsubdir////testfile"));
    QVERIFY(index.isValid());
}

void KDirModelTest::testUrlWithRef() // #171117
{
    const QString path = m_tempDir->path() + '/';
    KDirLister *dirLister = m_dirModel->dirLister();
    QUrl url = QUrl::fromLocalFile(path);
    url.setFragment(QStringLiteral("ref"));
    QVERIFY(url.url().endsWith(QLatin1String("#ref")));
    dirLister->openUrl(url, KDirLister::NoFlags);
    connect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    enterLoop();

    QCOMPARE(dirLister->url().toString(), url.toString(QUrl::StripTrailingSlash));
    collectKnownIndexes();
    disconnect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
}

//void KDirModelTest::testFontUrlWithHost() // #160057 --> moved to kio_fonts (kfontinst/kio/autotests)

void KDirModelTest::testRemoteUrlWithHost() // #178416
{
    if (!KProtocolInfo::isKnownProtocol(QStringLiteral("remote"))) {
        QSKIP("kio_remote not installed");
    }
    QUrl url(QStringLiteral("remote://foo"));
    KDirLister *dirLister = m_dirModel->dirLister();
    dirLister->openUrl(url, KDirLister::NoFlags);
    connect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    enterLoop();

    QCOMPARE(dirLister->url().toString(), QString("remote://foo"));
}

void KDirModelTest::testZipFile() // # 171721
{
    const QString path = QFileInfo(QFINDTESTDATA("wronglocalsizes.zip")).absolutePath();
    KDirLister *dirLister = m_dirModel->dirLister();
    dirLister->openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    connect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    enterLoop();
    disconnect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);

    QUrl zipUrl(QUrl::fromLocalFile(path));
    zipUrl.setPath(zipUrl.path() + "/wronglocalsizes.zip"); // just a zip file lying here for other reasons

    QVERIFY(QFile::exists(zipUrl.toLocalFile()));
    zipUrl.setScheme(QStringLiteral("zip"));
    QModelIndex index = m_dirModel->indexForUrl(zipUrl);
    QVERIFY(!index.isValid()); // protocol mismatch, can't find it!
    zipUrl.setScheme(QStringLiteral("file"));
    index = m_dirModel->indexForUrl(zipUrl);
    QVERIFY(index.isValid());
}

class MyDirLister : public KDirLister
{
public:
    void emitItemsDeleted(const KFileItemList &items)
    {
        Q_EMIT itemsDeleted(items);
    }
};

void KDirModelTest::testBug196695()
{
    KFileItem rootItem(QUrl::fromLocalFile(m_tempDir->path()), QString(), KFileItem::Unknown);
    KFileItem childItem(QUrl::fromLocalFile(QString(m_tempDir->path() + "/toplevelfile_1")), QString(), KFileItem::Unknown);

    KFileItemList list;
    // Important: the root item must not be first in the list to trigger bug 196695
    list << childItem << rootItem;

    MyDirLister *dirLister = static_cast<MyDirLister *>(m_dirModel->dirLister());
    dirLister->emitItemsDeleted(list);

    fillModel(true);
}

void KDirModelTest::testMimeData()
{
    QModelIndex index0 = m_dirModel->index(0, 0);
    QVERIFY(index0.isValid());
    QModelIndex index1 = m_dirModel->index(1, 0);
    QVERIFY(index1.isValid());
    QList<QModelIndex> indexes;
    indexes << index0 << index1;
    QMimeData *mimeData = m_dirModel->mimeData(indexes);
    QVERIFY(mimeData);
    QVERIFY(mimeData->hasUrls());
    const QList<QUrl> urls = mimeData->urls();
    QCOMPARE(urls.count(), indexes.count());
    delete mimeData;
}

void KDirModelTest::testDotHiddenFile_data()
{
    QTest::addColumn<QStringList>("fileContents");
    QTest::addColumn<QStringList>("expectedListing");

    const QStringList allItems{QStringLiteral("toplevelfile_1"), QStringLiteral("toplevelfile_2"),
                               QStringLiteral("toplevelfile_3"), SPECIALCHARS, QStringLiteral("subdir")};
    QTest::newRow("empty_file") << (QStringList{}) << allItems;

    QTest::newRow("simple_name") << (QStringList{QStringLiteral("toplevelfile_1")}) << QStringList(allItems.mid(1));

    QStringList allButSpecialChars = allItems; allButSpecialChars.removeAt(3);
    QTest::newRow("special_chars") << (QStringList{SPECIALCHARS}) << allButSpecialChars;

    QStringList allButSubdir = allItems; allButSubdir.removeAt(4);
    QTest::newRow("subdir") << (QStringList{QStringLiteral("subdir")}) << allButSubdir;

    QTest::newRow("many_lines") << (QStringList{QStringLiteral("subdir"), QStringLiteral("toplevelfile_1"),
                                                QStringLiteral("toplevelfile_3"), QStringLiteral("toplevelfile_2")})
                                << QStringList{SPECIALCHARS};
}

void KDirModelTest::testDotHiddenFile()
{
    QFETCH(QStringList, fileContents);
    QFETCH(QStringList, expectedListing);

    const QString path = m_tempDir->path() + '/';
    const QString dotHiddenFile = path + ".hidden";
    QTest::qWait(1000); // mtime-based cache, so we need to wait for 1 second
    QFile dh(dotHiddenFile);
    QVERIFY(dh.open(QIODevice::WriteOnly));
    dh.write(fileContents.join('\n').toUtf8());
    dh.close();

    // Do it twice: once to read from the file and once to use the cache
    for (int i = 0; i < 2; ++i) {
        fillModel(true, false);
        QStringList files;
        for (int row = 0; row < m_dirModel->rowCount(); ++row) {
            files.append(m_dirModel->index(row, KDirModel::Name).data().toString());
        }
        files.sort();
        expectedListing.sort();
        QCOMPARE(files, expectedListing);
    }

    dh.remove();
}

void KDirModelTest::testShowRoot()
{
    KDirModel dirModel;
    const QUrl homeUrl = QUrl::fromLocalFile(QDir::homePath());
    const QUrl fsRootUrl = QUrl(QStringLiteral("file:///"));

    // openUrl("/", ShowRoot) should create a "/" item
    dirModel.openUrl(fsRootUrl, KDirModel::ShowRoot);
    QTRY_COMPARE(dirModel.rowCount(), 1);
    const QModelIndex rootIndex = dirModel.index(0, 0);
    QVERIFY(rootIndex.isValid());
    QCOMPARE(rootIndex.data().toString(), QStringLiteral("/"));
    QVERIFY(!dirModel.parent(rootIndex).isValid());
    QCOMPARE(dirModel.itemForIndex(rootIndex).url(), QUrl(QStringLiteral("file:///")));
    QCOMPARE(dirModel.itemForIndex(rootIndex).name(), QStringLiteral("/"));

    // expandToUrl should work
    dirModel.expandToUrl(homeUrl);
    QTRY_VERIFY(dirModel.indexForUrl(homeUrl).isValid());

    // test itemForIndex and indexForUrl
    QCOMPARE(dirModel.itemForIndex(QModelIndex()).url(), QUrl());
    QVERIFY(!dirModel.indexForUrl(QUrl()).isValid());
    const QUrl slashUrl = QUrl::fromLocalFile(QStringLiteral("/"));
    QCOMPARE(dirModel.indexForUrl(slashUrl), rootIndex);

    // switching to another URL should also show a root node
    QSignalSpy spyRowsRemoved(&dirModel, &QAbstractItemModel::rowsRemoved);
    const QUrl tempUrl = QUrl::fromLocalFile(QDir::tempPath());
    dirModel.openUrl(tempUrl, KDirModel::ShowRoot);
    QTRY_COMPARE(dirModel.rowCount(), 1);
    QCOMPARE(spyRowsRemoved.count(), 1);
    const QModelIndex newRootIndex = dirModel.index(0, 0);
    QVERIFY(newRootIndex.isValid());
    QCOMPARE(newRootIndex.data().toString(), QFileInfo(QDir::tempPath()).fileName());
    QVERIFY(!dirModel.parent(newRootIndex).isValid());
    QVERIFY(!dirModel.indexForUrl(slashUrl).isValid());
    QCOMPARE(dirModel.itemForIndex(newRootIndex).url(), tempUrl);
}

void KDirModelTest::testShowRootWithTrailingSlash()
{
    // GIVEN
    KDirModel dirModel;
    const QUrl homeUrl = QUrl::fromLocalFile(QDir::homePath() + QLatin1Char('/'));

    // WHEN
    dirModel.openUrl(homeUrl, KDirModel::ShowRoot);
    QTRY_VERIFY(dirModel.indexForUrl(homeUrl).isValid());
}

void KDirModelTest::testShowRootAndExpandToUrl()
{
    // call expandToUrl without waiting for initial listing of root node
    KDirModel dirModel;
    dirModel.openUrl(QUrl(QStringLiteral("file:///")), KDirModel::ShowRoot);
    const QUrl homeUrl = QUrl::fromLocalFile(QDir::homePath());
    dirModel.expandToUrl(homeUrl);
    QTRY_VERIFY(dirModel.indexForUrl(homeUrl).isValid());
}

void KDirModelTest::testHasChildren_data()
{
    QTest::addColumn<bool>("dirsOnly");
    QTest::addColumn<bool>("withHidden");

    QTest::newRow("with_files_and_no_hidden") << false << false;
    QTest::newRow("dirs_only_and_no_hidden") << true << false;
    QTest::newRow("with_files_and_hidden") << false << true;
    QTest::newRow("dirs_only_with_hidden") << true << true;
}

// Test hasChildren without first populating the dirs
void KDirModelTest::testHasChildren()
{
    QFETCH(bool, dirsOnly);
    QFETCH(bool, withHidden);

    m_dirModel->dirLister()->setDirOnlyMode(dirsOnly);
    m_dirModel->dirLister()->setShowingDotFiles(withHidden);
    fillModel(true, false);

    QVERIFY(m_dirModel->hasChildren());

    auto findDir = [this](const QModelIndex &parentIndex, const QString &name){
        for (int row = 0; row < m_dirModel->rowCount(parentIndex); ++row) {
            QModelIndex idx = m_dirModel->index(row, 0, parentIndex);
            if (m_dirModel->itemForIndex(idx).isDir() && m_dirModel->itemForIndex(idx).name() == name) {
                return idx;
            }
        }
        return QModelIndex();
    };

    m_dirIndex = findDir(QModelIndex(), "subdir");
    QVERIFY(m_dirIndex.isValid());
    QVERIFY(m_dirModel->hasChildren(m_dirIndex));

    auto listDir = [this](const QModelIndex &index) {
        QSignalSpy completedSpy(m_dirModel->dirLister(), QOverload<>::of(&KDirLister::completed));
        m_dirModel->fetchMore(index);
        return completedSpy.wait();
    };
    // Now list subdir/
    QVERIFY(listDir(m_dirIndex));

    const QModelIndex subsubdirIndex = findDir(m_dirIndex, "subsubdir");
    QVERIFY(subsubdirIndex.isValid());
    QCOMPARE(m_dirModel->hasChildren(subsubdirIndex), !dirsOnly);

    const QModelIndex hasChildrenDirIndex = findDir(m_dirIndex, "hasChildren");
    QVERIFY(hasChildrenDirIndex.isValid());
    QVERIFY(m_dirModel->hasChildren(hasChildrenDirIndex));

    // Now list hasChildren/
    QVERIFY(listDir(hasChildrenDirIndex));

    QModelIndex testDirIndex = findDir(hasChildrenDirIndex, "emptyDir");
    QVERIFY(testDirIndex.isValid());
    QVERIFY(!m_dirModel->hasChildren(testDirIndex));

    testDirIndex = findDir(hasChildrenDirIndex, "hiddenfileDir");
    QVERIFY(testDirIndex.isValid());
    QCOMPARE(m_dirModel->hasChildren(testDirIndex), !dirsOnly && withHidden);

    testDirIndex = findDir(hasChildrenDirIndex, "hiddenDirDir");
    QVERIFY(testDirIndex.isValid());
    QCOMPARE(m_dirModel->hasChildren(testDirIndex), withHidden);

    testDirIndex = findDir(hasChildrenDirIndex, "pipeDir");
    QVERIFY(testDirIndex.isValid());
    QCOMPARE(m_dirModel->hasChildren(testDirIndex), !dirsOnly);

    testDirIndex = findDir(hasChildrenDirIndex, "symlinkDir");
    QVERIFY(testDirIndex.isValid());
    QCOMPARE(m_dirModel->hasChildren(testDirIndex), !dirsOnly);

    m_dirModel->dirLister()->setDirOnlyMode(false);
    m_dirModel->dirLister()->setShowingDotFiles(false);

}

void KDirModelTest::testDeleteFile()
{
    fillModel(true);

    QVERIFY(m_fileIndex.isValid());
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const QString path = m_tempDir->path() + '/';
    const QString file = path + "toplevelfile_1";
    const QUrl url = QUrl::fromLocalFile(file);

    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers rowsRemoved
    enterLoop();

    // If we come here, then rowsRemoved() was emitted - all good.
    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, oldTopLevelRowCount - 1); // one less than before
    QCOMPARE(spyRowsRemoved.count(), 1);
    QCOMPARE(spyRowsRemoved[0][1].toInt(), m_fileIndex.row());
    QCOMPARE(spyRowsRemoved[0][2].toInt(), m_fileIndex.row());
    disconnect(m_dirModel, &QAbstractItemModel::rowsRemoved,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    QModelIndex fileIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "toplevelfile_1"));
    QVERIFY(!fileIndex.isValid());

    // Recreate the file, for consistency in the next tests
    // So the second part of this test is a "testCreateFile"
    createTestFile(file);
    // Tricky problem - KDirLister::openUrl will emit items from cache
    // and then schedule an update; so just calling fillModel would
    // not wait enough, it would abort due to not finding toplevelfile_1
    // in the items from cache. This progressive-emitting behavior is fine
    // for GUIs but not for unit tests ;-)
    fillModel(true, false);
    fillModel(false);
}

void KDirModelTest::testDeleteFileWhileListing() // doesn't really test that yet, the kdirwatch deleted signal comes too late
{
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const QString path = m_tempDir->path() + '/';
    const QString file = path + "toplevelfile_1";
    const QUrl url = QUrl::fromLocalFile(file);

    KDirLister *dirLister = m_dirModel->dirLister();
    QSignalSpy spyCompleted(dirLister, QOverload<>::of(&KCoreDirLister::completed));
    connect(dirLister, QOverload<>::of(&KCoreDirLister::completed), this, &KDirModelTest::slotListingCompleted);
    dirLister->openUrl(QUrl::fromLocalFile(path), KDirLister::NoFlags);
    if (!spyCompleted.isEmpty()) {
        QSKIP("listing completed too early");
    }
    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    if (spyCompleted.isEmpty()) {
        enterLoop();
    }
    QVERIFY(spyRowsRemoved.wait(1000));

    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, oldTopLevelRowCount - 1); // one less than before
    QCOMPARE(spyRowsRemoved.count(), 1);
    QCOMPARE(spyRowsRemoved[0][1].toInt(), m_fileIndex.row());
    QCOMPARE(spyRowsRemoved[0][2].toInt(), m_fileIndex.row());

    QModelIndex fileIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "toplevelfile_1"));
    QVERIFY(!fileIndex.isValid());

    qDebug() << "Test done, recreating file";

    // Recreate the file, for consistency in the next tests
    // So the second part of this test is a "testCreateFile"
    createTestFile(file);
    fillModel(true, false); // see testDeleteFile
    fillModel(false);
}

void KDirModelTest::testOverwriteFileWithDir() // #151851 c4
{
    fillModel(false);
    const QString path = m_tempDir->path() + '/';
    const QString dir = path + "subdir";
    const QString file = path + "toplevelfile_1";
    const int oldTopLevelRowCount = m_dirModel->rowCount();

    bool removalWithinTopLevel = false;
    bool dataChangedAtFirstLevel = false;
    auto rrc = connect(m_dirModel, &KDirModel::rowsRemoved, this, [&removalWithinTopLevel](const QModelIndex &index) {
        if (!index.isValid()) {
            // yes, that's what we have been waiting for
            removalWithinTopLevel = true;
        }
    });
    auto dcc = connect(m_dirModel, &KDirModel::dataChanged, this, [&dataChangedAtFirstLevel](const QModelIndex &index) {
        if (index.isValid() && !index.parent().isValid()) {
            // a change of a node whose parent is root, yay, that's it
            dataChangedAtFirstLevel = true;
        }
    });

    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    KIO::Job *job = KIO::move(QUrl::fromLocalFile(dir), QUrl::fromLocalFile(file), KIO::HideProgressInfo);
    delete KIO::delegateExtension<KIO::AskUserActionInterface *>(job);
    auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
    askUserHandler->m_renameResult = KIO::Result_Overwrite;
    QVERIFY(job->exec());

    QCOMPARE(askUserHandler->m_askUserRenameCalled, 1);

    // Wait for a removal within the top level (that's for the old file going away), and also
    // for a dataChanged which notifies us that a file has become a directory

    int retries = 0;
    while ((!removalWithinTopLevel || !dataChangedAtFirstLevel) && retries < 100) {
        QTest::qWait(10);
        ++retries;
    }
    QVERIFY(removalWithinTopLevel);
    QVERIFY(dataChangedAtFirstLevel);

    m_dirModel->disconnect(rrc);
    m_dirModel->disconnect(dcc);

    // If we come here, then rowsRemoved() was emitted - all good.
    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, oldTopLevelRowCount - 1); // one less than before

    QVERIFY(!m_dirModel->indexForUrl(QUrl::fromLocalFile(dir)).isValid());
    QModelIndex newIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "toplevelfile_1"));
    QVERIFY(newIndex.isValid());
    KFileItem newItem = m_dirModel->itemForIndex(newIndex);
    QVERIFY(newItem.isDir()); // yes, the file is a dir now ;-)

    qDebug() << "========= Test done, recreating test data =========";

    recreateTestData();
    fillModel(false);
}

void KDirModelTest::testDeleteFiles()
{
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const QString file = m_tempDir->path() + "/toplevelfile_";
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(file + '1') << QUrl::fromLocalFile(file + '2') << QUrl::fromLocalFile(file + '3');

    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);

    KIO::DeleteJob *job = KIO::del(urls, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    int numRowsRemoved = 0;
    while (numRowsRemoved < 3) {

        QTest::qWait(20);

        numRowsRemoved = 0;
        for (int sigNum = 0; sigNum < spyRowsRemoved.count(); ++sigNum) {
            numRowsRemoved += spyRowsRemoved[sigNum][2].toInt() - spyRowsRemoved[sigNum][1].toInt() + 1;
        }
        qDebug() << "numRowsRemoved=" << numRowsRemoved;
    }

    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, oldTopLevelRowCount - 3); // three less than before

    qDebug() << "Recreating test data";
    recreateTestData();
    qDebug() << "Re-filling model";
    fillModel(false);
}

// A renaming that looks more like a deletion to the model
void KDirModelTest::testRenameFileToHidden() // #174721
{
    const QUrl url = QUrl::fromLocalFile(m_tempDir->path() + "/toplevelfile_2");
    const QUrl newUrl = QUrl::fromLocalFile(m_tempDir->path() + "/.toplevelfile_2");

    QSignalSpy spyDataChanged(m_dirModel, &QAbstractItemModel::dataChanged);
    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    QSignalSpy spyRowsInserted(m_dirModel, &QAbstractItemModel::rowsInserted);
    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    KIO::SimpleJob *job = KIO::rename(url, newUrl, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers KDirLister
    enterLoop();

    // If we come here, then rowsRemoved() was emitted - all good.
    QCOMPARE(spyDataChanged.count(), 0);
    QCOMPARE(spyRowsRemoved.count(), 1);
    QCOMPARE(spyRowsInserted.count(), 0);
    COMPARE_INDEXES(spyRowsRemoved[0][0].value<QModelIndex>(), QModelIndex()); // parent is invalid
    const int row = spyRowsRemoved[0][1].toInt();
    QCOMPARE(row, m_secondFileIndex.row()); // only compare row

    disconnect(m_dirModel, &QAbstractItemModel::rowsRemoved,
               &m_eventLoop, &QTestEventLoop::exitLoop);
    spyRowsRemoved.clear();

    // Put things back to normal, should make the file reappear
    connect(m_dirModel, &QAbstractItemModel::rowsInserted,
                   &m_eventLoop, &QTestEventLoop::exitLoop);
    job = KIO::rename(newUrl, url, KIO::HideProgressInfo);
    QVERIFY(job->exec());
    // Wait for the DBUS signal from KDirNotify, it's the one the triggers KDirLister
    enterLoop();
    QCOMPARE(spyDataChanged.count(), 0);
    QCOMPARE(spyRowsRemoved.count(), 0);
    QCOMPARE(spyRowsInserted.count(), 1);
    int newRow = spyRowsInserted[0][1].toInt();
    m_secondFileIndex = m_dirModel->index(newRow, 0);
    QVERIFY(m_secondFileIndex.isValid());
    QCOMPARE(m_dirModel->itemForIndex(m_secondFileIndex).url().toString(), url.toString());
}

void KDirModelTest::testDeleteDirectory()
{
    const QString path = m_tempDir->path() + '/';
    const QUrl url = QUrl::fromLocalFile(path + "subdir/subsubdir");

    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    QSignalSpy spyDirWatchDeleted(KDirWatch::self(), &KDirWatch::deleted);

    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers rowsRemoved
    enterLoop();

    // If we come here, then rowsRemoved() was emitted - all good.
    QCOMPARE(spyRowsRemoved.count(), 1);
    disconnect(m_dirModel, &QAbstractItemModel::rowsRemoved,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    QModelIndex deletedDirIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir/subsubdir"));
    QVERIFY(!deletedDirIndex.isValid());
    QModelIndex dirIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "subdir"));
    QVERIFY(dirIndex.isValid());

    // TODO!!! Bug in KDirWatch? ###
    // QCOMPARE(spyDirWatchDeleted.count(), 1);
}

void KDirModelTest::testDeleteCurrentDirectory()
{
    const int oldTopLevelRowCount = m_dirModel->rowCount();
    const QString path = m_tempDir->path() + '/';
    const QUrl url = QUrl::fromLocalFile(path);

    QSignalSpy spyRowsRemoved(m_dirModel, &QAbstractItemModel::rowsRemoved);
    connect(m_dirModel, &QAbstractItemModel::rowsRemoved,
                   &m_eventLoop, &QTestEventLoop::exitLoop);

    KDirWatch::self()->statistics();

    KIO::DeleteJob *job = KIO::del(url, KIO::HideProgressInfo);
    QVERIFY(job->exec());

    // Wait for the DBUS signal from KDirNotify, it's the one the triggers rowsRemoved
    enterLoop();

    // If we come here, then rowsRemoved() was emitted - all good.
    const int topLevelRowCount = m_dirModel->rowCount();
    QCOMPARE(topLevelRowCount, 0); // empty

    // We can get rowsRemoved for subdirs first, since kdirwatch notices that.
    QVERIFY(spyRowsRemoved.count() >= 1);

    // Look for the signal(s) that had QModelIndex() as parent.
    int i;
    int numDeleted = 0;
    for (i = 0; i < spyRowsRemoved.count(); ++i) {
        const int from = spyRowsRemoved[i][1].toInt();
        const int to = spyRowsRemoved[i][2].toInt();
        qDebug() << spyRowsRemoved[i][0].value<QModelIndex>() << from << to;
        if (!spyRowsRemoved[i][0].value<QModelIndex>().isValid()) {
            numDeleted += (to - from) + 1;
        }
    }

    QCOMPARE(numDeleted, oldTopLevelRowCount);
    disconnect(m_dirModel, &QAbstractItemModel::rowsRemoved,
               &m_eventLoop, &QTestEventLoop::exitLoop);

    QModelIndex fileIndex = m_dirModel->indexForUrl(QUrl::fromLocalFile(path + "toplevelfile_1"));
    QVERIFY(!fileIndex.isValid());
}

void KDirModelTest::testQUrlHash()
{
    const int count = 3000;
    // Prepare an array of QUrls so that url constructing isn't part of the timing
    QVector<QUrl> urls;
    urls.resize(count);
    for (int i = 0; i < count; ++i) {
        urls[i] = QUrl("http://www.kde.org/path/" + QString::number(i));
    }
    QHash<QUrl, int> qurlHash;
    QHash<QUrl, int> kurlHash;
    QElapsedTimer dt; dt.start();
    for (int i = 0; i < count; ++i) {
        qurlHash.insert(urls[i], i);
    }
    //qDebug() << "inserting" << count << "urls into QHash using old qHash:" << dt.elapsed() << "msecs";
    dt.start();
    for (int i = 0; i < count; ++i) {
        kurlHash.insert(urls[i], i);
    }
    //qDebug() << "inserting" << count << "urls into QHash using new qHash:" << dt.elapsed() << "msecs";
    // Nice results: for count=30000 I got 4515 (before) and 103 (after)

    dt.start();
    for (int i = 0; i < count; ++i) {
        QCOMPARE(qurlHash.value(urls[i]), i);
    }
    //qDebug() << "looking up" << count << "urls into QHash using old qHash:" << dt.elapsed() << "msecs";
    dt.start();
    for (int i = 0; i < count; ++i) {
        QCOMPARE(kurlHash.value(urls[i]), i);
    }
    //qDebug() << "looking up" << count << "urls into QHash using new qHash:" << dt.elapsed() << "msecs";
    // Nice results: for count=30000 I got 4296 (before) and 63 (after)
}
