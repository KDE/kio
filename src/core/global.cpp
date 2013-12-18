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

#include <kprotocolinfo.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <qmimedatabase.h>
#include <QDBusInterface>
#include <QDBusReply>
#include <QUrl>
#include <QDebug>
#include <QHash>
#include <QLocale>
#include <QFileInfo>


enum BinaryUnitDialect {
    DefaultBinaryDialect = -1, ///< Used if no specific preference
    IECBinaryDialect,          ///< KDE Default, KiB, MiB, etc. 2^(10*n)
    JEDECBinaryDialect,        ///< KDE 3.5 default, KB, MB, etc. 2^(10*n)
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

    KConfig entryFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation, QLatin1String("locale/") + QString::fromLatin1("l10n/%1/entry.desktop").arg(QLocale::countryToString(QLocale().country()))));
    entryFile.setLocale(QLocale::languageToString(QLocale().language()));
    KConfigGroup entryGroup(&entryFile, "KCM Locale");

    BinaryUnitDialect dialect = (BinaryUnitDialect) entryGroup.readEntry("BinaryUnitDialect", int(IECBinaryDialect));
    dialect = (BinaryUnitDialect) mainGroup.readEntry("BinaryUnitDialect", int(dialect));

    // Error checking
    if (dialect <= DefaultBinaryDialect || dialect > LastBinaryDialect) {
        dialect = IECBinaryDialect;
    }

    return dialect;
}

QStringList _k_loadBinaryDialectUnits()
{
    BinaryUnitDialect dialect = *_k_defaultBinaryDialect();

    // Choose appropriate units.
    QList<QString> dialectUnits;

    dialectUnits << i18nc("size in bytes", "%1 B");

    switch (dialect) {
    case MetricBinaryDialect:
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 1000 bytes", "%1 kB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^6 bytes", "%1 MB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^9 bytes", "%1 GB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^12 bytes", "%1 TB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^15 bytes", "%1 PB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^18 bytes", "%1 EB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^21 bytes", "%1 ZB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 10^24 bytes", "%1 YB");
        break;

    case JEDECBinaryDialect:
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 1024 bytes", "%1 KB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^20 bytes", "%1 MB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^30 bytes", "%1 GB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^40 bytes", "%1 TB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^50 bytes", "%1 PB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^60 bytes", "%1 EB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^70 bytes", "%1 ZB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("memory size in 2^80 bytes", "%1 YB");
        break;

    case IECBinaryDialect:
    default:
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 1024 bytes", "%1 KiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^20 bytes", "%1 MiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^30 bytes", "%1 GiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^40 bytes", "%1 TiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^50 bytes", "%1 PiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^60 bytes", "%1 EiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^70 bytes", "%1 ZiB");
        // i18n: Dumb message, avoid any markup or scripting.
        dialectUnits << i18nc("size in 2^80 bytes", "%1 YiB");
        break;
    }

    return dialectUnits;
}

KIOCORE_EXPORT QString KIO::convertSize( KIO::filesize_t fileSize )
{
    const BinaryUnitDialect dialect = *_k_defaultBinaryDialect();
    const QStringList dialectUnits = *_k_defaultBinaryDialectUnits();
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

    if (unit == 0) {
        // Bytes, no rounding
        return dialectUnits[unit].arg(QLocale().toString(size, 'f', 0));
    } else {
        return dialectUnits[unit].arg(QLocale().toString(size, 'f', 1));
    }
}

KIOCORE_EXPORT QString KIO::convertSizeFromKiB( KIO::filesize_t kibSize )
{
    return convertSize(kibSize * 1024);
}

KIOCORE_EXPORT QString KIO::number( KIO::filesize_t size )
{
    char charbuf[256];
    sprintf(charbuf, "%lld", size);
    return QLatin1String(charbuf);
}

KIOCORE_EXPORT unsigned int KIO::calculateRemainingSeconds( KIO::filesize_t totalSize,
                                                        KIO::filesize_t processedSize, KIO::filesize_t speed )
{
  if ( (speed != 0) && (totalSize != 0) )
    return ( totalSize - processedSize ) / speed;
  else
    return 0;
}

