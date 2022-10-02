/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QDir>
#include <QStandardPaths>
#include <QTextStream>

using LocationMap = QMap<QString, QString>;

static void getExtraXdgDirs(LocationMap &map)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0) && defined(Q_OS_UNIX)
    // Qt5 does not provide an easy way to receive the xdg dir for the templates and public
    // directory so we have to find it on our own (QTBUG-86106 and QTBUG-78092)
    using QS = QStandardPaths;
    const QString xdgUserDirs = QS::locate(QS::ConfigLocation, QStringLiteral("user-dirs.dirs"), QS::LocateFile);
    if (xdgUserDirs.isEmpty()) {
        return;
    }

    QFile file(xdgUserDirs);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&file);
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
#endif
}

static QMap<QString, QString> standardLocationsMap()
{
    struct LocationInfo {
        QStandardPaths::StandardLocation location;
        const char *iconName;
    };
    static const LocationInfo mapping[] = {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
        {QStandardPaths::TemplatesLocation, "folder-templates"},
        {QStandardPaths::PublicShareLocation, "folder-public"},
#endif
        {QStandardPaths::MusicLocation, "folder-music"},
        {QStandardPaths::MoviesLocation, "folder-videos"},
        {QStandardPaths::PicturesLocation, "folder-pictures"},
        {QStandardPaths::TempLocation, "folder-temp"},
        {QStandardPaths::DownloadLocation, "folder-download"},
        // Order matters here as paths can be reused for multiple purposes
        // We essentially want more generic choices to trump more specific
        // ones.
        // home > desktop > documents > *.
        {QStandardPaths::DocumentsLocation, "folder-documents"},
        {QStandardPaths::DesktopLocation, "user-desktop"},
        {QStandardPaths::HomeLocation, "user-home"},
    };

    LocationMap map;
    // Do this first so that e.g. QStandardPaths::HomeLocation is alwasy last
    // and it would get QStringLiteral("user-home") associated with it in "map"
    getExtraXdgDirs(map);

    for (const auto &row : mapping) {
        const QStringList locations = QStandardPaths::standardLocations(row.location);
        for (const QString &location : locations) {
            map.insert(location, QLatin1String(row.iconName));
        }
    }
    return map;
}

QString KIOPrivate::iconForStandardPath(const QString &localDirectory)
{
    static auto map = standardLocationsMap();
    return map.value(localDirectory, QString());
}
