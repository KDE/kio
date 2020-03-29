/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "untrustedprogramhandlerinterface.h"

#include <QFile>
#include <QSaveFile>
#include "kiocoredebug.h"

using namespace KIO;

UntrustedProgramHandlerInterface::UntrustedProgramHandlerInterface(QObject *parent)
    : QObject(parent), d(nullptr)
{
}

UntrustedProgramHandlerInterface::~UntrustedProgramHandlerInterface()
{
}

void UntrustedProgramHandlerInterface::showUntrustedProgramWarning(KJob *job, const QString &programName)
{
    Q_UNUSED(job)
    Q_UNUSED(programName)
    Q_EMIT result(false);
}

bool UntrustedProgramHandlerInterface::makeServiceFileExecutable(const QString &fileName, QString &errorString)
{
    // Open the file and read the first two characters, check if it's
    // #!.  If not, create a new file, prepend appropriate lines, and copy
    // over.
    QFile desktopFile(fileName);
    if (!desktopFile.open(QFile::ReadOnly)) {
        errorString = desktopFile.errorString();
        qCWarning(KIO_CORE) << "Error opening service" << fileName << errorString;
        return false;
    }

    QByteArray header = desktopFile.peek(2);   // First two chars of file
    if (header.size() == 0) {
        errorString = desktopFile.errorString();
        qCWarning(KIO_CORE) << "Error inspecting service" << fileName << errorString;
        return false; // Some kind of error
    }

    if (header != "#!") {
        // Add header
        QSaveFile saveFile;
        saveFile.setFileName(fileName);
        if (!saveFile.open(QIODevice::WriteOnly)) {
            errorString = saveFile.errorString();
            qCWarning(KIO_CORE) << "Unable to open replacement file for" << fileName << errorString;
            return false;
        }

        QByteArray shebang("#!/usr/bin/env xdg-open\n");
        if (saveFile.write(shebang) != shebang.size()) {
            errorString = saveFile.errorString();
            qCWarning(KIO_CORE) << "Error occurred adding header for" << fileName << errorString;
            saveFile.cancelWriting();
            return false;
        }

        // Now copy the one into the other and then close and reopen desktopFile
        QByteArray desktopData(desktopFile.readAll());
        if (desktopData.isEmpty()) {
            errorString = desktopFile.errorString();
            qCWarning(KIO_CORE) << "Unable to read service" << fileName << errorString;
            saveFile.cancelWriting();
            return false;
        }

        if (saveFile.write(desktopData) != desktopData.size()) {
            errorString = saveFile.errorString();
            qCWarning(KIO_CORE) << "Error copying service" << fileName << errorString;
            saveFile.cancelWriting();
            return false;
        }

        desktopFile.close();
        if (!saveFile.commit()) { // Figures....
            errorString = saveFile.errorString();
            qCWarning(KIO_CORE) << "Error committing changes to service" << fileName << errorString;
            return false;
        }

        if (!desktopFile.open(QFile::ReadOnly)) {
            errorString = desktopFile.errorString();
            qCWarning(KIO_CORE) << "Error re-opening service" << fileName << errorString;
            return false;
        }
    } // Add header

    return setExecuteBit(fileName, errorString);
}

bool UntrustedProgramHandlerInterface::setExecuteBit(const QString &fileName, QString &errorString)
{
    QFile file(fileName);

    // corresponds to owner on unix, which will have to do since if the user
    // isn't the owner we can't change perms anyways.
    if (!file.setPermissions(QFile::ExeUser | file.permissions())) {
        errorString = file.errorString();
        qCWarning(KIO_CORE) << "Unable to change permissions for" << fileName << errorString;
        return false;
    }

    return true;
}