KIOCORE_EXPORT QString KIO::convertSeconds( unsigned int seconds )
{
  unsigned int days  = seconds / 86400;
  unsigned int hours = (seconds - (days * 86400)) / 3600;
  unsigned int mins  = (seconds - (days * 86400) - (hours * 3600)) / 60;
  seconds            = (seconds - (days * 86400) - (hours * 3600) - (mins * 60));

  const QTime time(hours, mins, seconds);
  const QString timeStr( time.toString("hh:mm:ss") );
  if ( days > 0 )
    return i18np("1 day %2", "%1 days %2", days, timeStr);
  else
    return timeStr;
}

#ifndef KDE_NO_DEPRECATED
KIOCORE_EXPORT QTime KIO::calculateRemaining( KIO::filesize_t totalSize, KIO::filesize_t processedSize, KIO::filesize_t speed )
{
  QTime remainingTime;

  if ( speed != 0 ) {
    KIO::filesize_t secs;
    if ( totalSize == 0 ) {
      secs = 0;
    } else {
      secs = ( totalSize - processedSize ) / speed;
    }
    if (secs >= (24*60*60)) // Limit to 23:59:59
       secs = (24*60*60)-1;
    int hr = secs / ( 60 * 60 );
    int mn = ( secs - hr * 60 * 60 ) / 60;
    int sc = ( secs - hr * 60 * 60 - mn * 60 );

    remainingTime.setHMS( hr, mn, sc );
  }

  return remainingTime;
}
#endif

KIOCORE_EXPORT QString KIO::itemsSummaryString(uint items, uint files, uint dirs, KIO::filesize_t size, bool showSize)
{
    if ( files == 0 && dirs == 0 && items == 0 ) {
        return i18np( "%1 Item", "%1 Items", 0 );
    }

    QString summary;
    const QString foldersText = i18np( "1 Folder", "%1 Folders", dirs );
    const QString filesText = i18np( "1 File", "%1 Files", files );
    if ( files > 0 && dirs > 0 ) {
        summary = showSize ?
                  i18nc( "folders, files (size)", "%1, %2 (%3)", foldersText, filesText, KIO::convertSize( size ) ) :
                  i18nc( "folders, files", "%1, %2", foldersText, filesText );
    } else if ( files > 0 ) {
        summary = showSize ? i18nc( "files (size)", "%1 (%2)", filesText, KIO::convertSize( size ) ) : filesText;
    } else if ( dirs > 0 ) {
        summary = foldersText;
    }

    if ( items > dirs + files ) {
        const QString itemsText = i18np( "%1 Item", "%1 Items", items );
        summary = summary.isEmpty() ? itemsText : i18nc( "items: folders, files (size)", "%1: %2", itemsText, summary );
    }

    return summary;
}

KIOCORE_EXPORT QString KIO::encodeFileName( const QString & _str )
{
    QString str( _str );
    str.replace('/', QChar(0x2044)); // "Fraction slash"
    return str;
}

KIOCORE_EXPORT QString KIO::decodeFileName( const QString & _str )
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

  if (tmp == "cacheonly")
     return KIO::CC_CacheOnly;
  if (tmp == "cache")
     return KIO::CC_Cache;
  if (tmp == "verify")
     return KIO::CC_Verify;
  if (tmp == "refresh")
     return KIO::CC_Refresh;
  if (tmp == "reload")
     return KIO::CC_Reload;

  qDebug() << "unrecognized Cache control option:"<<cacheControl;
  return KIO::CC_Verify;
}

QString KIO::getCacheControlString(KIO::CacheControl cacheControl)
{
    if (cacheControl == KIO::CC_CacheOnly)
	return "CacheOnly";
    if (cacheControl == KIO::CC_Cache)
	return "Cache";
    if (cacheControl == KIO::CC_Verify)
	return "Verify";
    if (cacheControl == KIO::CC_Refresh)
	return "Refresh";
    if (cacheControl == KIO::CC_Reload)
	return "Reload";
    qDebug() << "unrecognized Cache control enum value:"<<cacheControl;
    return QString();
}

static bool useFavIcons()
{
    // this method will be called quite often, so better not read the config
    // again and again.
    static bool s_useFavIconsChecked = false;
    static bool s_useFavIcons = false;
    if (!s_useFavIconsChecked) {
        s_useFavIconsChecked = true;
        KConfigGroup cg( KSharedConfig::openConfig(), "HTML Settings" );
        s_useFavIcons = cg.readEntry("EnableFavicon", true);
    }
    return s_useFavIcons;
}

