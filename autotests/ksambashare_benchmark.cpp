/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <KSambaShare>

#include <QDir>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

/*
   KSambaShare runs testparm and "net usershare info" once per process. This measures
   what the first caller pays for that, which is what KFileItem::overlays() pays on the
   first directory listing that holds a folder.
*/
class KSambaShareBenchmark : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void firstQuery();
    void queryAfterLoad();
};

void KSambaShareBenchmark::firstQuery()
{
    // The load happens once per process, so the measurement is taken once and reported
    // again on the repeat runs QTest does for a benchmark function.
    static qreal blockedMs = -1;
    static qreal availableMs = -1;

    if (blockedMs < 0) {
        QElapsedTimer timer;
        timer.start();

        KSambaShare *const shares = KSambaShare::instance();
        QSignalSpy changedSpy(shares, &KSambaShare::changed);
        shares->isDirectoryShared(QDir::tempPath());
        blockedMs = timer.nsecsElapsed() / 1000000.0;

        if (changedSpy.isEmpty()) {
            changedSpy.wait(5000);
        }
        availableMs = timer.nsecsElapsed() / 1000000.0;
    }

    qInfo("shares available after %.1f ms", availableMs);
    QTest::setBenchmarkResult(blockedMs, QTest::WalltimeMilliseconds);
}

void KSambaShareBenchmark::queryAfterLoad()
{
    const QString path = QDir::tempPath();
    KSambaShare::instance()->sharedDirectories(); // waits for the load

    QBENCHMARK {
        KSambaShare::instance()->isDirectoryShared(path);
    }
}

QTEST_MAIN(KSambaShareBenchmark)

#include "ksambashare_benchmark.moc"
