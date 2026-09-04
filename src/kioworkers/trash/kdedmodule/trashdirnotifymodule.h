/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2026 Ramil Nurmanov <ramil2004nur@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef TRASHDIRNOTIFYMODULE_H
#define TRASHDIRNOTIFYMODULE_H

#include <KDEDModule>

#include "trashdirnotify.h"

class TrashDirNotifyModule : public KDEDModule
{
    Q_OBJECT
public:
    TrashDirNotifyModule(QObject *parent, const QList<QVariant> &);

private:
    TrashDirNotify notifier;
};

#endif
