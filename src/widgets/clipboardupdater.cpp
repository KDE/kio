/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "clipboardupdater_p.h"
#include "jobclasses.h"
#include "copyjob.h"
#include "deletejob.h"
#include <KUrlMimeData>
#include "../pathhelpers_p.h"

#include <QGuiApplication>
#include <QMimeData>
#include <QClipboard>

using namespace KIO;

static void overwriteUrlsInClipboard(KJob *job)
{
    CopyJob *copyJob = qobject_cast<CopyJob *>(job);
    FileCopyJob *fileCopyJob = qobject_cast<FileCopyJob *>(job);

    if (!copyJob && !fileCopyJob) {
        return;
    }

    QList<QUrl> newUrls;

    if (copyJob) {
        const auto srcUrls = copyJob->srcUrls();
        newUrls.reserve(srcUrls.size());
        for (const QUrl &url : srcUrls) {
            QUrl dUrl = copyJob->destUrl().adjusted(QUrl::StripTrailingSlash);
            dUrl.setPath(concatPaths(dUrl.path(), url.fileName()));
            newUrls.append(dUrl);
        }
    } else if (fileCopyJob) {
        newUrls << fileCopyJob->destUrl();
    }

    QMimeData *mime = new QMimeData();
    mime->setUrls(newUrls);
    QGuiApplication::clipboard()->setMimeData(mime);
}

static void updateUrlsInClipboard(KJob *job)
{
    CopyJob *copyJob = qobject_cast<CopyJob *>(job);
    FileCopyJob *fileCopyJob = qobject_cast<FileCopyJob *>(job);

    if (!copyJob && !fileCopyJob) {
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    auto mimeData = clipboard->mimeData();
    if (!mimeData) {
        return;
    }

    QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(mimeData);
    bool update = false;

    if (copyJob) {
        const QList<QUrl> urls = copyJob->srcUrls();
        for (const QUrl &url : urls) {
            const int index = clipboardUrls.indexOf(url);
            if (index > -1) {
                QUrl dUrl = copyJob->destUrl().adjusted(QUrl::StripTrailingSlash);
                dUrl.setPath(concatPaths(dUrl.path(), url.fileName()));
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
        QMimeData *mime = new QMimeData();
        mime->setUrls(clipboardUrls);
        clipboard->setMimeData(mime);
    }
}

static void removeUrlsFromClipboard(KJob *job)
{
    SimpleJob *simpleJob = qobject_cast<SimpleJob *>(job);
    DeleteJob *deleteJob = qobject_cast<DeleteJob *>(job);

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

    QClipboard *clipboard = QGuiApplication::clipboard();
    auto mimeData = clipboard->mimeData();
    if (!mimeData) {
        return;
    }

    QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(mimeData);
    quint32 removedCount = 0;

    for (const QUrl &url : qAsConst(deletedUrls)) {
        removedCount += clipboardUrls.removeAll(url);
    }

    if (removedCount > 0) {
        QMimeData *mime = new QMimeData();
        if (!clipboardUrls.isEmpty()) {
            mime->setUrls(clipboardUrls);
        }
        clipboard->setMimeData(mime);
    }
}

void ClipboardUpdater::slotResult(KJob *job)
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

void ClipboardUpdater::update(const QUrl &srcUrl, const QUrl &destUrl)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    auto mimeData = clipboard->mimeData();
    if (mimeData && mimeData->hasUrls()) {
        QList<QUrl> clipboardUrls = KUrlMimeData::urlsFromMimeData(clipboard->mimeData());
        const int index = clipboardUrls.indexOf(srcUrl);
        if (index > -1) {
            clipboardUrls.replace(index, destUrl);
            QMimeData *mime = new QMimeData();
            mime->setUrls(clipboardUrls);
            clipboard->setMimeData(mime);
        }
    }
}

ClipboardUpdater::ClipboardUpdater(Job *job, JobUiDelegateExtension::ClipboardUpdaterMode mode)
    : QObject(job),
      m_mode(mode)
{
    Q_ASSERT(job);
    connect(job, &KJob::result, this, &ClipboardUpdater::slotResult);
}
