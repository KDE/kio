/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2017 Anthony Fieroni <bvbfan@abv.bg>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_PATHHELPERS_P_H
#define KIO_PATHHELPERS_P_H

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

#endif // KIO_PATHHELPERS_P_H
