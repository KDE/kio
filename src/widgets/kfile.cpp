/* This file is part of the KDE libraries
    Copyright (C) 2006 David Faure <faure@kde.org>

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

#include "kfile.h"

bool KFile::isSortByName( const QDir::SortFlags& sort )
{
    return (sort & QDir::Time) != QDir::Time &&
	         (sort & QDir::Size) != QDir::Size &&
           (sort & QDir::Type) != QDir::Type;
}

bool KFile::isSortBySize( const QDir::SortFlags& sort )
{
    return (sort & QDir::Size) == QDir::Size;
}

bool KFile::isSortByDate( const QDir::SortFlags& sort )
{
    return (sort & QDir::Time) == QDir::Time;
}

bool KFile::isSortByType( const QDir::SortFlags& sort )
{
    return (sort & QDir::Type) == QDir::Type;
}

bool KFile::isSortDirsFirst( const QDir::SortFlags& sort )
{
    return (sort & QDir::DirsFirst) == QDir::DirsFirst;
}

bool KFile::isSortCaseInsensitive( const QDir::SortFlags& sort )
{
    return (sort & QDir::IgnoreCase) == QDir::IgnoreCase;
}

bool KFile::isDefaultView( const FileView& view )
{
    return (view & Default) == Default;
}

bool KFile::isSimpleView( const FileView& view )
{
    return (view & Simple) == Simple;
}

bool KFile::isDetailView( const FileView& view )
{
    return (view & Detail) == Detail;
}

bool KFile::isSeparateDirs( const FileView& view )
{
    return (view & SeparateDirs) == SeparateDirs;
}

bool KFile::isPreviewContents( const FileView& view )
{
    return (view & PreviewContents) == PreviewContents;
}

bool KFile::isPreviewInfo( const FileView& view )
{
    return (view & PreviewInfo) == PreviewInfo;
}

bool KFile::isTreeView( const FileView& view )
{
    return (view & Tree) == Tree;
}

bool KFile::isDetailTreeView( const FileView& view )
{
    return (view & DetailTree) == DetailTree;
}

#include "moc_kfile.cpp"
