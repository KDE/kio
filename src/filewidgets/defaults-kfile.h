/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Stephan Kulow <coolo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DEFAULTS_KFILE_H
#define DEFAULTS_KFILE_H

#include <QLatin1String>

static constexpr int kfile_area = 250;

static const bool DefaultMixDirsAndFiles = false;
static const bool DefaultShowHidden = false;
static const bool DefaultDirsFirst = true;
static const bool DefaultHiddenFilesLast = false;
static const bool DefaultSortReversed = false;
static constexpr int DefaultRecentURLsNumber = 15;
static const bool DefaultDirectoryFollowing = true;
static const bool DefaultAutoSelectExtChecked = true;
static const QLatin1String ConfigGroup("KFileDialog Settings");
static const QLatin1String RecentURLs("Recent URLs");
static const QLatin1String RecentFiles("Recent Files");
static const QLatin1String RecentURLsNumber("Maximum of recent URLs");
static const QLatin1String RecentFilesNumber("Maximum of recent files");
static const QLatin1String DialogWidth("Width (%1)");
static const QLatin1String DialogHeight("Height (%1)");
static const QLatin1String ConfigShowStatusLine("ShowStatusLine");
static const QLatin1String AutoDirectoryFollowing("Automatic directory following");
static const QLatin1String PathComboCompletionMode("PathCombo Completionmode");
static const QLatin1String LocationComboCompletionMode("LocationCombo Completionmode");
static const QLatin1String ShowSpeedbar("Show Speedbar");
static const QLatin1String SpeedbarWidth("Speedbar Width");
static const QLatin1String ShowBookmarks("Show Bookmarks");
static const QLatin1String AutoSelectExtChecked("Automatically select filename extension");
static const QLatin1String BreadcrumbNavigation("Breadcrumb Navigation");
static const QLatin1String ShowFullPath("Show Full Path");
static const QLatin1String PlacesIconsAutoresize("Places Icons Auto-resize");
static const QLatin1String PlacesIconsStaticSize("Places Icons Static Size");

#endif
