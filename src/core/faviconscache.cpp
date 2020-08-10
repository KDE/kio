/*
    This file is part of KIO.
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "faviconscache_p.h"

#include <KConfig>
#include <KConfigGroup>
#include <QMutex>

#include <QCache>
#include <QSet>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>

using namespace KIO;

static QString portForUrl(const QUrl &url)
{
    if (url.port() > 0) {
        return QLatin1Char('_') + QString::number(url.port());
    }
    return QString();
}

static QString simplifyUrl(const QUrl &url)
{
    // splat any = in the URL so it can be safely used as a config key
    QString result = url.host() + portForUrl(url) + url.path();
    result.replace(QLatin1Char('='), QLatin1Char('_'));
    while (result.endsWith(QLatin1Char('/'))) {
        result.chop(1);
    }
    return result;
}

static QString iconNameFromUrl(const QUrl &iconUrl)
{
    if (iconUrl.path() == QLatin1String("/favicon.ico")) {
        return iconUrl.host() + portForUrl(iconUrl);
    }

    QString result = simplifyUrl(iconUrl);
    // splat / so it can be safely used as a file name
    result.replace(QLatin1Char('/'), QLatin1Char('_'));

    const QStringRef ext = result.rightRef(4);
    if (ext == QLatin1String(".ico") || ext == QLatin1String(".png") || ext == QLatin1String(".xpm")) {
        result.chop(4);
    }

    return result;
}

////

class KIO::FavIconsCachePrivate
{
public:
    FavIconsCachePrivate()
    : cacheDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/favicons/")),
      config(cacheDir + QStringLiteral("index"))
    {}

    QString cachedIconUrlForUrl(const QUrl &url);

    const QString cacheDir;
    QMutex mutex; // protects all the member variables below
    KConfig config;
    QCache<QString, QString> faviconsCache;
    QSet<QUrl> failedDownloads;
};


QString FavIconsCachePrivate::cachedIconUrlForUrl(const QUrl &url)
{
    Q_ASSERT(!mutex.tryLock());
    const QString simplifiedUrl = simplifyUrl(url);
    QString *cachedIconUrl = faviconsCache[simplifiedUrl];
    return (cachedIconUrl ? *cachedIconUrl : config.group(QString()).readEntry(simplifiedUrl, QString()));
}

FavIconsCache *FavIconsCache::instance()
{
    static FavIconsCache s_cache; // remind me why we need Q_GLOBAL_STATIC, again, now that C++11 guarantees thread safety?
    return &s_cache;
}

FavIconsCache::FavIconsCache()
    : d(new FavIconsCachePrivate)
{
}

FavIconsCache::~FavIconsCache()
{
    delete d;
}

QString FavIconsCache::iconForUrl(const QUrl &url)
{
    if (url.host().isEmpty()) {
        return QString();
    }
    QMutexLocker locker(&d->mutex);
    const QString cachedIconUrl = d->cachedIconUrlForUrl(url);
    QString icon = d->cacheDir;
    if (!cachedIconUrl.isEmpty()) {
        icon += iconNameFromUrl(QUrl(cachedIconUrl));
    } else {
        icon += url.host();
    }
    icon += QStringLiteral(".png");
    if (QFile::exists(icon)) {
        return icon;
    }
    return QString();
}

QUrl FavIconsCache::iconUrlForUrl(const QUrl &url)
{
    QMutexLocker locker(&d->mutex);
    const QString cachedIconUrl = d->cachedIconUrlForUrl(url);
    if (!cachedIconUrl.isEmpty()) {
        return QUrl(cachedIconUrl);
    } else {
        QUrl iconUrl;
        iconUrl.setScheme(url.scheme());
        iconUrl.setHost(url.host());
        iconUrl.setPath(QStringLiteral("/favicon.ico"));
        iconUrl.setUserInfo(url.userInfo());
        return iconUrl;
    }
}

void FavIconsCache::setIconForUrl(const QUrl& url, const QUrl& iconUrl)
{
    QMutexLocker locker(&d->mutex);
    const QString simplifiedUrl = simplifyUrl(url);
    const QString iconUrlStr = iconUrl.url();
    d->faviconsCache.insert(simplifiedUrl, new QString(iconUrlStr));
    d->config.group(QString()).writeEntry(simplifiedUrl, iconUrlStr);
    d->config.sync();
}

QString FavIconsCache::cachePathForIconUrl(const QUrl &iconUrl) const
{
    QMutexLocker locker(&d->mutex);
    const QString iconName = iconNameFromUrl(iconUrl);
    return d->cacheDir + iconName  + QLatin1String(".png");
}

void FavIconsCache::ensureCacheExists()
{
    QMutexLocker locker(&d->mutex);
    QDir().mkpath(d->cacheDir);
}

void FavIconsCache::addFailedDownload(const QUrl &url)
{
    QMutexLocker locker(&d->mutex);
    d->failedDownloads.insert(url);
}

void FavIconsCache::removeFailedDownload(const QUrl &url)
{
    QMutexLocker locker(&d->mutex);
    d->failedDownloads.remove(url);
}

bool FavIconsCache::isFailedDownload(const QUrl &url) const
{
    QMutexLocker locker(&d->mutex);
    return d->failedDownloads.contains(url);
}
