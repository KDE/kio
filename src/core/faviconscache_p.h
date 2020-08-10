/*
    This file is part of KIO.
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
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
    FavIconsCachePrivate * const d;
};

}

#endif // FAVICONSCACHE_H
