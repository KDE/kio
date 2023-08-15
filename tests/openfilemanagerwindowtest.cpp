/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QApplication>
#include <QDebug>
#include <QList>
#include <QUrl>

#include <KIO/OpenFileManagerWindowJob>

int main(int argc, char **argv)
{
    QApplication::setApplicationName(QStringLiteral("openfilemanagerwindowtest"));
    QApplication app(argc, argv);

#ifdef Q_OS_WINDOWS
    const QList<QUrl> urls{QUrl(QStringLiteral("file:///c:/Windows/win.ini")), QUrl(QStringLiteral("file:///etc/passwd"))};
#else
    const QList<QUrl> urls{QUrl(QStringLiteral("file:///etc/fstab")), QUrl(QStringLiteral("file:///etc/passwd"))};
#endif

    auto *job = new KIO::OpenFileManagerWindowJob();
    job->setHighlightUrls(urls);
    job->start();

    QObject::connect(job, &KJob::result, job, [&](KJob *job) {
        app.exit(job->error());
    });

    return app.exec();
}
