/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kdesktopfileactions.h"

#include "kio_widgets_debug.h"

#include <KDialogJobUiDelegate>
#include <KIO/ApplicationLauncherJob>
#include <KMessageBox>
#include <kdirnotify.h>
#include <kmountpoint.h>

#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>
#include <KService>

#ifndef KIO_ANDROID_STUB
#include <QDBusInterface>
#include <QDBusReply>
#endif

enum BuiltinServiceType { ST_MOUNT = 0x0E1B05B0, ST_UNMOUNT = 0x0E1B05B1 }; // random numbers

QList<KServiceAction> KDesktopFileActions::userDefinedServices(const KService &service, bool bLocalFiles, const QList<QUrl> &file_list)
{
    QList<KServiceAction> result;

    if (!service.isValid()) { // e.g. TryExec failed
        return result;
    }

    QStringList keys;
    const QString actionMenu = service.property<QString>(QStringLiteral("X-KDE-GetActionMenu"));
    if (!actionMenu.isEmpty()) {
        const QStringList dbuscall = actionMenu.split(QLatin1Char(' '));
        if (dbuscall.count() >= 4) {
            const QString &app = dbuscall.at(0);
            const QString &object = dbuscall.at(1);
            const QString &interface = dbuscall.at(2);
            const QString &function = dbuscall.at(3);

#ifndef KIO_ANDROID_STUB
            QDBusInterface remote(app, object, interface);
            // Do NOT use QDBus::BlockWithGui here. It runs a nested event loop,
            // in which timers can fire, leading to crashes like #149736.
            QDBusReply<QStringList> reply = remote.call(function, QUrl::toStringList(file_list));
            keys = reply; // ensures that the reply was a QStringList
            if (keys.isEmpty()) {
                return result;
            }
#endif
        } else {
            qCWarning(KIO_WIDGETS) << "The desktop file" << service.entryPath() << "has an invalid X-KDE-GetActionMenu entry."
                                   << "Syntax is: app object interface function";
        }
    }

    // Now, either keys is empty (all actions) or it's set to the actions we want

    const QList<KServiceAction> list = service.actions();
    for (const KServiceAction &action : list) {
        if (keys.isEmpty() || keys.contains(action.name())) {
            const QString exec = action.exec();
            if (bLocalFiles || exec.contains(QLatin1String("%U")) || exec.contains(QLatin1String("%u"))) {
                result.append(action);
            }
        }
    }

    return result;
}
