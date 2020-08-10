/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 2009, 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <KService>
#include <KIO/ApplicationLauncherJob>
#include <KIO/JobUiDelegate>
#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString serviceId = QStringLiteral("org.kde.kwrite");
    if (argc > 1) {
        serviceId = QString::fromLocal8Bit(argv[1]);
    }
    QList<QUrl> urls;
    if (argc > 2) {
        urls << QUrl::fromUserInput(QString::fromLocal8Bit(argv[2]));
    }

    KService::Ptr service = KService::serviceByDesktopName(serviceId);
    if (!service) {
        service = KService::serviceByStorageId(serviceId + QLatin1String(".desktop"));
        if (!service) {
            qWarning() << "Service not found" << serviceId;
            return 1;
        }
    }

    KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(service);
    job->setUrls(urls);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();

    QObject::connect(job, &KJob::result, &app, [&]() {
        if (job->error()) {
            app.exit(1);
        } else {
            qDebug() << "Started. pid=" << job->pid();
        }
    });

    return app.exec();
}
