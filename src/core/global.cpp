/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "global.h"
#include "kioglobal_p.h"
#include "faviconscache_p.h"

#include <kprotocolinfo.h>
#include <KConfig>
#include <KConfigGroup>
#include <kfileitem.h>
#include <KLocalizedString>
#include <KSharedConfig>
#include <KFormat>
#include <KFileUtils>
#include <QMimeDatabase>
#include <QUrl>

#include "kiocoredebug.h"

KFormat::BinaryUnitDialect _k_loadBinaryDialect();
Q_GLOBAL_STATIC_WITH_ARGS(KFormat::BinaryUnitDialect, _k_defaultBinaryDialect, (_k_loadBinaryDialect()))

KFormat::BinaryUnitDialect _k_loadBinaryDialect()
{
    KConfigGroup mainGroup(KSharedConfig::openConfig(), "Locale");

    KFormat::BinaryUnitDialect dialect(KFormat::BinaryUnitDialect(mainGroup.readEntry("BinaryUnitDialect", int(KFormat::DefaultBinaryDialect))));
    dialect = static_cast<KFormat::BinaryUnitDialect>(mainGroup.readEntry("BinaryUnitDialect", int(dialect)));

    // Error checking
    if (dialect <= KFormat::DefaultBinaryDialect || dialect > KFormat::LastBinaryDialect) {
        dialect = KFormat::IECBinaryDialect;
    }

    return dialect;
}

KIOCORE_EXPORT QString KIO::convertSize(KIO::filesize_t fileSize)
{
    const KFormat::BinaryUnitDialect dialect = *_k_defaultBinaryDialect();

    return KFormat().formatByteSize(fileSize, 1, dialect);
}

KIOCORE_EXPORT QString KIO::convertSizeFromKiB(KIO::filesize_t kibSize)
{
    return convertSize(kibSize * 1024);
}

KIOCORE_EXPORT QString KIO::number(KIO::filesize_t size)
{
    char charbuf[256];
    sprintf(charbuf, "%lld", size);
    return QLatin1String(charbuf);
}

KIOCORE_EXPORT unsigned int KIO::calculateRemainingSeconds(KIO::filesize_t totalSize,
        KIO::filesize_t processedSize, KIO::filesize_t speed)
{
    if ((speed != 0) && (totalSize != 0)) {
        return (totalSize - processedSize) / speed;
    } else {
        return 0;
    }
}

