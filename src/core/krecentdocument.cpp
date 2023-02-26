/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>
    SPDX-FileCopyrightText: 2021 Martin Tobias Holmedahl Sandsmark
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

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
#include <KService>
#include <QCoreApplication>
#include <QDir>
#include <QLockFile>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QXmlStreamWriter>
#include <kio/global.h>

#include <KConfigGroup>
#include <KSharedConfig>

static QString xbelPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/recently-used.xbel");
}

static inline QString stringForRecentDocumentGroup(int val)
{
    switch (val) {
    case KRecentDocument::RecentDocumentGroup::Development:
        return QStringLiteral("Development");
    case KRecentDocument::RecentDocumentGroup::Office:
        return QStringLiteral("Office");
    case KRecentDocument::RecentDocumentGroup::Database:
        return QStringLiteral("Database");
    case KRecentDocument::RecentDocumentGroup::Email:
        return QStringLiteral("Email");
    case KRecentDocument::RecentDocumentGroup::Presentation:
        return QStringLiteral("Presentation");
    case KRecentDocument::RecentDocumentGroup::Spreadsheet:
        return QStringLiteral("Spreadsheet");
    case KRecentDocument::RecentDocumentGroup::WordProcessor:
        return QStringLiteral("WordProcessor");
    case KRecentDocument::RecentDocumentGroup::Graphics:
        return QStringLiteral("Graphics");
    case KRecentDocument::RecentDocumentGroup::TextEditor:
        return QStringLiteral("TextEditor");
    case KRecentDocument::RecentDocumentGroup::Viewer:
        return QStringLiteral("Viewer");
    case KRecentDocument::RecentDocumentGroup::Archive:
        return QStringLiteral("Archive");
    case KRecentDocument::RecentDocumentGroup::Multimedia:
        return QStringLiteral("Multimedia");
    case KRecentDocument::RecentDocumentGroup::Audio:
        return QStringLiteral("Audio");
    case KRecentDocument::RecentDocumentGroup::Video:
        return QStringLiteral("Video");
    case KRecentDocument::RecentDocumentGroup::Photo:
        return QStringLiteral("Photo");
    case KRecentDocument::RecentDocumentGroup::Application:
        return QStringLiteral("Application");
    };
    Q_UNREACHABLE();
}

static KRecentDocument::RecentDocumentGroups groupsForMimeType(const QString mimeType)
{
    // simple heuristics, feel free to expand as needed
    if (mimeType.startsWith(QStringLiteral("image/"))) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Graphics};
    }
    if (mimeType.startsWith(QStringLiteral("video/"))) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Video};
    }
    if (mimeType.startsWith(QStringLiteral("audio/"))) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Audio};
    }
    return KRecentDocument::RecentDocumentGroups{};
}

