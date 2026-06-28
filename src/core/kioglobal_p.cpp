/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QStandardPaths>

#include <type_traits>

using LocationMap = QMap<QString, QString>;

namespace
{
// QStandardPaths::ProjectsLocation was added in Qt 6.12. Detect the enumerator so this keeps
// building against older Qt, where the projects directory simply gets no dedicated icon. The use of
// ProjectsLocation must stay in a template so the discarded if-constexpr branch is not instantiated.
template<typename T, typename = void>
constexpr bool hasProjectsLocation = false;
template<typename T>
constexpr bool hasProjectsLocation<T, std::void_t<decltype(T::ProjectsLocation)>> = true;

template<typename T>
void insertProjectsLocation(LocationMap &map)
{
    if constexpr (hasProjectsLocation<T>) {
        const QStringList locations = QStandardPaths::standardLocations(T::ProjectsLocation);
        for (const QString &location : locations) {
            map.insert(location, QStringLiteral("folder-projects"));
        }
    }
}
}

static QMap<QString, QString> standardLocationsMap()
{
    struct LocationInfo {
        QStandardPaths::StandardLocation location;
        const char *iconName;
    };
    static const LocationInfo mapping[] = {
        {QStandardPaths::TemplatesLocation, "folder-templates"},
        {QStandardPaths::PublicShareLocation, "folder-public"},
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

    for (const auto &row : mapping) {
        const QStringList locations = QStandardPaths::standardLocations(row.location);
        for (const QString &location : locations) {
            map.insert(location, QLatin1String(row.iconName));
        }
    }

    insertProjectsLocation<QStandardPaths>(map);

    return map;
}

QString KIOPrivate::iconForStandardPath(const QString &localDirectory)
{
    static auto map = standardLocationsMap();
    QString path = localDirectory;
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return map.value(path, QString());
}
