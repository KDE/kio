/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2008 Tobias Koenig <tokoe@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef DISCSPACEUTIL_H
#define DISCSPACEUTIL_H

#include <QString>

/*!
 * A small utility class to access and calculate
 * size and usage of mount points.
 */
class DiscSpaceUtil
{
public:
    /*!
     * Creates a new disc space util.
     *
     * \a directory A directory the util shall work on.
     */
    explicit DiscSpaceUtil(const QString &directory);

    /*!
     * Returns the usage of the directory pass in the constructor on this
     * mount point in percent.
     *
     * \a size The current size of the directory.
     */
    double usage(qint64 size) const;

    /*!
     * Returns the size of the partition in bytes.
     */
    qint64 size() const;

    /*!
     * Returns the mount point of the directory.
     */
    QString mountPoint() const;

    /*!
     * Returns the size of the given path in bytes.
     */
    static qint64 sizeOfPath(const QString &path);

private:
    qint64 mFullSize;
    QString mMountPoint;
};

#endif
