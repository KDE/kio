/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "copyjob_p.h" // KIO::TestPrivate
#include "kiotesthelper.h" // createTestFile etc.
#include "mockcoredelegateextensions.h"

#include <kio/copyjob.h>
#include <kio/filecopyjob.h>
#include <kio/job.h>
#include <kio/simplejob.h>

#include <KJobUiDelegate>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRandomGenerator>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <qplatformdefs.h>

#include <algorithm>
#include <optional>
#include <ranges>

#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

class CopyJobTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void copyManyFilesBatchedRandomTrees();
    void copyManyFilesBatchedMalformedRequests();
    void copyManyFilesBatched();
    void copyManyFilesBatchedWithExistingDest();
    void copyManyFilesBatchedOverwriteExistingDest();
    void copyManyFilesBatchedWithUnreadableSource();
    void copyManyFilesBatchedCancelled();
    void copyManyFilesKeepsBatchingAfterADeferredFile();
    void killingABatchToARemovableDeviceKeepsItsFlush();
    void copyLargeFilesBatched();
    void copyManyFilesBatchedToSetgidDirectory();
};

void CopyJobTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::instance()->setApplicationName(QStringLiteral("kio/copyjobtest"));

    // to make sure io is not too fast
    qputenv("KIOWORKER_FILE_ENABLE_TESTMODE", "1");

    s_referenceTimeStamp = QDateTime::currentDateTime().addSecs(-30); // 30 seconds ago

    cleanupTestCase();
    homeTmpDir(); // create it
}

void CopyJobTest::cleanupTestCase()
{
    QDir(homeTmpDir()).removeRecursively();
}

static void simulatePressingSkip(KJob *job)
{
    // Simulate the user pressing "Skip" in the dialog.
    job->setUiDelegate(new KJobUiDelegate);
    auto *askUserHandler = new MockAskUserInterface(job->uiDelegate());
    askUserHandler->m_skipResult = KIO::Result_Skip;
}

#ifdef Q_OS_UNIX
// Turns dir into a folder a group shares: gives it a group of the user other than the effective
// one, so that the group it hands out is visible, and sets the setgid bit on it. Returns the group
// the folder was given, or nothing when the user belongs to a single group or none of its groups
// can be given to a folder here.
static std::optional<gid_t> shareDirectoryWithAnotherGroup(const QString &dir)
{
    QList<gid_t> groups(getgroups(0, nullptr));
    if (getgroups(groups.size(), groups.data()) == -1) {
        return {};
    }
    QList<gid_t> candidates;
    std::ranges::copy_if(groups, std::back_inserter(candidates), [](gid_t group) {
        return group != getegid();
    });
    // The groups of the user come before the system groups, which a folder is less likely to be
    // given, and each is tried in turn since being a member of one says little about that.
    std::ranges::stable_partition(candidates, [](gid_t group) {
        return group >= 1000;
    });

    const QByteArray dirPath = QFile::encodeName(dir);
    const auto group = std::ranges::find_if(candidates, [&dirPath](gid_t group) {
        return ::chown(dirPath.constData(), -1, group) == 0;
    });
    if (group == candidates.cend() || ::chmod(dirPath.constData(), 0755 | S_ISGID) != 0) {
        return {};
    }
    return *group;
}
#endif

// The file worker copies a run of plain local files in one command (CopyJobPrivate::tryBatchCopyFiles)
// rather than one file_copy sub-job each. These tests drive that path through KIO::copy and check the
// result is identical to the per-file path: content, attribute preservation, the per-file
// copyingDone() signal (undo and kpropsdlg rely on it), progress, and graceful per-file fallback for
// conflicts and errors. They QSKIP where the batch path does not engage (e.g. the destination's
// filesystem is not eligible), which KIO::TestPrivate::batchCopyCommandsSent() exposes.
// Randomized coverage for the batch path: KIO_BATCH_FUZZ_ROUNDS raises the round count and
// KIO_BATCH_FUZZ_SEED picks another tree, so the same test doubles as a fuzzer when run by hand.
static quint32 batchFuzzSeed()
{
    bool ok = false;
    const quint32 fromEnv = qEnvironmentVariableIntValue("KIO_BATCH_FUZZ_SEED", &ok);
    return ok ? fromEnv : 20260907u;
}

