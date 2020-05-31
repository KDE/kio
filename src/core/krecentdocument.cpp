/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#include "krecentdocument.h"

#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <KDesktopFile>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QRegularExpression>
#include <kio/global.h>
#include <QCoreApplication>
#include <QXmlStreamWriter>
#include <QMimeDatabase>
#include <QDateTime>
#include <QUrl>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QLockFile>
#include <qplatformdefs.h>

#include <KConfigGroup>
#include <KSharedConfig>

static QString xbelPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/recently-used.xbel");
}

static bool addToXbel(const QUrl &url, const QString &desktopEntryName)
{
    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qWarning() << "Failed to lock recently used";
        return false;
    }


    QByteArray existingContent;
    {
        QFile input(xbelPath());
        if (input.open(QIODevice::ReadOnly)) {
            existingContent = input.readAll();
        } else {
            qWarning() << "Failed to open existing recently used" << input.errorString();
        }
    }

    const QString newUrl = url.toString();

    const QString currentTimestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    QXmlStreamReader xml(existingContent);

    QSaveFile outputFile(xbelPath());
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to recently-used.xbel for writing:" << outputFile.errorString();
        return false;
    }

    QXmlStreamWriter output(&outputFile);
    output.setAutoFormatting(true);
    output.writeStartDocument();
    output.writeStartElement(QStringLiteral("xbel"));

    xml.readNextStartElement();
    if (xml.name() != QLatin1String("xbel")
            || xml.attributes().value(QLatin1String("version")) != QLatin1String("1.0")) {
        qWarning() << "The file is not an XBEL version 1.0 file.";
        return false;
    }
    output.writeAttributes(xml.attributes());

    for (const QXmlStreamNamespaceDeclaration &ns : xml.namespaceDeclarations()) {
        output.writeNamespace(ns.namespaceUri().toString(), ns.prefix().toString());
    }

    bool foundExisting = false;
    bool inRightBookmark = false;
    bool inRightApplications = false;
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("xbel")) {
            break;
        }
        switch(xml.tokenType()) {
        case QXmlStreamReader::StartElement: {
            QString tagName = xml.qualifiedName().toString();

            QXmlStreamAttributes attributes = xml.attributes();
            if (!foundExisting && xml.name() == QStringLiteral("bookmark") && attributes.value(QLatin1String("href")) == newUrl) {
                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : attributes) {
                    if (old.qualifiedName() == QLatin1String("count")) {
                        continue;
                    }
                    if (old.qualifiedName() == QLatin1String("modified")) {
                        continue;
                    }
                    if (old.qualifiedName() == QLatin1String("visited")) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(QLatin1String("modified"), currentTimestamp);
                newAttributes.append(QLatin1String("visited"), currentTimestamp);
                attributes = newAttributes;

                inRightBookmark = true;
            }
            if (inRightBookmark && tagName == QStringLiteral("bookmark:applications")) {
                inRightApplications = true;
            }

            // QT_NO_CAST_FROM_ASCII is premature optimization and kills readability, but that's just me
            if (inRightApplications && tagName == QStringLiteral("bookmark:application")  && attributes.value(QStringLiteral("name")) == desktopEntryName) {
                bool countOk;
                int count = attributes.value(QLatin1String("count")).toInt(&countOk);
                if (!countOk) {
                    count = 0;
                }

                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : attributes) {
                    if (old.qualifiedName() == QLatin1String("count")) {
                        continue;
                    }
                    if (old.qualifiedName() == QLatin1String("modified")) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(QLatin1String("count"), QString::number(count + 1));
                newAttributes.append(QLatin1String("modified"), currentTimestamp);
                attributes = newAttributes;

                foundExisting = true;
            }

            output.writeStartElement(tagName);
            output.writeAttributes(attributes);
            break;
        }
        case QXmlStreamReader::EndElement: {
            QString tagName = xml.qualifiedName().toString();
            if (tagName == QStringLiteral("bookmark:applications")) {
                inRightApplications = false;
            }
            if (tagName == QStringLiteral("bookmark")) {
                inRightBookmark = false;
            }
            output.writeEndElement();
            break;
        }
        case QXmlStreamReader::Characters:
            if (xml.isCDATA()) {
                output.writeCDATA(xml.text().toString());
            } else {
                output.writeCharacters(xml.text().toString());
            }
            break;
        case QXmlStreamReader::Comment:
            output.writeComment(xml.text().toString());
            break;
        case QXmlStreamReader::EndDocument:
            qWarning() << "Malformed, got end document before end of xbel" << xml.tokenString();
            return false;
        default:
            qWarning() << "unhandled token" << xml.tokenString();
            break;
        }
    }

    if (!foundExisting) {
        output.writeStartElement(QLatin1String("bookmark"));

        output.writeAttribute(QLatin1String("added"), currentTimestamp);
        output.writeAttribute(QLatin1String("modified"), currentTimestamp);
        output.writeAttribute(QLatin1String("visited"), currentTimestamp);
        output.writeAttribute(QLatin1String("href"), newUrl);

        {
            QMimeDatabase mimeDb;

            output.writeStartElement(QLatin1String("info"));
            output.writeStartElement(QLatin1String("metadata"));
            output.writeAttribute(QLatin1String("owner"), QLatin1String("http://freedesktop.org"));

            output.writeEmptyElement(QLatin1String("mime:mime-type"));
            output.writeAttribute(QLatin1String("type"), mimeDb.mimeTypeForUrl(url).name());

            {
                output.writeStartElement(QLatin1String("bookmark:applications"));
                output.writeEmptyElement(QLatin1String("bookmark:application"));
                output.writeAttribute(QLatin1String("name"), desktopEntryName);
                output.writeAttribute(QLatin1String("exec"), desktopEntryName);
                output.writeAttribute(QLatin1String("modified"), currentTimestamp);
                output.writeAttribute(QLatin1String("count"), QLatin1String("1"));
                output.writeEndElement();
            }

            output.writeEndElement();
            output.writeEndElement();
        }

        output.writeEndElement();
    }

    output.writeEndElement();

    output.writeEndDocument();

    if (!xml.error()) {
        outputFile.commit();
    }

    return !xml.error();
}

