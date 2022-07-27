/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef REMOTEIMPL_H
#define REMOTEIMPL_H

#include <KIO/UDSEntry>
#include <QUrl>

class RemoteImpl
{
public:
    RemoteImpl();

    void createTopLevelEntry(KIO::UDSEntry &entry) const;
    bool statNetworkFolder(KIO::UDSEntry &entry, const QString &filename) const;

    void listRoot(KIO::UDSEntryList &list) const;

    QUrl findBaseURL(const QString &filename) const;
    QString findDesktopFile(const QString &filename) const;

    bool deleteNetworkFolder(const QString &filename) const;
    bool renameFolders(const QString &src, const QString &dest, bool overwrite) const;
    bool changeFolderTarget(const QString &src, const QString &target, bool overwrite) const;

private:
    bool findDirectory(const QString &filename, QString &directory) const;
    bool createEntry(KIO::UDSEntry &entry, const QString &directory, const QString &file) const;
};

#endif
