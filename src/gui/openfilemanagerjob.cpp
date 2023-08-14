/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openfilemanagerjob.h"
#include "openfilemanagerjob_p.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>

#include <KWindowSystem>

#include <KIO/OpenUrlJob>

namespace KIO
{
class OpenFileManagerJobPrivate
{
public:
    OpenFileManagerJobPrivate(OpenFileManagerJob *qq)
        : q(qq)
        , strategy(nullptr)
    {
    }

    ~OpenFileManagerJobPrivate()
    {
        delete strategy;
    }

    AbstractOpenFileManagerJobStrategy *createDBusStrategy()
    {
        delete strategy;
        strategy = new OpenFileManagerDBusStrategy(q);
        return strategy;
    }

    AbstractOpenFileManagerJobStrategy *createKRunStrategy()
    {
        delete strategy;
        strategy = new OpenFileManagerKRunStrategy(q);
        return strategy;
    }

    OpenFileManagerJob *const q;
    QList<QUrl> highlightUrls;
    QByteArray startupId;

    AbstractOpenFileManagerJobStrategy *strategy;
};

OpenFileManagerJob::OpenFileManagerJob(QObject *parent)
    : KJob(parent)
    , d(new OpenFileManagerJobPrivate(this))
{
#ifdef Q_OS_LINUX
    d->createDBusStrategy();
#else
    d->createKRunStrategy();
#endif
}

OpenFileManagerJob::~OpenFileManagerJob() = default;

QList<QUrl> OpenFileManagerJob::highlightUrls() const
{
    return d->highlightUrls;
}

void OpenFileManagerJob::setHighlightUrls(const QList<QUrl> &highlightUrls)
{
    d->highlightUrls = highlightUrls;
}

QByteArray OpenFileManagerJob::startupId() const
{
    return d->startupId;
}

void OpenFileManagerJob::setStartupId(const QByteArray &startupId)
{
    d->startupId = startupId;
}

void OpenFileManagerJob::start()
{
    if (d->highlightUrls.isEmpty()) {
        setError(NoValidUrlsError);
        emitResult();
        return;
    }

    d->strategy->start(d->highlightUrls, d->startupId);
}

OpenFileManagerJob *highlightInFileManager(const QList<QUrl> &urls, const QByteArray &asn, KJobUiDelegate *delegate)
{
    auto *job = new OpenFileManagerJob();
    job->setHighlightUrls(urls);
    job->setUiDelegate(delegate);

    if (asn.isNull()) {
        auto window = qGuiApp->focusWindow();
        if (!window && !qGuiApp->allWindows().isEmpty()) {
            window = qGuiApp->allWindows().constFirst();
        }
        const int launchedSerial = KWindowSystem::lastInputSerial(window);
        QObject::connect(KWindowSystem::self(), &KWindowSystem::xdgActivationTokenArrived, job, [launchedSerial, job](int serial, const QString &token) {
            if (serial == launchedSerial) {
                job->setStartupId(token.toLatin1());
                job->start();
            }
        });
        KWindowSystem::requestXdgActivationToken(window, launchedSerial, {});
    } else {
        job->setStartupId(asn);
        job->start();
    }

    return job;
}

void OpenFileManagerDBusStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    // see the spec at: https://www.freedesktop.org/wiki/Specifications/file-manager-interface/

    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.FileManager1"),
                                                      QStringLiteral("/org/freedesktop/FileManager1"),
                                                      QStringLiteral("org.freedesktop.FileManager1"),
                                                      QStringLiteral("ShowItems"));

    msg << QUrl::toStringList(urls) << QString::fromUtf8(asn);

    QDBusPendingReply<void> reply = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, m_job);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, m_job, [=](QDBusPendingCallWatcher *watcher) {
        QDBusPendingReply<void> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            // Try the KRun strategy as fallback, also calls emitResult inside
            AbstractOpenFileManagerJobStrategy *kRunStrategy = m_job->d->createKRunStrategy();
            kRunStrategy->start(urls, asn);
            return;
        }

        emitResultProxy();
    });
}

void OpenFileManagerKRunStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    KIO::OpenUrlJob *urlJob = new KIO::OpenUrlJob(urls.at(0).adjusted(QUrl::RemoveFilename), QStringLiteral("inode/directory"));
    urlJob->setUiDelegate(m_job->uiDelegate());
    urlJob->setStartupId(asn);
    QObject::connect(urlJob, &KJob::result, m_job, [this](KJob *urlJob) {
        if (urlJob->error()) {
            emitResultProxy(OpenFileManagerJob::LaunchFailedError);
        } else {
            emitResultProxy();
        }
    });
    urlJob->start();
}

} // namespace KIO
