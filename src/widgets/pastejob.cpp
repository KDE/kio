/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2014 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "pastejob.h"
#include "pastejob_p.h"

#include "config-kiogui.h"

#include "kio_widgets_debug.h"
#include "paste.h"

#include <QGuiApplication>
#include <QMimeData>
#include <QTimer>

#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KJobWidgets>
#include <KLocalizedString>
#include <KUrlMimeData>

#if HAVE_WAYLAND
#include <KWaylandExtras>
#include <KWindowSystem>
#endif

#ifdef WITH_QTDBUS
#include <QDBusConnection>
#include <QDBusPendingCall>
#endif

using namespace KIO;

static const QString s_arkClipboardExtractService = QStringLiteral("application/x-kde-ark-clipboardextract-service");
static const QString s_arkClipboardExtractPath = QStringLiteral("application/x-kde-ark-clipboardextract-path");

extern std::pair<KIO::Job *, int> pasteMimeDataImpl(const QMimeData *mimeData, const QUrl &destUrl, const QString &dialogText, QWidget *widget, bool clipboard);

PasteJob::PasteJob(PasteJobPrivate &dd)
    : Job(dd)
{
    Q_D(PasteJob);

    QTimer::singleShot(0, this, [d]() {
        d->slotStart();
    });
}

PasteJob::~PasteJob()
{
}

QList<QUrl> sanitizeUrls(const QList<QUrl> &urls)
{
    QList<QUrl> result;
    for (const QUrl &url : urls) {
        if (url.isValid() && !url.scheme().isEmpty()) {
            result << url;
        } else {
            qCWarning(KIO_WIDGETS) << "PasteJob: Ignoring invalid URL" << url;
        }
    }
    return result;
}

bool PasteJobPrivate::hasArkClipboardFormat(const QMimeData *mimeData)
{
#ifdef WITH_QTDBUS
    return mimeData->hasFormat(s_arkClipboardExtractService) && mimeData->hasFormat(s_arkClipboardExtractPath);
#else
    Q_UNUSED(mimeData);
    return false;
#endif
}

void PasteJobPrivate::slotStart()
{
    Q_Q(PasteJob);
    if (!m_mimeData) {
        q->emitErrorResult(KIO::ERR_NO_CONTENT);
        return;
    }
    if (!KIO::canPasteMimeData(m_mimeData)) {
        q->emitResult();
        return;
    }

#ifdef WITH_QTDBUS
    const auto remoteArkDBusClient = QString::fromUtf8(m_mimeData->data(s_arkClipboardExtractService));
    const auto remoteArkDBusPath = QString::fromUtf8(m_mimeData->data(s_arkClipboardExtractPath));
    if (!remoteArkDBusClient.isEmpty() && !remoteArkDBusPath.isEmpty()) {
        const QString destDir = m_destDir.toDisplayString(QUrl::PreferLocalFile);
        auto doExtract = [q, remoteArkDBusClient, remoteArkDBusPath, destDir](const QString &activationToken) {
            QVariantMap args;
            if (!activationToken.isEmpty()) {
                args.insert(QStringLiteral("activation_token"), activationToken);
            }

            QDBusMessage message = QDBusMessage::createMethodCall(remoteArkDBusClient,
                                                                  remoteArkDBusPath,
                                                                  QStringLiteral("org.kde.ark.ClipboardExtract"),
                                                                  QStringLiteral("pasteFilesTo"));
            message.setArguments({destDir, args});

            const auto pending = QDBusConnection::sessionBus().asyncCall(message);
            auto watcher = std::make_shared<QDBusPendingCallWatcher>(pending);
            QObject::connect(watcher.get(), &QDBusPendingCallWatcher::finished, q, [watcher, q] {
                if (watcher->isError()) {
                    q->setError(KIO::ERR_UNKNOWN);
                    // Ark closed in the meantime?
                    if (watcher->error().type() == QDBusError::ServiceUnknown) {
                        q->setErrorText(i18nc("@info Error when copy paste from Ark failed", "Cannot paste, Ark does not appear to be running."));
                    } else {
                        q->setErrorText(watcher->error().message());
                    }
                }
                q->emitResult();
            });
        };

#if HAVE_WAYLAND
        if (KWindowSystem::isPlatformWayland()) {
            auto window = qGuiApp->focusWindow();
            if (!window && !qGuiApp->allWindows().isEmpty()) {
                window = qGuiApp->allWindows().constFirst();
            }
            if (window) {
                // Ark doesn't show a window unless there's an error or overwrite prompt,
                // not setting app ID to prevent bouncy cursor in the good case.
                KWaylandExtras::xdgActivationToken(window, QString()).then(q, doExtract);
                return;
            }
        }
#endif

        doExtract(QString());
        return;
    }
#endif

    const bool move = KIO::isClipboardDataCut(m_mimeData);
    KIO::Job *job = nullptr;
    KIO::CopyJob *copyJob = nullptr;
    if (m_mimeData->hasUrls()) {
        const QList<QUrl> urls = sanitizeUrls(KUrlMimeData::urlsFromMimeData(m_mimeData, KUrlMimeData::PreferLocalUrls));
        if (!urls.isEmpty()) {
            if (move) {
                copyJob = KIO::move(urls, m_destDir, m_flags);
            } else {
                copyJob = KIO::copy(urls, m_destDir, m_flags);
            }
            QObject::connect(copyJob, &KIO::CopyJob::copyingDone, q, [this](KIO::Job *job, const QUrl &src, const QUrl &dest) {
                slotCopyingDone(job, src, dest);
            });

            QObject::connect(copyJob, &KIO::CopyJob::copyingLinkDone, q, [this](KIO::Job *job, const QUrl &from, const QString &target, const QUrl &to) {
                slotCopyingLinkDone(job, from, target, to);
            });

            KIO::FileUndoManager::self()->recordJob(move ? KIO::FileUndoManager::Move : KIO::FileUndoManager::Copy, QList<QUrl>(), m_destDir, copyJob);
            job = copyJob;
        }
    } else {
        const QString dialogText = m_clipboard ? i18n("Filename for clipboard content:") : i18n("Filename for dropped contents:");
        auto [resultJob, error] = pasteMimeDataImpl(m_mimeData, m_destDir, dialogText, KJobWidgets::window(q), m_clipboard);

        if (error) {
            q->emitErrorResult(error);
            return;
        }

        job = resultJob;

        if (KIO::SimpleJob *simpleJob = qobject_cast<KIO::SimpleJob *>(job)) {
            KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::Put, QList<QUrl>(), simpleJob->url(), job);
        }
    }
    if (job) {
        q->addSubjob(job);
        if (copyJob) {
            Q_EMIT q->copyJobStarted(copyJob);
        }
    } else {
        q->emitErrorResult(KIO::ERR_NO_CONTENT);
    }
}

void PasteJob::slotResult(KJob *job)
{
    if (job->error()) {
        KIO::Job::slotResult(job); // will set the error and emit result(this)
        return;
    }
    KIO::SimpleJob *simpleJob = qobject_cast<KIO::SimpleJob *>(job);
    if (simpleJob) {
        Q_EMIT itemCreated(simpleJob->url());
    }

    removeSubjob(job);
    emitResult();
}

PasteJob *KIO::paste(const QMimeData *mimeData, const QUrl &destDir, JobFlags flags)
{
    return PasteJobPrivate::newJob(mimeData, destDir, flags, true /*clipboard*/);
}

#include "moc_pastejob.cpp"
