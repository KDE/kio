/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include <QSignalSpy>
#include <QTest>

#include <KFileItem>
#include <KIO/BatchRenameJob>
#include <KIO/UDSEntry>

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

    void createTestDirs(const QStringList &dirList)
    {
        for (const QString &dirName : dirList) {
            QVERIFY(QDir().mkpath(m_homeDir + dirName));
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

    /**
     * Given the items rather than their URLs, the job takes whether an item is a directory from
     * the item, as a listing reports it for any protocol, instead of looking at the filesystem,
     * which it can only do for local paths.
     */
    void itemsSayWhetherTheyAreDirectories()
    {
        // Names of their own: this test leaves its results behind like the others do.
        const QString dirName = QStringLiteral("shared_backup.tar.gz");
        const QString fileName = QStringLiteral("shared_notes.txt");
        createTestDirs({dirName});
        createTestFiles({fileName});

        // Entries complete enough that the item never needs to stat anything.
        const auto itemFor = [this](const QString &name, long long type) {
            KIO::UDSEntry entry;
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, type);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0700);
            return KFileItem(entry, QUrl::fromLocalFile(m_homeDir + name));
        };
        const KFileItemList items{itemFor(dirName, S_IFDIR), itemFor(fileName, S_IFREG)};

        const auto renameFunction = [](QStringView name) {
            return name.toString().replace(QLatin1String("shared_"), QLatin1String("renamed_"));
        };
        KIO::BatchRenameJob *job = KIO::batchRenameWithFunction(items, renameFunction);
        job->setUiDelegate(nullptr);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        // The directory keeps ".tar.gz" as part of its name; the file has ".txt" split off and
        // put back.
        const QStringList renamed{QStringLiteral("renamed_backup.tar.gz"), QStringLiteral("renamed_notes.txt")};
        QVERIFY(checkFileExistence(renamed));
        QVERIFY(!checkFileExistence({dirName, fileName}));
    }

    void batchRenameWithFunctionTest_data()
    {
        QTest::addColumn<QStringList>("oldFilenames");
        QTest::addColumn<bool>("isDir");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<QString>("replacement");
        QTest::addColumn<QStringList>("newFilenames");

        /* clang-format off */
        // The extension is unknown to QMimeDatabase, so it must not be split off at all:
        // splitting at the last dot used to hand "Title_Vol.1 Ch.3" to the rename
        // function and never put the ".1"/".2" back, collapsing both names onto "Ch.3" and
        // aborting the whole batch on the resulting conflict.
        QTest::newRow("unknown-extension-is-not-dropped") << (QStringList{"Title_Vol.1 Ch.3.1", "Title_Vol.1 Ch.3.2", "Title_Vol.1 Ch.4"})
                                                          << false
                                                          << "Title_Vol.1"
                                                          << QString()
                                                          << QStringList{"Ch.3.1", "Ch.3.2", "Ch.4"};

        // The very same names as directories, which is how the bug was reported.
        QTest::newRow("unknown-extension-directories") << (QStringList{"Title_Vol.1 Ch.3.1", "Title_Vol.1 Ch.3.2", "Title_Vol.1 Ch.4"})
                                                       << true
                                                       << "Title_Vol.1"
                                                       << QString()
                                                       << QStringList{"Ch.3.1", "Ch.3.2", "Ch.4"};

        // Multi-digit chapter numbers, i.e. the same shape with more dot-separated digits.
        QTest::newRow("unknown-extension-directories-multi-digit") << (QStringList{"Title_Vol.1 Ch.130.1", "Title_Vol.1 Ch.130.2", "Title_Vol.1 Ch.130"})
                                                                   << true
                                                                   << "Title_Vol.1"
                                                                   << QString()
                                                                   << QStringList{"Ch.130.1", "Ch.130.2", "Ch.130"};

        // A directory is never given an extension, even when its name ends in a suffix the
        // mime database does know. This is what keeps the fix independent of what
        // shared-mime-info happens to register.
        QTest::newRow("directory-with-known-suffix") << (QStringList{"old_backup.tar.gz"})
                                                     << true
                                                     << "old_"
                                                     << "new_"
                                                     << QStringList{"new_backup.tar.gz"};

        // ".123" is a registered glob, but directories are exempt from extension handling
        // entirely, so even this name survives as a folder.
        QTest::newRow("directory-ending-in-numeric-glob") << (QStringList{"Title_Vol.1 Ch.123", "Title_Vol.1 Ch.124"})
                                                          << true
                                                          << "Title_Vol.1"
                                                          << QString()
                                                          << QStringList{"Ch.123", "Ch.124"};

        // The same names as files: ".123" *is* split off here, but it is restored verbatim,
        // so the rename still produces the right result. Only a pattern that needed to match
        // "123" itself would be affected.
        QTest::newRow("files-ending-in-numeric-glob") << (QStringList{"Title_Vol.1 Ch.123", "Title_Vol.1 Ch.124"})
                                                      << false
                                                      << "Title_Vol.1"
                                                      << QString()
                                                      << QStringList{"Ch.123", "Ch.124"};

        // QMimeDatabase reports the full "tar.gz", so chopping only ".gz" used to append a
        // second copy of the suffix.
        QTest::newRow("multi-part-extension") << (QStringList{"old_archive.tar.gz"})
                                              << false
                                              << "old_"
                                              << "new_"
                                              << QStringList{"new_archive.tar.gz"};

        // A known extension is still hidden from the rename function and restored after it.
        QTest::newRow("known-extension-is-preserved") << (QStringList{"old_photo.jpg", "old_notes.txt"})
                                                      << false
                                                      << "old_"
                                                      << "new_"
                                                      << QStringList{"new_photo.jpg", "new_notes.txt"};

        // A pattern that only occurs in the extension must not match, since the rename
        // function never sees the extension.
        QTest::newRow("extension-is-hidden-from-pattern") << (QStringList{"keepme.jpg"})
                                                          << false
                                                          << "jpg"
                                                          << "png"
                                                          << QStringList{"keepme.jpg"};

        // lastIndexOf('.') returns -1 here; left(-1) happened to return the whole string.
        QTest::newRow("no-dot-at-all") << (QStringList{"old_README"})
                                       << false
                                       << "old_"
                                       << "new_"
                                       << QStringList{"new_README"};

        // Leading dot only: the name is entirely "extension-less" as far as this is concerned.
        QTest::newRow("hidden-file") << (QStringList{".old_hiddenfile"})
                                     << false
                                     << "old_"
                                     << "new_"
                                     << QStringList{".new_hiddenfile"};

        // A hidden directory goes through the directory exemption rather than the mime
        // database, and used to fail the same way: an empty name after the split.
        QTest::newRow("hidden-directory") << (QStringList{".old_hiddendir"})
                                          << true
                                          << "old_"
                                          << "new_"
                                          << QStringList{".new_hiddendir"};

        // Hidden and with a multi-part suffix: the suffix is "tar.gz" and everything before
        // it, leading dot included, is the base name. Splitting at the last dot instead gave
        // ".new_local.tar.tar.gz".
        QTest::newRow("hidden-file-with-multi-part-extension") << (QStringList{".old_local.tar.gz"})
                                                               << false
                                                               << "old_"
                                                               << "new_"
                                                               << QStringList{".new_local.tar.gz"};

        // Numeric trailing segments raised in review of the alternative fix (MR !1846). None
        // of ".10", ".11" or ".03.10" is a registered glob, so the whole name has to reach
        // the rename function. Deriving the extension from the last dot instead would hide
        // those segments from the pattern.
        // A trailing number is not an extension and must not be registered as one, so the
        // whole name has to reach the rename function. Two names, so that dropping the tail
        // collapses both onto "photos" and aborts the batch.
        QTest::newRow("year-segment") << (QStringList{"my-photos.2028", "my-photos.2027"})
                                      << false
                                      << "my-"
                                      << QString()
                                      << QStringList{"photos.2028", "photos.2027"};

        QTest::newRow("numeric-segments") << (QStringList{"old_result.10", "old_result.11"})
                                          << false
                                          << "old_"
                                          << "new_"
                                          << QStringList{"new_result.10", "new_result.11"};

        QTest::newRow("date-like-segments") << (QStringList{"old_data.2025.03.10"})
                                            << false
                                            << "old_"
                                            << "new_"
                                            << QStringList{"new_data.2025.03.10"};

        // A trailing dot is part of the name and must survive too.
        QTest::newRow("trailing-dot") << (QStringList{"old_data.2025.03.10."})
                                      << false
                                      << "old_"
                                      << "new_"
                                      << QStringList{"new_data.2025.03.10."};

        // Honest edge case: ".123" *is* a registered glob (application/vnd.lotus-1-2-3), so
        // it is treated as an extension. It must still round trip untouched; only matching
        // the pattern against it is not possible.
        QTest::newRow("numeric-segment-that-is-a-known-glob") << (QStringList{"old_data.123"})
                                                              << false
                                                              << "old_"
                                                              << "new_"
                                                              << QStringList{"new_data.123"};
        /* clang-format on */
    }

    void batchRenameWithFunctionTest()
    {
        QFETCH(QStringList, oldFilenames);
        QFETCH(bool, isDir);
        QFETCH(QString, pattern);
        QFETCH(QString, replacement);
        QFETCH(QStringList, newFilenames);

        if (isDir) {
            createTestDirs(oldFilenames);
        } else {
            createTestFiles(oldFilenames);
        }
        QVERIFY(checkFileExistence(oldFilenames));

        // Mirrors ReplaceStrategy::renameFunction() in KIOWidgets' RenameFileDialog.
        const auto renameFunction = [pattern, replacement](QStringView fileName) {
            QString output = fileName.toString();
            if (pattern.isEmpty()) {
                return output;
            }
            output.replace(pattern, replacement);
            while (output.startsWith(QLatin1Char(' '))) {
                output = output.mid(1);
            }
            return output;
        };

        KIO::BatchRenameJob *job = KIO::batchRenameWithFunction(createUrlList(oldFilenames), renameFunction);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::BatchRenameJob::fileRenamed);
        // A name collision caused by a dropped extension makes the whole job fail here.
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        QStringList result;
        for (const auto &signal : spy) {
            result.append(signal[1].toUrl().fileName());
        }
        QCOMPARE(result, newFilenames);
        QCOMPARE(job->processedAmount(KJob::Items), oldFilenames.count());
        QCOMPARE(job->totalAmount(KJob::Items), oldFilenames.count());
        QVERIFY(checkFileExistence(newFilenames));

        // Clean up so that the next data row starts from a known state. Several rows
        // deliberately reuse the same names for the file and the directory variant.
        for (const QString &filename : std::as_const(newFilenames)) {
            if (isDir) {
                QDir(m_homeDir + filename).removeRecursively();
            } else {
                QFile::remove(m_homeDir + filename);
            }
        }
    }

private:
    QString m_homeDir;
};

QTEST_MAIN(BatchRenameJobTest)

#include "batchrenamejobtest.moc"
