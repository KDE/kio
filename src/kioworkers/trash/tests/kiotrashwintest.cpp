/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include <kio/copyjob.h>
#include <kio/deletejob.h>
#include <kio/listjob.h>
#include <kio/restorejob.h>
#include <kio/statjob.h>
#include <kio/storedtransferjob.h>
#include <kio/udsentry.h>

#include <qt_windows.h>

#include <ole2.h>
#include <shellapi.h>

// The worker discovery and trashrc the worker writes both follow test mode.
static void initEnv()
{
    qputenv("KIOWORKER_ENABLE_TESTMODE", "1");
    qputenv("KDE_SKIP_KDERC", "1");
}
Q_CONSTRUCTOR_FUNCTION(initEnv)

// Empty the recycle bin of the drive the test uses by removing the $I (metadata) and
// $R (content) entries the shell keeps under <drive>\$Recycle.Bin\<user SID>\. With the
// $I files gone the bin reads as empty. SHEmptyRecycleBin is avoided: on a machine with
// a volume that is not a ready local disk it walks that volume and hangs, even when it is
// given a specific drive root.
static void emptyDriveRecycleBin(const QString &driveRoot)
{
    QDir bin(driveRoot + QLatin1String("$Recycle.Bin"));
    if (!bin.exists()) {
        return;
    }
    // The bin holds one subfolder per user, named after the user's SID. Another user's
    // folder is not ours to touch and is denied to us anyway, so failures are ignored.
    const QFileInfoList userBins = bin.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &userBin : userBins) {
        QDir dir(userBin.absoluteFilePath());
        const QFileInfoList items =
            dir.entryInfoList({QStringLiteral("$I*"), QStringLiteral("$R*")}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &item : items) {
            // A trashed directory is stored as a $R directory; a file as a $R file.
            if (item.isDir()) {
                QDir(item.absoluteFilePath()).removeRecursively();
            } else {
                QFile::remove(item.absoluteFilePath());
            }
        }
    }
}

// End-to-end test of the Windows recycle-bin worker. It drives the worker through
// the public KIO jobs and checks the result against the real recycle bin, so it
// refuses to run when the bin already holds items (see initTestCase()).
class KioTrashWinTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanup();

    void trashAbsoluteUrl();
    void trashEmptyDirectory();
    void trashDirectoryWithFiles();
    void restoreFromTrash();
    void deleteFromTrash();
    void getFileContents();
    void statTrashRoot();
    void statTrashItem();
    void listTrashContents();

private:
    QString createTestFile(const QString &name) const;
    QString createTestDir(const QString &name, int fileCount) const;
    qint64 recycleBinItemCount() const;
    QList<KIO::UDSEntry> listTrash() const;

    QTemporaryDir m_tmpDir;
};

QString KioTrashWinTest::createTestFile(const QString &name) const
{
    const QString path = m_tmpDir.filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write("trash me");
    return path;
}

QString KioTrashWinTest::createTestDir(const QString &name, int fileCount) const
{
    const QString path = m_tmpDir.filePath(name);
    if (!QDir().mkpath(path)) {
        return QString();
    }
    for (int i = 0; i < fileCount; ++i) {
        QFile file(path + QLatin1String("/file") + QString::number(i) + QLatin1String(".txt"));
        if (!file.open(QIODevice::WriteOnly)) {
            return QString();
        }
        file.write("trash me");
    }
    return path;
}

qint64 KioTrashWinTest::recycleBinItemCount() const
{
    SHQUERYRBINFO info;
    info.cbSize = sizeof(info);
    // Query only the drive the test operates on. SHQueryRecycleBin(nullptr) enumerates every
    // drive's recycle bin, which can block on the first cold access in a fresh CI session.
    const QString root = m_tmpDir.path().left(2) + QLatin1Char('\\');
    const HRESULT res = SHQueryRecycleBin(reinterpret_cast<LPCWSTR>(root.utf16()), &info);
    if (res != S_OK) {
        return -1;
    }
    return info.i64NumItems;
}

QList<KIO::UDSEntry> KioTrashWinTest::listTrash() const
{
    KIO::ListJob *job = KIO::listDir(QUrl(QStringLiteral("trash:/")),
                                     KIO::HideProgressInfo,
                                     KIO::ListJob::ListFlag::IncludeHidden | KIO::ListJob::ListFlag::ExcludeDotAndDotDot);
    QList<KIO::UDSEntry> entries;
    connect(job, &KIO::ListJob::entries, job, [&entries](KIO::Job *, const KIO::UDSEntryList &list) {
        entries << list;
    });
    if (!job->exec()) {
        return {};
    }
    return entries;
}

