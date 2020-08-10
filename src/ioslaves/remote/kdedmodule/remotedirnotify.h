/*
    This file is part of the KDE Project
    SPDX-FileCopyrightText: 2004 Kévin Ottens <ervin ipsquad net>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef REMOTEDIRNOTIFY_H
#define REMOTEDIRNOTIFY_H

#include <QObject>

class KDirWatch;

class RemoteDirNotify : public QObject
{
    Q_OBJECT

public:
    RemoteDirNotify();

private Q_SLOTS:
    void slotRemoteChanged();

private:
    KDirWatch *m_dirWatch;
};

#endif
