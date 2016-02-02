/*
 * This file is part of KIO.
 * Copyright 2016 David Faure <faure@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef FAVICONSCACHE_P_H
#define FAVICONSCACHE_P_H

#include <QObject>

#include <kiocore_export.h>

namespace KIO {

class FavIconsCachePrivate;

/**
 * @internal
 * Singleton handling the cache (memory + disk) for favicons.
 * Exported for KIOGui's FavIconsManager
 */
class KIOCORE_EXPORT FavIconsCache : public QObject
{
    Q_OBJECT

public:
    static FavIconsCache* instance();

    // Fast cache lookup, used by KIO::favIconForUrl
    QString iconForUrl(const QUrl &url);

    // Look for a custom icon URL in the cache, otherwise assemble default host icon URL
    QUrl iconUrlForUrl(const QUrl &url);

    // Remember association to a custom icon URL
    void setIconForUrl(const QUrl &url, const QUrl &iconUrl);

    QString cachePathForIconUrl(const QUrl &iconUrl) const;

    void ensureCacheExists();

    void addFailedDownload(const QUrl &url);
    void removeFailedDownload(const QUrl &url);
    bool isFailedDownload(const QUrl &url) const;

Q_SIGNALS:

private:
    FavIconsCache();
    ~FavIconsCache();
    FavIconsCachePrivate *d;
};

}

#endif // FAVICONSCACHE_H
