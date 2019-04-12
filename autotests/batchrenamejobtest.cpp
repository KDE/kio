/* This file is part of the KDE libraries
   Copyright (C) 2017 by Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) version 3, or any
   later version accepted by the membership of KDE e.V. (or its
   successor approved by the membership of KDE e.V.), which shall
   act as a proxy defined in Section 6 of version 3 of the license.
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QTest>
#include <QSignalSpy>

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

        // To avoid a runtime dependency on klauncher
        qputenv("KDE_FORK_SLAVES", "yes");

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

        QTest::newRow("different-extensions-single-placeholder")  << (QStringList() << "old_file_without_extension"
                                                                                    << "old_file.txt"
                                                                                    << "old_file.zip")
                                                                  << "#-new_name" << 1 << QChar('#')
                                                                  << (QStringList() << "1-new_name"
                                                                                    << "2-new_name.txt"
                                                                                    << "3-new_name.zip");

        QTest::newRow("same-extensions-placeholder-sequence")     << (QStringList() << "first_source.cpp"
                                                                                    << "second_source.cpp"
                                                                                    << "third_source.java")
                                                                  << "new_source###" << 8 << QChar('#')
                                                                  << (QStringList() << "new_source008.cpp"
                                                                                    << "new_source009.cpp"
                                                                                    << "new_source010.java");

        QTest::newRow("different-extensions-invalid-placeholder") << (QStringList() << "audio.mp3"
                                                                                    << "video.mp4"
                                                                                    << "movie.mkv")
                                                                  << "me#d#ia" << 0 << QChar('#')
                                                                  << (QStringList() << "me#d#ia.mp3"
                                                                                    << "me#d#ia.mp4"
                                                                                    << "me#d#ia.mkv");

        QTest::newRow("same-extensions-invalid-placeholder")      << (QStringList() << "random_headerfile.h"
                                                                                    << "another_headerfile.h"
                                                                                    << "random_sourcefile.c")
                                                                  << "##file#" << 4 << QChar('#')
                                                                  << (QStringList() << "##file#4.h"
                                                                                    << "##file#5.h"
                                                                                    << "##file#6.c");

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
        KIO::Job *job = KIO::batchRename(createUrlList(oldFilenames), baseName, index, indexPlaceholder);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, SIGNAL(fileRenamed(QUrl,QUrl)));
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), oldFilenames.count());
        QVERIFY(!checkFileExistence(oldFilenames));
        QVERIFY(checkFileExistence(newFilenames));
    }

private:
    QString m_homeDir;
};

QTEST_MAIN(BatchRenameJobTest)

#include "batchrenamejobtest.moc"
