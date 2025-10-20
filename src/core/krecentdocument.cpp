/* -*- c++ -*-
    SPDX-FileCopyrightText: 2000 Daniel M. Duley <mosfet@kde.org>
    SPDX-FileCopyrightText: 2021 Martin Tobias Holmedahl Sandsmark
    SPDX-FileCopyrightText: 2022 Méven Car <meven.car@kdemail.net>

    SPDX-License-Identifier: BSD-2-Clause
*/

#include "krecentdocument.h"

#include "kiocoredebug.h"

#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QLockFile>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QXmlStreamWriter>

#include <KConfigGroup>
#include <KService>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

static QString xbelPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/recently-used.xbel");
}

static inline QString stringForRecentDocumentGroup(int val)
{
    switch (val) {
    case KRecentDocument::RecentDocumentGroup::Development:
        return "Development"_L1;
    case KRecentDocument::RecentDocumentGroup::Office:
        return "Office"_L1;
    case KRecentDocument::RecentDocumentGroup::Database:
        return "Database"_L1;
    case KRecentDocument::RecentDocumentGroup::Email:
        return "Email"_L1;
    case KRecentDocument::RecentDocumentGroup::Presentation:
        return "Presentation"_L1;
    case KRecentDocument::RecentDocumentGroup::Spreadsheet:
        return "Spreadsheet"_L1;
    case KRecentDocument::RecentDocumentGroup::WordProcessor:
        return "WordProcessor"_L1;
    case KRecentDocument::RecentDocumentGroup::Graphics:
        return "Graphics"_L1;
    case KRecentDocument::RecentDocumentGroup::TextEditor:
        return "TextEditor"_L1;
    case KRecentDocument::RecentDocumentGroup::Viewer:
        return "Viewer"_L1;
    case KRecentDocument::RecentDocumentGroup::Archive:
        return "Archive"_L1;
    case KRecentDocument::RecentDocumentGroup::Multimedia:
        return "Multimedia"_L1;
    case KRecentDocument::RecentDocumentGroup::Audio:
        return "Audio"_L1;
    case KRecentDocument::RecentDocumentGroup::Video:
        return "Video"_L1;
    case KRecentDocument::RecentDocumentGroup::Photo:
        return "Photo"_L1;
    case KRecentDocument::RecentDocumentGroup::Application:
        return "Application"_L1;
    };
    Q_UNREACHABLE();
}

static KRecentDocument::RecentDocumentGroups groupsForMimeType(const QString mimeType)
{
    // simple heuristics, feel free to expand as needed
    if (mimeType.startsWith("image/"_L1)) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Graphics};
    }
    if (mimeType.startsWith("video/"_L1)) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Video};
    }
    if (mimeType.startsWith("audio/"_L1)) {
        return KRecentDocument::RecentDocumentGroups{KRecentDocument::RecentDocumentGroup::Audio};
    }
    return KRecentDocument::RecentDocumentGroups{};
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

static bool removeOldestEntries(int &maxEntries)
{
    QFile input(xbelPath());
    if (!input.exists()) {
        return true;
    }

    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return false;
    }

    if (!input.open(QIODevice::ReadOnly)) {
        qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
        return false;
    }

    QDomDocument document;
    document.setContent(&input);
    input.close();

    auto xbelTags = document.elementsByTagName(xbelTag);
    if (xbelTags.length() != 1) {
        qCWarning(KIO_CORE) << "Invalid Xbel file, missing xbel element";
        return false;
    }
    auto xbelElement = xbelTags.item(0);
    auto bookmarkList = xbelElement.childNodes();
    if (bookmarkList.length() <= maxEntries) {
        return true;
    }

    QMultiMap<QDateTime, QDomNode> bookmarksByModifiedDate;
    for (int i = 0; i < bookmarkList.length(); ++i) {
        const auto node = bookmarkList.item(i);
        const auto modifiedString = node.attributes().namedItem(modifiedAttribute);
        const auto modifiedTime = QDateTime::fromString(modifiedString.nodeValue(), Qt::ISODate);

        bookmarksByModifiedDate.insert(modifiedTime, node);
    }

    int i = 0;
    // entries are traversed in ascending key order
    for (auto entry = bookmarksByModifiedDate.keyValueBegin(); entry != bookmarksByModifiedDate.keyValueEnd(); ++entry) {
        // only keep the maxEntries last nodes
        if (bookmarksByModifiedDate.size() - i > maxEntries) {
            xbelElement.removeChild(entry->second);
        }
        ++i;
    }

    if (input.open(QIODevice::WriteOnly) && input.write(document.toByteArray(2)) != -1) {
        return true;
    }
    return false;
}

