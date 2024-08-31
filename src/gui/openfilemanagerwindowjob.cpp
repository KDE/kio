/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Sune Stolborg Vuorela <sune@vuorela.dk>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openfilemanagerwindowjob.h"
#include "openfilemanagerwindowjob_p.h"

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif
#if defined(Q_OS_WINDOWS)
#include <QDir>
#include <shlobj.h>
#include <vector>
#endif
#include <QGuiApplication>

#include <KWindowSystem>

#include "config-kiogui.h"
#if HAVE_WAYLAND
#include <KWaylandExtras>
#endif

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

#ifdef WITH_QTDBUS
    void createDBusStrategy()
    {
        strategy = std::make_unique<OpenFileManagerWindowDBusStrategy>();
    }
#endif
#if defined(Q_OS_WINDOWS)
    void createWindowsShellStrategy()
    {
        strategy = std::make_unique<OpenFileManagerWindowWindowsShellStrategy>();
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
#ifdef WITH_QTDBUS
    d->createDBusStrategy();
#elif defined(Q_OS_WINDOWS)
    d->createWindowsShellStrategy();
#else
    d->createKRunStrategy();
#endif

    connect(d->strategy.get(), &AbstractOpenFileManagerWindowStrategy::finished, this, [this](int result) {
        if (result == KJob::NoError) {
            emitResult();
        } else {
#ifdef WITH_QTDBUS
            // DBus strategy failed, fall back to KRun strategy
            d->strategy = std::make_unique<OpenFileManagerWindowKRunStrategy>(this);
            d->strategy->start(d->highlightUrls, d->startupId);

            connect(d->strategy.get(), &KIO::AbstractOpenFileManagerWindowStrategy::finished, this, [this](int result) {
                setError(result);
                emitResult();
            });
#else
            setError(result);
            emitResult();
#endif
        }
    });
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

#ifdef WITH_QTDBUS
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
        QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, [urls, asn, this](QDBusPendingCallWatcher *watcher) {
            QDBusPendingReply<void> reply = *watcher;
            watcher->deleteLater();

            Q_EMIT finished(reply.isError() ? KJob::UserDefinedError : KJob::NoError);
        });
    };

    if (asn.isEmpty()) {
#if HAVE_WAYLAND
        if (KWindowSystem::isPlatformWayland()) {
            auto window = qGuiApp->focusWindow();
            if (!window && !qGuiApp->allWindows().isEmpty()) {
                window = qGuiApp->allWindows().constFirst();
            }
            const int launchedSerial = KWaylandExtras::lastInputSerial(window);
            QObject::connect(
                KWaylandExtras::self(),
                &KWaylandExtras::xdgActivationTokenArrived,
                this,
                [launchedSerial, runWithToken](int serial, const QString &token) {
                    if (serial == launchedSerial) {
                        runWithToken(token.toUtf8());
                    }
                },
                Qt::SingleShotConnection);
            KWaylandExtras::requestXdgActivationToken(window, launchedSerial, {});
        } else {
            runWithToken({});
        }
#else
        runWithToken({});
#endif
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
    QObject::connect(urlJob, &KJob::result, this, [this](KJob *urlJob) {
        if (urlJob->error()) {
            Q_EMIT finished(OpenFileManagerWindowJob::LaunchFailedError);
        } else {
            Q_EMIT finished(KJob::NoError);
        }
    });
    urlJob->start();
}

#if defined(Q_OS_WINDOWS)
void OpenFileManagerWindowWindowsShellStrategy::start(const QList<QUrl> &urls, const QByteArray &asn)
{
    Q_UNUSED(asn);
    LPITEMIDLIST dir = ILCreateFromPathW(QDir::toNativeSeparators(urls.at(0).adjusted(QUrl::RemoveFilename).toLocalFile()).toStdWString().data());

    std::vector<LPCITEMIDLIST> items;
    for (const auto &url : urls) {
        LPITEMIDLIST item = ILCreateFromPathW(QDir::toNativeSeparators(url.toLocalFile()).toStdWString().data());
        items.push_back(item);
    }

    auto result = SHOpenFolderAndSelectItems(dir, items.size(), items.data(), 0);
    if (SUCCEEDED(result)) {
        Q_EMIT finished(KJob::NoError);
    } else {
        Q_EMIT finished(OpenFileManagerWindowJob::LaunchFailedError);
    }
    ILFree(dir);
    for (auto &item : items) {
        ILFree(const_cast<LPITEMIDLIST>(item));
    }
}
#endif
} // namespace KIO

#include "moc_openfilemanagerwindowjob.cpp"
