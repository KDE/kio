/* This file is part of the KDE libraries
    Copyright 1999 Stephan Kulow <coolo@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
#ifndef CONFIG_KFILE_H
#define CONFIG_KFILE_H

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

#endif
