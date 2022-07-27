/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2004 Kévin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "remotedirnotifymodule.h"

#include <KPluginFactory>

K_PLUGIN_CLASS_WITH_JSON(RemoteDirNotifyModule, "remotedirnotify.json")

RemoteDirNotifyModule::RemoteDirNotifyModule(QObject *parent, const QList<QVariant> &)
    : KDEDModule(parent)
{
}

#include "remotedirnotifymodule.moc"
