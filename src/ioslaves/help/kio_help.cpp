/* This file is part of the KDE libraries
   Copyright (C) 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
   Copyright (C) 2001 Stephan Kulow <coolo@kde.org>
   Copyright (C) 2003 Cornelius Schumacher <schumacher@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later versio

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/


#include <config-help.h>

#include "kio_help.h"
#include "xslt.h"
#include "xslt_help.h"

#include <QDebug>

#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>
#include <QtCore/QLocale>
#include <QtCore/QRegExp>
#include <QtCore/QTextCodec>
#include <QStandardPaths>
#include <QTextDocument>
#include <QUrl>

#include <libxslt/xsltutils.h>
#include <libxslt/transform.h>

using namespace KIO;

QString HelpProtocol::langLookup(const QString &fname)
{
    QStringList search;

    // assemble the local search paths
    const QStringList localDoc = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "doc/HTML", QStandardPaths::LocateDirectory);

    QStringList langs = QLocale().uiLanguages();
    langs.append( "en" );
    langs.removeAll( "C" );

    // this is kind of compat hack as we install our docs in en/ but the
    // default language is en_US
    for (QStringList::Iterator it = langs.begin(); it != langs.end(); ++it)
        if ( *it == "en_US" )
            *it = "en";

    // look up the different languages
    int ldCount = localDoc.count();
    for (int id=0; id < ldCount; id++)
    {
        QStringList::ConstIterator lang;
        for (lang = langs.constBegin(); lang != langs.constEnd(); ++lang)
            search.append(QString("%1%2/%3").arg(localDoc[id], *lang, fname));
    }

    // try to locate the file
    for (QStringList::ConstIterator it = search.constBegin(); it != search.constEnd(); ++it)
    {
        //qDebug() << "Looking for help in: " << *it;

        QFileInfo info(*it);
        if (info.exists() && info.isFile() && info.isReadable())
            return *it;

        if ( ( *it ).endsWith( QLatin1String(".html") ) )
        {
            QString file = (*it).left((*it).lastIndexOf('/')) + "/index.docbook";
            //qDebug() << "Looking for help in: " << file;
            info.setFile(file);
            if (info.exists() && info.isFile() && info.isReadable())
                return *it;
        }
    }


    return QString();
}


QString HelpProtocol::lookupFile(const QString &fname,
                                 const QString &query, bool &redirect)
{
    redirect = false;

    const QString path = fname;

    QString result = langLookup(path);
    if (result.isEmpty())
    {
        result = langLookup(path+"/index.html");
        if (!result.isEmpty())
        {
            QUrl red;
            red.setScheme("help");
            red.setPath( path + "/index.html" );
            red.setQuery( query );
            redirection(red);
            //qDebug() << "redirect to " << red;
            redirect = true;
        }
        else
        {
            const QString documentationNotFound = "khelpcenter/documentationnotfound/index.html";
            if (!langLookup(documentationNotFound).isEmpty())
            {
                QUrl red;
                red.setScheme("help");
                red.setPath(documentationNotFound);
                red.setQuery(query);
                redirection(red);
                redirect = true;
            }
            else
            {
                unicodeError( i18n("There is no documentation available for %1." , path.toHtmlEscaped()) );
                return QString();
            }
        }
    } else {
        //qDebug() << "result " << result;
    }

    return result;
}


void HelpProtocol::unicodeError( const QString &t )
{
#ifdef Q_OS_WIN
   QString encoding = "UTF-8";
#else
   QString encoding = QTextCodec::codecForLocale()->name();
#endif
   data(fromUnicode( QString(
        "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=%1\"></head>\n"
        "%2</html>" ).arg( encoding, t.toHtmlEscaped() ) ) );

}

HelpProtocol *slave = 0;

HelpProtocol::HelpProtocol( bool ghelp, const QByteArray &pool, const QByteArray &app )
  : SlaveBase( ghelp ? "ghelp" : "help", pool, app ), mGhelp( ghelp )
{
    slave = this;
}

void HelpProtocol::get( const QUrl& url )
{
    ////qDebug() << "path=" << url.path()
                   //<< "query=" << url.query();

    bool redirect;
    QString doc = QDir::cleanPath(url.path());
    if (doc.contains("..")) {
        error(KIO::ERR_DOES_NOT_EXIST, url.toString());
        return;
    }

    if ( !mGhelp ) {
        if (!doc.startsWith('/'))
            doc = doc.prepend(QLatin1Char('/'));

        if (doc.endsWith('/'))
            doc += "index.html";
    }

    infoMessage(i18n("Looking up correct file"));

    if ( !mGhelp ) {
      doc = lookupFile(doc, url.query(), redirect);

      if (redirect)
      {
          finished();
          return;
      }
    }

    if (doc.isEmpty())
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toString());
        return;
    }

    mimeType("text/html");
    QUrl target;
    target.setPath(doc);
    if (url.hasFragment())
        target.setFragment(url.fragment());

    //qDebug() << "target " << target;

    QString file = target.isLocalFile() ? target.toLocalFile() : target.path();

    if ( mGhelp ) {
      if ( !file.endsWith( QLatin1String( ".xml" ) ) ) {
         get_file(file);
         return;
      }
    } else {
        QString docbook_file = file.left(file.lastIndexOf('/')) + "/index.docbook";
        if (!QFile::exists(file)) {
            file = docbook_file;
        } else {
            QFileInfo fi(file);
            if (fi.isDir()) {
                file = file + "/index.docbook";
            } else {
                if ( !file.endsWith( QLatin1String( ".html" ) ) || !compareTimeStamps( file, docbook_file ) ) {
                    get_file(file);
                    return;
                } else
                    file = docbook_file;
            }
        }
    }

    infoMessage(i18n("Preparing document"));

    if ( mGhelp ) {
        QString xsl = "customization/kde-nochunk.xsl";
        mParsed = transform(file, locateFileInDtdResource(xsl));

        //qDebug() << "parsed " << mParsed.length();

        if (mParsed.isEmpty()) {
            unicodeError( i18n( "The requested help file could not be parsed:<br />%1" ,  file ) );
        } else {
            int pos1 = mParsed.indexOf( "charset=" );
            if ( pos1 > 0 ) {
              int pos2 = mParsed.indexOf( '"', pos1 );
              if ( pos2 > 0 ) {
                mParsed.replace( pos1, pos2 - pos1, "charset=UTF-8" );
              }
            }
            data( mParsed.toUtf8() );
        }
    } else {

        //qDebug() << "look for cache for " << file;

        mParsed = lookForCache( file );

        //qDebug() << "cached parsed " << mParsed.length();

        if ( mParsed.isEmpty() ) {
            mParsed = transform(file, locateFileInDtdResource("customization/kde-chunk.xsl"));
            if ( !mParsed.isEmpty() ) {
                infoMessage( i18n( "Saving to cache" ) );
#ifdef Q_OS_WIN
                QFileInfo fi(file);
                // make sure filenames do not contain the base path, otherwise
                // accessing user data from another location invalids cached files
                // Accessing user data under a different path is possible
                // when using usb sticks - this may affect unix/mac systems also
                const QString installPath = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "doc/HTML", QStandardPaths::LocateDirectory).last();

                QString cache = '/' + fi.absolutePath().remove(installPath,Qt::CaseInsensitive).replace('/','_') + '_' + fi.baseName() + '.';
#else
                QString cache = file.left( file.length() - 7 );
#endif
                saveToCache( mParsed, QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                                      + "/kio_help" + cache + "cache.bz2" );
            }
        } else infoMessage( i18n( "Using cached version" ) );

        //qDebug() << "parsed " << mParsed.length();

        if (mParsed.isEmpty()) {
            unicodeError( i18n( "The requested help file could not be parsed:<br />%1" ,  file ) );
        } else {
            QString anchor;
            QString query = url.query();

            // if we have a query, look if it contains an anchor
            if (!query.isEmpty())
                if (query.startsWith(QLatin1String("?anchor="))) {
                    anchor = query.mid(8).toLower();

			    QUrl redirURL(url);
			    redirURL.setQuery(QString());
			    redirURL.setFragment(anchor);
			    redirection(redirURL);
			    finished();
			    return;
		    }
            if (anchor.isEmpty() && url.hasFragment())
	        anchor = url.fragment();

            //qDebug() << "anchor: " << anchor;

            if ( !anchor.isEmpty() )
            {
                int index = 0;
                while ( true ) {
                    index = mParsed.indexOf( QRegExp( "<a name=" ), index);
                    if ( index == -1 ) {
                        //qDebug() << "no anchor\n";
                        break; // use whatever is the target, most likely index.html
                    }

                    if ( mParsed.mid( index, 11 + anchor.length() ).toLower() ==
                         QString( "<a name=\"%1\">" ).arg( anchor ) )
                    {
                        index = mParsed.lastIndexOf( "<FILENAME filename=", index ) +
                                 strlen( "<FILENAME filename=\"" );
                        QString filename=mParsed.mid( index, 2000 );
                        filename = filename.left( filename.indexOf( '\"' ) );
                        QString path = target.path();
                        path = path.left( path.lastIndexOf( '/' ) + 1) + filename;
                        target.setPath( path );
                        //qDebug() << "anchor found in " << target;
                        break;
                    }
                    index++;
                }
            }
            emitFile( target );
        }
    }

    finished();
}

void HelpProtocol::emitFile( const QUrl& url )
{
    infoMessage(i18n("Looking up section"));

    QString filename = url.path().mid(url.path().lastIndexOf('/') + 1);

    int index = mParsed.indexOf(QString("<FILENAME filename=\"%1\"").arg(filename));
    if (index == -1) {
        if ( filename == "index.html" ) {
            data( fromUnicode( mParsed ) );
            return;
        }

        unicodeError(i18n("Could not find filename %1 in %2.", filename, url.toString()));
        return;
    }

    QString filedata = splitOut(mParsed, index);
    replaceCharsetHeader( filedata );

    data( fromUnicode( filedata ) );
    data( QByteArray() );
}

void HelpProtocol::mimetype( const QUrl &)
{
    mimeType("text/html");
    finished();
}

// Copied from kio_file to avoid redirects

#define MAX_IPC_SIZE (1024*32)

void HelpProtocol::get_file(const QString& path)
{
    //qDebug() << path;

    QFile f(path);
    if (!f.exists()) {
        error(KIO::ERR_DOES_NOT_EXIST, path);
        return;
    }
    if (!f.open(QIODevice::ReadOnly) || f.isSequential() /*socket, fifo or pipe*/) {
        error(KIO::ERR_CANNOT_OPEN_FOR_READING, path);
        return;
    }
    int processed_size = 0;
    totalSize( f.size() );

    QByteArray array;
    array.resize(MAX_IPC_SIZE);

    Q_FOREVER
    {
        const qint64 n = f.read(array.data(), array.size());
        if (n == -1) {
            error( KIO::ERR_COULD_NOT_READ, path);
            return;
       }
       if (n == 0)
            break; // Finished

       data( array );

       processed_size += n;
       processedSize( processed_size );
    }

    data( QByteArray() );
    f.close();

    processedSize( f.size() );
    finished();
}
