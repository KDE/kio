/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "trashdirnotifymodule.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(TrashDirNotifyModule, "trashdirnotify.json")

TrashDirNotifyModule::TrashDirNotifyModule(QObject *parent, const QList<QVariant> &)
    : KDEDModule(parent)
{
}

#include "moc_trashdirnotifymodule.cpp"
#include "trashdirnotifymodule.moc"
