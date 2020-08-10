/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openfilemanagerwindowjob.h"
#include "openfilemanagerwindowjob_p.h"

#include <QDBusMessage>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>


#include <KRun>
#include <KJobWidgets>

#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

namespace KIO
{

class OpenFileManagerWindowJobPrivate
{
public:
    OpenFileManagerWindowJobPrivate()
        : strategy(nullptr)
    {

    }

    ~OpenFileManagerWindowJobPrivate()
    {
        delete strategy;
    }

    QList<QUrl> highlightUrls;
    QByteArray startupId;

    AbstractOpenFileManagerWindowStrategy *strategy;
};

OpenFileManagerWindowJob::OpenFileManagerWindowJob(QObject *parent)
    : KJob(parent)
    , d(new OpenFileManagerWindowJobPrivate())
{

#ifdef Q_OS_LINUX
    d->strategy = new OpenFileManagerWindowDBusStrategy(this);
#else
    d->strategy = new OpenFileManagerWindowKRunStrategy(this);
#endif

}

OpenFileManagerWindowJob::~OpenFileManagerWindowJob()
{
    delete d;
}

QList<QUrl> OpenFileManagerWindowJob::highlightUrls() const
{
    return d->highlightUrls;
}

void OpenFileManagerWindowJob::setHighlightUrls(const QList<QUrl> &highlightUrls)
{
    d->highlightUrls = highlightUrls;
}

QByteArray OpenFileManagerWindowJob::startupId() const
{
    return d->startupId;
}

void OpenFileManagerWindowJob::setStartupId(const QByteArray &startupId)
{
    d->startupId = startupId;
}

void OpenFileManagerWindowJob::start()
{
    if (d->highlightUrls.isEmpty()) {
        setError(NoValidUrlsError);
        emitResult();
        return;
    }

    d->strategy->start(d->highlightUrls, d->startupId);
}

OpenFileManagerWindowJob *highlightInFileManager(const QList<QUrl> &urls, const QByteArray &asn)
{
    auto *job = new OpenFileManagerWindowJob();
    job->setHighlightUrls(urls);
    job->setStartupId(asn);
    job->start();
    return job;
}

void OpenFileManagerWindowDBusStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    // see the spec at: https://www.freedesktop.org/wiki/Specifications/file-manager-interface/

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                      QStringLiteral("/org/freedesktop/FileManager1"),
                                                      QStringLiteral("org.freedesktop.FileManager1"),
                                                      QStringLiteral("ShowItems"));

    msg << QUrl::toStringList(urls) << QString::fromUtf8(asn);

    QDBusPendingReply<void> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, job);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, job, [=](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<void> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            // Try the KRun strategy as fallback, also calls emitResult inside
            OpenFileManagerWindowKRunStrategy kRunStrategy(job);
            kRunStrategy.start(urls, asn);
            return;
        }

        emitResultProxy();
    });
}

void OpenFileManagerWindowKRunStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    KIO::OpenUrlJob *urlJob = new KIO::OpenUrlJob(urls.at(0).adjusted(QUrl::RemoveFilename), QStringLiteral("inode/directory"));
    urlJob->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, KJobWidgets::window(urlJob)));
    urlJob->setStartupId(asn);
    QObject::connect(urlJob, &KJob::result, job, [this](KJob *urlJob) {
        if (urlJob->error()) {
            emitResultProxy(OpenFileManagerWindowJob::LaunchFailedError);
        } else {
            emitResultProxy();
        }
    });
    urlJob->start();
}

} // namespace KIO