static int batchFuzzRounds(int automated)
{
    bool ok = false;
    const int fromEnv = qEnvironmentVariableIntValue("KIO_BATCH_FUZZ_ROUNDS", &ok);
    return (ok && fromEnv > 0) ? fromEnv : automated;
}

void CopyJobTest::copyManyFilesBatchedRandomTrees()
{
    // A copy of a random run of files ends with every destination holding its source's bytes and
    // mode, whichever files the batch took and which it deferred, and with no ".part" left over.
    QRandomGenerator rng(batchFuzzSeed());
    const int rounds = batchFuzzRounds(8);

    for (int round = 0; round < rounds; ++round) {
        const QString src = homeTmpDir() + QStringLiteral("fuzz_src%1/").arg(round);
        const QString dst = homeTmpDir() + QStringLiteral("fuzz_dst%1/").arg(round);
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
        QVERIFY(QDir().mkpath(src));
        QVERIFY(QDir().mkpath(dst));
        auto cleaner = qScopeGuard([&] {
            QDir(src).removeRecursively();
            QDir(dst).removeRecursively();
        });

        const int n = 2 + int(rng.bounded(30));
        QList<QUrl> sources;
        QHash<QString, QByteArray> wanted; // file name -> the bytes the destination must end with
        QHash<QString, QFile::Permissions> wantedPerms;
        QHash<QString, QDateTime> wantedTimes;
        for (int i = 0; i < n; ++i) {
            const QString name = QStringLiteral("f%1").arg(i);
            const QString path = src + name;
            // sizes around the worker's 512 KiB chunk, plus empty files
            static const int sizes[] = {0, 1, 4096, 200000, 600000};
            const int size = sizes[rng.bounded(int(std::size(sizes)))];
            QByteArray content(size, Qt::Uninitialized);
            for (int b = 0; b < size; ++b) {
                content[b] = char(rng.bounded(256));
            }
            QFile f(path); // not createTestFile(): an empty file has to stay empty
            QVERIFY(f.open(QIODevice::WriteOnly));
            QCOMPARE(f.write(content), qint64(content.size()));
            f.close();
            static const QFile::Permissions modes[] = {QFile::ReadOwner | QFile::WriteOwner,
                                                       QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther,
                                                       QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner};
            QVERIFY(QFile::setPermissions(path, modes[rng.bounded(int(std::size(modes)))]));
            sources << QUrl::fromLocalFile(path);
            wanted.insert(name, content);
            // what the source ends up carrying, which is what the destination has to come out with
            wantedPerms.insert(name, QFileInfo(path).permissions());
            wantedTimes.insert(name, QFileInfo(path).lastModified());
            // some destinations are already there, which the batch has to defer, one by one
            if (rng.bounded(6) == 0) {
                createTestFile(dst + name, true, "OLD");
                wanted[name] = QByteArrayLiteral("OLD"); // the copy skips it, so the old bytes stay
                wantedPerms.remove(name);
                wantedTimes.remove(name);
            }
        }

        KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->setAutoSkip(true); // an existing destination is skipped, so the round stays deterministic
        QVERIFY2(job->exec(), qPrintable(job->errorString()));

        for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
            const QString path = dst + it.key();
            QVERIFY2(QFile::exists(path), qPrintable(path));
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            QCOMPARE(f.readAll(), it.value());
            f.close();
            if (wantedPerms.contains(it.key())) {
                QCOMPARE(QFileInfo(path).permissions(), wantedPerms.value(it.key()));
                QCOMPARE(QFileInfo(path).lastModified(), wantedTimes.value(it.key()));
            }
        }
        const QStringList leftovers = QDir(dst).entryList({QStringLiteral("*.part")}, QDir::Files);
        QVERIFY2(leftovers.isEmpty(), qPrintable(leftovers.join(QLatin1Char(' '))));
    }
}

