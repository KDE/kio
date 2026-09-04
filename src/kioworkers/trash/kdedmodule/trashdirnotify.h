/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef TRASHDIRNOTIFY_H
#define TRASHDIRNOTIFY_H

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
    void updateWatchedTrashDirectory(bool accessible, const QString &udi);

private:
    void watchStorageAccessChanges(const Solid::Device &device);
    void pruneUnavailableWatchedDirectories();

    TrashImpl m_trashImpl;
    KDirWatch *m_dirWatch;
    QSet<QString> m_watchedDirs;
};

#endif
