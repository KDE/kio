/*
 *    SPDX-FileCopyrightText: 2025 Akseli Lahtinen <akselmo@akselmo.dev>
 *
 *    SPDX-License-Identifier: LGPL-2.0-only
 */

#include "kiotesthelper.h"
#include <QDebug>
#include <QFile>
#include <QImage>
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
    QString testFilePath;
    QString testPngPath;
    QString cacheDir;
    const QRegularExpression pngRegexp = QRegularExpression(u".*png"_s);

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
    testDirPath = homeTmpDir() + "testdir";
    testFilePath = testDirPath + "/testfile.txt";
    testPngPath = testDirPath + "/test.png";
    cacheDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + u"/thumbnails/normal/"_s;
    createTestDirectory(testDirPath, Empty);
    createTestFile(testFilePath);

    QImage testImage(128, 128, QImage::Format_ARGB32_Premultiplied);
    testImage.fill(Qt::blue);
    testImage.save(testPngPath);

    QVERIFY(KIO::PreviewJob::availablePlugins().contains(u"mockthumbnailplugin"_s));
}

void PreviewJobTest::testPreviewGenerating_data()
{
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<bool>("success");
    // mockthumbnailplugin only supports png files
    QTest::newRow("Textfile, fail") << testFilePath << false;
    QTest::newRow("Png, success") << testPngPath << true;
}

void PreviewJobTest::testPreviewGenerating()
{
    QFETCH(QString, filePath);
    QFETCH(bool, success);
    KFileItem item(QUrl::fromLocalFile(filePath));
    // Make sure the file is initialized
    item.refresh();
    QVERIFY(item.exists());
    QDir thumbnailFolder(cacheDir);
    QStringList plugins{u"mockthumbnailplugin"_s};

    auto previewJob = KIO::filePreview(KFileItemList{item}, QSize(64, 64), &plugins);
    if (success) {
        QSignalSpy spySuccess(previewJob, &KIO::PreviewJob::gotPreview);
        previewJob->start();
        QVERIFY(spySuccess.wait());
        QTest::qWait(200); // Wait for the item to be created in the folder
        thumbnailFolder.refresh();
        QVERIFY(thumbnailFolder.entryList().filter(pngRegexp).count() > 0);
    } else {
        QSignalSpy spyFail(previewJob, &KIO::PreviewJob::failed);
        previewJob->start();
        QVERIFY(spyFail.wait());
    }

    // Clean up so we dont check for cached items
    if (QStandardPaths::isTestModeEnabled()) {
        thumbnailFolder.removeRecursively();
    }
}

QTEST_MAIN(PreviewJobTest)

#include "previewjobtest.moc"