static bool addToXbel(const QUrl &url, const QString &desktopEntryName, KRecentDocument::RecentDocumentGroups groups, int maxEntries, bool ignoreHidden)
{
    if (!QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))) {
        qCWarning(KIO_CORE) << "Could not create GenericDataLocation";
        return false;
    }

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
    output.writeNamespace("http://www.freedesktop.org/standards/desktop-bookmarks"_L1, bookmarkTag);
    output.writeNamespace("http://www.freedesktop.org/standards/shared-mime-info"_L1, "mime"_L1);

    const QString newUrl = QString::fromLatin1(url.toEncoded());
    const QString currentTimestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).chopped(1) + "000Z"_L1;

    auto addApplicationTag = [&output, desktopEntryName, currentTimestamp, url]() {
        output.writeEmptyElement(applicationBookmarkTag);
        output.writeAttribute(nameAttribute, desktopEntryName);
        auto service = KService::serviceByDesktopName(desktopEntryName);
        QString exec;
        bool shouldAddParameter = true;
        if (service) {
            exec = service->exec();
            exec.replace(QLatin1String(" %U"), QLatin1String(" %u"));
            exec.replace(QLatin1String(" %F"), QLatin1String(" %f"));
            shouldAddParameter = !exec.contains(QLatin1String(" %u")) && !exec.contains(QLatin1String(" %f"));
        } else {
            exec = QCoreApplication::instance()->applicationName();
        }
        if (shouldAddParameter) {
            if (url.isLocalFile()) {
                exec += QLatin1String(" %f");
            } else {
                exec += QLatin1String(" %u");
            }
        }
        output.writeAttribute(execAttribute, exec);
        output.writeAttribute(modifiedAttribute, currentTimestamp);
        output.writeAttribute(countAttribute, "1"_L1);
    };

    bool foundExistingApp = false;
    bool inRightBookmark = false;
    bool foundMatchingBookmark = false;
    bool firstBookmark = true;
    int nbEntries = 0;
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::EndElement && xml.name() == xbelTag) {
            break;
        }
        switch (xml.tokenType()) {
        case QXmlStreamReader::StartElement: {
            const QStringView tagName = xml.qualifiedName();
            QXmlStreamAttributes attributes = xml.attributes();

            if (tagName == bookmarkTag) {
                foundExistingApp = false;
                firstBookmark = false;

                const QStringView hrefValue = attributes.value(hrefAttribute);
                inRightBookmark = hrefValue == newUrl;

                // remove hidden files if some were added by GTK
                if (ignoreHidden && hrefValue.contains(QLatin1String("/."))) {
                    xml.skipCurrentElement();
                    break;
                }

                if (inRightBookmark) {
                    foundMatchingBookmark = true;

                    QXmlStreamAttributes newAttributes;
                    for (const QXmlStreamAttribute &old : std::as_const(attributes)) {
                        if (old.name() == modifiedAttribute) {
                            continue;
                        }
                        if (old.name() == visitedAttribute) {
                            continue;
                        }
                        newAttributes.append(old);
                    }
                    newAttributes.append(modifiedAttribute, currentTimestamp);
                    newAttributes.append(visitedAttribute, currentTimestamp);
                    attributes = newAttributes;
                }

                nbEntries += 1;
            }

            else if (inRightBookmark && tagName == applicationBookmarkTag && attributes.value(nameAttribute) == desktopEntryName) {
                // case found right bookmark and same application
                const int count = attributes.value(countAttribute).toInt();

                QXmlStreamAttributes newAttributes;
                for (const QXmlStreamAttribute &old : std::as_const(attributes)) {
                    if (old.name() == countAttribute) {
                        continue;
                    }
                    if (old.name() == modifiedAttribute) {
                        continue;
                    }
                    newAttributes.append(old);
                }
                newAttributes.append(modifiedAttribute, currentTimestamp);
                newAttributes.append(countAttribute, QString::number(count + 1));
                attributes = newAttributes;

                foundExistingApp = true;
            }

            output.writeStartElement(tagName.toString());
            output.writeAttributes(attributes);
            break;
        }
        case QXmlStreamReader::EndElement: {
            const QStringView tagName = xml.qualifiedName();
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
            output.writeCharacters("\n"_L1);
        }
        output.writeCharacters("  "_L1);
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

    if (outputFile.commit()) {
        lockFile.unlock();
        // tolerate 10 more entries than threshold to limit overhead of cleaning old data
        return nbEntries - maxEntries > 10 || removeOldestEntries(maxEntries);
    }
    return false;
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

QList<QUrl> KRecentDocument::recentUrls()
{
    QMap<QUrl, QDateTime> documents = xbelRecentlyUsedList();

    QList<QUrl> ret = documents.keys();
    std::sort(ret.begin(), ret.end(), [&](const QUrl &doc1, const QUrl &doc2) {
        return documents.value(doc1) < documents.value(doc2);
    });

    return ret;
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

    // qDebug() << "KRecentDocument::add for " << openStr;
    KConfigGroup config = KSharedConfig::openConfig()->group("RecentDocuments"_L1);
    bool useRecent = config.readEntry("UseRecent"_L1, true);
    int maxEntries = config.readEntry("MaxEntries"_L1, 300);
    bool ignoreHidden = config.readEntry("IgnoreHidden"_L1, true);

    if (!useRecent || maxEntries == 0) {
        clear();
        return;
    }
    if (ignoreHidden && url.toLocalFile().contains(QLatin1String("/."))) {
        return;
    }

    if (!addToXbel(url, desktopEntryName, groups, maxEntries, ignoreHidden)) {
        qCWarning(KIO_CORE) << "Failed to add to recently used bookmark file";
    }
}

void KRecentDocument::clear()
{
    QFile(xbelPath()).remove();
}

int KRecentDocument::maximumItems()
{
    KConfigGroup cg(KSharedConfig::openConfig(), "RecentDocuments"_L1);
    return cg.readEntry("MaxEntries"_L1, 300);
}

void KRecentDocument::removeFile(const QUrl &url)
{
    QFile input(xbelPath());
    if (!input.exists()) {
        return;
    }

    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return;
    }

    if (!input.open(QIODevice::ReadOnly)) {
        qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
        return;
    }

    QDomDocument document;
    document.setContent(&input);
    input.close();

    auto xbelTags = document.elementsByTagName(xbelTag);
    if (xbelTags.length() != 1) {
        qCWarning(KIO_CORE) << "Invalid Xbel file, missing xbel element";
        return;
    }
    auto xbelElement = xbelTags.item(0);
    auto bookmarkList = xbelElement.childNodes();

    bool fileChanged = false;
    for (int i = 0; i < bookmarkList.length(); ++i) {
        const auto node = bookmarkList.item(i);

        const auto hrefValue = node.attributes().namedItem(hrefAttribute);
        if (!hrefValue.isAttr() || hrefValue.nodeValue().isEmpty()) {
            qCInfo(KIO_CORE) << "Invalid bookmark in" << input.fileName() << "invalid href attribute";
            continue;
        }

        const QUrl hrefUrl = QUrl::fromEncoded(hrefValue.nodeValue().toLatin1());
        if (hrefUrl == url) {
            xbelElement.removeChild(node);
            fileChanged = true;
        }
    }

    if (fileChanged) {
        if (!input.open(QIODevice::WriteOnly) || (input.write(document.toByteArray(2)) < 0)) {
            qCWarning(KIO_CORE) << "Couldn't save bookmark file " << input.fileName();
        }
    }
}

