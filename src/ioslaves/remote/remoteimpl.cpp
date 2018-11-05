/* This file is part of the KDE project
   Copyright (c) 2004 Kevin Ottens <ervin ipsquad net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "remoteimpl.h"

#include "debug.h"
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KService>

#include <QDir>
#include <QFile>

namespace {
namespace Strings {
QString wizardUrl()     { return QStringLiteral("remote:/x-wizard_service.desktop"); }
QString wizardService() { return QStringLiteral("org.kde.knetattach"); }
}
}

RemoteImpl::RemoteImpl()
{
    const QString path = QStringLiteral("%1/remoteview").arg(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    QDir().mkpath(path);
}

void RemoteImpl::listRoot(KIO::UDSEntryList &list) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::listRoot";

    QStringList names_found;
    const QStringList dirList = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("remoteview"),
                                                          QStandardPaths::LocateDirectory);

    QStringList::ConstIterator dirpath = dirList.constBegin();
    const QStringList::ConstIterator end = dirList.constEnd();
    for (; dirpath != end; ++dirpath) {
        QDir dir = *dirpath;
        if (!dir.exists()) {
            continue;
        }

        const QStringList filenames = dir.entryList({QStringLiteral("*.desktop")},
                                                    QDir::Files | QDir::Readable);

        KIO::UDSEntry entry;

        QStringList::ConstIterator name = filenames.constBegin();
        QStringList::ConstIterator endf = filenames.constEnd();

        for (; name != endf; ++name) {
            if (!names_found.contains(*name) && createEntry(entry, *dirpath, *name)) {
                list.append(entry);
                names_found.append(*name);
            }
        }
    }
}

bool RemoteImpl::findDirectory(const QString &filename, QString &directory) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::findDirectory";

    const QStringList dirList = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("remoteview"),
                                                          QStandardPaths::LocateDirectory);

    QStringList::ConstIterator dirpath = dirList.constBegin();
    const QStringList::ConstIterator end = dirList.constEnd();
    for (; dirpath != end; ++dirpath) {
        if (QFileInfo::exists(*dirpath + QLatin1Char('/') + filename)) {
            directory = *dirpath + QLatin1Char('/');
            return true;
        }
    }

    return false;
}

QString RemoteImpl::findDesktopFile(const QString &filename) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::findDesktopFile";

    QString directory;
    const QString desktopFileName = filename + QLatin1String(".desktop");
    if (findDirectory(desktopFileName, directory)) {
        return directory + desktopFileName;
    }

    return QString();
}

QUrl RemoteImpl::findBaseURL(const QString &filename) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::findBaseURL";

    const QString file = findDesktopFile(filename);
    if (!file.isEmpty()) {
        KDesktopFile desktop(file);
        return QUrl::fromUserInput(desktop.readUrl());
    }

    return QUrl();
}

void RemoteImpl::createTopLevelEntry(KIO::UDSEntry &entry) const
{
    entry.clear();
    entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0777);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-remote"));
    entry.fastInsert(KIO::UDSEntry::UDS_USER, QStringLiteral("root"));
    entry.fastInsert(KIO::UDSEntry::UDS_GROUP, QStringLiteral("root"));
}

static QUrl findWizardRealURL()
{
    QUrl url;
    KService::Ptr service = KService::serviceByDesktopName(Strings::wizardService());

    if (service && service->isValid()) {
        url = QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                                         QStringLiteral("%1.desktop").arg(Strings::wizardService())));
    }

    return url;
}

bool RemoteImpl::createWizardEntry(KIO::UDSEntry &entry) const
{
    entry.clear();

    QUrl url = findWizardRealURL();

    if (!url.isValid()) {
        return false;
    }

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, i18n("Add Network Folder"));
    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFREG);
    entry.fastInsert(KIO::UDSEntry::UDS_URL, Strings::wizardUrl());
    entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, url.path());
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0500);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("application/x-desktop"));
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, QStringLiteral("folder-new"));

    return true;
}

bool RemoteImpl::isWizardURL(const QUrl &url) const
{
    return url == QUrl(Strings::wizardUrl());
}

bool RemoteImpl::createEntry(KIO::UDSEntry &entry, const QString &directory,
                             const QString &file) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::createEntry";

    QString dir = directory;
    if (!dir.endsWith(QLatin1Char('/'))) {
        dir += QLatin1Char('/');
    }

    KDesktopFile desktop(dir + file);

    qCDebug(KIOREMOTE_LOG) << "path = " << directory << file << desktop.readName();

    entry.clear();

    if (desktop.readName().isEmpty())
        return false;

    QString new_filename = file;
    new_filename.truncate(file.length()-8);

    entry.fastInsert(KIO::UDSEntry::UDS_NAME, desktop.readName());
    entry.fastInsert(KIO::UDSEntry::UDS_URL, QLatin1String("remote:/") + new_filename);

    entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
    entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0500);
    entry.fastInsert(KIO::UDSEntry::UDS_MIME_TYPE, QStringLiteral("inode/directory"));

    const QString icon = desktop.readIcon();
    entry.fastInsert(KIO::UDSEntry::UDS_ICON_NAME, icon);
    entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, desktop.readUrl());
    entry.fastInsert(KIO::UDSEntry::UDS_TARGET_URL, desktop.readUrl());
    return true;
}

bool RemoteImpl::statNetworkFolder(KIO::UDSEntry &entry, const QString &filename) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::statNetworkFolder: " << filename;

    QString directory;
    const QString desktopFileName = filename + QLatin1String(".desktop");
    return findDirectory(desktopFileName, directory)
        && createEntry(entry, directory, desktopFileName);
}

bool RemoteImpl::deleteNetworkFolder(const QString &filename) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::deleteNetworkFolder: " << filename;

    QString directory;
    const QString desktopFileName = filename + QLatin1String(".desktop");
    if (findDirectory(desktopFileName, directory)) {
        qCDebug(KIOREMOTE_LOG) << "Removing " << directory << filename << ".desktop";
        return QFile::remove(directory + desktopFileName);
    }

    return false;
}

bool RemoteImpl::renameFolders(const QString &src, const QString &dest, bool overwrite) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::renameFolders: "
                           << src << ", " << dest << endl;

    QString directory;
    const QString srcDesktopFileName = src + QLatin1String(".desktop");
    if (findDirectory(srcDesktopFileName, directory)) {
        const QString destDesktopFileName = dest + QLatin1String(".desktop");
        const QString destDesktopFilePath = directory + destDesktopFileName;
        if (!overwrite && QFile::exists(destDesktopFilePath)) {
            return false;
        }

        qCDebug(KIOREMOTE_LOG) << "Renaming " << directory << src << ".desktop";
        QDir dir(directory);
        bool res = dir.rename(srcDesktopFileName, destDesktopFileName);
        if (res) {
            KDesktopFile desktop(destDesktopFilePath);
            desktop.desktopGroup().writeEntry("Name", dest);
        }
        return res;
    }

    return false;
}

bool RemoteImpl::changeFolderTarget(const QString &src, const QString &target, bool overwrite) const
{
    qCDebug(KIOREMOTE_LOG) << "RemoteImpl::changeFolderTarget: "
                           << src << ", " << target << endl;

    QString directory;
    const QString srcDesktopFileName = src + QLatin1String(".desktop");
    if (findDirectory(srcDesktopFileName, directory)) {
        const QString srcDesktopFilePath = directory + srcDesktopFileName;
        if (!overwrite || !QFile::exists(srcDesktopFilePath)) {
            return false;
        }

        qCDebug(KIOREMOTE_LOG) << "Changing target " << directory << src << ".desktop";
        KDesktopFile desktop(srcDesktopFilePath);
        desktop.desktopGroup().writeEntry("URL", target);
        return true;
    }

    return false;
}
