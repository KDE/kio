#include "xslt_help.h"
#include "xslt.h"

#include <libxslt/xsltconfig.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxml/xmlIO.h>
#include <libxml/parserInternals.h>
#include <libxml/catalog.h>

#include <QtCore/QDate>
#include <QtCore/QDir>
#include <QtCore/QRegExp>
#include <QtCore/QStandardPaths>

#include <QDebug>
#include <kfilterdev.h>

static bool readCache( const QString &filename,
                       const QString &cache, QString &output)
{
    //qDebug() << filename << cache;
    if ( !compareTimeStamps( filename, cache ) )
        return false;
    if ( !compareTimeStamps(locateFileInDtdResource("customization/kde-chunk.xsl"), cache))
        return false;

    //qDebug() << "create filter";
    KFilterDev fd(cache);

    if (!fd.open(QIODevice::ReadOnly))
    {
       QFile::remove(cache);
       return false;
    }

    //qDebug() << "reading";

    char buffer[32000];
    int n;
    QByteArray text;
    // Also end loop in case of error, when -1 is returned
    while ( ( n = fd.read(buffer, 31900) ) > 0)
    {
        buffer[n] = 0;
        text += buffer;
    }
    //qDebug() << "read " << text.length();
    fd.close();

    output = QString::fromUtf8( text );

    if (n == -1)
        return false;

    //qDebug() << "finished ";

    return true;
}

QString lookForCache( const QString &filename )
{
    //qDebug() << "lookForCache" << filename;
    Q_ASSERT( filename.endsWith( QLatin1String(".docbook") ) );
    Q_ASSERT( QDir::isAbsolutePath(filename));
    QString cache = filename.left( filename.length() - 7 );
    QString output;
    if ( readCache( filename, cache + "cache.bz2", output) )
        return output;
#ifdef Q_OS_WIN
    QFileInfo fi(filename);
    // make sure filenames do not contain the base path, otherwise
    // accessing user data from another location invalids cached files.
    // Accessing user data under a different path is possible
    // when using usb sticks - this may affect unix/mac systems also
    const QString installPath = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "doc/HTML", QStandardPaths::LocateDirectory).last();
    cache = '/' + fi.absolutePath().remove(installPath,Qt::CaseInsensitive).replace('/','_') + '_' + fi.baseName() + '.';
#endif
    if ( readCache( filename,
                    QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QLatin1Char('/') + "kio_help" + cache + "cache.bz2", output ) )
        return output;

    return QString();
}

bool compareTimeStamps( const QString &older, const QString &newer )
{
    QFileInfo _older( older );
    QFileInfo _newer( newer );
    Q_ASSERT( _older.exists() );
    if ( !_newer.exists() )
        return false;
    return ( _newer.lastModified() > _older.lastModified() );
}