void KRecentDocument::removeApplication(const QString &desktopEntryName)
{
    QFile input(xbelPath());
    if (!input.exists()) {
        return;
    }

    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return;
    }

    if (!input.open(QIODevice::ReadOnly)) {
        qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
        return;
    }

    QDomDocument document;
    document.setContent(&input);
    input.close();

    auto xbelTags = document.elementsByTagName(xbelTag);
    if (xbelTags.length() != 1) {
        qCWarning(KIO_CORE) << "Invalid Xbel file, missing xbel element";
        return;
    }
    auto xbelElement = xbelTags.item(0);
    auto bookmarkList = xbelElement.childNodes();

    bool fileChanged = false;
    for (int i = 0; i < bookmarkList.length(); ++i) {
        const auto bookmarkNode = bookmarkList.item(i);
        const auto infoNode = bookmarkNode.firstChild();
        if (!infoNode.isElement()) {
            qCWarning(KIO_CORE) << "Invalid Xbel file, missing info element";
            return;
        }
        const auto metadataElement = infoNode.firstChild();
        if (!metadataElement.isElement()) {
            qCWarning(KIO_CORE) << "Invalid Xbel file, missing metadata element";
            return;
        }

        auto bookmarksElement = metadataElement.firstChildElement(applicationsBookmarkTag);
        if (!bookmarksElement.isElement()) {
            qCWarning(KIO_CORE) << "Invalid Xbel file, missing bookmarks element";
            return;
        }

        auto applicationList = bookmarksElement.childNodes();
        for (int i = 0; i < applicationList.length(); ++i) {
            auto appNode = applicationList.item(i);
            const auto appName = appNode.attributes().namedItem(nameAttribute).nodeValue();

            if (appName == desktopEntryName) {
                bookmarksElement.removeChild(appNode);
                fileChanged = true;
            }
        }
        if (bookmarksElement.childNodes().length() == 0) {
            // no more application associated with the file
            xbelElement.removeChild(bookmarkNode);
        }
    }

    if (fileChanged) {
        if (!input.open(QIODevice::WriteOnly) || (input.write(document.toByteArray(2)) < 0)) {
            qCWarning(KIO_CORE) << "Couldn't save bookmark file " << input.fileName();
        }
    }
}

