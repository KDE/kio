/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef TRASHDIRNOTIFY_H
#define TRASHDIRNOTIFY_H

#include <KMountPoint>

#include <QObject>
#include <QSet>
#include <QString>

#include "../trashimpl.h"

class KDirWatch;

namespace Solid
{
class Device;
}

class TrashDirNotify : public QObject
{
    Q_OBJECT

public:
    TrashDirNotify();

private Q_SLOTS:
    void slotTrashChanged();
    void updateWatchedTrashDirectories();

private:
    void watchStorageAccessChanges(const Solid::Device &device);
    // Returns the files/ directory of the trash on the given mount point, or an empty
    // string if there is none or if it must not be watched.
    QString trashFilesDirForMountPoint(const KMountPoint::Ptr &mountPoint) const;

    TrashImpl m_trashImpl;
    QString m_homeFilesDir;
    KDirWatch *m_dirWatch;
    QSet<QString> m_watchedDirs;
};

#endif
