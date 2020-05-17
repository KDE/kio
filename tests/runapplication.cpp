/* This file is part of the KDE libraries
    Copyright (c) 1999 Waldo Bastian <bastian@kde.org>
    Copyright (c) 2009, 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
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