void CopyJobTest::copyManyFilesBatchedMalformedRequests()
{
    // The batch command reads a payload another process wrote, so it has to survive nonsense: a
    // count that does not match what follows, a truncated stream, absurd lengths, empty names.
    QRandomGenerator rng(batchFuzzSeed());
    const int rounds = batchFuzzRounds(200);

    const QString dst = homeTmpDir() + "fuzz_special/";
    QDir(dst).removeRecursively();
    QVERIFY(QDir().mkpath(dst));
    auto cleaner = qScopeGuard([&] {
        QDir(dst).removeRecursively();
    });

    for (int round = 0; round < rounds; ++round) {
        QByteArray payload;
        QDataStream out(&payload, QIODevice::WriteOnly);
        out << qint32(4); // the batch-copy sub-command
        out << qint32(rng.bounded(4) == 0 ? int(rng.bounded(1u << 31)) : int(rng.bounded(8))); // flags
        const qint32 count = rng.bounded(6) == 0 ? qint32(rng.bounded(1u << 31)) : qint32(rng.bounded(8));
        out << count;
        const int pairs = int(rng.bounded(6));
        for (int i = 0; i < pairs; ++i) {
            static const char *names[] = {"", "/nonexistent/fuzz/src", "relative/name", "/", ".."};
            out << QString::fromLatin1(names[rng.bounded(int(std::size(names)))]) << QString(dst + QStringLiteral("out%1").arg(i));
        }
        if (rng.bounded(3) == 0) {
            payload.truncate(int(rng.bounded(uint(payload.size() + 1)))); // a stream that stops mid-value
        }

        KIO::SimpleJob *job = KIO::special(QUrl::fromLocalFile(dst), payload, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->exec(); // pass or fail, but it must answer and it must not take anything down with it
    }

    // Nothing was copied from names that do not exist.
    QCOMPARE(QDir(dst).entryList(QDir::Files | QDir::NoDotAndDotDot).count(), 0);

    // The control: the same route with a well-formed payload does copy, so the rounds above were
    // read by the command and not rejected before it.
    const QString good = homeTmpDir() + "fuzz_control";
    createTestFile(good, true, "control");
    QByteArray payload;
    QDataStream out(&payload, QIODevice::WriteOnly);
    out << qint32(4) << qint32(0) << qint32(2);
    out << good << QString(dst + QStringLiteral("control0")) << good << QString(dst + QStringLiteral("control1"));
    KIO::SimpleJob *batch = KIO::special(QUrl::fromLocalFile(dst), payload, KIO::HideProgressInfo);
    batch->setUiDelegate(nullptr);
    QVERIFY2(batch->exec(), qPrintable(batch->errorString()));
    QVERIFY(QFile::exists(dst + "control0"));
    QVERIFY(QFile::exists(dst + "control1"));
    QVERIFY(QFile::remove(dst + "control0"));
    QVERIFY(QFile::remove(dst + "control1"));
    QVERIFY(QFile::remove(good));
    const QString src = homeTmpDir() + "fuzz_special_src";
    createTestFile(src, true, "still alive");
    KIO::Job *copy = KIO::file_copy(QUrl::fromLocalFile(src), QUrl::fromLocalFile(dst + "copied"), -1, KIO::HideProgressInfo);
    copy->setUiDelegate(nullptr);
    QVERIFY2(copy->exec(), qPrintable(copy->errorString()));
    QVERIFY(QFile::remove(src));
}

void CopyJobTest::copyManyFilesBatched()
{
    const QString src = homeTmpDir() + "manybatch_src/";
    const QString dst = homeTmpDir() + "manybatch_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 20;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray("batch-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }
    // Distinctive perms + mtime on one file, to confirm attribute preservation through the batch.
    QFile::setPermissions(src + "f0", QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup); // 0640
    const QDateTime when(QDate(2001, 2, 3), QTime(4, 5, 6));
    setTimeStamp(src + "f0", when);

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    // Every file copied with the right content.
    for (int i = 0; i < n; ++i) {
        QFile out(dst + QStringLiteral("f%1").arg(i));
        QVERIFY2(out.open(QIODevice::ReadOnly), qPrintable(out.fileName()));
        QCOMPARE(out.readAll(), QByteArray("batch-") + QByteArray::number(i));
    }
    // The per-file copyingDone() signal must fire once per file even when batched, and each must
    // carry the right source/destination - i.e. the indices the worker streams map back correctly.
    QCOMPARE(spyCopyingDone.count(), n);
    QStringList copiedFrom;
    QStringList copiedTo;
    for (const QList<QVariant> &args : std::as_const(spyCopyingDone)) {
        copiedFrom << args.at(1).toUrl().toLocalFile();
        copiedTo << args.at(2).toUrl().toLocalFile();
    }
    QStringList expectedFrom;
    QStringList expectedTo;
    for (int i = 0; i < n; ++i) {
        expectedFrom << src + QStringLiteral("f%1").arg(i);
        expectedTo << dst + QStringLiteral("f%1").arg(i);
    }
    copiedFrom.sort();
    copiedTo.sort();
    expectedFrom.sort();
    expectedTo.sort();
    QCOMPARE(copiedFrom, expectedFrom);
    QCOMPARE(copiedTo, expectedTo);
    QCOMPARE(job->totalAmount(KJob::Files), n);
    QCOMPARE(job->processedAmount(KJob::Files), n);
    QCOMPARE(job->percent(), 100);
    // Attribute preservation survives the batch.
    QCOMPARE(QFile::permissions(dst + "f0"), QFile::permissions(src + "f0"));
    QCOMPARE(QFileInfo(dst + "f0").lastModified().toSecsSinceEpoch(), when.toSecsSinceEpoch());
}

