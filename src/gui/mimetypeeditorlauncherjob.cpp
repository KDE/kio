/*
 * SPDX-FileCopyrightText: 2023 Kai Uwe Broulik <kde@broulik.de>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 */

#include "mimetypeeditorlauncherjob.h"

#include "config-kiogui.h"

#include <KIO/CommandLauncherJob>
#include <KIO/Job>

#include <KLocalizedString>
#include <KWindowSystem>

#if HAVE_WAYLAND
#include <KWaylandExtras>
#endif

#include <QMimeDatabase>
#include <QPointer>
#include <QStandardPaths>
#include <QWindow>

namespace KIO
{

constexpr QLatin1String g_editorExec{"keditfiletype"};

class MimeTypeEditorLauncherJobPrivate
{
public:
    MimeTypeEditorLauncherJobPrivate(MimeTypeEditorLauncherJob *q)
        : q(q)
    {
    }

    void emitDelayedResult()
    {
        // Use delayed invocation so the caller has time to connect to the signal
        QMetaObject::invokeMethod(q, &MimeTypeEditorLauncherJob::emitResult, Qt::QueuedConnection);
    }

    void start(const QStringList &args)
    {
        auto *subjob = new KIO::CommandLauncherJob(g_editorExec, args, q);
        subjob->setDesktopName(QStringLiteral("org.kde.keditfiletype"));
        subjob->setStartupId(m_startupId);
        QObject::connect(subjob, &KJob::result, q, [this, subjob] {
            if (subjob->error()) {
                q->setError(subjob->error());
                q->setErrorText(subjob->errorText());
            }
            q->emitResult();
        });
        subjob->start();
    }

    MimeTypeEditorLauncherJob *q;

    QString m_mimeType;
    QPointer<QWindow> m_parentWindow;
    QByteArray m_startupId;
};

MimeTypeEditorLauncherJob::MimeTypeEditorLauncherJob(const QString &mimeType, QObject *parent)
    : KJob(parent)
    , d(new MimeTypeEditorLauncherJobPrivate(this))
{
    d->m_mimeType = mimeType;
}

MimeTypeEditorLauncherJob::~MimeTypeEditorLauncherJob() = default;

void MimeTypeEditorLauncherJob::setParentWindow(QWindow *parentWindow)
{
    d->m_parentWindow = parentWindow;
}

void MimeTypeEditorLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void MimeTypeEditorLauncherJob::start()
{
    // Asterisk means "create new type".
    if (!d->m_mimeType.startsWith(QLatin1Char('*'))) {
        const QMimeType mime = QMimeDatabase().mimeTypeForName(d->m_mimeType);
        if (!mime.isValid()) {
            setError(KJob::UserDefinedError);
            setErrorText(i18n("File type \"%1\" not found.", d->m_mimeType));
            d->emitDelayedResult();
            return;
        }
    }

    if (!isSupported()) {
        setError(KIO::ERR_DOES_NOT_EXIST);
        setErrorText(KIO::buildErrorString(KIO::ERR_DOES_NOT_EXIST, g_editorExec));
        d->emitDelayedResult();
        return;
    }

    bool waitForXdgForeign = false;
    QStringList args;
    args << d->m_mimeType;

    if (d->m_parentWindow) {
        if (KWindowSystem::isPlatformWayland()) {
#if HAVE_WAYLAND
            KWaylandExtras::exportWindow(d->m_parentWindow);
            connect(
                KWaylandExtras::self(),
                &KWaylandExtras::windowExported,
                this,
                [this, args](QWindow *window, const QString &handle) mutable {
                    if (window == d->m_parentWindow && !handle.isEmpty()) {
                        args << QStringLiteral("--parent") << handle;
                    }
                    d->start(args);
                },
                Qt::SingleShotConnection);
            waitForXdgForeign = true;
#endif
        } else {
            args << QStringLiteral("--parent") << QString::number(d->m_parentWindow->winId());
        }
    }

    if (!waitForXdgForeign) {
        d->start(args);
    }
}

bool MimeTypeEditorLauncherJob::isSupported()
{
    return !QStandardPaths::findExecutable(g_editorExec).isEmpty();
}

} // namespace KIO

#include "moc_mimetypeeditorlauncherjob.cpp"
