/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Waldo Bastian <bastian@kde.org>
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kdesktopfileactions.h"

#include "../core/config-kmountpoint.h" // for HAVE_VOLMGT (yes I cheat a bit)
#include "kio_widgets_debug.h"

#include <KIO/ApplicationLauncherJob>
#include "krun.h"
#include "kautomount.h"
#include <KMessageBox>
#include <kdirnotify.h>
#include <KDialogJobUiDelegate>
#include <kmountpoint.h>

#include <KDesktopFile>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KService>

#include <QDBusInterface>
#include <QDBusReply>

enum BuiltinServiceType { ST_MOUNT = 0x0E1B05B0, ST_UNMOUNT = 0x0E1B05B1 }; // random numbers

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
static bool runFSDevice(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn);
static bool runLink(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn);
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KDesktopFileActions::run(const QUrl &u, bool _is_local)
{
    return runWithStartup(u, _is_local, QByteArray());
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
bool KDesktopFileActions::runWithStartup(const QUrl &u, bool _is_local, const QByteArray &asn)
{
    // It might be a security problem to run external untrusted desktop
    // entry files
    if (!_is_local) {
        return false;
    }

    if (u.fileName() == QLatin1String(".directory")) {
        // We cannot execute a .directory file. Open with a text editor instead.
        return KRun::runUrl(u, QStringLiteral("text/plain"), nullptr, KRun::RunFlags(), QString(), asn);
    }

    KDesktopFile cfg(u.toLocalFile());
    if (!cfg.desktopGroup().hasKey("Type")) {
        QString tmp = i18n("The desktop entry file %1 "
                           "has no Type=... entry.", u.toLocalFile());
        KMessageBox::error(nullptr, tmp);
        return false;
    }

    //qDebug() << "TYPE = " << type.data();

    if (cfg.hasDeviceType()) {
        return runFSDevice(u, cfg, asn);
    } else if (cfg.hasApplicationType()
               || (cfg.readType() == QLatin1String("Service") && !cfg.desktopGroup().readEntry("Exec").isEmpty())) { // for kio_settings
        KService service(u.toLocalFile());
        return KRun::runApplication(service, QList<QUrl>(), nullptr /*TODO - window*/, KRun::RunFlags{}, QString(), asn);
    } else if (cfg.hasLinkType()) {
        return runLink(u, cfg, asn);
    }

    QString tmp = i18n("The desktop entry of type\n%1\nis unknown.",  cfg.readType());
    KMessageBox::error(nullptr, tmp);

    return false;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
static bool runFSDevice(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn)
{
    bool retval = false;

    QString dev = cfg.readDevice();

    if (dev.isEmpty()) {
        QString tmp = i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  _url.toLocalFile());
        KMessageBox::error(nullptr, tmp);
        return retval;
    }

    KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice(dev);
    // Is the device already mounted ?
    if (mp) {
        const QUrl mpURL = QUrl::fromLocalFile(mp->mountPoint());
        // Open a new window
        retval = KRun::runUrl(mpURL, QStringLiteral("inode/directory"), nullptr /*TODO - window*/, KRun::RunFlags(KRun::RunExecutables), QString(), asn);
    } else {
        KConfigGroup cg = cfg.desktopGroup();
        bool ro = cg.readEntry("ReadOnly", false);
        QString fstype = cg.readEntry("FSType");
        if (fstype == QLatin1String("Default")) { // KDE-1 thing
            fstype.clear();
        }
        QString point = cg.readEntry("MountPoint");
#ifndef Q_OS_WIN
        (void) new KAutoMount(ro, fstype.toLatin1(), dev, point, _url.toLocalFile());
#endif
        retval = false;
    }

    return retval;
}
#endif

#if KIOWIDGETS_BUILD_DEPRECATED_SINCE(5, 71)
static bool runLink(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn)
{
    QString u = cfg.readUrl();
    if (u.isEmpty()) {
        QString tmp = i18n("The desktop entry file\n%1\nis of type Link but has no URL=... entry.",  _url.toString());
        KMessageBox::error(nullptr, tmp);
        return false;
    }

    QUrl url = QUrl::fromUserInput(u);
    KRun *run = new KRun(url, (QWidget *)nullptr, true, asn);

    // X-KDE-LastOpenedWith holds the service desktop entry name that
    // should be preferred for opening this URL if possible.
    // This is used by the Recent Documents menu for instance.
    QString lastOpenedWidth = cfg.desktopGroup().readEntry("X-KDE-LastOpenedWith");
    if (!lastOpenedWidth.isEmpty()) {
        run->setPreferredService(lastOpenedWidth);
    }

    return false;
}
#endif

QList<KServiceAction> KDesktopFileActions::builtinServices(const QUrl &_url)
{
    QList<KServiceAction> result;

    if (!_url.isLocalFile()) {
        return result;
    }

    bool offerMount = false;
    bool offerUnmount = false;

    KDesktopFile cfg(_url.toLocalFile());
    if (cfg.hasDeviceType()) {    // url to desktop file
        const QString dev = cfg.readDevice();
        if (dev.isEmpty()) {
            QString tmp = i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  _url.toLocalFile());
            KMessageBox::error(nullptr, tmp);
            return result;
        }

        KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice(dev);
        if (mp) {
            offerUnmount = true;
        } else {
            offerMount = true;
        }
    }

    if (offerMount) {
        KServiceAction mount(QStringLiteral("mount"), i18n("Mount"), QString(), QString(), false, {});
        mount.setData(QVariant(ST_MOUNT));
        result.append(mount);
    }

    if (offerUnmount) {
        QString text;
#ifdef HAVE_VOLMGT
        /*
         *  Solaris' volume management can only umount+eject
         */
        text = i18n("Eject");
#else
        text = i18n("Unmount");
#endif
        KServiceAction unmount(QStringLiteral("unmount"), text, QString(), QString(), false, {});
        unmount.setData(QVariant(ST_UNMOUNT));
        result.append(unmount);
    }

    return result;
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices(const QString &path, bool bLocalFiles)
{
    KDesktopFile cfg(path);
    return userDefinedServices(path, cfg, bLocalFiles);
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices(const QString &path, const KDesktopFile &cfg, bool bLocalFiles, const QList<QUrl> &file_list)
{
    Q_UNUSED(path); // this was just for debugging; we use service.entryPath() now.
    KService service(&cfg);
    return userDefinedServices(service, bLocalFiles, file_list);
}

QList<KServiceAction> KDesktopFileActions::userDefinedServices(const KService &service, bool bLocalFiles, const QList<QUrl> &file_list)
{
    QList<KServiceAction> result;

    if (!service.isValid()) { // e.g. TryExec failed
        return result;
    }

    QStringList keys;
    const QString actionMenu = service.property(QStringLiteral("X-KDE-GetActionMenu"), QVariant::String).toString();
    if (!actionMenu.isEmpty()) {
        const QStringList dbuscall = actionMenu.split(QLatin1Char(' '));
        if (dbuscall.count() >= 4) {
            const QString &app       = dbuscall.at(0);
            const QString &object    = dbuscall.at(1);
            const QString &interface = dbuscall.at(2);
            const QString &function  = dbuscall.at(3);

            QDBusInterface remote(app, object, interface);
            // Do NOT use QDBus::BlockWithGui here. It runs a nested event loop,
            // in which timers can fire, leading to crashes like #149736.
            QDBusReply<QStringList> reply = remote.call(function, QUrl::toStringList(file_list));
            keys = reply;               // ensures that the reply was a QStringList
            if (keys.isEmpty()) {
                return result;
            }
        } else {
            qCWarning(KIO_WIDGETS) << "The desktop file" << service.entryPath()
                       << "has an invalid X-KDE-GetActionMenu entry."
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

// KF6 TODO remove this method and use ApplicationLauncherJob instead
void KDesktopFileActions::executeService(const QList<QUrl> &urls, const KServiceAction &action)
{
    //qDebug() << "EXECUTING Service " << action.name();

    int actionData = action.data().toInt();
    if (actionData == ST_MOUNT || actionData == ST_UNMOUNT) {
        Q_ASSERT(urls.count() == 1);
        const QString path = urls.first().toLocalFile();
        //qDebug() << "MOUNT&UNMOUNT";

        KDesktopFile cfg(path);
        if (cfg.hasDeviceType()) { // path to desktop file
            const QString dev = cfg.readDevice();
            if (dev.isEmpty()) {
                QString tmp = i18n("The desktop entry file\n%1\nis of type FSDevice but has no Dev=... entry.",  path);
                KMessageBox::error(nullptr /*TODO window*/, tmp);
                return;
            }
            KMountPoint::Ptr mp = KMountPoint::currentMountPoints().findByDevice(dev);

            if (actionData == ST_MOUNT) {
                // Already mounted? Strange, but who knows ...
                if (mp) {
                    //qDebug() << "ALREADY Mounted";
                    return;
                }

                const KConfigGroup group = cfg.desktopGroup();
                bool ro = group.readEntry("ReadOnly", false);
                QString fstype = group.readEntry("FSType");
                if (fstype == QLatin1String("Default")) { // KDE-1 thing
                    fstype.clear();
                }
                QString point = group.readEntry("MountPoint");
#ifndef Q_OS_WIN
                (void)new KAutoMount(ro, fstype.toLatin1(), dev, point, path, false);
#endif
            } else if (actionData == ST_UNMOUNT) {
                // Not mounted? Strange, but who knows ...
                if (!mp) {
                    return;
                }

#ifndef Q_OS_WIN
                (void)new KAutoUnmount(mp->mountPoint(), path);
#endif
            }
        }
    } else {
        KIO::ApplicationLauncherJob *job = new KIO::ApplicationLauncherJob(action);
        job->setUrls(urls);
        QObject::connect(job, &KJob::result, qApp, [urls]() {
                // The action may update the desktop file. Example: eject unmounts (#5129).
                org::kde::KDirNotify::emitFilesChanged(urls);
                });
        job->setUiDelegate(new KDialogJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, nullptr /*TODO window*/));
        job->start();
    }
}

