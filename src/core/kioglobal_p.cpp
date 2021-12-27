/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

static QMap<QString, QString> standardLocationsMap()
{
    static const struct {
        QStandardPaths::StandardLocation location;
        QString name;
    } mapping[] = {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
                   {QStandardPaths::TemplatesLocations, QStringLiteral("folder-templates")},
                   {QStandardPaths::PublicShareLocation, QStringLiteral("folder-public")},
#endif
                   {QStandardPaths::MusicLocation, QStringLiteral("folder-music")},
                   {QStandardPaths::MoviesLocation, QStringLiteral("folder-videos")},
                   {QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures")},
                   {QStandardPaths::TempLocation, QStringLiteral("folder-temp")},
                   {QStandardPaths::DownloadLocation, QStringLiteral("folder-download")},
                   // Order matters here as paths can be reused for multiple purposes
                   // We essentially want more generic choices to trump more specific
                   // ones.
                   // home > desktop > documents > *.
                   {QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents")},
                   {QStandardPaths::DesktopLocation, QStringLiteral("user-desktop")},
                   {QStandardPaths::HomeLocation, QStringLiteral("user-home")}};

    QMap<QString, QString> map;
    for (const auto &row : mapping) {
        const QStringList locations = QStandardPaths::standardLocations(row.location);
        for (const QString &location : locations) {
            map.insert(location, row.name);
        }
        // Qt does not provide an easy way to receive the xdg dir for the templates and public directory so we have to find it on our own (QTBUG-86106 and QTBUG-78092)
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#ifdef Q_OS_UNIX
        const QString xdgUserDirs = QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("user-dirs.dirs"), QStandardPaths::LocateFile);
        QFile xdgUserDirsFile(xdgUserDirs);
        if (!xdgUserDirs.isEmpty() && xdgUserDirsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&xdgUserDirsFile);
            const QLatin1String templatesLine("XDG_TEMPLATES_DIR=\"");
            const QLatin1String publicShareLine("XDG_PUBLICSHARE_DIR=\"");
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (line.startsWith(templatesLine)) {
                    QString xdgTemplates = line.mid(templatesLine.size()).chopped(1);
                    xdgTemplates.replace(QStringLiteral("$HOME"), QDir::homePath());
                    map.insert(xdgTemplates, QStringLiteral("folder-templates"));
                } else if (line.startsWith(publicShareLine)) {
                    QString xdgPublicShare = line.mid(publicShareLine.size()).chopped(1);
                    xdgPublicShare.replace(QStringLiteral("$HOME"), QDir::homePath());
                    map.insert(xdgPublicShare, QStringLiteral("folder-public"));
                }
            }
        }
#endif
#endif
    }
    return map;
}

QString KIOPrivate::iconForStandardPath(const QString &localDirectory)
{
    static auto map = standardLocationsMap();
    return map.value(localDirectory, QString());
}
