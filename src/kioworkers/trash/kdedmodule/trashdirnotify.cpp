/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "trashdirnotify.h"
#include "debug.h"

#include <KDirNotify>
#include <KDirWatch>

#include <Solid/Device>
#include <Solid/DeviceNotifier>
#include <Solid/StorageAccess>

#include <QFileInfo>
#include <QUrl>

TrashDirNotify::TrashDirNotify()
    : m_dirWatch(new KDirWatch(this))
{
    if (!m_trashImpl.init()) {
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
        connect(access, &Solid::StorageAccess::accessibilityChanged, this, &TrashDirNotify::updateWatchedTrashDirectory);
    }
}

void TrashDirNotify::updateWatchedTrashDirectory(bool accessible, const QString &udi)
{
    if (!accessible) {
        pruneUnavailableWatchedDirectories();
        return;
    }

    const Solid::Device device(udi);
    auto *access = device.as<Solid::StorageAccess>();
    const QString mountPoint = access ? access->filePath() : QString();
    if (mountPoint.isEmpty()) {
        return;
    }

    const QString trashDir = m_trashImpl.trashDirectoryForMountPoint(mountPoint);
    if (trashDir.isEmpty()) {
        return;
    }

    const QString filesDir = trashDir + QLatin1String("/files");
    if (!QFileInfo::exists(filesDir) || m_watchedDirs.contains(filesDir)) {
        return;
    }

    m_watchedDirs.insert(filesDir);
    m_dirWatch->addDir(filesDir);
    slotTrashChanged();
}

void TrashDirNotify::pruneUnavailableWatchedDirectories()
{
    bool changed = false;

    for (auto it = m_watchedDirs.begin(); it != m_watchedDirs.end();) {
        if (!QFileInfo::exists(*it)) {
            m_dirWatch->removeDir(*it);
            it = m_watchedDirs.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        slotTrashChanged();
    }
}

void TrashDirNotify::updateWatchedTrashDirectories()
{
    m_trashImpl.invalidateTrashDirectoriesCache();

    QSet<QString> currentDirs;
    const TrashImpl::TrashDirMap trashDirs = m_trashImpl.trashDirectories();
    for (const QString &trashDir : trashDirs) {
        const QString filesDir = trashDir + QLatin1String("/files");
        if (QFileInfo::exists(filesDir)) {
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
