/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "trashdirnotify.h"
#include "debug.h"

#include <KDirNotify>
#include <KDirWatch>
#include <KMountPoint>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>

#include <QFileInfo>
#include <QUrl>

TrashDirNotify::TrashDirNotify()
    : m_dirWatch(new KDirWatch(this))
{
    if (m_trashImpl.init()) {
        m_homeFilesDir = m_trashImpl.homeTrashDirectory() + QLatin1String("/files");
    } else {
        qCWarning(KIO_TRASHNOTIFY_LOG) << "Failed to initialize the home trash directory, trash notifications may be incomplete";
    }

    connect(m_dirWatch, &KDirWatch::created, this, &TrashDirNotify::slotTrashChanged);
    connect(m_dirWatch, &KDirWatch::deleted, this, &TrashDirNotify::slotTrashChanged);
    connect(m_dirWatch, &KDirWatch::dirty, this, &TrashDirNotify::slotTrashChanged);

    Solid::DeviceNotifier *notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this, [this](const QString &udi) {
        watchStorageAccessChanges(Solid::Device(udi));
        updateWatchedTrashDirectories();
    });
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this, &TrashDirNotify::updateWatchedTrashDirectories);

    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::StorageAccess);
    for (const Solid::Device &device : devices) {
        watchStorageAccessChanges(device);
    }

    updateWatchedTrashDirectories();
}

void TrashDirNotify::watchStorageAccessChanges(const Solid::Device &device)
{
    if (auto *access = device.as<Solid::StorageAccess>()) {
        connect(access, &Solid::StorageAccess::accessibilityChanged, this, &TrashDirNotify::updateWatchedTrashDirectories);
    }
}

QString TrashDirNotify::trashFilesDirForMountPoint(const KMountPoint::Ptr &mountPoint) const
{
    // Accessing an unavailable network filesystem may block for a long time.
    // KDirWatch may poll watched directories with synchronous stat() calls, which
    // would block kded as well. Avoid probing and watching slow mount points.
    // Encrypted FUSE filesystems such as Plasma Vaults are considered slow, but are backed by local storage.
    if (!mountPoint->isEncryptedFs() && (mountPoint->isPseudoFs() || mountPoint->probablySlow())) {
        return QString();
    }

    const QString trashDir = m_trashImpl.trashDirectoryForMountPoint(mountPoint->mountPoint());
    if (trashDir.isEmpty()) {
        return QString();
    }

    const QString filesDir = trashDir + QLatin1String("/files");
    return QFileInfo::exists(filesDir) ? filesDir : QString();
}

void TrashDirNotify::updateWatchedTrashDirectories()
{
    QSet<QString> currentDirs;
    if (!m_homeFilesDir.isEmpty() && QFileInfo::exists(m_homeFilesDir)) {
        currentDirs.insert(m_homeFilesDir);
    }

    // Not TrashImpl::trashDirectories(): it probes every mount, including the slow ones.
    const KMountPoint::List mountPoints = KMountPoint::currentMountPoints();
    for (const KMountPoint::Ptr &mountPoint : mountPoints) {
        const QString filesDir = trashFilesDirForMountPoint(mountPoint);
        if (!filesDir.isEmpty()) {
            currentDirs.insert(filesDir);
        }
    }

    bool changed = false;

    for (const QString &dir : std::as_const(currentDirs)) {
        if (!m_watchedDirs.contains(dir)) {
            m_dirWatch->addDir(dir);
            changed = true;
        }
    }

    for (const QString &dir : std::as_const(m_watchedDirs)) {
        if (!currentDirs.contains(dir)) {
            m_dirWatch->removeDir(dir);
            changed = true;
        }
    }

    m_watchedDirs = currentDirs;

    if (changed) {
        slotTrashChanged();
    }
}

void TrashDirNotify::slotTrashChanged()
{
    org::kde::KDirNotify::emitFilesChanged({QUrl(QStringLiteral("trash:/"))});
}

#include "moc_trashdirnotify.cpp"