void CopyJobTest::copyManyFilesBatchedWithExistingDest()
{
    const QString src = homeTmpDir() + "manybatchc_src/";
    const QString dst = homeTmpDir() + "manybatchc_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 20;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray("new-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }
    // One destination already exists. The batch must defer it (DeferConflict) and the per-file path
    // must resolve it (here: the user skips); the rest copy normally.
    createTestFile(dst + "f7", true, "OLD");

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    simulatePressingSkip(job); // answers Skip without setting auto-skip, so batching stays enabled
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    // The conflicting file is left untouched; everything else is copied.
    QFile conflicted(dst + "f7");
    QVERIFY(conflicted.open(QIODevice::ReadOnly));
    QCOMPARE(conflicted.readAll(), QByteArray("OLD"));
    for (int i = 0; i < n; ++i) {
        if (i == 7) {
            continue;
        }
        QFile out(dst + QStringLiteral("f%1").arg(i));
        QVERIFY2(out.open(QIODevice::ReadOnly), qPrintable(out.fileName()));
        QCOMPARE(out.readAll(), QByteArray("new-") + QByteArray::number(i));
    }
    QCOMPARE(spyCopyingDone.count(), n - 1); // the skipped file emits no copyingDone
    QCOMPARE(job->processedAmount(KJob::Files), n - 1);
}

void CopyJobTest::copyManyFilesBatchedOverwriteExistingDest()
{
    const QString src = homeTmpDir() + "manybatchov_src/";
    const QString dst = homeTmpDir() + "manybatchov_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 20;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray("new-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }
    // Two destinations already exist. Under Overwrite the batch must NOT forfeit the fast path: the
    // gather stops the run at an existing destination (the batch never overwrites, it opens O_EXCL),
    // and each existing file is overwritten by the per-file path. Every file ends up with the new
    // content, and the run before/after each existing file still goes through the batch.
    createTestFile(dst + "f7", true, "OLD-7");
    createTestFile(dst + "f13", true, "OLD-13");

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo | KIO::Overwrite);
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    // Every destination - including the two that pre-existed - now holds the fresh source content.
    for (int i = 0; i < n; ++i) {
        QFile out(dst + QStringLiteral("f%1").arg(i));
        QVERIFY2(out.open(QIODevice::ReadOnly), qPrintable(out.fileName()));
        QCOMPARE(out.readAll(), QByteArray("new-") + QByteArray::number(i));
    }
    QCOMPARE(spyCopyingDone.count(), n); // all files copied (fresh via batch, existing via per-file)
    QCOMPARE(job->processedAmount(KJob::Files), n);
}

