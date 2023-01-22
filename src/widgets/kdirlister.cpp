/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2003-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirlister.h"
#include <KJobUiDelegate>
#include <KJobWidgets>
#include <kio/listjob.h>

#include <QWidget>

class KDirListerPrivate
{
public:
    KDirListerPrivate()
    {
    }

    QWidget *m_window = nullptr; // Main window this lister is associated with
};

KDirLister::KDirLister(QObject *parent)
    : KCoreDirLister(parent)
    , d(new KDirListerPrivate)
{
}

KDirLister::~KDirLister()
{
}

bool KDirLister::autoErrorHandlingEnabled() const
{
    return KCoreDirLister::autoErrorHandlingEnabled();
}

void KDirLister::setMainWindow(QWidget *window)
{
    d->m_window = window;
}

QWidget *KDirLister::mainWindow()
{
    return d->m_window;
}

void KDirLister::jobStarted(KIO::ListJob *job)
{
    if (d->m_window) {
        KJobWidgets::setWindow(job, d->m_window);
    }
}

#include "moc_kdirlister.cpp"