void KioTrashWinTest::initTestCase()
{
    // The shell recycle-bin API used here (SHQueryRecycleBin) operates on the recycle-bin
    // shell folder and needs OLE initialised on the calling thread. QApplication does this
    // via its Windows platform plugin; QCoreApplication does not, so initialise OLE
    // explicitly or the first shell call hangs.
    OleInitialize(nullptr);
    QStandardPaths::setTestModeEnabled(true);

    const qint64 count = recycleBinItemCount();
    if (count != 0) {
        // Running would mix our files with the existing ones and the cleanup would
        // empty trash that does not belong to the test, so stop here instead.
        QSKIP("The recycle bin is not empty (or could not be queried); refusing to run.");
    }

    QVERIFY(m_tmpDir.isValid());

    // Some environments (service accounts, restricted CI sessions) have no usable
    // recycle bin and deny trash operations. Probe once and skip the whole suite rather
    // than fail every test.
    const QString probe = createTestFile(QStringLiteral("probe"));
    QVERIFY(!probe.isEmpty());
    const bool trashed = KIO::trash(QUrl::fromLocalFile(probe), KIO::HideProgressInfo)->exec() && !QFileInfo::exists(probe);
    const QString root = m_tmpDir.path().left(2) + QLatin1Char('\\');
    emptyDriveRecycleBin(root);
    if (!trashed) {
        QSKIP("Trashing is not available in this environment (the recycle bin denied the operation).");
    }
}

void KioTrashWinTest::cleanup()
{
    // The bin was empty when the test started, so anything left is ours to drop.
    const QString root = m_tmpDir.path().left(2) + QLatin1Char('\\');
    emptyDriveRecycleBin(root);
}

void KioTrashWinTest::trashAbsoluteUrl()
{
    const QString filePath = createTestFile(QStringLiteral("absolute.txt"));
    QVERIFY(!filePath.isEmpty());

    const QUrl src = QUrl::fromLocalFile(filePath);
    QVERIFY(src.isLocalFile());

    KIO::CopyJob *job = KIO::trash(src, KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    QVERIFY(!QFileInfo::exists(filePath));
    QCOMPARE(recycleBinItemCount(), 1);
}

void KioTrashWinTest::trashEmptyDirectory()
{
    const QString dirPath = createTestDir(QStringLiteral("emptydir"), 0);
    QVERIFY(!dirPath.isEmpty());

    KIO::CopyJob *job = KIO::trash(QUrl::fromLocalFile(dirPath), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // An empty directory leaves its location and counts as a single recycle-bin item.
    // Restoring and permanently deleting behave as for any single item, which the file
    // and directory-with-files tests already cover, so they are not repeated here.
    QVERIFY(!QFileInfo::exists(dirPath));
    QCOMPARE(recycleBinItemCount(), 1);
}

void KioTrashWinTest::trashDirectoryWithFiles()
{
    const QString dirPath = createTestDir(QStringLiteral("fulldir"), 2);
    QVERIFY(!dirPath.isEmpty());
    QVERIFY(QFileInfo::exists(dirPath + QLatin1String("/file0.txt")));

    KIO::CopyJob *job = KIO::trash(QUrl::fromLocalFile(dirPath), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // The directory and its contents go together, still as one recycle-bin item.
    QVERIFY(!QFileInfo::exists(dirPath));
    QCOMPARE(recycleBinItemCount(), 1);

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 1);

    // The single item is the trashed directory, named after it, with its original
    // location recorded.
    const KIO::UDSEntry &entry = entries.first();
    QVERIFY(entry.isDir());
    QCOMPARE(entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME), QStringLiteral("fulldir"));
    QVERIFY(entry.stringValue(KIO::UDSEntry::UDS_EXTRA).endsWith(QLatin1String("fulldir")));

    QUrl trashUrl;
    trashUrl.setScheme(QStringLiteral("trash"));
    trashUrl.setPath(QLatin1Char('/') + entry.stringValue(KIO::UDSEntry::UDS_NAME));

    KIO::RestoreJob *restoreJob = KIO::restoreFromTrash({trashUrl}, KIO::HideProgressInfo);
    QVERIFY2(restoreJob->exec(), qPrintable(restoreJob->errorString()));

    // The directory and both files are restored together and the bin is empty again.
    QVERIFY(QFileInfo(dirPath).isDir());
    QVERIFY(QFileInfo::exists(dirPath + QLatin1String("/file0.txt")));
    QVERIFY(QFileInfo::exists(dirPath + QLatin1String("/file1.txt")));
    QCOMPARE(recycleBinItemCount(), 0);
    // Permanently deleting a trashed item is covered by deleteortrashjobtest.
}

void KioTrashWinTest::restoreFromTrash()
{
    const QString filePath = createTestFile(QStringLiteral("restore.txt"));
    QVERIFY(!filePath.isEmpty());

    KIO::CopyJob *trashJob = KIO::trash(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo);
    QVERIFY2(trashJob->exec(), qPrintable(trashJob->errorString()));
    QVERIFY(!QFileInfo::exists(filePath));

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 1);

    QUrl trashUrl;
    trashUrl.setScheme(QStringLiteral("trash"));
    trashUrl.setPath(QLatin1Char('/') + entries.first().stringValue(KIO::UDSEntry::UDS_NAME));

    KIO::RestoreJob *restoreJob = KIO::restoreFromTrash({trashUrl}, KIO::HideProgressInfo);
    QVERIFY2(restoreJob->exec(), qPrintable(restoreJob->errorString()));

    // The item is moved back to where it came from.
    QVERIFY(QFileInfo::exists(filePath));
    QCOMPARE(recycleBinItemCount(), 0);
}