static QMap<QUrl, QDateTime> xbelRecentlyUsedList()
{
    QMap<QUrl, QDateTime> ret;
    QFile input(xbelPath());
    if (!input.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << input.fileName() << input.errorString();
        return ret;
    }

    QXmlStreamReader xml(&input);
    xml.readNextStartElement();
    if (xml.name() != QLatin1String("xbel")
            || xml.attributes().value(QLatin1String("version")) != QLatin1String("1.0")) {
        qWarning() << "The file is not an XBEL version 1.0 file.";
        return ret;
    }

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QStringLiteral("bookmark")) {
            continue;
        }
        const QString urlString = xml.attributes().value(QLatin1String("href")).toString();
        if (urlString.isEmpty()) {
            qWarning() << "Invalid bookmark in" << input.fileName();
            continue;
        }
        const QUrl url(urlString);
        if (url.isLocalFile() && !QFile(url.toLocalFile()).exists()) {
            continue;
        }
        const QDateTime modified = QDateTime::fromString(xml.attributes().value(QLatin1String("modified")).toString(), Qt::ISODate);
        const QDateTime visited = QDateTime::fromString(xml.attributes().value(QLatin1String("visited")).toString(), Qt::ISODate);
        const QDateTime added = QDateTime::fromString(xml.attributes().value(QLatin1String("added")).toString(), Qt::ISODate);
        if (modified > visited && modified > added) {
            ret[url] = modified;
        } else if (visited > added) {
            ret[url] = visited;
        } else {
            ret[url] = added;
        }

    }

    return ret;
}

QString KRecentDocument::recentDocumentDirectory()
{
    // need to change this path, not sure where
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/RecentDocuments/");
}

QList<QUrl> KRecentDocument::recentUrls()
{
    QMap<QUrl, QDateTime> documents = xbelRecentlyUsedList();
    for (const QString &pathDesktop : recentDocuments()) {
        const KDesktopFile tmpDesktopFile(pathDesktop);
        const QUrl url(tmpDesktopFile.desktopGroup().readPathEntry("URL", QString()));
        if (url.isEmpty()) {
            continue;
        }
        const QDateTime lastModified = QFileInfo(pathDesktop).lastModified();
        if (documents.contains(url) && documents[url] > lastModified) {
            continue;
        }
        documents[url] = lastModified;
    }
    QList<QUrl> ret = documents.keys();
    std::sort(ret.begin(), ret.end(), [&](const QUrl &doc1, const QUrl &doc2) {
        return documents[doc1] < documents[doc2];
    });

    return ret;
}

