/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Matthias Hoelzer-Kluepfel <hoelzer@kde.org>
    SPDX-FileCopyrightText: 2001 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2003 Cornelius Schumacher <schumacher@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <config-help.h>

#include "kio_help.h"
#include "xslt_help.h"

#include <docbookxslt.h>

#include <KLocalizedString>

#include <QDebug>

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QUrl>

#include <libxslt/xsltutils.h>
#include <libxslt/transform.h>

using namespace KIO;

QString HelpProtocol::langLookup(const QString &fname)
{
    QStringList search;

    // assemble the local search paths
    const QStringList localDoc = KDocTools::documentationDirs();

    QStringList langs = KLocalizedString::languages();
    langs.append(QStringLiteral("en"));
    langs.removeAll(QStringLiteral("C"));

    // this is kind of compat hack as we install our docs in en/ but the
    // default language is en_US
    for (QString &lang : langs) {
        if (lang == QLatin1String("en_US")) {
            lang = QStringLiteral("en");
        }
    }

    // look up the different languages
    int ldCount = localDoc.count();
    search.reserve(ldCount * langs.size());
    for (int id = 0; id < ldCount; id++) {
        for (const QString &lang : qAsConst(langs)) {
            search.append(QStringLiteral("%1/%2/%3").arg(localDoc[id], lang, fname));
        }
    }

    // try to locate the file
    for (const QString &path : qAsConst(search)) {
        //qDebug() << "Looking for help in: " << path;

        QFileInfo info(path);
        if (info.exists() && info.isFile() && info.isReadable()) {
            return path;
        }

        if (path.endsWith(QLatin1String(".html"))) {
            const QString file = path.leftRef(path.lastIndexOf(QLatin1Char('/'))) + QLatin1String("/index.docbook");
            //qDebug() << "Looking for help in: " << file;
            info.setFile(file);
            if (info.exists() && info.isFile() && info.isReadable()) {
                return path;
            }
        }
    }

    return QString();
}

QString HelpProtocol::lookupFile(const QString &fname,
                                 const QString &query, bool &redirect)
{
    redirect = false;

    const QString &path = fname;

    QString result = langLookup(path);
    if (result.isEmpty()) {
        result = langLookup(path + QLatin1String("/index.html"));
        if (!result.isEmpty()) {
            QUrl red;
            red.setScheme(QStringLiteral("help"));
            red.setPath(path + QLatin1String("/index.html"));
            red.setQuery(query);
            redirection(red);
            //qDebug() << "redirect to " << red;
            redirect = true;
        } else {
            const QString documentationNotFound = QStringLiteral("kioslave5/help/documentationnotfound/index.html");
            if (!langLookup(documentationNotFound).isEmpty()) {
                QUrl red;
                red.setScheme(QStringLiteral("help"));
                red.setPath(documentationNotFound);
                red.setQuery(query);
                redirection(red);
                redirect = true;
            } else {
                sendError(i18n("There is no documentation available for %1.", path.toHtmlEscaped()));
                return QString();
            }
        }
    } else {
        //qDebug() << "result " << result;
    }

    return result;
}

