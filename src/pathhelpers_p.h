/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2017 Anthony Fieroni <bvbfan@abv.bg>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_PATHHELPERS_P_H
#define KIO_PATHHELPERS_P_H

#include <QDir>
#include <QString>

inline
QString concatPaths(const QString &path1, const QString &path2)
{
    Q_ASSERT(!path2.startsWith(QLatin1Char('/')));

    if (path1.isEmpty()) {
        return path2;
    } else if (!path1.endsWith(QLatin1Char('/'))) {
        return path1 + QLatin1Char('/') + path2;
    } else {
        return path1 + path2;
    }
}

inline
bool isAbsoluteLocalPath(const QString &path)
{
    // QDir::isAbsolutePath() will return true if "path" starts with ':', the latter denotes a
    // Qt Resource (qrc).
    // "Local" as in on local disk not in memory (qrc)
    return !path.startsWith(QLatin1Char(':')) && QDir::isAbsolutePath(path);
}

#endif // KIO_PATHHELPERS_P_H
