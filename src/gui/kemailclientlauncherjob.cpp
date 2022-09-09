/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kemailclientlauncherjob.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMacroExpander>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <QProcessEnvironment>
#include <QUrlQuery>

#include "desktopexecparser.h"
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>

#if defined(Q_OS_UNIX) && defined(QT_DBUS_LIB)
#define USE_PORTAL
#endif

// for XDG Portal support
#ifdef USE_PORTAL
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QGuiApplication>
#include <QWindow>

#include <KWaylandExtras>
#include <KWindowSystem>

#include <fcntl.h>
#include <unistd.h>
#endif // USE_PORTAL

#ifdef Q_OS_WIN
#include <windows.h> // Must be included before shellapi.h

#include <shellapi.h>
#endif

class KEMailClientLauncherJobPrivate
{
public:
    QStringList m_to;
    QStringList m_cc;
    QStringList m_bcc;
    QString m_subject;
    QString m_body;
    QList<QUrl> m_attachments;

    QByteArray m_startupId;
};

KEMailClientLauncherJob::KEMailClientLauncherJob(QObject *parent)
    : KJob(parent)
    , d(new KEMailClientLauncherJobPrivate)
{
}

KEMailClientLauncherJob::~KEMailClientLauncherJob() = default;

void KEMailClientLauncherJob::setTo(const QStringList &to)
{
    d->m_to = to;
}

void KEMailClientLauncherJob::setCc(const QStringList &cc)
{
    d->m_cc = cc;
}

void KEMailClientLauncherJob::setBcc(const QStringList &bcc)
{
    d->m_bcc = bcc;
}

void KEMailClientLauncherJob::setSubject(const QString &subject)
{
    d->m_subject = subject;
}

void KEMailClientLauncherJob::setBody(const QString &body)
{
    d->m_body = body;
}

void KEMailClientLauncherJob::setAttachments(const QList<QUrl> &urls)
{
    d->m_attachments = urls;
}

void KEMailClientLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

#ifdef USE_PORTAL
void KEMailClientLauncherJob::useXdgPortal()
{
    auto window = qGuiApp->focusWindow();
    if (!window && !qGuiApp->allWindows().isEmpty()) {
        window = qGuiApp->allWindows().constFirst();
    }

    if (!window) {
        callXdgPortal(QString());
        return;
    }

    switch (KWindowSystem::platform()) {
    case KWindowSystem::Platform::X11:
        callXdgPortal(QStringLiteral("x11:%1").arg(window->winId(), 0, 16));
        return;
    case KWindowSystem::Platform::Wayland: {
        connect(
            KWaylandExtras::self(),
            &KWaylandExtras::windowExported,
            this,
            [this](QWindow * /*window*/, const QString &handle) {
                callXdgPortal(handle);
            },
            Qt::SingleShotConnection);

        KWaylandExtras::exportWindow(window);
    }
    case KWindowSystem::Platform::Unknown:
        break;
    }

    callXdgPortal(QString());
}

void KEMailClientLauncherJob::callXdgPortal(const QString &parentWindow)
{
    QVector<QDBusUnixFileDescriptor> attachment_fds;
    attachment_fds.reserve(d->m_attachments.size());

    QStringList failed_attachments;

    for (const auto &attachment : std::as_const(d->m_attachments)) {
        auto fd = ::open(QFile::encodeName(attachment.toLocalFile()).constData(), O_PATH);
        if (fd < 0) {
            failed_attachments << attachment.toString();
            continue;
        }
        attachment_fds << QDBusUnixFileDescriptor{fd};
        ::close(fd);
    }

    auto composeEmail = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                       QStringLiteral("/org/freedesktop/portal/desktop"),
                                                       QStringLiteral("org.freedesktop.portal.Email"),
                                                       QStringLiteral("ComposeEmail"));

    const QVariantMap options = {
        {QStringLiteral("addresses"), d->m_to},
        {QStringLiteral("cc"), d->m_cc},
        {QStringLiteral("bcc"), d->m_bcc},
        {QStringLiteral("subject"), d->m_subject},
        {QStringLiteral("body"), d->m_body},
        {QStringLiteral("attachment_fds"), QVariant::fromValue(attachment_fds)},
    };

    composeEmail.setArguments({parentWindow, options});

    const auto call = QDBusConnection::sessionBus().asyncCall(composeEmail);
    const auto *const watcher = new QDBusPendingCallWatcher{call, this};

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, failed_attachments = std::move(failed_attachments)](auto *watcher) {
        const QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            setError(KJob::UserDefinedError);
            setErrorText(i18n("Launching email client failed with: “%1”.", reply.error().message()));
        } else if (!failed_attachments.empty()) {
            Q_EMIT warning(this,
                           i18np("The file <b>%2</b> could not be attached to your email.",
                                 "The following %1 files could not be attached to your email:<ul><li>%2</li></ul>",
                                 failed_attachments.size(),
                                 failed_attachments.join(QLatin1String{"</li><li>"})));
        }
        emitResult();
    });
}
#endif // USE_PORTAL

