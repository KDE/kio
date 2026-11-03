/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QStandardPaths>

#include <algorithm>

using LocationMap = QMap<QString, QString>;

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

// https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file#naming-conventions
QLatin1StringView KIOPrivate::msdosInvalidChars()
{
    return QLatin1StringView(R"(<>:"/\|?*)");
}

bool KIOPrivate::hasMsdosInvalidChars(QStringView name)
{
    const QLatin1StringView invalidChars = msdosInvalidChars();
    return std::any_of(invalidChars.begin(), invalidChars.end(), [name](const char c) {
        return name.contains(QLatin1Char(c));
    });
}

// MTP storage rejects the same set of reserved characters libmtp inherits from the
// Windows MTP API (see bug 489288), regardless of the device's actual filesystem.
static bool isMtpScheme(const QUrl &url)
{
    return url.scheme() == QLatin1String("mtp");
}

bool KIOPrivate::destinationDisallowsMsdosChars(const QUrl &url, KFileSystemType::Type fsType)
{
    if (url.isLocalFile()) {
#ifdef Q_OS_WIN
        // The Windows file APIs reject these characters on every volume, and KFileSystemType
        // reports Unknown for all of them.
        Q_UNUSED(fsType)
        return true;
#else
        return fsType == KFileSystemType::Fat || fsType == KFileSystemType::Exfat || fsType == KFileSystemType::Ntfs;
#endif
    }
    return isMtpScheme(url);
}

QString KIOPrivate::destinationFileSystemName(const QUrl &url, KFileSystemType::Type fsType)
{
    if (isMtpScheme(url)) {
        return QStringLiteral("MTP");
    }
#ifdef Q_OS_WIN
    if (url.isLocalFile()) {
        if (const QString name = windowsFileSystemName(url.toLocalFile()); !name.isEmpty()) {
            return name;
        }
    }
#endif
    return KFileSystemType::fileSystemName(fsType);
}
