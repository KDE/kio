/*
 *    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 */

#include "kiotesthelper.h"
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QUrl>
#include <kio/previewjob.h>

using namespace Qt::StringLiterals;

class PreviewJobTest : public QObject
{
    Q_OBJECT

    QString testDirPath;
    QString testSymlinkPath;
    QString testFilePath;
    QString testSvgPath;
    QString testPngPath;
    QString cacheDir;
    const QRegularExpression pngRegexp = QRegularExpression(u".*png"_s);
    QStringList defaultPlugins = QStringList{u"directorythumbnail"_s, u"svgthumbnail"_s, u"imagethumbnail"_s, u"textthumbnail"_s};

private Q_SLOTS:
    void initTestCase();
    void testPreviewGenerating_data();
    void testPreviewGenerating();
};

void PreviewJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    if (QStandardPaths::isTestModeEnabled()) {
        QDir(homeTmpDir()).removeRecursively();
    }
    QIcon::setThemeName(u"breeze"_s);
    testDirPath = homeTmpDir() + "testdir";
    testSymlinkPath = homeTmpDir() + "testsymlink";
    testFilePath = testDirPath + "/testfile.txt";
    testSvgPath = testDirPath + "/test.svg";
    testPngPath = testDirPath + "/test2.png";
    cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/thumbnails/normal"_s;
    createTestDirectory(testDirPath, Empty);
    createTestFile(testFilePath);
    createTestSymlink(testSymlinkPath, testDirPath.toLatin1());
    QVERIFY(QFile::copy(QFINDTESTDATA("previewjobtest/test.svg"), testSvgPath));
    QVERIFY(QFile::copy(QFINDTESTDATA("previewjobtest/test2.png"), testPngPath));
}

void PreviewJobTest::testPreviewGenerating_data()
{
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<bool>("success");
    QTest::addColumn<QStringList>("plugins"); // Empty loads defaults
    QTest::addColumn<bool>("cached");

    QTest::newRow("Txt, default plugins") << testFilePath << false << QStringList{} << false;
    QTest::newRow("Txt, textthumbnail plugin only") << testFilePath << true << QStringList{u"textthumbnail"_s} << false;

    QTest::newRow("Png, default plugins") << testPngPath << true << QStringList{} << false;
    QTest::newRow("Png, textthumbnail plugin only") << testPngPath << false << QStringList{u"textthumbnail"_s} << false;

    QTest::newRow("Svg, default plugins") << testSvgPath << false << QStringList{} << true;
    QTest::newRow("Svg, jpegthumbnail plugin only") << testSvgPath << false << QStringList{u"jpegthumbnail"_s} << true;
    QTest::newRow("Svg, svgthumbnail plugin only") << testSvgPath << true << QStringList{u"svgthumbnail"_s} << false;
    QTest::newRow("Svg, textthumbnail plugin, svg resolves into text preview") << testSvgPath << true << QStringList{u"textthumbnail"_s} << false;

    QTest::newRow("Directory, default plugins") << testDirPath << true << QStringList{} << false;
    QTest::newRow("Directory, imagethumbnail plugin only") << testDirPath << false << QStringList{u"imagethumbnail"_s} << false;
    QTest::newRow("Directory, directory+svg+image plugins") << testDirPath << true << defaultPlugins << false;

    QTest::newRow("Symlink, default plugins") << testSymlinkPath << true << QStringList{} << false;
    QTest::newRow("Symlink, imagethumbnail plugin only") << testSymlinkPath << false << QStringList{u"imagethumbnail"_s} << false;
    QTest::newRow("Symlink, directory+svg+image plugins") << testSymlinkPath << true << defaultPlugins << false;
}

void PreviewJobTest::testPreviewGenerating()
{
    QFETCH(QString, filePath);
    QFETCH(bool, success);
    QFETCH(QStringList, plugins);
    QFETCH(bool, cached);
    KFileItem item(QUrl::fromLocalFile(filePath));
    // Make sure the file is initialized
    item.refresh();
    QVERIFY(item.exists());
    QDir thumbnailFolder(cacheDir);

    // The job expects pointer of plugins, so if the list is empty, give it nullptr explicitly
    auto previewJob = KIO::filePreview(KFileItemList{item}, QSize(64, 64), plugins.isEmpty() ? nullptr : &plugins);
    if (success) {
        QSignalSpy spySuccess(previewJob, &KIO::PreviewJob::gotPreview);
        previewJob->start();
        QVERIFY(spySuccess.wait());
        if (cached) {
            thumbnailFolder.refresh();
            QVERIFY(thumbnailFolder.entryList().filter(pngRegexp).count() > 0);
        }
    } else {
        QSignalSpy spyFail(previewJob, &KIO::PreviewJob::failed);
        previewJob->start();
        QVERIFY(spyFail.wait());
    }

    if (QStandardPaths::isTestModeEnabled()) {
        thumbnailFolder.removeRecursively();
    }
}

QTEST_MAIN(PreviewJobTest)

#include "previewjobtest.moc"
