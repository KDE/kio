/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2015 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "kioglobal_p.h"

#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>

static QMap<QString, QString> standardLocationsMap()
{
    static const struct {
        QStandardPaths::StandardLocation location;
        QString name;
    } mapping[] = {
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
                   {QStandardPaths::TemplatesLocations, QStringLiteral("folder-templates")},
                   {QStandardPaths::PublicShareLocation, QStringLiteral("folder-public")},
#endif
                   {QStandardPaths::MusicLocation, QStringLiteral("folder-music")},
                   {QStandardPaths::MoviesLocation, QStringLiteral("folder-videos")},
                   {QStandardPaths::PicturesLocation, QStringLiteral("folder-pictures")},
                   {QStandardPaths::TempLocation, QStringLiteral("folder-temp")},
                   {QStandardPaths::DownloadLocation, QStringLiteral("folder-download")},
                   // Order matters here as paths can be reused for multiple purposes
                   // We essentially want more generic choices to trump more specific
                   // ones.
                   // home > desktop > documents > *.
                   {QStandardPaths::DocumentsLocation, QStringLiteral("folder-documents")},
                   {QStandardPaths::DesktopLocation, QStringLiteral("user-desktop")},
                   {QStandardPaths::HomeLocation, QStringLiteral("user-home")}};

    QMap<QString, QString> map;
    for (const auto &row : mapping) {
        const QStringList locations = QStandardPaths::standardLocations(row.location);
        for (const QString &location : locations) {
            map.insert(location, row.name);
        }
        // Qt does not provide an easy way to receive the xdg dir for the templates and public directory so we have to find it on our own (QTBUG-86106 and QTBUG-78092)
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#ifdef Q_OS_UNIX
        const QString xdgUserDirs = QStandardPaths::locate(QStandardPaths::ConfigLocation, QStringLiteral("user-dirs.dirs"), QStandardPaths::LocateFile);
        QFile xdgUserDirsFile(xdgUserDirs);
        if (!xdgUserDirs.isEmpty() && xdgUserDirsFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&xdgUserDirsFile);
            const QLatin1String templatesLine("XDG_TEMPLATES_DIR=\"");
            const QLatin1String publicShareLine("XDG_PUBLICSHARE_DIR=\"");
            while (!in.atEnd()) {
                const QString line = in.readLine();
                if (line.startsWith(templatesLine)) {
                    QString xdgTemplates = line.mid(templatesLine.size()).chopped(1);
                    xdgTemplates.replace(QStringLiteral("$HOME"), QDir::homePath());
                    map.insert(xdgTemplates, QStringLiteral("folder-templates"));
                } else if (line.startsWith(publicShareLine)) {
                    QString xdgPublicShare = line.mid(publicShareLine.size()).chopped(1);
                    xdgPublicShare.replace(QStringLiteral("$HOME"), QDir::homePath());
                    map.insert(xdgPublicShare, QStringLiteral("folder-public"));
                }
            }
        }
#endif
#endif
    }
    return map;
}

QString KIOPrivate::iconForStandardPath(const QString &localDirectory)
{
    static auto map = standardLocationsMap();
    return map.value(localDirectory, QString());
}

KIOPrivate::KFileItemIconCache::KFileItemIconCache()
{
    connect(Solid::DeviceNotifier().instance(), &Solid::DeviceNotifier::deviceAdded, this, &KIOPrivate::KFileItemIconCache::slotDeviceAdded);
    connect(Solid::DeviceNotifier().instance(), &Solid::DeviceNotifier::deviceRemoved, this, &KIOPrivate::KFileItemIconCache::slotDeviceRemoved);
}

KIOPrivate::KFileItemIconCache *KIOPrivate::KFileItemIconCache::instance()
{
    static KFileItemIconCache *instance = new KFileItemIconCache();

    return instance;
}

QString KIOPrivate::KFileItemIconCache::iconForMountPoint(const QString &localDirectory)
{
    if (m_mountPointToIconProxyMap.empty()) {
        initializeMountPointsMap();
    }

    return m_mountPointToIconProxyMap.value(localDirectory, QString());
}

void KIOPrivate::KFileItemIconCache::refreshStorageAccess(const Solid::Device &device)
{
    const Solid::StorageAccess *const access = device.as<Solid::StorageAccess>();

    if (!access) {
        return;
    }

    connect(access, &Solid::StorageAccess::setupDone, this, &KIOPrivate::KFileItemIconCache::slotUpdateMountPointsMap);
    connect(access, &Solid::StorageAccess::teardownDone, this, &KIOPrivate::KFileItemIconCache::slotUpdateMountPointsMap);

    slotUpdateMountPointsMap(Solid::ErrorType::NoError, QVariant(), device.udi());
}

void KIOPrivate::KFileItemIconCache::slotDeviceAdded(const QString &udi)
{
    refreshStorageAccess(Solid::Device(udi));
}

void KIOPrivate::KFileItemIconCache::slotDeviceRemoved(const QString &udi)
{
    slotUpdateMountPointsMap(Solid::ErrorType::NoError, QVariant(), udi);
}

void KIOPrivate::KFileItemIconCache::slotUpdateMountPointsMap(Solid::ErrorType error, const QVariant &errorData, const QString &udi)
{
    Q_UNUSED(errorData)

    if (error != Solid::ErrorType::NoError) {
        return;
    }

    const Solid::Device device = Solid::Device(udi);
    const Solid::StorageAccess *const access = device.as<Solid::StorageAccess>();

    if (access && !access->filePath().isEmpty()) {
        // Add the mount point to the maps
        m_mountPointToIconProxyMap.insert(access->filePath(), device.icon());
        m_udiToMountPointProxyMap.insert(udi, access->filePath());
    } else if (m_udiToMountPointProxyMap.contains(udi)) {
        // Remove the mount point from the maps
        m_mountPointToIconProxyMap.remove(m_udiToMountPointProxyMap.take(udi));
    }
}

void KIOPrivate::KFileItemIconCache::initializeMountPointsMap()
{
    const QList<Solid::Device> deviceList = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);

    for (const auto &device : deviceList) {
        if (!m_udiToMountPointProxyMap.contains(device.udi())) {
            refreshStorageAccess(device);
        }
    }
}