void KioTrashWinTest::deleteFromTrash()
{
    const QString filePath = createTestFile(QStringLiteral("delete.txt"));
    QVERIFY(!filePath.isEmpty());

    QVERIFY2(KIO::trash(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo)->exec(), "could not trash file");
    QVERIFY(!QFileInfo::exists(filePath));

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 1);

    QUrl trashUrl;
    trashUrl.setScheme(QStringLiteral("trash"));
    trashUrl.setPath(QLatin1Char('/') + entries.first().stringValue(KIO::UDSEntry::UDS_NAME));

    // Deleting the item by its trash URL removes it from the bin for good and does not
    // put it back at its original location.
    KIO::DeleteJob *deleteJob = KIO::del(trashUrl, KIO::HideProgressInfo);
    QVERIFY2(deleteJob->exec(), qPrintable(deleteJob->errorString()));

    QVERIFY(!QFileInfo::exists(filePath));
    QCOMPARE(recycleBinItemCount(), 0);
}

void KioTrashWinTest::getFileContents()
{
    // createTestFile() writes this exact content.
    const QByteArray contents = "trash me";

    const QString filePath = createTestFile(QStringLiteral("readme.txt"));
    QVERIFY(!filePath.isEmpty());

    QVERIFY2(KIO::trash(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo)->exec(), "could not trash file");

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 1);

    QUrl trashUrl;
    trashUrl.setScheme(QStringLiteral("trash"));
    trashUrl.setPath(QLatin1Char('/') + entries.first().stringValue(KIO::UDSEntry::UDS_NAME));

    // Reading the trashed item back must yield the original file's bytes.
    KIO::StoredTransferJob *job = KIO::storedGet(trashUrl, KIO::NoReload, KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));
    QCOMPARE(job->data(), contents);
}

void KioTrashWinTest::statTrashRoot()
{
    KIO::StatJob *job = KIO::stat(QUrl(QStringLiteral("trash:/")), KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    // The trash root is reported as a directory.
    QVERIFY(job->statResult().isDir());
}

void KioTrashWinTest::statTrashItem()
{
    // createTestFile() writes these 8 bytes.
    const QByteArray contents = "trash me";
    const QString filePath = createTestFile(QStringLiteral("stat.txt"));
    QVERIFY(!filePath.isEmpty());

    QVERIFY2(KIO::trash(QUrl::fromLocalFile(filePath), KIO::HideProgressInfo)->exec(), "could not trash file");

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 1);

    QUrl trashUrl;
    trashUrl.setScheme(QStringLiteral("trash"));
    trashUrl.setPath(QLatin1Char('/') + entries.first().stringValue(KIO::UDSEntry::UDS_NAME));

    KIO::StatJob *job = KIO::stat(trashUrl, KIO::HideProgressInfo);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    const KIO::UDSEntry entry = job->statResult();
    QVERIFY(!entry.isDir());
    QCOMPARE(entry.numberValue(KIO::UDSEntry::UDS_SIZE), static_cast<long long>(contents.size()));
    QVERIFY(entry.contains(KIO::UDSEntry::UDS_MODIFICATION_TIME));

    // The original location is recorded as the full path the item restores to.
    QCOMPARE(QDir::fromNativeSeparators(entry.stringValue(KIO::UDSEntry::UDS_EXTRA)), filePath);

    // The deletion date is recorded and is the moment the file was just trashed.
    const QDateTime deletedAt = QDateTime::fromString(entry.stringValue(KIO::UDSEntry::UDS_EXTRA + 1), Qt::ISODate);
    QVERIFY(deletedAt.isValid());
    QVERIFY(qAbs(deletedAt.secsTo(QDateTime::currentDateTimeUtc())) < 3600);
}

void KioTrashWinTest::listTrashContents()
{
    const QString first = createTestFile(QStringLiteral("one.txt"));
    const QString second = createTestFile(QStringLiteral("two.txt"));
    QVERIFY(!first.isEmpty() && !second.isEmpty());

    QVERIFY2(KIO::trash(QUrl::fromLocalFile(first), KIO::HideProgressInfo)->exec(), "could not trash first file");
    QVERIFY2(KIO::trash(QUrl::fromLocalFile(second), KIO::HideProgressInfo)->exec(), "could not trash second file");

    const QList<KIO::UDSEntry> entries = listTrash();
    QCOMPARE(entries.count(), 2);

    for (const KIO::UDSEntry &entry : entries) {
        // The fields the file views and the restore step rely on must be present.
        QVERIFY(entry.contains(KIO::UDSEntry::UDS_NAME));
        QVERIFY(entry.contains(KIO::UDSEntry::UDS_DISPLAY_NAME));
        QVERIFY(entry.contains(KIO::UDSEntry::UDS_FILE_TYPE));
        QVERIFY(entry.contains(KIO::UDSEntry::UDS_ACCESS));
    }
}

QTEST_GUILESS_MAIN(KioTrashWinTest)

#include "kiotrashwintest.moc"