static bool addToXbel(const QUrl &url, const QString &desktopEntryName, KRecentDocument::RecentDocumentGroups groups)
{
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));

    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return false;
    }

    QByteArray existingContent;
    QFile input(xbelPath());
    if (input.open(QIODevice::ReadOnly)) {
        existingContent = input.readAll();
    } else if (!input.exists()) { // That it doesn't exist is a very uncommon case
        qCDebug(KIO_CORE) << input.fileName() << "does not exist, creating new";
    } else {
        qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
        return false;
    }

    // Marginally more readable to avoid all the QStringLiteral() spam below
    static const QLatin1String xbelTag("xbel");
    static const QLatin1String versionAttribute("version");
    static const QLatin1String expectedVersion("1.0");

    static const QLatin1String applicationsBookmarkTag("bookmark:applications");
    static const QLatin1String applicationBookmarkTag("bookmark:application");
    static const QLatin1String bookmarkTag("bookmark");
    static const QLatin1String infoTag("info");
    static const QLatin1String metadataTag("metadata");
    static const QLatin1String mimeTypeTag("mime:mime-type");
    static const QLatin1String bookmarkGroups("bookmark:groups");
    static const QLatin1String bookmarkGroup("bookmark:group");

    static const QLatin1String nameAttribute("name");
    static const QLatin1String countAttribute("count");
    static const QLatin1String modifiedAttribute("modified");
    static const QLatin1String visitedAttribute("visited");
    static const QLatin1String hrefAttribute("href");
    static const QLatin1String addedAttribute("added");
    static const QLatin1String execAttribute("exec");
    static const QLatin1String ownerAttribute("owner");
    static const QLatin1String ownerValue("http://freedesktop.org");
    static const QLatin1String typeAttribute("type");

    QXmlStreamReader xml(existingContent);

    xml.readNextStartElement();
    if (!existingContent.isEmpty()) {
        if (xml.name().isEmpty() || xml.name() != xbelTag || !xml.attributes().hasAttribute(versionAttribute)) {
            qCDebug(KIO_CORE) << "The recently-used.xbel is not an XBEL file, overwriting.";
        } else if (xml.attributes().value(versionAttribute) != expectedVersion) {
            qCDebug(KIO_CORE) << "The recently-used.xbel is not an XBEL version 1.0 file but has version: " << xml.attributes().value(versionAttribute)
                              << ", overwriting.";
        }
    }

    QSaveFile outputFile(xbelPath());
    if (!outputFile.open(QIODevice::WriteOnly)) {
        qCWarning(KIO_CORE) << "Failed to recently-used.xbel for writing:" << outputFile.errorString();
        return false;
    }

    QXmlStreamWriter output(&outputFile);
    output.setAutoFormatting(true);
    output.setAutoFormattingIndent(2);
    output.writeStartDocument();
    output.writeStartElement(xbelTag);

    output.writeAttribute(versionAttribute, expectedVersion);
    output.writeNamespace(QStringLiteral("http://www.freedesktop.org/standards/desktop-bookmarks"), QStringLiteral("bookmark"));
    output.writeNamespace(QStringLiteral("http://www.freedesktop.org/standards/shared-mime-info"), QStringLiteral("mime"));

    const QString newUrl = QString::fromLatin1(url.toEncoded());
    const QString currentTimestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).chopped(1) + QStringLiteral("000Z");

    auto addApplicationTag = [&output, desktopEntryName, currentTimestamp]() {
        output.writeEmptyElement(applicationBookmarkTag);
        output.writeAttribute(nameAttribute, desktopEntryName);
        auto service = KService::serviceByDesktopName(desktopEntryName);
        if (service) {
            output.writeAttribute(execAttribute, service->exec() + QLatin1String(" %u"));
        } else {
            output.writeAttribute(execAttribute, QCoreApplication::instance()->applicationName() + QLatin1String(" %u"));
        }
        output.writeAttribute(modifiedAttribute, currentTimestamp);
        output.writeAttribute(countAttribute, QStringLiteral("1"));
    };

    bool foundExistingApp = false;
    bool inRightBookmark = false;
    bool foundMatchingBookmark = false;
    bool firstBookmark = true;
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::EndElement && xml.name() == xbelTag) {
            break;
        }
        switch (xml.tokenType()) {
        case QXmlStreamReader::StartElement: {
            QString tagName = xml.qualifiedName().toString();
            QXmlStreamAttributes attributes = xml.attributes();

            if (xml.name() == bookmarkTag) {
                foundExistingApp = false;
                firstBookmark = false;

                inRightBookmark = attributes.value(hrefAttribute) == newUrl;

                if (inRightBookmark) {
                    foundMatchingBookmark = true;

                    QXmlStreamAttributes newAttributes;
                    for (const QXmlStreamAttribute &old : attributes) {
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
                }
            }

            if (inRightBookmark && tagName == applicationBookmarkTag && attributes.value(nameAttribute) == desktopEntryName) {
                // case found right bookmark and same application
                const int count = attributes.value(countAttribute).toInt();

                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : std::as_const(attributes)) {
                    if (old.qualifiedName() == countAttribute) {
                        continue;
                    }
                    if (old.qualifiedName() == modifiedAttribute) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(modifiedAttribute, currentTimestamp);
                newAttributes.append(countAttribute, QString::number(count + 1));
                attributes = newAttributes;

                foundExistingApp = true;
            }

            output.writeStartElement(tagName);
            output.writeAttributes(attributes);
            break;
        }
        case QXmlStreamReader::EndElement: {
            QString tagName = xml.qualifiedName().toString();
            if (tagName == applicationsBookmarkTag && inRightBookmark && !foundExistingApp) {
                // add an application to the applications already known for the bookmark
                addApplicationTag();
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
            qCWarning(KIO_CORE) << "Malformed, got end document before end of xbel" << xml.tokenString() << url;
            return false;
        default:
            qCWarning(KIO_CORE) << "unhandled token" << xml.tokenString() << url;
            break;
        }
    }

    if (!foundMatchingBookmark) {
        // must create new bookmark tag
        if (firstBookmark) {
            output.writeCharacters(QStringLiteral("\n"));
        }
        output.writeCharacters(QStringLiteral("  "));
        output.writeStartElement(bookmarkTag);

        output.writeAttribute(hrefAttribute, newUrl);
        output.writeAttribute(addedAttribute, currentTimestamp);
        output.writeAttribute(modifiedAttribute, currentTimestamp);
        output.writeAttribute(visitedAttribute, currentTimestamp);

        {
            QMimeDatabase mimeDb;
            const auto fileMime = mimeDb.mimeTypeForUrl(url).name();

            output.writeStartElement(infoTag);
            output.writeStartElement(metadataTag);
            output.writeAttribute(ownerAttribute, ownerValue);

            output.writeEmptyElement(mimeTypeTag);
            output.writeAttribute(typeAttribute, fileMime);

            // write groups metadata
            if (groups.isEmpty()) {
                groups = groupsForMimeType(fileMime);
            }
            if (!groups.isEmpty()) {
                output.writeStartElement(bookmarkGroups);
                for (const auto &group : std::as_const(groups)) {
                    output.writeTextElement(bookmarkGroup, stringForRecentDocumentGroup(group));
                }
                // bookmarkGroups
                output.writeEndElement();
            }

            {
                output.writeStartElement(applicationsBookmarkTag);
                addApplicationTag();
                // end applicationsBookmarkTag
                output.writeEndElement();
            }

            // end infoTag
            output.writeEndElement();
            // end metadataTag
            output.writeEndElement();
        }

        // end bookmarkTag
        output.writeEndElement();
    }

    // end xbelTag
    output.writeEndElement();

    // end document
    output.writeEndDocument();

    return outputFile.commit();
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
    if (xml.name() != QLatin1String("xbel") || xml.attributes().value(QLatin1String("version")) != QLatin1String("1.0")) {
        qCWarning(KIO_CORE) << "The file is not an XBEL version 1.0 file.";
        return ret;
    }

    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() != QXmlStreamReader::StartElement || xml.name() != QLatin1String("bookmark")) {
            continue;
        }

        const auto urlString = xml.attributes().value(QLatin1String("href"));
        if (urlString.isEmpty()) {
            qCInfo(KIO_CORE) << "Invalid bookmark in" << input.fileName();
            continue;
        }
        const QUrl url = QUrl::fromEncoded(urlString.toLatin1());
        if (url.isLocalFile() && !QFile(url.toLocalFile()).exists()) {
            continue;
        }
        const auto attributes = xml.attributes();
        const QDateTime modified = QDateTime::fromString(attributes.value(QLatin1String("modified")).toString(), Qt::ISODate);
        const QDateTime visited = QDateTime::fromString(attributes.value(QLatin1String("visited")).toString(), Qt::ISODate);
        const QDateTime added = QDateTime::fromString(attributes.value(QLatin1String("added")).toString(), Qt::ISODate);
        if (modified > visited && modified > added) {
            ret[url] = modified;
        } else if (visited > added) {
            ret[url] = visited;
        } else {
            ret[url] = added;
        }
    }

    if (xml.hasError()) {
        qCWarning(KIO_CORE) << "Failed to read" << input.fileName() << xml.errorString();
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
    const auto recentDocs = recentDocuments();
    for (const QString &pathDesktop : recentDocs) {
        const KDesktopFile tmpDesktopFile(pathDesktop);
        const QUrl url(tmpDesktopFile.readUrl());
        if (url.isEmpty()) {
            continue;
        }
        const QDateTime lastModified = QFileInfo(pathDesktop).lastModified();
        const QDateTime documentLastModified = documents.value(url);
        if (documentLastModified.isValid() && documentLastModified > lastModified) {
            continue;
        }
        documents[url] = lastModified;
    }
    QList<QUrl> ret = documents.keys();
    std::sort(ret.begin(), ret.end(), [&](const QUrl &doc1, const QUrl &doc2) {
        return documents.value(doc1) < documents.value(doc2);
    });

    return ret;
}

