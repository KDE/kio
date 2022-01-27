/*
    SPDX-FileCopyrightText: 2022 Carson Black <uhhadd@gmail.com>

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#include "filemanagementadaptor.h"
#include "filemanagement.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto fileManagement = new FileManagement(&app);
    new FilemanagementAdaptor(fileManagement);

    if (!QDBusConnection::systemBus().registerObject(QStringLiteral("/"), fileManagement)) {
        qWarning() << "Failed to reigster the daemon object" << QDBusConnection::systemBus().lastError().message();
        exit(1);
    }
    if (!QDBusConnection::systemBus().registerService(QStringLiteral(SERVICE_NAME))) {
        qWarning() << "Failed to register the service" << QDBusConnection::systemBus().lastError().message();
        exit(1);
    }

    return app.exec();
}
