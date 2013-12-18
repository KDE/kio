/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
                 2000 Carsten Pfeiffer <pfeiffer@kde.org>
                 2003-2005 David Faure <faure@kde.org>
                 2001-2006 Michael Brade <brade@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kdirlister.h"
#include <kio/jobclasses.h> // ListJob
#include <kjobwidgets.h>
#include <kjobuidelegate.h>

#include <kmessagebox.h>
#include <QWidget>

class KDirLister::Private
{
public:
    Private()
    : errorParent(NULL),
      window(NULL),
      autoErrorHandling(false)
    {}

    QWidget *errorParent;
    QWidget *window; // Main window this lister is associated with
    bool autoErrorHandling;

};

KDirLister::KDirLister(QObject* parent)
    : KCoreDirLister(parent), d(new Private)
{
    setAutoErrorHandlingEnabled(true, 0);
}

KDirLister::~KDirLister()
{
    delete d;
}

bool KDirLister::autoErrorHandlingEnabled() const
{
    return d->autoErrorHandling;
}

void KDirLister::setAutoErrorHandlingEnabled( bool enable, QWidget* parent )
{
    d->autoErrorHandling = enable;
    d->errorParent = parent;
}

void KDirLister::setMainWindow( QWidget *window )
{
    d->window = window;
}

QWidget *KDirLister::mainWindow()
{
    return d->window;
}

void KDirLister::handleError(KIO::Job *job)
{
    if (d->autoErrorHandling)
        job->ui()->showErrorMessage();
    else
        KCoreDirLister::handleError(job);
}

void KDirLister::handleErrorMessage(const QString &message)
{
    if (d->autoErrorHandling)
        KMessageBox::error(d->errorParent, message);
    else
        KCoreDirLister::handleErrorMessage(message);
}

void KDirLister::jobStarted(KIO::ListJob *job)
{
    if (d->window)
        KJobWidgets::setWindow(job, d->window);
}

#include "moc_kdirlister.cpp"
