/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2026 Méven Car <meven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later

    Manual benchmark (not run by ctest): reads a large local file's content through KIO
    (KIO::storedGet), which streams the bytes from the file worker to the application over the
    worker connection. Run it on this branch (in-process thread backend) and on a build without
    it (socket backend) to compare end-to-end file-read throughput.
*/

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

#include <KIO/StoredTransferJob>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTemporaryDir dir;
    const QString path = dir.path() + QStringLiteral("/big.bin");
    const qint64 sizeBytes = qint64(512) * 1024 * 1024;
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) {
            std::fprintf(stderr, "cannot create test file\n");
            return 1;
        }
        const QByteArray chunk(1024 * 1024, 'x');
        for (qint64 written = 0; written < sizeBytes; written += chunk.size()) {
            f.write(chunk);
        }
    }

    const QUrl url = QUrl::fromLocalFile(path);
    const int runs = 5;
    double best = 1e30;
    double total = 0;
    for (int i = 0; i < runs; ++i) {
        QElapsedTimer timer;
        timer.start();
        auto *job = KIO::storedGet(url, KIO::NoReload, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        if (!job->exec()) {
            std::fprintf(stderr, "storedGet failed: %s\n", qPrintable(job->errorString()));
            return 1;
        }
        const double ms = timer.nsecsElapsed() / 1e6;
        total += ms;
        best = qMin(best, ms);
    }

    const double mb = double(sizeBytes) / (1024.0 * 1024.0);
    std::printf("storedGet %.0f MB: best %.1f ms (%.0f MB/s), avg %.1f ms (%.0f MB/s) over %d runs\n",
                mb,
                best,
                mb / (best / 1000.0),
                total / runs,
                mb / ((total / runs) / 1000.0),
                runs);
    return 0;
}
