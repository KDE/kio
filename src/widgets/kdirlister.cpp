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

class Q_DECL_HIDDEN KDirLister::Private
{
public:
    Private()
        : errorParent(nullptr),
          window(nullptr),
          autoErrorHandling(false)
    {}

    QWidget *errorParent;
    QWidget *window; // Main window this lister is associated with
    bool autoErrorHandling;

};

KDirLister::KDirLister(QObject *parent)
    : KCoreDirLister(parent), d(new Private)
{
    setAutoErrorHandlingEnabled(true, nullptr);
}

KDirLister::~KDirLister()
{
    delete d;
}

bool KDirLister::autoErrorHandlingEnabled() const
{
    return d->autoErrorHandling;
}

void KDirLister::setAutoErrorHandlingEnabled(bool enable, QWidget *parent)
{
    d->autoErrorHandling = enable;
    d->errorParent = parent;
}

void KDirLister::setMainWindow(QWidget *window)
{
    d->window = window;
}

QWidget *KDirLister::mainWindow()
{
    return d->window;
}

void KDirLister::handleError(KIO::Job *job)
{
    if (d->autoErrorHandling) {
        job->uiDelegate()->showErrorMessage();
    } else {
        KCoreDirLister::handleError(job);
    }
}

void KDirLister::handleErrorMessage(const QString &message)
{
    if (d->autoErrorHandling) {
        KMessageBox::error(d->errorParent, message);
    } else {
        KCoreDirLister::handleErrorMessage(message);
    }
}

void KDirLister::jobStarted(KIO::ListJob *job)
{
    if (d->window) {
        KJobWidgets::setWindow(job, d->window);
    }
}

#include "moc_kdirlister.cpp"
