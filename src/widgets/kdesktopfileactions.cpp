/*  This file is part of the KDE libraries
 *  Copyright (C) 1999 Waldo Bastian <bastian@kde.org>
 *                     David Faure   <faure@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#include "kdesktopfileactions.h"

#include "config-kiowidgets.h" // KIO_NO_SOLID
#include "../core/config-kmountpoint.h" // for HAVE_VOLMGT (yes I cheat a bit)
#include "kio_widgets_debug.h"

#include "krun.h"
#include "kautomount.h"
#include <kmessagebox.h>
#include <kdirnotify.h>
#include <kmountpoint.h>

#include <kdesktopfile.h>
#include <kconfiggroup.h>
#include <klocalizedstring.h>
#include <kservice.h>

#if ! KIO_NO_SOLID
//Solid
#include <solid/devicenotifier.h>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/predicate.h>
#include <solid/storageaccess.h>
#include <solid/opticaldrive.h>
#include <solid/opticaldisc.h>
#include <solid/block.h>
#endif

#include <QDBusInterface>
#include <QDBusReply>

enum BuiltinServiceType { ST_MOUNT = 0x0E1B05B0, ST_UNMOUNT = 0x0E1B05B1 }; // random numbers

static bool runFSDevice(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn);
static bool runApplication(const QUrl &_url, const QString &_serviceFile, const QByteArray &asn);
static bool runLink(const QUrl &_url, const KDesktopFile &cfg, const QByteArray &asn);



bool KDesktopFileActions::run(const QUrl &u, bool _is_local)
{
    return runWithStartup(u, _is_local, QByteArray());
}

bool KDesktopFileActions::runWithStartup(const QUrl &u, bool _is_local, const QByteArray &asn)
{
    // It might be a security problem to run external untrusted desktop
    // entry files
    if (!_is_local) {
        return false;
    }

    if (u.fileName() == QLatin1String(".directory")) {
        // We cannot execute a .directory file. Open with a text editor instead.
        return KRun::runUrl(u, QStringLiteral("text/plain"), nullptr, false /*tempFile*/, false /*runExecutables*/, QString(), asn);
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
        return runApplication(u, u.toLocalFile(), asn);
    } else if (cfg.hasLinkType()) {
        return runLink(u, cfg, asn);
    }

    QString tmp = i18n("The desktop entry of type\n%1\nis unknown.",  cfg.readType());
    KMessageBox::error(nullptr, tmp);

    return false;
}

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
        retval = KRun::runUrl(mpURL, QStringLiteral("inode/directory"), nullptr /*TODO - window*/, false, true, QString(), asn);
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

static bool runApplication(const QUrl &_url, const QString &_serviceFile, const QByteArray &asn)
{
    KService s(_serviceFile);
    if (!s.isValid())
    {
        QString tmp = i18n("The desktop entry file\n%1\nis not valid.", _url.toString());
        KMessageBox::error(nullptr, tmp);
        return false;
    }

    QList<QUrl> lst;
    return KRun::runService(s, lst, nullptr /*TODO - window*/, false, QString(), asn);
}

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
    // was should be preferred for opening this URL if possible.
    // This is used by the Recent Documents menu for instance.
    QString lastOpenedWidth = cfg.desktopGroup().readEntry("X-KDE-LastOpenedWith");
    if (!lastOpenedWidth.isEmpty()) {
        run->setPreferredService(lastOpenedWidth);
    }

    return false;
}

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
#if ! KIO_NO_SOLID
    else { // url to device
        Solid::Predicate predicate(Solid::DeviceInterface::Block, "device", _url.toLocalFile());
        const QList<Solid::Device> devList = Solid::Device::listFromQuery(predicate, QString());
        if (devList.empty()) {
            //qDebug() << "Device" << _url.toLocalFile() << "not found";
            return result;
        }
        Solid::Device device = devList[0];
        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
        Solid::StorageDrive *drive = device.parent().as<Solid::StorageDrive>();
        bool mounted = access && access->isAccessible();

        if ((mounted || device.is<Solid::OpticalDisc>()) && drive && drive->isRemovable()) {
            offerUnmount = true;
        }

        if (!mounted && ((drive && drive->isHotpluggable()) || device.is<Solid::OpticalDisc>())) {
            offerMount = true;
        }
    }
#endif

    if (offerMount) {
        KServiceAction mount(QStringLiteral("mount"), i18n("Mount"), QString(), QString(), false);
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
        KServiceAction unmount(QStringLiteral("unmount"), text, QString(), QString(), false);
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
        const QStringList dbuscall = actionMenu.split(QChar(' '));
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

    foreach (const KServiceAction &action, service.actions()) {
        if (keys.isEmpty() || keys.contains(action.name())) {
            const QString exec = action.exec();
            if (bLocalFiles || exec.contains(QStringLiteral("%U")) || exec.contains(QStringLiteral("%u"))) {
                result.append(action);
            }
        }
    }

    return result;
}

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
                KMessageBox::error(nullptr, tmp);
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
#if ! KIO_NO_SOLID
        else { // path to device
            Solid::Predicate predicate(Solid::DeviceInterface::Block, "device", path);
            const QList<Solid::Device> devList = Solid::Device::listFromQuery(predicate, QString());
            if (!devList.empty()) {
                Solid::Device device = devList[0];
                if (actionData == ST_MOUNT) {
                    if (device.is<Solid::StorageVolume>()) {
                        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
                        if (access) {
                            access->setup();
                        }
                    }
                } else if (actionData == ST_UNMOUNT) {
                    if (device.is<Solid::OpticalDisc>()) {
                        Solid::OpticalDrive *drive = device.parent().as<Solid::OpticalDrive>();
                        if (drive != 0) {
                            drive->eject();
                        }
                    } else if (device.is<Solid::StorageVolume>()) {
                        Solid::StorageAccess *access = device.as<Solid::StorageAccess>();
                        if (access && access->isAccessible()) {
                            access->teardown();
                        }
                    }
                }
            } else {
                //qDebug() << "Device" << path << "not found";
            }
        }
#endif
    } else {
        //qDebug() << action.name() << "first url's path=" << urls.first().toLocalFile() << "exec=" << action.exec();
        KRun::run(action.exec(), urls, nullptr, action.text(), action.icon());
        // The action may update the desktop file. Example: eject unmounts (#5129).
        org::kde::KDirNotify::emitFilesChanged(urls);
    }
}

