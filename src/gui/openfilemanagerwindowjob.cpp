/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openfilemanagerwindowjob.h"
#include "openfilemanagerwindowjob_p.h"

#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif
#include <QGuiApplication>

#include <KWindowSystem>

#include <KIO/OpenUrlJob>

namespace KIO
{
class OpenFileManagerWindowJobPrivate
{
public:
    OpenFileManagerWindowJobPrivate(OpenFileManagerWindowJob *qq)
        : q(qq)
        , strategy(nullptr)
    {
    }

    ~OpenFileManagerWindowJobPrivate() = default;

#ifdef Q_OS_LINUX
    void createDBusStrategy()
    {
        strategy = std::make_unique<OpenFileManagerWindowDBusStrategy>(q);
    }
#endif

    void createKRunStrategy()
    {
        strategy = std::make_unique<OpenFileManagerWindowKRunStrategy>(q);
    }

    OpenFileManagerWindowJob *const q;
    QList<QUrl> highlightUrls;
    QByteArray startupId;

    std::unique_ptr<AbstractOpenFileManagerWindowStrategy> strategy;
};

OpenFileManagerWindowJob::OpenFileManagerWindowJob(QObject *parent)
    : KJob(parent)
    , d(new OpenFileManagerWindowJobPrivate(this))
{
#ifdef Q_OS_LINUX
    d->createDBusStrategy();
#else
    d->createKRunStrategy();
#endif
}

OpenFileManagerWindowJob::~OpenFileManagerWindowJob() = default;

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

#ifdef Q_OS_LINUX
void OpenFileManagerWindowDBusStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    // see the spec at: https://www.freedesktop.org/wiki/Specifications/file-manager-interface/

    auto runWithToken = [this, urls](const QByteArray &asn) {
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
                m_job->d->createKRunStrategy();
                m_job->d->strategy->start(urls, asn);
                return;
            }

            emitResultProxy();
        });
    };

    if (asn.isEmpty()) {
        auto window = qGuiApp->focusWindow();
        if (!window && !qGuiApp->allWindows().isEmpty()) {
            window = qGuiApp->allWindows().constFirst();
        }
        const int launchedSerial = KWindowSystem::lastInputSerial(window);
        QObject::connect(
            KWindowSystem::self(),
            &KWindowSystem::xdgActivationTokenArrived,
            m_job,
            [launchedSerial, runWithToken](int serial, const QString &token) {
                if (serial == launchedSerial) {
                    runWithToken(token.toUtf8());
                }
            },
            Qt::SingleShotConnection);
        KWindowSystem::requestXdgActivationToken(window, launchedSerial, {});
    } else {
        runWithToken(asn);
    }
}
#endif

void OpenFileManagerWindowKRunStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    KIO::OpenUrlJob *urlJob = new KIO::OpenUrlJob(urls.at(0).adjusted(QUrl::RemoveFilename), QStringLiteral("inode/directory"));
    urlJob->setUiDelegate(m_job->uiDelegate());
    urlJob->setStartupId(asn);
    QObject::connect(urlJob, &KJob::result, m_job, [this](KJob *urlJob) {
        if (urlJob->error()) {
            emitResultProxy(OpenFileManagerWindowJob::LaunchFailedError);
        } else {
            emitResultProxy();
        }
    });
    urlJob->start();
}

} // namespace KIO

#include "moc_openfilemanagerwindowjob.cpp"