QStringList KRecentDocument::recentDocuments()
{
    QDir d(recentDocumentDirectory(), QStringLiteral("*.desktop"), QDir::Time, QDir::Files | QDir::Readable | QDir::Hidden);

    if (!d.exists()) {
        d.mkdir(recentDocumentDirectory());
    }

    const QStringList list = d.entryList();
    QStringList fullList;

    for (const QString &fileName : list) {
        QString pathDesktop;
        if (fileName.startsWith(QLatin1Char(':'))) {
            // See: https://bugreports.qt.io/browse/QTBUG-11223
            pathDesktop = KRecentDocument::recentDocumentDirectory() + fileName;
        } else {
            pathDesktop = d.absoluteFilePath(fileName);
        }
        KDesktopFile tmpDesktopFile(pathDesktop);
        QUrl urlDesktopFile(tmpDesktopFile.desktopGroup().readPathEntry("URL", QString()));
        if (urlDesktopFile.isLocalFile() && !QFile(urlDesktopFile.toLocalFile()).exists()) {
            d.remove(pathDesktop);
        } else {
            fullList.append(pathDesktop);
        }
    }

    return fullList;
}

void KRecentDocument::add(const QUrl &url)
{
    // desktopFileName is in QGuiApplication but we're in KIO Core here
    QString desktopEntryName = QCoreApplication::instance()->property("desktopFileName").toString();
    if (desktopEntryName.isEmpty()) {
        desktopEntryName = QCoreApplication::applicationName();
    }
    KRecentDocument::add(url, desktopEntryName);
    // ### componentName might not match the service filename...
}

void KRecentDocument::add(const QUrl &url, const QString &desktopEntryName)
{
    if (url.isLocalFile() && url.toLocalFile().startsWith(QDir::tempPath())) {
        return; // inside tmp resource, do not save
    }

    QString openStr = url.toDisplayString();
    openStr.replace(QRegularExpression(QStringLiteral("\\$")), QStringLiteral("$$")); // Desktop files with type "Link" are $-variable expanded

    // qDebug() << "KRecentDocument::add for " << openStr;
    KConfigGroup config = KSharedConfig::openConfig()->group(QByteArray("RecentDocuments"));
    bool useRecent = config.readEntry(QStringLiteral("UseRecent"), true);
    int maxEntries = config.readEntry(QStringLiteral("MaxEntries"), 10);

    if (!useRecent || maxEntries <= 0) {
        return;
    }

    const QString path = recentDocumentDirectory();
    const QString fileName = url.fileName();
    // don't create a file called ".desktop", it will lead to an empty name in kio_recentdocuments
    const QString dStr = path + (fileName.isEmpty() ? QStringLiteral("unnamed") : fileName);

    QString ddesktop = dStr + QLatin1String(".desktop");

    int i = 1;
    // check for duplicates
    while (QFile::exists(ddesktop)) {
        // see if it points to the same file and application
        KDesktopFile tmp(ddesktop);
        if (tmp.desktopGroup().readPathEntry("URL", QString()) == url.toDisplayString()
            && tmp.desktopGroup().readEntry("X-KDE-LastOpenedWith") == desktopEntryName) {
            // Set access and modification time to current time
            ::utime(QFile::encodeName(ddesktop).constData(), nullptr);
            return;
        }
        // if not append a (num) to it
        ++i;
        if (i > maxEntries) {
            break;
        }
        ddesktop = dStr + QStringLiteral("[%1].desktop").arg(i);
    }

    QDir dir(path);
    // check for max entries, delete oldest files if exceeded
    const QStringList list = dir.entryList(QDir::Files | QDir::Hidden, QFlags<QDir::SortFlag>(QDir::Time | QDir::Reversed));
    i = list.count();
    if (i > maxEntries - 1) {
        QStringList::ConstIterator it;
        it = list.begin();
        while (i > maxEntries - 1) {
            QFile::remove(dir.absolutePath() + QLatin1Char('/') + (*it));
            --i;
            ++it;
        }
    }

    // create the applnk
    KDesktopFile configFile(ddesktop);
    KConfigGroup conf = configFile.desktopGroup();
    conf.writeEntry("Type", QStringLiteral("Link"));
    conf.writePathEntry("URL", openStr);
    // If you change the line below, change the test in the above loop
    conf.writeEntry("X-KDE-LastOpenedWith", desktopEntryName);
    conf.writeEntry("Name", url.fileName());
    conf.writeEntry("Icon", KIO::iconNameForUrl(url));

    if (!addToXbel(url, desktopEntryName)) {
        qWarning() << "Failed to add to recently used bookmark file";
    }
}

void KRecentDocument::clear()
{
    const QStringList list = recentDocuments();
    QDir dir;
    for (const QString &desktopFilePath : list) {
        dir.remove(desktopFilePath);
    }
}

int KRecentDocument::maximumItems()
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("RecentDocuments"));
    return cg.readEntry(QStringLiteral("MaxEntries"), 10);
}
