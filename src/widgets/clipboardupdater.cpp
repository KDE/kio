/* This file is part of the KDE libraries
    Copyright (C) 2013 Dawit Alemayehu <adawit@kde.org>

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

#include "clipboardupdater_p.h"
#include "jobclasses.h"
#include "copyjob.h"
#include "deletejob.h"
#include <kurlmimedata.h>

#include <QGuiApplication>
#include <QMimeData>
#include <QClipboard>

using namespace KIO;


static void overwriteUrlsInClipboard(KJob* job)
{
    CopyJob* copyJob = qobject_cast<CopyJob*>(job);
    FileCopyJob* fileCopyJob = qobject_cast<FileCopyJob*>(job);

    if (!copyJob && !fileCopyJob) {
        return;
    }

    QList<QUrl> newUrls;

    if (copyJob) {
        Q_FOREACH(const QUrl& url, copyJob->srcUrls()) {
            QUrl dUrl = copyJob->destUrl().adjusted(QUrl::StripTrailingSlash);
            dUrl.setPath(dUrl.path() + '/' + url.fileName());
            newUrls.append(dUrl);
        }
    } else if (fileCopyJob) {
        newUrls << fileCopyJob->destUrl();
    }

    QMimeData* mime = new QMimeData();
    mime->setUrls(newUrls);
    QGuiApplication::clipboard()->setMimeData(mime);
}

static void updateUrlsInClipboard(KJob* job)
{
    CopyJob* copyJob = qobject_cast<CopyJob*>(job);
    FileCopyJob* fileCopyJob = qobject_cast<FileCopyJob*>(job);

    if (!copyJob && !fileCopyJob) {
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
    bool update = false;

    if (copyJob) {
        Q_FOREACH(const QUrl& url, copyJob->srcUrls()) {
            const int index = clipboardUrls.indexOf(url);
            if (index > -1) {
                QUrl dUrl = copyJob->destUrl().adjusted(QUrl::StripTrailingSlash);
                dUrl.setPath(dUrl.path() + '/' + url.fileName());
                clipboardUrls.replace(index, dUrl);
                update = true;
            }
        }
    } else if (fileCopyJob) {
        const int index = clipboardUrls.indexOf(fileCopyJob->srcUrl());
        if (index > -1) {
            clipboardUrls.replace(index, fileCopyJob->destUrl());
            update = true;
        }
    }

    if (update) {
        QMimeData* mime = new QMimeData();
        mime->setUrls(clipboardUrls);
        clipboard->setMimeData(mime);
    }
}

static void removeUrlsFromClipboard(KJob* job)
{
    SimpleJob* simpleJob = qobject_cast<SimpleJob*>(job);
    DeleteJob* deleteJob = qobject_cast<DeleteJob*>(job);

    if (!simpleJob && !deleteJob) {
        return;
    }

    QList<QUrl> deletedUrls;
    if (simpleJob) {
        deletedUrls << simpleJob->url();
    } else if (deleteJob) {
        deletedUrls << deleteJob->urls();
    }

    if (deletedUrls.isEmpty()) {
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
    quint32 removedCount = 0;

    Q_FOREACH(const QUrl& url, deletedUrls) {
        removedCount += clipboardUrls.removeAll(url);
    }

    if (removedCount > 0) {
        QMimeData* mime = new QMimeData();
        if (!clipboardUrls.isEmpty()) {
            mime->setUrls(clipboardUrls);
        }
        clipboard->setMimeData(mime);
    }
}

void ClipboardUpdater::slotResult(KJob* job)
{
    if (job->error()) {
        return;
    }

    switch (m_mode) {
    case JobUiDelegateExtension::UpdateContent:
        updateUrlsInClipboard(job);
        break;
    case JobUiDelegateExtension::OverwriteContent:
        overwriteUrlsInClipboard(job);
        break;
    case JobUiDelegateExtension::RemoveContent:
        removeUrlsFromClipboard(job);
        break;
    }
}

void ClipboardUpdater::setMode(JobUiDelegateExtension::ClipboardUpdaterMode mode)
{
    m_mode = mode;
}

void ClipboardUpdater::update(const QUrl& srcUrl, const QUrl& destUrl)
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard->mimeData()->hasUrls()) {
        QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
        const int index = clipboardUrls.indexOf(srcUrl);
        if (index > -1) {
            clipboardUrls.replace(index, destUrl);
            QMimeData* mime = new QMimeData();
            mime->setUrls(clipboardUrls);
            clipboard->setMimeData(mime);
        }
    }
}

ClipboardUpdater::ClipboardUpdater(Job* job, JobUiDelegateExtension::ClipboardUpdaterMode mode)
    :QObject(job),
     m_mode(mode)
{
    Q_ASSERT(job);
    connect(job, SIGNAL(result(KJob*)), this, SLOT(slotResult(KJob*)));
}
