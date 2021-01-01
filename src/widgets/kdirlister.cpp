/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2003-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kdirlister.h"
#include <kio/listjob.h>
#include <KJobWidgets>
#include <KJobUiDelegate>

#include <KMessageBox>
#include <QWidget>

class KDirListerPrivate
{
public:
    KDirListerPrivate()
    {
    }

    QWidget *m_errorParent = nullptr;
    QWidget *m_window = nullptr; // Main window this lister is associated with
    bool m_autoErrorHandling = false;
};

KDirLister::KDirLister(QObject *parent)
    : KCoreDirLister(parent), d(new KDirListerPrivate)
{
    setAutoErrorHandlingEnabled(true, nullptr);
}

KDirLister::~KDirLister()
{
}

bool KDirLister::autoErrorHandlingEnabled() const
{
    return d->m_autoErrorHandling;
}

void KDirLister::setAutoErrorHandlingEnabled(bool enable, QWidget *parent)
{
    d->m_autoErrorHandling = enable;
    d->m_errorParent = parent;
}

void KDirLister::setMainWindow(QWidget *window)
{
    d->m_window = window;
}

QWidget *KDirLister::mainWindow()
{
    return d->m_window;
}

void KDirLister::handleError(KIO::Job *job)
{
    if (d->m_autoErrorHandling) {
        job->uiDelegate()->showErrorMessage();
    } else {
        KCoreDirLister::handleError(job);
    }
}

void KDirLister::handleErrorMessage(const QString &message)
{
    if (d->m_autoErrorHandling) {
        KMessageBox::error(d->m_errorParent, message);
    } else {
        KCoreDirLister::handleErrorMessage(message);
    }
}

void KDirLister::jobStarted(KIO::ListJob *job)
{
    if (d->m_window) {
        KJobWidgets::setWindow(job, d->m_window);
    }
}

#include "moc_kdirlister.cpp"
