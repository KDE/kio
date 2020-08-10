/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QStandardPaths>

static QMap<QString, QString> standardLocationsMap()
{
    static const
            struct { QStandardPaths::StandardLocation location;
                     QString name; }
                   mapping[] = {
            { QStandardPaths::MusicLocation, QStringLiteral("folder-music") },
            { QStandardPaths::MoviesLocation, QStringLiteral("folder-videos") },
            { QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures") },
            { QStandardPaths::TempLocation, QStringLiteral("folder-temp") },
            { QStandardPaths::DownloadLocation, QStringLiteral("folder-download") },
            // Order matters here as paths can be reused for multiple purposes
            // We essentially want more generic choices to trump more specific
            // ones.
            // home > desktop > documents > *.
            { QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents") },
            { QStandardPaths::DesktopLocation, QStringLiteral("user-desktop") },
            { QStandardPaths::HomeLocation, QStringLiteral("user-home") } };

    QMap<QString, QString> map;
    for (const auto &row : mapping) {
        const QStringList locations = QStandardPaths::standardLocations(row.location);
        for (const QString &location : locations) {
            map.insert(location, row.name);
        }
    }
    return map;
}

QString KIOPrivate::iconForStandardPath(const QString &localDirectory)
{
    static auto map = standardLocationsMap();
    return map.value(localDirectory, QString());
}
