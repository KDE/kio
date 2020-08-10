/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2004 Kévin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef REMOTEDIRNOTIFYMODULE_H
#define REMOTEDIRNOTIFYMODULE_H

#include <KDEDModule>

#include "remotedirnotify.h"

class RemoteDirNotifyModule : public KDEDModule
{
    Q_OBJECT
public:
    RemoteDirNotifyModule(QObject *parent, const QList<QVariant> &);
private:
    RemoteDirNotify notifier;
};

#endif
