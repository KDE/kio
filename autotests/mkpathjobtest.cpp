/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>
#include <QSignalSpy>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <KIO/MkpathJob>

class MkpathJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);

        // To avoid a runtime dependency on klauncher
        qputenv("KDE_FORK_SLAVES", "yes");

        QVERIFY(m_tempDir.isValid());
        m_dir = m_tempDir.path();
    }

    void cleanupTestCase()
    {
    }

    void shouldDoNothingIfExists()
    {
        QVERIFY(QFile::exists(m_dir));
        const QStringList oldEntries = QDir(m_dir).entryList();
        KIO::MkpathJob *job = KIO::mkpath(QUrl::fromLocalFile(m_dir));
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QVERIFY(QFile::exists(m_dir));
        QCOMPARE(spy.count(), 0);
        QCOMPARE(QDir(m_dir).entryList(), oldEntries); // nothing got created in there
    }

    void shouldCreateOneDirectory()
    {
        QUrl url = QUrl::fromLocalFile(m_dir);
        url.setPath(url.path() + "/subdir1");
        KIO::MkpathJob *job = KIO::mkpath(url);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 1);
        QVERIFY(QFile::exists(url.toLocalFile()));
    }

    void shouldCreateTwoDirectories()
    {
        QUrl url = QUrl::fromLocalFile(m_dir);
        url.setPath(url.path() + "/subdir2/subsubdir");
        KIO::MkpathJob *job = KIO::mkpath(url);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 2);
        QVERIFY(QFile::exists(url.toLocalFile()));
    }

    void shouldDoNothingIfExistsWithBasePath()
    {
        const QStringList oldEntries = QDir(m_dir).entryList();
        QUrl url = QUrl::fromLocalFile(m_dir);
        KIO::MkpathJob *job = KIO::mkpath(url, url);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(job->totalAmount(KJob::Directories), 0ULL);
        QCOMPARE(spy.count(), 0);
        QVERIFY(QFile::exists(url.toLocalFile()));
        QCOMPARE(QDir(m_dir).entryList(), oldEntries); // nothing got created in there
    }

    void shouldCreateOneDirectoryWithBasePath()
    {
        QUrl url = QUrl::fromLocalFile(m_dir);
        const QUrl baseUrl = url;
        url.setPath(url.path() + "/subdir3");
        KIO::MkpathJob *job = KIO::mkpath(url, baseUrl);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(job->totalAmount(KJob::Directories), 1ULL);
        QVERIFY(QFile::exists(url.toLocalFile()));
    }

    void shouldCreateTwoDirectoriesWithBasePath()
    {
        QUrl url = QUrl::fromLocalFile(m_dir);
        const QUrl baseUrl = url;
        url.setPath(url.path() + "/subdir4/subsubdir");
        KIO::MkpathJob *job = KIO::mkpath(url, baseUrl);
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 2);
        QCOMPARE(job->totalAmount(KJob::Directories), 2ULL);
        QVERIFY(QFile::exists(url.toLocalFile()));
    }

    void shouldIgnoreUnrelatedBasePath()
    {
        QUrl url = QUrl::fromLocalFile(m_dir);
        url.setPath(url.path() + "/subdir5/subsubdir");
        KIO::MkpathJob *job = KIO::mkpath(url, QUrl::fromLocalFile(QStringLiteral("/does/not/exist")));
        job->setUiDelegate(nullptr);
        QSignalSpy spy(job, &KIO::MkpathJob::directoryCreated);
        QVERIFY2(job->exec(), qPrintable(job->errorString()));
        QCOMPARE(spy.count(), 2);
        QVERIFY(QFile::exists(url.toLocalFile()));
    }

private:
    QTemporaryDir m_tempDir;
    QString m_dir;
};

QTEST_MAIN(MkpathJobTest)

#include "mkpathjobtest.moc"