void HelpProtocol::sendError(const QString &t)
{
    data(QStringLiteral(
         "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"></head>\n%1</html>").arg(t.toHtmlEscaped()).toUtf8());

}

HelpProtocol *slave = nullptr;

HelpProtocol::HelpProtocol(bool ghelp, const QByteArray &pool, const QByteArray &app)
    : SlaveBase(ghelp ? QByteArrayLiteral("ghelp") : QByteArrayLiteral("help"), pool, app), mGhelp(ghelp)
{
    slave = this;
}

void HelpProtocol::get(const QUrl &url)
{
    ////qDebug() << "path=" << url.path()
    //<< "query=" << url.query();

    bool redirect;
    QString doc = QDir::cleanPath(url.path());
    if (doc.contains(QLatin1String(".."))) {
        error(KIO::ERR_DOES_NOT_EXIST, url.toString());
        return;
    }

    if (!mGhelp) {
        if (!doc.startsWith(QLatin1Char('/'))) {
            doc.prepend(QLatin1Char('/'));
        }

        if (doc.endsWith(QLatin1Char('/'))) {
            doc += QLatin1String("index.html");
        }
    }

    infoMessage(i18n("Looking up correct file"));

    if (!mGhelp) {
        doc = lookupFile(doc, url.query(), redirect);

        if (redirect) {
            finished();
            return;
        }
    }

    if (doc.isEmpty()) {
        error(KIO::ERR_DOES_NOT_EXIST, url.toString());
        return;
    }

    QUrl target;
    target.setPath(doc);
    if (url.hasFragment()) {
        target.setFragment(url.fragment());
    }

    //qDebug() << "target " << target;

    QString file = target.isLocalFile() ? target.toLocalFile() : target.path();

    if (mGhelp) {
        if (!file.endsWith(QLatin1String(".xml"))) {
            get_file(file);
            return;
        }
    } else {
        const QString docbook_file = file.leftRef(file.lastIndexOf(QLatin1Char('/'))) + QLatin1String("/index.docbook");
        if (!QFile::exists(file)) {
            file = docbook_file;
        } else {
            QFileInfo fi(file);
            if (fi.isDir()) {
                file = file + QLatin1String("/index.docbook");
            } else {
                if (!file.endsWith(QLatin1String(".html")) || !compareTimeStamps(file, docbook_file)) {
                    get_file(file);
                    return;
                } else {
                    file = docbook_file;
                }
            }
        }
    }

    infoMessage(i18n("Preparing document"));
    mimeType(QStringLiteral("text/html"));

    if (mGhelp) {
        QString xsl = QStringLiteral("customization/kde-nochunk.xsl");
        mParsed = KDocTools::transform(file, KDocTools::locateFileInDtdResource(xsl));

        //qDebug() << "parsed " << mParsed.length();

        if (mParsed.isEmpty()) {
            sendError(i18n("The requested help file could not be parsed:<br />%1",  file));
        } else {
            int pos1 = mParsed.indexOf(QLatin1String("charset="));
            if (pos1 > 0) {
                int pos2 = mParsed.indexOf(QLatin1Char('"'), pos1);
                if (pos2 > 0) {
                    mParsed.replace(pos1, pos2 - pos1, QStringLiteral("charset=UTF-8"));
                }
            }
            data(mParsed.toUtf8());
        }
    } else {

        //qDebug() << "look for cache for " << file;

        mParsed = lookForCache(file);

        //qDebug() << "cached parsed " << mParsed.length();

        if (mParsed.isEmpty()) {
            mParsed = KDocTools::transform(file, KDocTools::locateFileInDtdResource(QStringLiteral("customization/kde-chunk.xsl")));
            if (!mParsed.isEmpty()) {
                infoMessage(i18n("Saving to cache"));
#ifdef Q_OS_WIN
                QFileInfo fi(file);
                // make sure filenames do not contain the base path, otherwise
                // accessing user data from another location invalids cached files
                // Accessing user data under a different path is possible
                // when using usb sticks - this may affect unix/mac systems also
                const QString installPath = KDocTools::documentationDirs().last();

                QString cache = QLatin1Char('/') + fi.absolutePath().remove(installPath, Qt::CaseInsensitive).replace(QLatin1Char('/'), QLatin1Char('_')) + QLatin1Char('_') + fi.baseName() + QLatin1Char('.');
#else
                QString cache = file.left(file.length() - 7);
#endif
                KDocTools::saveToCache(mParsed, QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                                       + QLatin1String("/kio_help") + cache + QLatin1String("cache.bz2"));
            }
        } else {
            infoMessage(i18n("Using cached version"));
        }

        //qDebug() << "parsed " << mParsed.length();

        if (mParsed.isEmpty()) {
            sendError(i18n("The requested help file could not be parsed:<br />%1",  file));
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
            if (anchor.isEmpty() && url.hasFragment()) {
                anchor = url.fragment();
            }

            //qDebug() << "anchor: " << anchor;

            if (!anchor.isEmpty()) {
                int index = 0;
                while (true) {
                    index = mParsed.indexOf(QStringLiteral("<a name="), index);
                    if (index == -1) {
                        //qDebug() << "no anchor\n";
                        break; // use whatever is the target, most likely index.html
                    }

                    if (mParsed.mid(index, 11 + anchor.length()).toLower() ==
                            QStringLiteral("<a name=\"%1\">").arg(anchor)) {
                        index = mParsed.lastIndexOf(QLatin1String("<FILENAME filename="), index) +
                                strlen("<FILENAME filename=\"");
                        QString filename = mParsed.mid(index, 2000);
                        filename = filename.left(filename.indexOf(QLatin1Char('\"')));
                        QString path = target.path();
                        path = path.leftRef(path.lastIndexOf(QLatin1Char('/')) + 1) + filename;
                        target.setPath(path);
                        //qDebug() << "anchor found in " << target;
                        break;
                    }
                    index++;
                }
            }
            emitFile(target);
        }
    }

    finished();
}

void HelpProtocol::emitFile(const QUrl &url)
{
    infoMessage(i18n("Looking up section"));

    QString filename = url.path().mid(url.path().lastIndexOf(QLatin1Char('/')) + 1);

    QByteArray result = KDocTools::extractFileToBuffer(mParsed, filename);

    if (result.isNull()) {
        sendError(i18n("Could not find filename %1 in %2.", filename, url.toString()));
    } else {
        data(result);
    }
    data(QByteArray());
}

void HelpProtocol::mimetype(const QUrl &)
{
    mimeType(QStringLiteral("text/html"));
    finished();
}

// Copied from kio_file to avoid redirects

#define MAX_IPC_SIZE (1024*32)

void HelpProtocol::get_file(const QString &path)
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
    mimeType(QMimeDatabase().mimeTypeForFile(path).name());
    int processed_size = 0;
    totalSize(f.size());

    char array[MAX_IPC_SIZE];

    Q_FOREVER {
        const qint64 n = f.read(array, sizeof(array));
        if (n == -1)
        {
            error(KIO::ERR_CANNOT_READ, path);
            return;
        }
        if (n == 0)
        {
            break;    // Finished
        }

        data(QByteArray::fromRawData(array, n));

        processed_size += n;
        processedSize(processed_size);
    }

    data(QByteArray());
    f.close();

    processedSize(f.size());
    finished();
}
