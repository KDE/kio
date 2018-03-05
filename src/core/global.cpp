/* This file is part of the KDE libraries
   Copyright (C) 2000 David Faure <faure@kde.org>

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

#include "global.h"
#include "kioglobal_p.h"
#include "faviconscache_p.h"

#include <kprotocolinfo.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <qmimedatabase.h>
#include <QUrl>
#include <QLocale>
#include <QFileInfo>

#include "kiocoredebug.h"

enum BinaryUnitDialect {
    DefaultBinaryDialect = -1, ///< Used if no specific preference
    IECBinaryDialect,          ///< KiB, MiB, etc. 2^(10*n)
    JEDECBinaryDialect,        ///< KB, MB, etc. 2^(10*n)
    MetricBinaryDialect,       ///< SI Units, kB, MB, etc. 10^(3*n)
    LastBinaryDialect = MetricBinaryDialect
};

BinaryUnitDialect _k_loadBinaryDialect();
Q_GLOBAL_STATIC_WITH_ARGS(BinaryUnitDialect, _k_defaultBinaryDialect, (_k_loadBinaryDialect()))
QStringList _k_loadBinaryDialectUnits();
Q_GLOBAL_STATIC_WITH_ARGS(QStringList, _k_defaultBinaryDialectUnits, (_k_loadBinaryDialectUnits()))

BinaryUnitDialect _k_loadBinaryDialect()
{
    KConfigGroup mainGroup(KSharedConfig::openConfig(), "Locale");

    BinaryUnitDialect dialect(BinaryUnitDialect(mainGroup.readEntry("BinaryUnitDialect", int(DefaultBinaryDialect))));
    dialect = static_cast<BinaryUnitDialect>(mainGroup.readEntry("BinaryUnitDialect", int(dialect)));

    // Error checking
    if (dialect <= DefaultBinaryDialect || dialect > LastBinaryDialect) {
        dialect = IECBinaryDialect;
    }

    return dialect;
}

QStringList _k_loadBinaryDialectUnits()
{
    BinaryUnitDialect dialect = *_k_defaultBinaryDialect();
    // hack: i18nc expects all %-arguments to be replaced,
    // so just pass a argument placeholder again
    // That way just .arg(size) has to be called on the chosen unit string.
    const QString dummyArgument = QStringLiteral("%1");

    // Choose appropriate units.
    QStringList dialectUnits;
    dialectUnits.reserve(9);

    dialectUnits << i18nc("size in bytes", "%1 B", dummyArgument);

    switch (dialect) {
    case MetricBinaryDialect:
        dialectUnits << i18nc("size in 1000 bytes",  "%1 kB", dummyArgument)
                     << i18nc("size in 10^6 bytes",  "%1 MB", dummyArgument)
                     << i18nc("size in 10^9 bytes",  "%1 GB", dummyArgument)
                     << i18nc("size in 10^12 bytes", "%1 TB", dummyArgument)
                     << i18nc("size in 10^15 bytes", "%1 PB", dummyArgument)
                     << i18nc("size in 10^18 bytes", "%1 EB", dummyArgument)
                     << i18nc("size in 10^21 bytes", "%1 ZB", dummyArgument)
                     << i18nc("size in 10^24 bytes", "%1 YB", dummyArgument);
        break;

    case JEDECBinaryDialect:
        dialectUnits << i18nc("memory size in 1024 bytes", "%1 KB", dummyArgument)
                     << i18nc("memory size in 2^20 bytes", "%1 MB", dummyArgument)
                     << i18nc("memory size in 2^30 bytes", "%1 GB", dummyArgument)
                     << i18nc("memory size in 2^40 bytes", "%1 TB", dummyArgument)
                     << i18nc("memory size in 2^50 bytes", "%1 PB", dummyArgument)
                     << i18nc("memory size in 2^60 bytes", "%1 EB", dummyArgument)
                     << i18nc("memory size in 2^70 bytes", "%1 ZB", dummyArgument)
                     << i18nc("memory size in 2^80 bytes", "%1 YB", dummyArgument);
        break;

    case IECBinaryDialect:
    default:
        dialectUnits << i18nc("size in 1024 bytes", "%1 KiB", dummyArgument)
                     << i18nc("size in 2^20 bytes", "%1 MiB", dummyArgument)
                     << i18nc("size in 2^30 bytes", "%1 GiB", dummyArgument)
                     << i18nc("size in 2^40 bytes", "%1 TiB", dummyArgument)
                     << i18nc("size in 2^50 bytes", "%1 PiB", dummyArgument)
                     << i18nc("size in 2^60 bytes", "%1 EiB", dummyArgument)
                     << i18nc("size in 2^70 bytes", "%1 ZiB", dummyArgument)
                     << i18nc("size in 2^80 bytes", "%1 YiB", dummyArgument);
        break;
    }

    return dialectUnits;
}

KIOCORE_EXPORT QString KIO::convertSize(KIO::filesize_t fileSize)
{
    const BinaryUnitDialect dialect = *_k_defaultBinaryDialect();
    const QStringList &dialectUnits = *_k_defaultBinaryDialectUnits();
    double size = fileSize;
    int unit = 0; // Selects what unit to use from cached list
    double multiplier = 1024.0;

    if (dialect == MetricBinaryDialect) {
        multiplier = 1000.0;
    }

    while (qAbs(size) >= multiplier && unit < (dialectUnits.size() - 1)) {
        size /= multiplier;
        unit++;
    }

    const int precision = (unit == 0) ? 0 : 1; // unit == 0 -> Bytes, no rounding
    return dialectUnits.at(unit).arg(QLocale().toString(size, 'f', precision));
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

#ifndef KIOCORE_NO_DEPRECATED
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
    str.replace('/', QChar(0x2044)); // "Fraction slash"
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
    const QLatin1String unknown("unknown");
    if (url.isEmpty()) {
        return unknown;
    }
    QMimeDatabase db;
    const QMimeType mt = db.mimeTypeForUrl(url);
    const QString mimeTypeIcon = mt.iconName();
    QString i = mimeTypeIcon;

    // check whether it's a xdg location (e.g. Pictures folder)
    if (url.isLocalFile() && mt.inherits(QLatin1String("inode/directory"))) {
        i = KIOPrivate::iconForStandardPath(url.toLocalFile());
    }

    // if we don't find an icon, maybe we can use the one for the protocol
    if (i == unknown || i.isEmpty() || mt.isDefault()
            // and for the root of the protocol (e.g. trash:/) the protocol icon has priority over the mimetype icon
            || url.path().length() <= 1) {
        i = favIconForUrl(url); // maybe there is a favicon?

        // reflect actual fill state of trash can
        if (url.scheme() == QLatin1String("trash") && url.path().length() <= 1) {
            KConfig trashConfig(QStringLiteral("trashrc"), KConfig::SimpleConfig);
            if (trashConfig.group("Status").readEntry("Empty", true)) {
                i = QStringLiteral("user-trash");
            } else {
                i = QStringLiteral("user-trash-full");
            }
        }

        if (i.isEmpty()) {
            i = KProtocolInfo::icon(url.scheme());
        }

        // root of protocol: if we found nothing, revert to mimeTypeIcon (which is usually "folder")
        if (url.path().length() <= 1 && (i == unknown || i.isEmpty())) {
            i = mimeTypeIcon;
        }
    }
    return !i.isEmpty() ? i : unknown;
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

QString KIO::suggestName(const QUrl &baseURL, const QString &oldName)
{
    QString basename;

    // Extract the original file extension from the filename
    QMimeDatabase db;
    QString nameSuffix = db.suffixForFileName(oldName);

    if (oldName.lastIndexOf('.') == 0) {
        basename = QStringLiteral(".");
        nameSuffix = oldName;
    } else if (nameSuffix.isEmpty()) {
        const int lastDot = oldName.lastIndexOf('.');
        if (lastDot == -1) {
            basename = oldName;
        } else {
            basename = oldName.left(lastDot);
            nameSuffix = oldName.mid(lastDot);
        }
    } else {
        nameSuffix.prepend('.');
        basename = oldName.left(oldName.length() - nameSuffix.length());
    }

    // check if (number) exists from the end of the oldName and increment that number
    QRegExp numSearch(QStringLiteral("\\(\\d+\\)"));
    int start = numSearch.lastIndexIn(oldName);
    if (start != -1) {
        QString numAsStr = numSearch.cap(0);
        QString number = QString::number(numAsStr.midRef(1, numAsStr.size() - 2).toInt() + 1);
        basename = basename.left(start) + '(' + number + ')';
    } else {
        // number does not exist, so just append " (1)" to filename
        basename += QLatin1String(" (1)");
    }
    const QString suggestedName = basename + nameSuffix;

    // Check if suggested name already exists
    bool exists = false;

    // TODO: network transparency. However, using NetAccess from a modal dialog
    // could be a problem, no? (given that it uses a modal widget itself....)
    if (baseURL.isLocalFile()) {
        exists = QFileInfo(baseURL.toLocalFile() + '/' + suggestedName).exists();
    }

    if (!exists) {
        return suggestedName;
    } else { // already exists -> recurse
        return suggestName(baseURL, suggestedName);
    }
}
