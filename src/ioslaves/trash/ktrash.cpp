/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QCoreApplication>
#include <QDebug>
#include <QUrl>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDataStream>
#include <KLocalizedString>
#include <KIO/EmptyTrashJob>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ktrash"));
    app.setApplicationVersion(QStringLiteral(PROJECT_VERSION));
    app.setOrganizationDomain(QStringLiteral("kde.org"));

    QCommandLineParser parser;
    parser.addVersionOption();
    parser.addHelpOption();
    parser.setApplicationDescription(i18n("Helper program to handle the KDE trash can\n"
                                          "Note: to move files to the trash, do not use ktrash, but \"kioclient move 'url' trash:/\""));

    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("empty")},
                                        i18n("Empty the contents of the trash")));
    parser.addOption(QCommandLineOption(QStringList{QStringLiteral("restore")},
                                        i18n("Restore a trashed file to its original location"),
                                        QStringLiteral("file")));

    parser.process(app);

    if (parser.isSet(QStringLiteral("empty"))) {
        // We use a kio job instead of linking to TrashImpl, for a smaller binary
        // (and the possibility of a central service at some point)
        KIO::Job *job = KIO::emptyTrash();
        job->exec();
        return 0;
    }

    QString restoreArg = parser.value(QStringLiteral("restore"));
    if (!restoreArg.isEmpty()) {

        if (restoreArg.indexOf(QLatin1String("system:/trash")) == 0) {
            restoreArg.replace(0, 13, QStringLiteral("trash:"));
        }

        QUrl trashURL(restoreArg);
        if (!trashURL.isValid() || trashURL.scheme() != QLatin1String("trash")) {
            qCritical() << "Invalid URL for restoring a trashed file, trash:// URL expected:" << trashURL;
            return 1;
        }

        QByteArray packedArgs;
        QDataStream stream(&packedArgs, QIODevice::WriteOnly);
        stream << (int)3 << trashURL;
        KIO::Job *job = KIO::special(trashURL, packedArgs);
        bool ok = job->exec() ? true : false;
        if (!ok) {
            qCritical() << job->errorString();
        }
        return 0;
    }

    return 0;
}
