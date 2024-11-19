/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "../utils_p.h"
#include "clipboardupdater_p.h"
#include "copyjob.h"
#include "deletejob.h"
#include "filecopyjob.h"
#include "simplejob.h"
#include <KUrlMimeData>

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QMimeData>

using namespace KIO;

namespace
{
QByteArray createUuidFromFileJob(KJob *job)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);

    if (CopyJob *copyJob = qobject_cast<CopyJob *>(job)) {
        const QList<QUrl> urls = copyJob->srcUrls();
        for (const QUrl &url : urls) {
            hash.addData(url.toEncoded());
        }
        hash.addData(copyJob->destUrl().toEncoded());
    } else if (FileCopyJob *fileCopyJob = qobject_cast<FileCopyJob *>(job)) {
        hash.addData(fileCopyJob->srcUrl().toEncoded());
        hash.addData(fileCopyJob->destUrl().toEncoded());
    } else if (SimpleJob *simpleJob = qobject_cast<SimpleJob *>(job)) {
        hash.addData(simpleJob->url().toEncoded());
    } else if (DeleteJob *deleteJob = qobject_cast<DeleteJob *>(job)) {
        const QList<QUrl> urls = deleteJob->urls();
        for (const QUrl &url : urls) {
            hash.addData(url.toEncoded());
        }
    } else {
        return {};
    }

    return hash.result();
}

void overwriteUrlsInClipboard(KJob *job, const QByteArray &uuid)
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
            dUrl.setPath(Utils::concatPaths(dUrl.path(), url.fileName()));
            newUrls.append(dUrl);
        }
    } else if (fileCopyJob) {
        newUrls << fileCopyJob->destUrl();
    }

    QMimeData *mime = new QMimeData();
    mime->setUrls(newUrls);
    mime->setData(QStringLiteral("application/x-kde-kio-clipboardupdater"), uuid);
    QGuiApplication::clipboard()->setMimeData(mime);
}

void updateUrlsInClipboard(KJob *job, const QByteArray &uuid)
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
                dUrl.setPath(Utils::concatPaths(dUrl.path(), url.fileName()));
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
        mime->setData(QStringLiteral("application/x-kde-kio-clipboardupdater"), uuid);
        clipboard->setMimeData(mime);
    }
}

void removeUrlsFromClipboard(KJob *job, const QByteArray &uuid)
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

    for (const QUrl &url : std::as_const(deletedUrls)) {
        removedCount += clipboardUrls.removeAll(url);
    }

    if (removedCount > 0) {
        QMimeData *mime = new QMimeData();
        if (!clipboardUrls.isEmpty()) {
            mime->setUrls(clipboardUrls);
            mime->setData(QStringLiteral("application/x-kde-kio-clipboardupdater"), uuid);
        }
        clipboard->setMimeData(mime);
    }
}
}

void ClipboardUpdater::slotResult(KJob *job)
{
    if (job->error()) {
        return;
    }

    switch (m_mode) {
    case JobUiDelegateExtension::UpdateContent:
        updateUrlsInClipboard(job, m_uuid);
        break;
    case JobUiDelegateExtension::OverwriteContent:
        overwriteUrlsInClipboard(job, m_uuid);
        break;
    case JobUiDelegateExtension::RemoveContent:
        removeUrlsFromClipboard(job, m_uuid);
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
    : QObject(job)
    , m_mode(mode)
    , m_uuid(createUuidFromFileJob(job))
{
    Q_ASSERT(job);
    connect(job, &KJob::result, this, &ClipboardUpdater::slotResult);
}

#include "moc_clipboardupdater_p.cpp"