QStringList KRecentDocument::recentDocuments()
{
    // TODO KF6: Consider deprecating this, also see the comment above in recentUrls()
    static const auto flags = QDir::Files | QDir::Readable | QDir::Hidden;
    QDir d(recentDocumentDirectory(), QStringLiteral("*.desktop"), QDir::Time, flags);

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
    add(url, RecentDocumentGroups());
}

void KRecentDocument::add(const QUrl &url, KRecentDocument::RecentDocumentGroups groups)
{
    // desktopFileName is in QGuiApplication but we're in KIO Core here
    QString desktopEntryName = QCoreApplication::instance()->property("desktopFileName").toString();
    if (desktopEntryName.isEmpty()) {
        desktopEntryName = QCoreApplication::applicationName();
    }
    add(url, desktopEntryName, groups);
}

void KRecentDocument::add(const QUrl &url, const QString &desktopEntryName)
{
    add(url, desktopEntryName, RecentDocumentGroups());
}

void KRecentDocument::add(const QUrl &url, const QString &desktopEntryName, KRecentDocument::RecentDocumentGroups groups)
{
    if (url.isLocalFile() && url.toLocalFile().startsWith(QDir::tempPath())) {
        return; // inside tmp resource, do not save
    }

    if (!addToXbel(url, desktopEntryName, groups)) {
        qCWarning(KIO_CORE) << "Failed to add to recently used bookmark file";
    }

    QString openStr = url.toDisplayString();
    openStr.replace(QRegularExpression(QStringLiteral("\\$")), QStringLiteral("$$")); // Desktop files with type "Link" are $-variable expanded

    // qDebug() << "KRecentDocument::add for " << openStr;
    KConfigGroup config = KSharedConfig::openConfig()->group(QByteArray("RecentDocuments"));
    bool useRecent = config.readEntry(QStringLiteral("UseRecent"), true);
    int maxEntries = config.readEntry(QStringLiteral("MaxEntries"), 10);
    bool ignoreHidden = config.readEntry(QStringLiteral("IgnoreHidden"), true);

    if (!useRecent || maxEntries <= 0) {
        return;
    }
    if (ignoreHidden && QRegularExpression(QStringLiteral("/\\.")).match(url.toLocalFile()).hasMatch()) {
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
}

void KRecentDocument::clear()
{
    const QStringList list = recentDocuments();
    QDir dir;
    for (const QString &desktopFilePath : list) {
        dir.remove(desktopFilePath);
    }
    QFile(xbelPath()).remove();
}

int KRecentDocument::maximumItems()
{
    KConfigGroup cg(KSharedConfig::openConfig(), QStringLiteral("RecentDocuments"));
    return cg.readEntry(QStringLiteral("MaxEntries"), 10);
}
