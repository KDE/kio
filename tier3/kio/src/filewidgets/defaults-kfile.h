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
#define ConfigGroup QLatin1String("KFileDialog Settings")
#define RecentURLs QLatin1String("Recent URLs")
#define RecentFiles QLatin1String("Recent Files")
#define RecentURLsNumber QLatin1String("Maximum of recent URLs")
#define RecentFilesNumber QLatin1String("Maximum of recent files")
#define DialogWidth QLatin1String("Width (%1)")
#define DialogHeight QLatin1String("Height (%1)")
#define ConfigShowStatusLine QLatin1String("ShowStatusLine")
#define AutoDirectoryFollowing QLatin1String("Automatic directory following")
#define PathComboCompletionMode QLatin1String("PathCombo Completionmode")
#define LocationComboCompletionMode QLatin1String("LocationCombo Completionmode")
#define ShowSpeedbar QLatin1String("Show Speedbar")
#define SpeedbarWidth QLatin1String("Speedbar Width")
#define ShowBookmarks QLatin1String("Show Bookmarks")
#define AutoSelectExtChecked QLatin1String("Automatically select filename extension")
#define BreadcrumbNavigation QLatin1String("Breadcrumb Navigation")
#define ShowFullPath QLatin1String("Show Full Path")

#endif
