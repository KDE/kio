/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QSignalSpy>
#include <QTest>

#include <KIO/BatchRenameJob>

#include "kiotesthelper.h"

class BatchRenameJobTest : public QObject
{
    Q_OBJECT

private:
    void createTestFiles(const QStringList &fileList)
    {
        for (const QString &filename : fileList) {
            createTestFile(m_homeDir + filename);
        }
    }

    bool checkFileExistence(const QStringList &fileList)
    {
        for (const QString &filename : fileList) {
            const QString filePath = m_homeDir + filename;
            if (!QFile::exists(filePath)) {
                return false;
            }
        }
        return true;
    }

    QList<QUrl> createUrlList(const QStringList &fileList)
    {
        QList<QUrl> srcList;
        srcList.reserve(fileList.count());
        for (const QString &filename : fileList) {
            const QString filePath = m_homeDir + filename;
            srcList.append(QUrl::fromLocalFile(filePath));
        }
        return srcList;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        cleanupTestCase();

        // Create temporary home directory
        m_homeDir = homeTmpDir();
    }

    void cleanupTestCase()
    {
        QDir(homeTmpDir()).removeRecursively();
    }

    void batchRenameJobTest_data()
    {
        QTest::addColumn<QStringList>("oldFilenames");
        QTest::addColumn<QString>("baseName");
        QTest::addColumn<int>("index");
        QTest::addColumn<QChar>("indexPlaceholder");
        QTest::addColumn<QStringList>("newFilenames");

        /* clang-format off */
        QTest::newRow("different-extensions-single-placeholder") << (QStringList{"old_file_without_extension", "old_file.txt", "old_file.zip"})
                                                                 << "#-new_name"
                                                                 << 1
                                                                 << QChar('#')
                                                                 << QStringList{"1-new_name", "2-new_name.txt", "3-new_name.zip"};

        QTest::newRow("same-extensions-placeholder-sequence") << (QStringList{"first_source.cpp", "second_source.cpp", "third_source.java"})
                                                              << "new_source###"
                                                              << 8
                                                              << QChar('#')
                                                              << QStringList{"new_source008.cpp", "new_source009.cpp", "new_source010.java"};

        QTest::newRow("different-extensions-invalid-placeholder") << (QStringList{"audio.mp3", "video.mp4", "movie.mkv"})
                                                                  << "me#d#ia"
                                                                  << 0
                                                                  << QChar('#')
                                                                  << QStringList{"me#d#ia.mp3", "me#d#ia.mp4", "me#d#ia.mkv"};

        QTest::newRow("same-extensions-invalid-placeholder") << (QStringList{"random_headerfile.h", "another_headerfile.h", "random_sourcefile.c"})
                                                             << "##file#"
                                                             << 4
                                                             << QChar('#')
                                                             << QStringList{"##file#4.h", "##file#5.h", "##file#6.c"};

        // The MIME database knows nothing of a .nessus file, which is no reason to rename
        // it into a file of no kind at all.
        QTest::newRow("extension-of-an-unknown-kind") << (QStringList{"report.nessus", "scan_notes.txt"})
                                                      << "#-scan"
                                                      << 1
                                                      << QChar('#')
                                                      << QStringList{"1-scan.nessus", "2-scan.txt"};

        QTest::newRow("extension-of-two-parts") << (QStringList{"backup.tar.gz", "backup_notes.txt"})
                                                << "#-backup"
                                                << 1
                                                << QChar('#')
                                                << QStringList{"1-backup.tar.gz", "2-backup.txt"};

        // A dot in the middle of a name that ends in no extension at all.
        QTest::newRow("name-with-a-dot-and-no-extension") << (QStringList{"version.1.2.final", "version_notes.txt"})
                                                          << "#-version"
                                                          << 1
                                                          << QChar('#')
                                                          << QStringList{"1-version.final", "2-version.txt"};
        /* clang-format on */
    }

    void batchRenameJobTest()
    {
        QFETCH(QStringList, oldFilenames);
        QFETCH(QString, baseName);
        QFETCH(int, index);
        QFETCH(QChar, indexPlaceholder);
        QFETCH(QStringList, newFilenames);
        createTestFiles(oldFilenames);
        QVERIFY(checkFileExistence(oldFilenames));
        KIO::BatchRenameJob *job = KIO::batchRename(createUrlList(oldFilenames), baseName, index, indexPlaceholder);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::BatchRenameJob::fileRenamed);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QStringList result;
        for (const auto &signal : spy) {
            // collects the resulting filenames
            result.append(signal[1].toUrl().fileName());
        }
        QCOMPARE(result, newFilenames);
        QCOMPARE(job->processedAmount(KJob::Items), oldFilenames.count());
        QCOMPARE(job->totalAmount(KJob::Items), oldFilenames.count());
        QVERIFY(!checkFileExistence(oldFilenames));
        QVERIFY(checkFileExistence(newFilenames));
    }

    void addingTextKeepsTheWholeName()
    {
        // Adding text in front of a name hands the rest of it back untouched, so nothing
        // of a folder is lost, and the extension of a file the MIME database knows
        // nothing about is still its extension.
        const QStringList folders{QStringLiteral("2026.01.holiday"), QStringLiteral("2026.02.work")};
        for (const QString &folder : folders) {
            QVERIFY(QDir().mkpath(m_homeDir + folder));
        }
        const QStringList files{QStringLiteral("scan_of_a_host.nessus")};
        createTestFiles(files);

        auto addText = [](const QStringView name) {
            return QStringLiteral("H ") + name.toString();
        };
        KIO::BatchRenameJob *job = KIO::batchRenameWithFunction(createUrlList(folders + files), addText);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        const QStringList renamed{QStringLiteral("H 2026.01.holiday"), QStringLiteral("H 2026.02.work"), QStringLiteral("H scan_of_a_host.nessus")};
        QVERIFY(checkFileExistence(renamed));
        QVERIFY(!checkFileExistence(folders + files));
    }

private:
    QString m_homeDir;
};

QTEST_MAIN(BatchRenameJobTest)

#include "batchrenamejobtest.moc"
