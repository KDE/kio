/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2021 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "kmailclientlauncherjob.h"

#include <KApplicationTrader>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMacroExpander>
#include <KService>
#include <KSharedConfig>
#include <KShell>
#include <QProcessEnvironment>
#include <QUrlQuery>

#include <KIO/ApplicationLauncherJob>

class KMailClientLauncherJobPrivate
{
public:
    QStringList m_to;
    QStringList m_cc;
    QString m_subject;
    QString m_body;
    QList<QUrl> m_attachments;

    QByteArray m_startupId;
};

KMailClientLauncherJob::KMailClientLauncherJob(QObject *parent)
    : KJob(parent)
    , d(new KMailClientLauncherJobPrivate)
{
}

KMailClientLauncherJob::~KMailClientLauncherJob() = default;

void KMailClientLauncherJob::setTo(const QStringList &to)
{
    d->m_to = to;
}

void KMailClientLauncherJob::setCc(const QStringList &cc)
{
    d->m_cc = cc;
}

void KMailClientLauncherJob::setSubject(const QString &subject)
{
    d->m_subject = subject;
}

void KMailClientLauncherJob::setBody(const QString &body)
{
    d->m_body = body;
}

void KMailClientLauncherJob::setAttachments(const QList<QUrl> &urls)
{
    d->m_attachments = urls;
}

void KMailClientLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KMailClientLauncherJob::start()
{
#ifndef Q_OS_WIN
    KService::Ptr service = KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/mailto"));
    if (!service) {
        setError(KJob::UserDefinedError);
        setErrorText(i18n("No mail client found"));
        emitDelayedResult();
        return;
    }
    const QString entryPath = service->entryPath();
    if (entryPath.contains(QLatin1String("thunderbird")) || entryPath.contains(QLatin1String("dovecot"))) {
        const QString exec = KShell::splitArgs(service->exec()).at(0);
        auto *subjob = new KIO::CommandLauncherJob(thunderbirdCommandLine(exec), this);
        subjob->setStartupId(d->m_startupId);
        connect(subjob, &KJob::result, this, &KJob::result);
        subjob->start();
    } else {
        auto *subjob = new KIO::ApplicationLauncherJob(service, this);
        subjob->setUrls({mailToUrl()});
        subjob->setStartupId(d->m_startupId);
        connect(subjob, &KJob::result, this, &KJob::result);
        subjob->start();
    }
#else
    const QString url = mailToUrl().toString();
    const QString sOpen = QLatin1String("open");
    ShellExecuteW(0, (LPCWSTR)sOpen.utf16(), (LPCWSTR)url.utf16(), 0, 0, SW_NORMAL);
    emitDelayedResult();
#endif
}

void KMailClientLauncherJob::emitDelayedResult()
{
    // Use delayed invocation so the caller has time to connect to the signal
    QMetaObject::invokeMethod(this, &KMailClientLauncherJob::emitResult, Qt::QueuedConnection);
}

QUrl KMailClientLauncherJob::mailToUrl() const
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

QString KMailClientLauncherJob::thunderbirdCommandLine(const QString &exec) const
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
    addList(",attachment=", QUrl::toStringList(d->m_attachments));
    addString(",subject=", d->m_subject);
    addString(",body=", d->m_body);

    QString command = exec + QLatin1String(" -compose");
    if (!arg.isEmpty()) {
        command += QLatin1Char(' ') + KShell::quoteArg(arg.mid(1)); // remove first comma
    }
    return command;
}