QString KIO::favIconForUrl(const QUrl& url)
{
    /* The kded module also caches favicons, for one week, without any way
     * to clean up the cache meanwhile.
     * On the other hand, this QHash will get cleaned up after 5000 request
     * (a selection in konsole of 80 chars generates around 500 requests)
     * or by simply restarting the application (or the whole desktop,
     * more likely, for the case of konqueror or konsole).
     */
    static QHash<QUrl, QString> iconNameCache;
    static int autoClearCache = 0;
    const QString notFound = QLatin1String("NOTFOUND");

    if (url.isLocalFile()
        || !url.scheme().startsWith(QLatin1String("http"))
        || !useFavIcons())
        return QString();

    QString iconNameFromCache = iconNameCache.value(url, notFound);
    if (iconNameFromCache != notFound) {
        if ((++autoClearCache) < 5000) {
            return iconNameFromCache;
        } else {
            iconNameCache.clear();
            autoClearCache = 0;
        }
    }

    QDBusInterface kded( QString::fromLatin1("org.kde.kded5"),
                         QString::fromLatin1("/modules/favicons"),
                         QString::fromLatin1("org.kde.FavIcon") );
    QDBusReply<QString> result = kded.call( QString::fromLatin1("iconForUrl"), url.toString() );
    iconNameCache.insert(url, result.value());
    return result;              // default is QString()
}

QString KIO::iconNameForUrl(const QUrl& url)
{
    QMimeDatabase db;
    const QMimeType mt = db.mimeTypeForUrl(url);
    const QLatin1String unknown("unknown");
    const QString mimeTypeIcon = mt.iconName();
    QString i = mimeTypeIcon;

    // if we don't find an icon, maybe we can use the one for the protocol
    if (i == unknown || i.isEmpty() || mt.isDefault()
        // and for the root of the protocol (e.g. trash:/) the protocol icon has priority over the mimetype icon
        || url.path().length() <= 1)
    {
        i = favIconForUrl(url); // maybe there is a favicon?

        if (i.isEmpty())
            i = KProtocolInfo::icon(url.scheme());

        // root of protocol: if we found nothing, revert to mimeTypeIcon (which is usually "folder")
        if (url.path().length() <= 1 && (i == unknown || i.isEmpty()))
            i = mimeTypeIcon;
    }
    return !i.isEmpty() ? i : unknown;
}

QUrl KIO::upUrl(const QUrl &url)
{
    if (!url.isValid() || url.isRelative())
        return QUrl();

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

QString KIO::suggestName(const QUrl &baseURL, const QString& oldName)
{
    QString dotSuffix, suggestedName;
    QString basename = oldName;
    const QChar spacer(' ');

    //ignore dots at the beginning, that way "..aFile.tar.gz" will become "..aFile 1.tar.gz" instead of " 1..aFile.tar.gz"
    int index = basename.indexOf('.');
    int continous = 0;
    while (continous == index) {
        index = basename.indexOf('.', index + 1);
        ++continous;
    }

    if (index != -1) {
        dotSuffix = basename.mid(index);
        basename.truncate(index);
    }

    int pos = basename.lastIndexOf(spacer);

    if (pos != -1) {
        QString tmp = basename.mid(pos + 1);
        bool ok;
        int number = tmp.toInt(&ok);

        if (!ok) {  // ok there is no number
            suggestedName = basename + spacer + '1' + dotSuffix;
        } else {
            // yes there's already a number behind the spacer so increment it by one
            basename.replace(pos + 1, tmp.length(), QString::number(number + 1));
            suggestedName = basename + dotSuffix;
        }
    } else // no spacer yet
        suggestedName = basename + spacer + "1" + dotSuffix ;

    // Check if suggested name already exists
    bool exists = false;
    // TODO: network transparency. However, using NetAccess from a modal dialog
    // could be a problem, no? (given that it uses a modal widget itself....)
    if (baseURL.isLocalFile())
        exists = QFileInfo(baseURL.toLocalFile() + '/' + suggestedName).exists();

    if (!exists)
        return suggestedName;
    else // already exists -> recurse
        return suggestName(baseURL, suggestedName);
}
