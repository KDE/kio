/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "openfilemanagerwindowjob.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QGuiApplication>

#include <KJobWidgets>
#include <KWindowSystem>

#include <KIO/JobUiDelegate>
#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenFileManagerJob>
#include <KIO/OpenUrlJob>

namespace KIO
{
class OpenFileManagerWindowJobPrivate
{
public:
    OpenFileManagerWindowJobPrivate(OpenFileManagerJob *job)
        : underlying(job)
    {
    }

    ~OpenFileManagerWindowJobPrivate()
    {
    }

    OpenFileManagerJob *const underlying;
};

OpenFileManagerWindowJob::OpenFileManagerWindowJob(QObject *parent)
    : KJob(parent)
    , d(new OpenFileManagerWindowJobPrivate(new OpenFileManagerJob(this)))
{
}

OpenFileManagerWindowJob::~OpenFileManagerWindowJob() = default;

QList<QUrl> OpenFileManagerWindowJob::highlightUrls() const
{
    return d->underlying->highlightUrls();
}

void OpenFileManagerWindowJob::setHighlightUrls(const QList<QUrl> &highlightUrls)
{
    d->underlying->setHighlightUrls(highlightUrls);
}

QByteArray OpenFileManagerWindowJob::startupId() const
{
    return d->underlying->startupId();
}

void OpenFileManagerWindowJob::setStartupId(const QByteArray &startupId)
{
    d->underlying->setStartupId(startupId);
}

void OpenFileManagerWindowJob::start()
{
    connect(d->underlying, &KJob::result, this, [this](KJob *job) {
        if (job->error()) {
            this->setError(job->error());
            this->setErrorText(job->errorText());
        }
        emitResult();
    });
    connect(d->underlying, &KJob::infoMessage, this, &OpenFileManagerWindowJob::infoMessage);

    d->underlying->start();
}

OpenFileManagerWindowJob *highlightInFileManager(const QList<QUrl> &urls, const QByteArray &asn)
{
    // because the OpenFileManagerJob's highlightInFileManager starts the job
    // we can't just call it from here. We kind of need to duplicate the logic
    // Luckily, by deprecating this function in here, it is only temporary
    auto *job = new OpenFileManagerWindowJob();
    job->setHighlightUrls(urls);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, KJobWidgets::window(job)));

    if (asn.isNull()) {
        auto window = qGuiApp->focusWindow();
        if (!window && !qGuiApp->allWindows().isEmpty()) {
            window = qGuiApp->allWindows().constFirst();
        }
        const int launchedSerial = KWindowSystem::lastInputSerial(window);
        QObject::connect(KWindowSystem::self(), &KWindowSystem::xdgActivationTokenArrived, job, [launchedSerial, job](int serial, const QString &token) {
            QObject::disconnect(KWindowSystem::self(), &KWindowSystem::xdgActivationTokenArrived, job, nullptr);
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

} // namespace KIO

#include "moc_openfilemanagerwindowjob.cpp"
