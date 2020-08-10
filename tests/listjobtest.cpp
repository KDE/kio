/*
    SPDX-FileCopyrightText: 2014 Frank Reininghaus <frank78ac@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <kio/job.h>

#include <iostream>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

int main(int argc, char **argv)
{
    if (argc < 2) {
        qWarning() << "Expected a path or URL.";
        return 1;
    }

    QCoreApplication app(argc, argv);
    quint64 entriesListed = 0;

    for (int i = 1; i < argc; ++i) {
        QUrl url = QUrl::fromUserInput(QString::fromLocal8Bit(argv[i]), QDir::currentPath());
        qDebug() << "Starting listJob for the URL:" << url;

        KIO::ListJob *job = KIO::listDir(url, KIO::HideProgressInfo);
        job->setUiDelegate(nullptr);
        job->addMetaData(QStringLiteral("statDetails"), QString::number(KIO::StatDefaultDetails));

        QObject::connect(job, &KIO::ListJob::entries,
                         [&entriesListed] (KIO::Job*, const KIO::UDSEntryList &entries) {
                            entriesListed += entries.size();
                            qDebug() << "Listed" << entriesListed << "files.";
                         });
    }

    return app.exec();
}