void KRecentDocument::removeBookmarksModifiedSince(const QDateTime &since)
{
    QFile input(xbelPath());
    if (!input.exists()) {
        return;
    }

    // Won't help for GTK applications and whatnot, but we can be good citizens ourselves
    QLockFile lockFile(xbelPath() + QLatin1String(".lock"));
    lockFile.setStaleLockTime(0);
    if (!lockFile.tryLock(100)) { // give it 100ms
        qCWarning(KIO_CORE) << "Failed to lock recently used";
        return;
    }

    if (!input.open(QIODevice::ReadOnly)) {
        qCWarning(KIO_CORE) << "Failed to open existing recently used" << input.errorString();
        return;
    }

    QDomDocument document;
    document.setContent(&input);
    input.close();

    auto xbelTags = document.elementsByTagName(xbelTag);
    if (xbelTags.length() != 1) {
        qCWarning(KIO_CORE) << "Invalid Xbel file, missing xbel element";
        return;
    }
    auto xbelElement = xbelTags.item(0);
    auto bookmarkList = xbelElement.childNodes();

    bool fileChanged = false;
    for (int i = 0; i < bookmarkList.length(); ++i) {
        const auto node = bookmarkList.item(i);
        const auto modifiedString = node.attributes().namedItem(modifiedAttribute);
        const auto modifiedTime = QDateTime::fromString(modifiedString.nodeValue(), Qt::ISODate);

        if (modifiedTime >= since) {
            fileChanged = true;
            xbelElement.removeChild(node);
        }
    }
    if (fileChanged) {
        if (!input.open(QIODevice::WriteOnly) || (input.write(document.toByteArray(2)) < 0)) {
            qCWarning(KIO_CORE) << "Couldn't save bookmark file " << input.fileName();
        }
    }
}
