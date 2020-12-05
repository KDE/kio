/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DEFAULTS_KFILE_H
#define DEFAULTS_KFILE_H

const int kfile_area = 250;

#define DefaultMixDirsAndFiles false
#define DefaultShowHidden false
#define DefaultDirsFirst true
#define DefaultSortReversed false
#define DefaultRecentURLsNumber 15
#define DefaultDirectoryFollowing true
#define DefaultAutoSelectExtChecked true
#define ConfigGroup QStringLiteral("KFileDialog Settings")
#define RecentURLs QStringLiteral("Recent URLs")
#define RecentFiles QStringLiteral("Recent Files")
#define RecentURLsNumber QStringLiteral("Maximum of recent URLs")
#define RecentFilesNumber QStringLiteral("Maximum of recent files")
#define DialogWidth QStringLiteral("Width (%1)")
#define DialogHeight QStringLiteral("Height (%1)")
#define ConfigShowStatusLine QStringLiteral("ShowStatusLine")
#define AutoDirectoryFollowing QStringLiteral("Automatic directory following")
#define PathComboCompletionMode QStringLiteral("PathCombo Completionmode")
#define LocationComboCompletionMode QStringLiteral("LocationCombo Completionmode")
#define ShowSpeedbar QStringLiteral("Show Speedbar")
#define SpeedbarWidth QStringLiteral("Speedbar Width")
#define ShowBookmarks QStringLiteral("Show Bookmarks")
#define AutoSelectExtChecked QStringLiteral("Automatically select filename extension")
#define BreadcrumbNavigation QStringLiteral("Breadcrumb Navigation")
#define ShowFullPath QStringLiteral("Show Full Path")
#define PlacesIconsAutoresize QStringLiteral("Places Icons Auto-resize")
#define PlacesIconsStaticSize QStringLiteral("Places Icons Static Size")

#endif
