/*
    This file is part of the KDE project.
    SPDX-FileCopyrightText: 2017 Anthony Fieroni <bvbfan@abv.bg>
    SPDX-FileCopyrightText: 2022 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_UTILS_P_H
#define KIO_UTILS_P_H

#include <QDir>
#include <QString>
#include <QUrl>
#include <QtGlobal>
#include <qplatformdefs.h>

// QT_STAT_LNK on Windows MinGW
#include "kioglobal_p.h"

namespace Utils
{

static const QLatin1Char s_slash('/');

inline bool isAbsoluteLocalPath(const QString &path)
{
    // QDir::isAbsolutePath() will return true if "path" starts with ':', the latter denotes a
    // Qt Resource (qrc).
    // "Local" as in on local disk not in memory (qrc)
    return !path.startsWith(QLatin1Char(':')) && QDir::isAbsolutePath(path);
}

/**
 * Appends a slash to @p path if it's not empty, and doesn't already end with a '/'.
 * This method modifies its arg directly:
 * QString p = "foo";
 * appenSlash(p); // Now p is "foo/"
 *
 * For `const QString&` use slashAppended().
 *
 * All the slash-related methods are modeled after:
 * - QString::chop(), which modifies the string it's called on, and returns void
 * - QString::chopped(), which takes a copy, modifies it, and return it
 */
inline void appendSlash(QString &path)
{
    if (path.isEmpty()) {
        Q_ASSERT_X(false, Q_FUNC_INFO, "Not appending '/' to an empty string");
        return;
    }

    const auto slash = QLatin1Char('/');
    if (!path.endsWith(slash)) {
        path += slash;
    }
}

[[nodiscard]] inline QString slashAppended(QString &&path)
{
    appendSlash(path);
    return path;
}

[[nodiscard]] inline QString slashAppended(const QString &s)
{
    QString path{s};
    appendSlash(path);
    return path;
}

inline void removeTrailingSlash(QString &path)
{
    if (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
}

inline QString trailingSlashRemoved(QString &&path)
{
    removeTrailingSlash(path);
    return path;
}

inline QString trailingSlashRemoved(const QString &s)
{
    QString path = s;
    removeTrailingSlash(path);
    return path;
}

/**
 * Appends a slash '/' to @p url path, if url.path() isn't empty and doesn't already
 * end with a slash.
 */
inline void appendSlashToPath(QUrl &url)
{
    const auto slash = QLatin1Char('/');
    QString path = url.path();
    if (!path.isEmpty() && !path.endsWith(slash)) {
        appendSlash(path);
        url.setPath(path);
    }
}

// concatPaths()
inline QString concatPaths(const QString &path1, const QString &path2)
{
    Q_ASSERT(!path2.startsWith(QLatin1Char('/')));

    if (path1.isEmpty()) {
        return path2;
    }

    QString ret = slashAppended(path1);
    ret += path2;
    return ret;
}

inline QString concatPaths(QString &&path1, const QString &path2)
{
    Q_ASSERT(!path2.startsWith(s_slash));

    if (path1.isEmpty()) {
        return path2;
    }

    appendSlash(path1);
    path1 += path2;
    return path1;
}

inline QString concatPaths(const QString &path1, QString &&path2)
{
    Q_ASSERT(!path2.startsWith(s_slash));

    if (path1.isEmpty()) {
        return path2;
    }

    path2.prepend(s_slash);
    path2.prepend(path1);
    return path2;
}

inline QString concatPaths(QString &&path1, QString &&path2)
{
    Q_ASSERT(!path2.startsWith(s_slash));

    if (path1.isEmpty()) {
        return path2;
    }

    appendSlash(path1);
    path1 += path2;
    return path1;
}

// mode_t
inline bool isRegFileMask(mode_t mode)
{
    return (mode & QT_STAT_MASK) == QT_STAT_REG;
}

inline bool isDirMask(mode_t mode)
{
    return (mode & QT_STAT_MASK) == QT_STAT_DIR;
}

inline bool isLinkMask(mode_t mode)
{
    return (mode & QT_STAT_MASK) == QT_STAT_LNK;
}

} // namespace

#endif // KIO_UTILS_P_H
