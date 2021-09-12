/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <KEMailClientLauncherJob>
#include <KIO/JobUiDelegate>
#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    auto *job = new KEMailClientLauncherJob;
    job->setTo({"David Faure <faure@kde.org>", "Another person <null@kde.org>"});
    job->setCc({"CC me please <null@kde.org>"});
    job->setSubject("This is the test email's subject");
    job->setBody("This email was created by kemailclientlauncherjobtest_gui in KIO.");
    const QStringList urls = app.arguments();
    QList<QUrl> attachments;
    std::transform(urls.cbegin(), urls.cend(), std::back_inserter(attachments), [](const QString &arg) {
        return QUrl::fromUserInput(arg, QDir::currentPath());
    });
    job->setAttachments(attachments);
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr));
    job->start();

    QObject::connect(job, &KJob::result, &app, [&]() {
        if (job->error()) {
            qWarning() << job->errorString();
            app.exit(1);
        } else {
            qDebug() << "Successfully started";
            app.exit(0);
        }
    });

    return app.exec();
}
