/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2006 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kfile.h"

bool KFile::isSortByName(const QDir::SortFlags &sort)
{
    return (sort & QDir::Time) != QDir::Time &&
           (sort & QDir::Size) != QDir::Size &&
           (sort & QDir::Type) != QDir::Type;
}

bool KFile::isSortBySize(const QDir::SortFlags &sort)
{
    return (sort & QDir::Size) == QDir::Size;
}

bool KFile::isSortByDate(const QDir::SortFlags &sort)
{
    return (sort & QDir::Time) == QDir::Time;
}

bool KFile::isSortByType(const QDir::SortFlags &sort)
{
    return (sort & QDir::Type) == QDir::Type;
}

bool KFile::isSortDirsFirst(const QDir::SortFlags &sort)
{
    return (sort & QDir::DirsFirst) == QDir::DirsFirst;
}

bool KFile::isSortCaseInsensitive(const QDir::SortFlags &sort)
{
    return (sort & QDir::IgnoreCase) == QDir::IgnoreCase;
}

bool KFile::isDefaultView(const FileView &view)
{
    return (view & Default) == Default;
}

bool KFile::isSimpleView(const FileView &view)
{
    return (view & Simple) == Simple;
}

bool KFile::isDetailView(const FileView &view)
{
    return (view & Detail) == Detail;
}

bool KFile::isSeparateDirs(const FileView &view)
{
    return (view & SeparateDirs) == SeparateDirs;
}

bool KFile::isPreviewContents(const FileView &view)
{
    return (view & PreviewContents) == PreviewContents;
}

bool KFile::isPreviewInfo(const FileView &view)
{
    return (view & PreviewInfo) == PreviewInfo;
}

bool KFile::isTreeView(const FileView &view)
{
    return (view & Tree) == Tree;
}

bool KFile::isDetailTreeView(const FileView &view)
{
    return (view & DetailTree) == DetailTree;
}

#include "moc_kfile.cpp"
