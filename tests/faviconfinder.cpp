/*
 *  SPDX-FileCopyrightText: 2025 Nicolas Fella <nicolas.fella@gmx.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QUrl>

#include <KIO/FavIconRequestJob>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument("url", "URL to get the favicon for");
    parser.process(app);

    if (parser.positionalArguments().length() != 1) {
        qWarning() << "Wrong number of arguments";
        return 2;
    }

    KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(QUrl(parser.positionalArguments()[0]));

    QObject::connect(job, &KJob::finished, &app, [job, &app] {
        QTextStream out(stdout);

        if (job->error() != KJob::NoError) {
            out << "Error: " << job->error() << " " << job->errorText() << Qt::endl;
            app.exit(1);
            return;
        }

        out << "Favicon: " << job->iconFile() << Qt::endl;
    });

    return app.exec();
}
