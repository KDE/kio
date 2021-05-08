/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020-2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "mimetypefinderjobtest.h"
#include "mimetypefinderjob.h"

#include <kio/global.h>

#include <KConfigGroup>
#include <KSharedConfig>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

QTEST_GUILESS_MAIN(MimeTypeFinderJobTest)

void MimeTypeFinderJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void MimeTypeFinderJobTest::cleanupTestCase()
{
}

void MimeTypeFinderJobTest::init()
{
}

static void createSrcFile(const QString &path)
{
    QFile srcFile(path);
    QVERIFY2(srcFile.open(QFile::WriteOnly), qPrintable(srcFile.errorString()));
    srcFile.write("Hello world\n");
}

void MimeTypeFinderJobTest::determineMimeType_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QString>("fileName");

    /* clang-format off */
    QTest::newRow("text_file") << "text/plain" << "srcfile.txt";
    QTest::newRow("text_file_no_extension") << "text/plain" << "srcfile";
    QTest::newRow("desktop_file") << "application/x-desktop" << "foo.desktop";
    QTest::newRow("script") << "application/x-shellscript" << "srcfile.sh";
    QTest::newRow("directory") << "inode/directory" << "srcdir";
    /* clang-format on */
}

void MimeTypeFinderJobTest::determineMimeType()
{
    QFETCH(QString, mimeType);
    QFETCH(QString, fileName);

    // Given a file to open
    QTemporaryDir tempDir;
    const QString srcDir = tempDir.path();
    const QString srcFile = srcDir + QLatin1Char('/') + fileName;
    if (mimeType == "inode/directory") {
        QVERIFY(QDir(srcDir).mkdir(fileName));
    } else {
        createSrcFile(srcFile);
    }

    QVERIFY(QFile::exists(srcFile));
    const QUrl url = QUrl::fromLocalFile(srcFile);

    // When running a MimeTypeFinderJob
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(url, this);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->mimeType(), mimeType);

    // Check that the result is the same when accessing the source
    // file through a symbolic link (bug #436708)
    const QString srcLink = srcDir + QLatin1String("/link_") + fileName;
    QVERIFY(QFile::link(srcFile, srcLink));
    const QUrl linkUrl = QUrl::fromLocalFile(srcLink);

    job = new KIO::MimeTypeFinderJob(linkUrl, this);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->mimeType(), mimeType);
}

void MimeTypeFinderJobTest::invalidUrl()
{
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(QUrl(":/"), this);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_MALFORMED_URL);
    QCOMPARE(job->errorString(), QStringLiteral("Malformed URL\nRelative URL's path component contains ':' before any '/'; source was \":/\"; path = \":/\""));

    QUrl u;
    u.setPath(QStringLiteral("/pathonly"));
    KIO::MimeTypeFinderJob *job2 = new KIO::MimeTypeFinderJob(u, this);
    QVERIFY(!job2->exec());
    QCOMPARE(job2->error(), KIO::ERR_MALFORMED_URL);
    QCOMPARE(job2->errorString(), QStringLiteral("Malformed URL\n/pathonly"));
}

void MimeTypeFinderJobTest::nonExistingFile()
{
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(QUrl::fromLocalFile(QStringLiteral("/does/not/exist")), this);
    QVERIFY(!job->exec());
    QCOMPARE(job->error(), KIO::ERR_DOES_NOT_EXIST);
    QCOMPARE(job->errorString(), "The file or folder /does/not/exist does not exist.");
}

void MimeTypeFinderJobTest::httpUrlWithKIO()
{
    // This tests the scanFileWithGet() code path
    const QUrl url(QStringLiteral("https://www.google.com/"));
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(url, this);
    job->setFollowRedirections(false);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->mimeType(), QStringLiteral("text/html"));
}

void MimeTypeFinderJobTest::killHttp()
{
    // This tests the scanFileWithGet() code path
    const QUrl url(QStringLiteral("https://www.google.com/"));
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(url, this);
    job->start();
    QVERIFY(job->kill());
}

void MimeTypeFinderJobTest::ftpUrlWithKIO()
{
    // This is just to test the statFile() code at least a bit
    const QUrl url(QStringLiteral("ftp://localhost:2")); // unlikely that anything is running on that port
    KIO::MimeTypeFinderJob *job = new KIO::MimeTypeFinderJob(url, this);
    QVERIFY(!job->exec());
    QVERIFY(job->errorString() == QLatin1String("Could not connect to host localhost: Connection refused.")
            || job->errorString() == QLatin1String("Could not connect to host localhost: Network unreachable."));
}
