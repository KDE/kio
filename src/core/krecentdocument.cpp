/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>

    SPDX-License-Identifier: BSD-2-Clause
*/

#include "krecentdocument.h"

#include "kiocoredebug.h"

#ifdef Q_OS_WIN
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <KDesktopFile>
#include <QCoreApplication>
#include <QDebug>
#include <kio/global.h>
#include <kdesktopfile.h>
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
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return false;
    }

    QByteArray existingContent;
    {
        QFile input(xbelPath());
        if (input.exists() && input.open(QIODevice::ReadOnly)) {
            existingContent = input.readAll();
        } else if (!input.exists()) { // That it doesn't exist is a very uncommon case
            qCDebug(KIO_CORE) << input.fileName() << "does not exist, creating new";
        } else {
            qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
            return false;
        }
    }

    const QString newUrl = url.toString();

    const QString currentTimestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    QXmlStreamReader xml(existingContent);

    QSaveFile outputFile(xbelPath());
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qCWarning(KIO_CORE) << "Failed to recently-used.xbel for writing:" << outputFile.errorString();
        return false;
    }

    // Marginally more readable to avoid all the QStringLiteral() spam below
    static const QLatin1String xbelTag("xbel");
    static const QLatin1String versionAttribute("version");
    static const QLatin1String expectedVersion("1.0");

    QXmlStreamWriter output(&outputFile);
    output.setAutoFormatting(true);
    output.writeStartDocument();
    output.writeStartElement(xbelTag);

    xml.readNextStartElement();
    if (xml.name() != xbelTag
            || xml.attributes().value(versionAttribute) != expectedVersion) {
        qCWarning(KIO_CORE) << "The file is not an XBEL version 1.0 file.";
        return false;
    }
    output.writeAttributes(xml.attributes());

    for (const QXmlStreamNamespaceDeclaration &ns : xml.namespaceDeclarations()) {
        output.writeNamespace(ns.namespaceUri().toString(), ns.prefix().toString());
    }

    // Actually parse the old XML
    static const QLatin1String applicationsBookmarkTag("bookmark:applications");
    static const QLatin1String applicationBookmarkTag("bookmark:application");
    static const QLatin1String bookmarkTag("bookmark");
    static const QLatin1String nameAttribute("name");
    static const QLatin1String countAttribute("count");
    static const QLatin1String modifiedAttribute("modified");
    static const QLatin1String visitedAttribute("visited");
    static const QLatin1String hrefAttribute("href");

    bool foundExisting = false;
    bool inRightBookmark = false;
    bool inRightApplications = false;
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::EndElement && xml.name() == xbelTag) {
            break;
        }
        switch(xml.tokenType()) {
        case QXmlStreamReader::StartElement: {
            QString tagName = xml.qualifiedName().toString();

            QXmlStreamAttributes attributes = xml.attributes();
            if (!foundExisting && xml.name() == bookmarkTag && attributes.value(hrefAttribute) == newUrl) {
                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : attributes) {
                    if (old.qualifiedName() == countAttribute) {
                        continue;
                    }
                    if (old.qualifiedName() == modifiedAttribute) {
                        continue;
                    }
                    if (old.qualifiedName() == visitedAttribute) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(modifiedAttribute, currentTimestamp);
                newAttributes.append(visitedAttribute, currentTimestamp);
                attributes = newAttributes;

                inRightBookmark = true;
            }
            if (inRightBookmark && tagName == applicationBookmarkTag) {
                inRightApplications = true;
            }

            if (inRightApplications && tagName == applicationBookmarkTag && attributes.value(nameAttribute) == desktopEntryName) {
                bool countOk;
                int count = attributes.value(countAttribute).toInt(&countOk);
                if (!countOk) {
                    count = 0;
                }

                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : attributes) {
                    if (old.qualifiedName() == countAttribute) {
                        continue;
                    }
                    if (old.qualifiedName() == modifiedAttribute) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(countAttribute, QString::number(count + 1));
                newAttributes.append(modifiedAttribute, currentTimestamp);
                attributes = newAttributes;

                foundExisting = true;
            }

            output.writeStartElement(tagName);
            output.writeAttributes(attributes);
            break;
        }
        case QXmlStreamReader::EndElement: {
            QString tagName = xml.qualifiedName().toString();
            if (tagName == applicationsBookmarkTag) {
                inRightApplications = false;
            }
            if (tagName == bookmarkTag) {
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
            qCWarning(KIO_CORE) << "Malformed, got end document before end of xbel" << xml.tokenString();
            return false;
        default:
            qCWarning(KIO_CORE) << "unhandled token" << xml.tokenString();
            break;
        }
    }

    static const QLatin1String infoTag("info");
    static const QLatin1String metadataTag("metadata");
    static const QLatin1String addedAttribute("added");
    static const QLatin1String execAttribute("added");
    static const QLatin1String ownerAttribute("owner");
    static const QLatin1String ownerValue("http://freedesktop.org");
    static const QLatin1String mimeTypeTag("mime:mime-type");
    static const QLatin1String typeAttribute("type");
    static const QLatin1String defaultCountValue("1");

    if (!foundExisting) {
        output.writeStartElement(bookmarkTag);

        output.writeAttribute(addedAttribute, currentTimestamp);
        output.writeAttribute(modifiedAttribute, currentTimestamp);
        output.writeAttribute(visitedAttribute, currentTimestamp);
        output.writeAttribute(hrefAttribute, newUrl);

        {
            QMimeDatabase mimeDb;

            output.writeStartElement(infoTag);
            output.writeStartElement(metadataTag);
            output.writeAttribute(ownerAttribute, ownerValue);

            output.writeEmptyElement(mimeTypeTag);
            output.writeAttribute(typeAttribute, mimeDb.mimeTypeForUrl(url).name());

            {
                output.writeStartElement(applicationsBookmarkTag);
                output.writeEmptyElement(applicationBookmarkTag);
                output.writeAttribute(nameAttribute, desktopEntryName);
                output.writeAttribute(execAttribute, desktopEntryName);
                output.writeAttribute(modifiedAttribute, currentTimestamp);
                output.writeAttribute(countAttribute, defaultCountValue);
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
        qCWarning(KIO_CORE) << "Failed to open" << input.fileName() << input.errorString();
        return ret;
    }

    QXmlStreamReader xml(&input);
    xml.readNextStartElement();
    if (xml.name() != QLatin1String("xbel")
            || xml.attributes().value(QLatin1String("version")) != QLatin1String("1.0")) {
        qCWarning(KIO_CORE) << "The file is not an XBEL version 1.0 file.";
        return ret;
    }

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QStringLiteral("bookmark")) {
            continue;
        }
        const QString urlString = xml.attributes().value(QLatin1String("href")).toString();
        if (urlString.isEmpty()) {
            qCWarning(KIO_CORE) << "Invalid bookmark in" << input.fileName();
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

    // TODO KF6: Revisit if we should still continue to fetch the old recentDocuments()
    // We need to do it to be compatible with older versions of ourselves, and
    // possibly others who for some reason did the same as us, but it could
    // possibly also be done as a one-time migration.
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
    // TODO KF6: Consider deprecating this, also see the comment above in recentUrls()
    QDir d(recentDocumentDirectory(), QStringLiteral("*.desktop"), QDir::Time,
           QDir::Files | QDir::Readable | QDir::Hidden);

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
        qCWarning(KIO_CORE) << "Failed to add to recently used bookmark file";
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
