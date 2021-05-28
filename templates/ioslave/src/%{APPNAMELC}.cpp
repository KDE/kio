/*
    SPDX-FileCopyrightText: %{CURRENT_YEAR} %{AUTHOR} <%{EMAIL}>

    SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include "%{APPNAMELC}.h"

#include "%{APPNAMELC}_log.h"
#include "mydatasystem.h"
// KF
#include <KIO/UDSEntry>
#include <KLocalizedString>
// Qt
#include <QCoreApplication>

// Pseudo plugin class to embed meta data
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.slave.myproto" FILE "myproto.json")
};

extern "C" {
int Q_DECL_EXPORT kdemain(int argc, char **argv);
}

int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("%{APPNAME}"));

    %{APPNAME} slave(argv[2], argv[3]);
    slave.dispatchLoop();

    return 0;
}

// utils methods to map our data into KIO data
namespace
{

QStringList groupPath(const QUrl &url)
{
    QString path = url.adjusted(QUrl::StripTrailingSlash).path();
    if (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    return path.isEmpty() ? QStringList() : path.split(QLatin1Char('/'));
}

KIO::UDSEntry fileEntry(const DataItem &item)
{
    KIO::UDSEntry entry;
    entry.reserve(5);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, item.name);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("text/plain"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.fastInsert(KIO::UDSEntry::UDS_SIZE, item.data().size());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);
    return entry;
}

KIO::UDSEntry dirEntry(const QString &name)
{
    KIO::UDSEntry entry;

    entry.reserve(4);
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, name);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, S_IRUSR | S_IRGRP | S_IROTH);

    return entry;
}

}

%{APPNAME}::%{APPNAME}(const QByteArray &pool_socket, const QByteArray &app_socket)
    : KIO::SlaveBase("myproto", pool_socket, app_socket)
    , m_dataSystem(new MyDataSystem)
{
    qCDebug(%{APPNAMEUC}_LOG) << "%{APPNAME} starting up";
}

%{APPNAME}::~%{APPNAME}()
{
    qCDebug(%{APPNAMEUC}_LOG) << "%{APPNAME} shutting down";
}


void %{APPNAME}::get(const QUrl &url)
{
    qCDebug(%{APPNAMEUC}_LOG) << "%{APPNAME} starting get" << url;

    QStringList groupPath = ::groupPath(url);
    const QString itemName = groupPath.takeLast();
    const DataItem item = m_dataSystem->item(groupPath, itemName);
    if (!item.isValid()) {
        groupPath.append(itemName);
        if (m_dataSystem->hasGroup(groupPath)) {
            error(KIO::ERR_IS_DIRECTORY, itemName);
        } else {
        }
            error(KIO::ERR_DOES_NOT_EXIST, itemName);
        return;
    }

    // as first notify about the MIME type, so the handler can be selected
    mimeType("text/plain");

    // now emit the data...
    data(item.data());

    // and we are done
    finished();
}

void %{APPNAME}::stat(const QUrl &url)
{
    qCDebug(%{APPNAMEUC}_LOG) << "%{APPNAME} starting stat" << url;

    QStringList groupPath = ::groupPath(url);

    // is root directory?
    if (groupPath.isEmpty()) {
        statEntry(dirEntry(QStringLiteral(".")));
        finished();
        return;
    }
    // test subgroup
    if (m_dataSystem->hasGroup(groupPath)) {
        statEntry(dirEntry(groupPath.last()));
        finished();
        return;
    }

    // test item
    const QString itemName = groupPath.takeLast();
    const DataItem item = m_dataSystem->item(groupPath, itemName);
    if (item.isValid()) {
        statEntry(fileEntry(item));
        finished();
        return;
    }

    error(KIO::ERR_DOES_NOT_EXIST, i18n("No such path."));
}

void %{APPNAME}::listDir(const QUrl &url)
{
    const QStringList groupPath = ::groupPath(url);
    qCDebug(%{APPNAMEUC}_LOG) << "%{APPNAME} starting listDir" << url << groupPath;

    if (!m_dataSystem->hasGroup(groupPath)) {
        error(KIO::ERR_DOES_NOT_EXIST, i18n("No such directory."));
        return;
    }

    const QStringList subGroupNames = m_dataSystem->subGroupNames(groupPath);
    const QList<DataItem> items = m_dataSystem->items(groupPath);

    // report number of expected entries
    totalSize(1 + subGroupNames.size() + items.size());
    // own dir
    listEntry(dirEntry(QStringLiteral(".")));
    // subdirs
    for (const QString &subGroupName : subGroupNames) {
        listEntry(dirEntry(subGroupName));
    }
    // files
    for (const auto &item : items) {
        listEntry(fileEntry(item));
    }
    finished();
}
