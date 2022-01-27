/*
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QDBusContext>
#include <QDBusUnixFileDescriptor>
#include <QObject>
#include <fcntl.h>

#define SERVICE_NAME "org.kde.kio.filemanagement"

class FileManagement : public QObject, protected QDBusContext
{
    Q_OBJECT

    bool isAuthorized();

public:
    explicit FileManagement(QObject *parent = nullptr);
    ~FileManagement();

    uint ChangeMode(const QString &file, int mode);
    uint ChangeOwner(const QString &file, uint user, uint group);
    uint CreateSymlink(const QString &destination, const QString &pointingTo);
    uint Delete(const QString &file);
    uint MakeDirectory(const QString &directory, uint permissions);
    uint RemoveDir(const QString &directory);
    uint Rename(const QString &source, const QString &destination);
    uint UpdateTime(const QString &file, uint accessTime, uint modifiedTime);
    QDBusUnixFileDescriptor Open(const QString &path, uint flags, uint mode, uint &errnum);

    QDBusUnixFileDescriptor OpenDirectory(const QString &path, uint flags, uint mode, uint &errnum)
    { return Open(path, flags | O_DIRECTORY, mode, errnum); }

};
