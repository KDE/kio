/* This file is part of the KDE libraries
Copyright (C) 2015 Harald Sitter <sitter@kde.org>

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
