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

#include <KMessageBox>
#include <QWidget>

class KDirListerPrivate
{
public:
    KDirListerPrivate()
    {
    }

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 82)
    QWidget *m_errorParent = nullptr;
#endif
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

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 82)
void KDirLister::setAutoErrorHandlingEnabled(bool enable, QWidget *parent)
{
    KCoreDirLister::setAutoErrorHandlingEnabled(enable);
    d->m_errorParent = parent;
}
#endif

void KDirLister::setMainWindow(QWidget *window)
{
    d->m_window = window;
}

QWidget *KDirLister::mainWindow()
{
    return d->m_window;
}

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 82)
void KDirLister::handleError(KIO::Job *job)
{
    // auto error handling moved to KCoreDirLister
    KCoreDirLister::handleError(job);
}
#endif

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 81)
void KDirLister::handleErrorMessage(const QString &message) // not called anymore
{
    // auto error handling moved to KCoreDirLister
    KCoreDirLister::handleErrorMessage(message);
}
#endif

void KDirLister::jobStarted(KIO::ListJob *job)
{
    if (d->m_window) {
        KJobWidgets::setWindow(job, d->m_window);
    }
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 82)
    else if (d->m_errorParent) {
        KJobWidgets::setWindow(job, d->m_errorParent);
    }
#endif
}

#include "moc_kdirlister.cpp"