void CopyJobTest::copyManyFilesBatchedWithUnreadableSource()
{
    if (::geteuid() == 0) {
        QSKIP("Running as root bypasses the read permission check");
    }
    const QString src = homeTmpDir() + "manybatche_src/";
    const QString dst = homeTmpDir() + "manybatche_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);

    const int n = 20;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray("e-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }
    const QString bad = src + "f3";
    QFile::setPermissions(bad, QFileDevice::Permissions{}); // 000 -> EACCES on open
    auto cleaner = qScopeGuard([&] {
        QFile::setPermissions(bad, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    simulatePressingSkip(job); // a per-item error becomes a skip dialog the user answers
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    // The unreadable source is skipped; the rest copy.
    QVERIFY(!QFileInfo::exists(dst + "f3"));
    for (int i = 0; i < n; ++i) {
        if (i == 3) {
            continue;
        }
        QFile out(dst + QStringLiteral("f%1").arg(i));
        QVERIFY2(out.open(QIODevice::ReadOnly), qPrintable(out.fileName()));
        QCOMPARE(out.readAll(), QByteArray("e-") + QByteArray::number(i));
    }
    QCOMPARE(spyCopyingDone.count(), n - 1);
    QCOMPARE(job->processedAmount(KJob::Files), n - 1);
}

void CopyJobTest::copyManyFilesBatchedCancelled()
{
    // Cancelling a batch copy must fail the job with ERR_USER_CANCELED, never report success. (A
    // user kill() is handled by Job::doKill(), which clears the in-flight batch subjob quietly; the
    // symmetric guard in slotResultCopyingBatch additionally stops it resuming the per-file path if
    // a worker ever delivers ERR_USER_CANCELED as a batch result, mirroring the single-file path.)
    const QString src = homeTmpDir() + "batchcancel_src/";
    const QString dst = homeTmpDir() + "batchcancel_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 40;
    const QByteArray blob(512 * 1024, 'x');
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, blob);
        sources << QUrl::fromLocalFile(f);
    }

    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);

    // Cancel as soon as the batch reports byte progress, i.e. while it is still copying.
    bool killed = false;
    connect(job, &KIO::Job::processedSize, this, [job, &killed](KJob *, qulonglong processedSize) {
        if (processedSize > 0 && !killed) {
            killed = true;
            job->kill();
        }
    });

    QVERIFY(!job->exec()); // the cancel must fail the job, not report success
    QVERIFY(killed); // sanity: the cancel actually fired
    QCOMPARE(job->error(), KIO::ERR_USER_CANCELED);
}

void CopyJobTest::copyManyFilesKeepsBatchingAfterADeferredFile()
{
    // A destination that exists is copied on its own, and the files after it still go in batches:
    // 600 files take two batches, the first of 512 files and the second of the 88 after it.
    const QString src = homeTmpDir() + "batchdeferred_src/";
    const QString dst = homeTmpDir() + "batchdeferred_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 600;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i, 3, 10, QLatin1Char('0'));
        createTestFile(f, true, QByteArray("new-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }
    createTestFile(dst + "f005", true, "OLD");

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    simulatePressingSkip(job);
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }
    QCOMPARE(KIO::TestPrivate::batchCopyCommandsSent() - batchesBefore, 2);
    QFile kept(dst + "f005");
    QVERIFY(kept.open(QIODevice::ReadOnly));
    QCOMPARE(kept.readAll(), QByteArray("OLD"));
    QCOMPARE(spyCopyingDone.count(), n - 1);
    QCOMPARE(QDir(dst).entryList(QDir::Files).count(), n);
}

void CopyJobTest::killingABatchToARemovableDeviceKeepsItsFlush()
{
    // A copy to a removable device that is cancelled during a batch still flushes the device, so the
    // job knows about it once the first file is written there.
#ifndef Q_OS_LINUX
    QSKIP("The file worker flushes a removable device with syncfs, which only Linux has");
#endif
    qputenv("KIO_FILE_FORCE_REMOVABLE", "1");
    const auto unsetRemovable = qScopeGuard([] {
        qunsetenv("KIO_FILE_FORCE_REMOVABLE");
    });
    const QString src = homeTmpDir() + "batchkillflush_src/";
    const QString dst = homeTmpDir() + "batchkillflush_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 40;
    const QByteArray blob(512 * 1024, 'x');
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, blob);
        sources << QUrl::fromLocalFile(f);
    }

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    // The worker names the file it starts in a packet, which comes before its answer, so the batch
    // is still running for the job when the job is killed.
    bool killed = false;
    connect(job, &KIO::CopyJob::copying, this, [job, &killed] {
        if (!killed) {
            killed = true;
            job->kill();
        }
    });
    QVERIFY(!job->exec());
    QVERIFY(killed);
    QCOMPARE(KIO::TestPrivate::batchCopyCommandsSent() - batchesBefore, 1);
    QCOMPARE(job->metaData().value(QStringLiteral("flush-destination")), QStringLiteral("true"));
}