void KEMailClientLauncherJob::start()
{
#ifdef USE_PORTAL
    if (QCoreApplication::applicationName() == QStringLiteral("xdg-desktop-portal-kde")) {
        QMetaObject::invokeMethod(this, &KEMailClientLauncherJob::launchEMailClient, Qt::QueuedConnection);
        return;
    }

    const auto listActivatableNames = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"),
                                                                     QStringLiteral("/"),
                                                                     QStringLiteral("org.freedesktop.DBus"),
                                                                     QStringLiteral("ListActivatableNames"));

    const auto activatableNamesPending = QDBusConnection::sessionBus().asyncCall(listActivatableNames);
    const auto *const activatableNamesWatcher = new QDBusPendingCallWatcher{activatableNamesPending, this};

    connect(activatableNamesWatcher, &QDBusPendingCallWatcher::finished, this, [this](auto *activatableNamesWatcher) {
        const QDBusPendingReply<QStringList> activatableNames = *activatableNamesWatcher;

        if (activatableNames.value().contains(QStringLiteral("org.freedesktop.portal.Desktop"))) {
            useXdgPortal();
        } else {
            launchEMailClient();
        }
    });
#else
    QMetaObject::invokeMethod(this, &KEMailClientLauncherJob::launchEMailClient, Qt::QueuedConnection);
#endif // USE_PORTAL
}

void KEMailClientLauncherJob::launchEMailClient()
{
#ifndef Q_OS_WIN
    KService::Ptr service = KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/mailto"));
    if (!service) {
        setError(KJob::UserDefinedError);
        setErrorText(i18n("No mail client found"));
        emitResult();
        return;
    }
    const QString entryPath = service->entryPath().toLower();
    if (entryPath.contains(QLatin1String("thunderbird")) || entryPath.contains(QLatin1String("dovecot"))) {
        const QString exec = KIO::DesktopExecParser::executableName(service->exec());
        auto *subjob = new KIO::CommandLauncherJob(exec, thunderbirdArguments(), this);
        subjob->setStartupId(d->m_startupId);
        connect(subjob, &KJob::result, this, &KEMailClientLauncherJob::emitResult);
        subjob->start();
    } else {
        auto *subjob = new KIO::ApplicationLauncherJob(service, this);
        subjob->setUrls({mailToUrl()});
        subjob->setStartupId(d->m_startupId);
        connect(subjob, &KJob::result, this, &KEMailClientLauncherJob::emitResult);
        subjob->start();
    }
#else
    const QString url = mailToUrl().toString();
    const QString sOpen = QStringLiteral("open");
    ShellExecuteW(0, (LPCWSTR)sOpen.utf16(), (LPCWSTR)url.utf16(), 0, 0, SW_NORMAL);
    emitResult();
#endif
}

QUrl KEMailClientLauncherJob::mailToUrl() const
{
    QUrl url;
    QUrlQuery query;
    for (const QString &to : std::as_const(d->m_to)) {
        if (url.path().isEmpty()) {
            url.setPath(to);
        } else {
            query.addQueryItem(QStringLiteral("to"), to);
        }
    }
    for (const QString &cc : std::as_const(d->m_cc)) {
        query.addQueryItem(QStringLiteral("cc"), cc);
    }
    for (const QString &bcc : std::as_const(d->m_bcc)) {
        query.addQueryItem(QStringLiteral("bcc"), bcc);
    }
    for (const QUrl &url : std::as_const(d->m_attachments)) {
        query.addQueryItem(QStringLiteral("attach"), url.toString());
    }
    if (!d->m_subject.isEmpty()) {
        query.addQueryItem(QStringLiteral("subject"), d->m_subject);
    }
    if (!d->m_body.isEmpty()) {
        query.addQueryItem(QStringLiteral("body"), d->m_body);
    }
    url.setQuery(query);
    if (!url.path().isEmpty() || url.hasQuery()) {
        url.setScheme(QStringLiteral("mailto"));
    }
    return url;
}

QStringList KEMailClientLauncherJob::thunderbirdArguments() const
{
    // Thunderbird supports mailto URLs, but refuses attachments for security reasons
    // (https://bugzilla.mozilla.org/show_bug.cgi?id=1613425)
    // It however supports a "command-line" syntax (also used by xdg-email)
    // which includes attachments.
    QString arg;
    const QChar quote = QLatin1Char('\'');
    auto addString = [&](const char *token, const QString &str) {
        if (!str.isEmpty()) {
            arg += QLatin1String(token) + quote + str + quote;
        }
    };
    auto addList = [&](const char *token, const QStringList &list) {
        if (!list.isEmpty()) {
            arg += QLatin1String(token) + quote + list.join(QLatin1Char(',')) + quote;
        }
    };
    addList(",to=", d->m_to);
    addList(",cc=", d->m_cc);
    addList(",bcc=", d->m_bcc);
    addList(",attachment=", QUrl::toStringList(d->m_attachments));
    addString(",subject=", d->m_subject);
    addString(",body=", d->m_body);

    QStringList resultArgs{QLatin1String("-compose")};
    if (!arg.isEmpty()) {
        resultArgs.push_back(arg.mid(1)); // remove first comma
    }
    return resultArgs;
}

#include "moc_kemailclientlauncherjob.cpp"