KIOCORE_EXPORT QString KIO::convertSeconds(unsigned int seconds)
{
    unsigned int days  = seconds / 86400;
    unsigned int hours = (seconds - (days * 86400)) / 3600;
    unsigned int mins  = (seconds - (days * 86400) - (hours * 3600)) / 60;
    seconds            = (seconds - (days * 86400) - (hours * 3600) - (mins * 60));

    const QTime time(hours, mins, seconds);
    const QString timeStr(time.toString(QStringLiteral("hh:mm:ss")));
    if (days > 0) {
        return i18np("1 day %2", "%1 days %2", days, timeStr);
    } else {
        return timeStr;
    }
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(3, 4)
KIOCORE_EXPORT QTime KIO::calculateRemaining(KIO::filesize_t totalSize, KIO::filesize_t processedSize, KIO::filesize_t speed)
{
    QTime remainingTime;

    if (speed != 0) {
        KIO::filesize_t secs;
        if (totalSize == 0) {
            secs = 0;
        } else {
            secs = (totalSize - processedSize) / speed;
        }
        if (secs >= (24 * 60 * 60)) { // Limit to 23:59:59
            secs = (24 * 60 * 60) - 1;
        }
        int hr = secs / (60 * 60);
        int mn = (secs - hr * 60 * 60) / 60;
        int sc = (secs - hr * 60 * 60 - mn * 60);

        remainingTime.setHMS(hr, mn, sc);
    }

    return remainingTime;
}
#endif

KIOCORE_EXPORT QString KIO::itemsSummaryString(uint items, uint files, uint dirs, KIO::filesize_t size, bool showSize)
{
    if (files == 0 && dirs == 0 && items == 0) {
        return i18np("%1 Item", "%1 Items", 0);
    }

    QString summary;
    const QString foldersText = i18np("1 Folder", "%1 Folders", dirs);
    const QString filesText = i18np("1 File", "%1 Files", files);
    if (files > 0 && dirs > 0) {
        summary = showSize ?
                  i18nc("folders, files (size)", "%1, %2 (%3)", foldersText, filesText, KIO::convertSize(size)) :
                  i18nc("folders, files", "%1, %2", foldersText, filesText);
    } else if (files > 0) {
        summary = showSize ? i18nc("files (size)", "%1 (%2)", filesText, KIO::convertSize(size)) : filesText;
    } else if (dirs > 0) {
        summary = foldersText;
    }

    if (items > dirs + files) {
        const QString itemsText = i18np("%1 Item", "%1 Items", items);
        summary = summary.isEmpty() ? itemsText : i18nc("items: folders, files (size)", "%1: %2", itemsText, summary);
    }

    return summary;
}

KIOCORE_EXPORT QString KIO::encodeFileName(const QString &_str)
{
    QString str(_str);
    str.replace(QLatin1Char('/'), QChar(0x2044)); // "Fraction slash"
    return str;
}

KIOCORE_EXPORT QString KIO::decodeFileName(const QString &_str)
{
    // Nothing to decode. "Fraction slash" is fine in filenames.
    return _str;
}

/***************************************************************
 *
 * Utility functions
 *
 ***************************************************************/

KIO::CacheControl KIO::parseCacheControl(const QString &cacheControl)
{
    QString tmp = cacheControl.toLower();

    if (tmp == QLatin1String("cacheonly")) {
        return KIO::CC_CacheOnly;
    }
    if (tmp == QLatin1String("cache")) {
        return KIO::CC_Cache;
    }
    if (tmp == QLatin1String("verify")) {
        return KIO::CC_Verify;
    }
    if (tmp == QLatin1String("refresh")) {
        return KIO::CC_Refresh;
    }
    if (tmp == QLatin1String("reload")) {
        return KIO::CC_Reload;
    }

    qCDebug(KIO_CORE) << "unrecognized Cache control option:" << cacheControl;
    return KIO::CC_Verify;
}

QString KIO::getCacheControlString(KIO::CacheControl cacheControl)
{
    if (cacheControl == KIO::CC_CacheOnly) {
        return QStringLiteral("CacheOnly");
    }
    if (cacheControl == KIO::CC_Cache) {
        return QStringLiteral("Cache");
    }
    if (cacheControl == KIO::CC_Verify) {
        return QStringLiteral("Verify");
    }
    if (cacheControl == KIO::CC_Refresh) {
        return QStringLiteral("Refresh");
    }
    if (cacheControl == KIO::CC_Reload) {
        return QStringLiteral("Reload");
    }
    qCDebug(KIO_CORE) << "unrecognized Cache control enum value:" << cacheControl;
    return QString();
}

QString KIO::favIconForUrl(const QUrl &url)
{
    if (url.isLocalFile()
        || !url.scheme().startsWith(QLatin1String("http"))) {
        return QString();
    }

    return FavIconsCache::instance()->iconForUrl(url);
}

QString KIO::iconNameForUrl(const QUrl &url)
{
    if (url.scheme().isEmpty()) { // empty URL or relative URL (e.g. '~')
        return QStringLiteral("unknown");
    }
    QMimeDatabase db;
    const QMimeType mt = db.mimeTypeForUrl(url);
    QString iconName;

    if (url.isLocalFile()) {
        // Check to see whether it's an xdg location (e.g. Pictures folder)
        if (mt.inherits(QStringLiteral("inode/directory"))) {
            iconName = KIOPrivate::iconForStandardPath(url.toLocalFile());
        }

        // Let KFileItem::iconName handle things for us
        if (iconName.isEmpty()) {
            const KFileItem item(url, mt.name());
            iconName = item.iconName();
        }

    } else {
        // It's non-local and maybe on a slow filesystem

        // Look for a favicon
        if (url.scheme().startsWith(QLatin1String("http"))) {
            iconName = favIconForUrl(url);
        }

        // Then handle the trash
        else if (url.scheme() == QLatin1String("trash")) {
            if (url.path().length() <= 1) { // trash:/ itself
                KConfig trashConfig(QStringLiteral("trashrc"), KConfig::SimpleConfig);
                iconName = trashConfig.group("Status").readEntry("Empty", true) ?
                           QStringLiteral("user-trash") : QStringLiteral("user-trash-full");
            } else { // url.path().length() > 1, it's a file/folder under trash:/
                iconName = mt.iconName();
            }
        }

        // and other protocols
        if (iconName.isEmpty() && (mt.isDefault() || url.path().size() <= 1)) {
            iconName = KProtocolInfo::icon(url.scheme());
        }
    }
    // if we found nothing, return QMimeType.iconName()
    // (which fallbacks to "application-octet-stream" when no MIME type could be determined)
    return !iconName.isEmpty() ? iconName : mt.iconName();
}

QUrl KIO::upUrl(const QUrl &url)
{
    if (!url.isValid() || url.isRelative()) {
        return QUrl();
    }

    QUrl u(url);
    if (url.hasQuery()) {
        u.setQuery(QString());
        return u;
    }
    if (url.hasFragment()) {
        u.setFragment(QString());
    }
    u = u.adjusted(QUrl::StripTrailingSlash); /// don't combine with the line below
    return u.adjusted(QUrl::RemoveFilename);
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 61)
QString KIO::suggestName(const QUrl &baseURL, const QString &oldName)
{
    return KFileUtils::suggestName(baseURL, oldName);
}
#endif