void CopyJobTest::copyLargeFilesBatched()
{
    // Large files are not excluded from the batch: the worker copies them in page-sized chunks and
    // polls for cancellation, so they stay interruptible. Confirm several multi-MiB files are
    // batched (not handled per-file) and copied byte-for-byte.
    const QString src = homeTmpDir() + "manybatchcap_src/";
    const QString dst = homeTmpDir() + "manybatchcap_dst/";
    QDir(src).removeRecursively();
    QDir(dst).removeRecursively();
    QDir().mkpath(src);
    QDir().mkpath(dst);
    auto cleaner = qScopeGuard([&] {
        QDir(src).removeRecursively();
        QDir(dst).removeRecursively();
    });

    const int n = 5;
    const int sz = 5 * 1024 * 1024; // 5 files x 5 MiB: multi-chunk files that still batch
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray(sz, char('a' + i)));
        sources << QUrl::fromLocalFile(f);
    }

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(dst), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QSignalSpy spyCopyingDone(job, &KIO::CopyJob::copyingDone);
    // The file count reported while the batch runs counts the files the worker has finished and the
    // one it is on, so it walks up to the total and never past it, and never backwards.
    qulonglong lastFiles = 0;
    connect(job, &KJob::processedAmountChanged, this, [&](KJob *, KJob::Unit unit, qulonglong amount) {
        if (unit != KJob::Files) {
            return;
        }
        QVERIFY(amount <= qulonglong(n));
        QVERIFY(amount >= lastFiles);
        lastFiles = amount;
    });
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    // Every file copied with the right content across the batch boundaries.
    for (int i = 0; i < n; ++i) {
        QFile out(dst + QStringLiteral("f%1").arg(i));
        QVERIFY2(out.open(QIODevice::ReadOnly), qPrintable(out.fileName()));
        const QByteArray data = out.readAll();
        QCOMPARE(data.size(), sz);
        QCOMPARE(data, QByteArray(sz, char('a' + i)));
    }
    QCOMPARE(spyCopyingDone.count(), n);
    QCOMPARE(job->processedAmount(KJob::Files), n);
}

void CopyJobTest::copyManyFilesBatchedToSetgidDirectory()
{
#ifdef Q_OS_UNIX
    // Files copied in one batch land in a folder shared by a group with the group of that folder,
    // the same as a file copied on its own does. See bug 399270.
    QTemporaryDir sandbox(homeTmpDir() + "setgidBatchXXXXXX");
    QVERIFY(sandbox.isValid());
    const QString src = sandbox.filePath(QStringLiteral("filesToShare")) + QLatin1Char('/');
    QVERIFY(QDir().mkpath(src));
    const QString sharedDir = sandbox.filePath(QStringLiteral("sharedFolder"));
    QVERIFY(QDir().mkpath(sharedDir));
    const std::optional<gid_t> otherGroup = shareDirectoryWithAnotherGroup(sharedDir);
    if (!otherGroup) {
        QSKIP("No group of the user this runs as can be given to a folder here");
    }

    const int n = 20;
    QList<QUrl> sources;
    for (int i = 0; i < n; ++i) {
        const QString f = src + QStringLiteral("f%1").arg(i);
        createTestFile(f, true, QByteArray("shared-") + QByteArray::number(i));
        sources << QUrl::fromLocalFile(f);
    }

    const int batchesBefore = KIO::TestPrivate::batchCopyCommandsSent();
    KIO::CopyJob *job = KIO::copy(sources, QUrl::fromLocalFile(sharedDir), KIO::HideProgressInfo);
    job->setUiDelegate(nullptr);
    QVERIFY2(job->exec(), qPrintable(job->errorString()));

    if (KIO::TestPrivate::batchCopyCommandsSent() == batchesBefore) {
        QSKIP("CopyJob did not take the batch path on this filesystem");
    }

    for (int i = 0; i < n; ++i) {
        const QByteArray dest = QFile::encodeName(sharedDir + QStringLiteral("/f%1").arg(i));
        QT_STATBUF buff;
        QVERIFY2(QT_STAT(dest.constData(), &buff) == 0, dest.constData());
        QCOMPARE(buff.st_gid, *otherGroup);
    }
#else
    QSKIP("Setgid folders are a UNIX matter");
#endif
}

QTEST_MAIN(CopyJobTest)

#include "copyjobtest.moc"
