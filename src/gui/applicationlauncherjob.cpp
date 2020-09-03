/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "applicationlauncherjob.h"
#include "kprocessrunner_p.h"
#include "untrustedprogramhandlerinterface.h"
#include "kiogui_debug.h"
#include "openwithhandlerinterface.h"
#include "../core/global.h"

#include <KAuthorized>
#include <KDesktopFile>
#include <KLocalizedString>
#include <QFileInfo>

// KF6 TODO: Remove
static KIO::UntrustedProgramHandlerInterface *s_untrustedProgramHandler = nullptr;

extern KIO::OpenWithHandlerInterface *s_openWithHandler; // defined in openurljob.cpp

namespace KIO {
// Hidden API because in KF6 we'll just check if the job's uiDelegate implements UntrustedProgramHandlerInterface.
KIOGUI_EXPORT void setDefaultUntrustedProgramHandler(KIO::UntrustedProgramHandlerInterface *iface) { s_untrustedProgramHandler = iface; }
// For OpenUrlJob
KIO::UntrustedProgramHandlerInterface *defaultUntrustedProgramHandler() { return s_untrustedProgramHandler; }
}

#include <KLocalizedString>

class KIO::ApplicationLauncherJobPrivate
{
public:
    explicit ApplicationLauncherJobPrivate(KIO::ApplicationLauncherJob *job, const KService::Ptr &service)
        : m_service(service), q(job) {}

    void slotStarted(KProcessRunner *processRunner) {
        m_pids.append(processRunner->pid());
        if (--m_numProcessesPending == 0) {
            q->emitResult();
        }
    }

    void showOpenWithDialog();

    KService::Ptr m_service;
    QList<QUrl> m_urls;
    KIO::ApplicationLauncherJob::RunFlags m_runFlags;
    QString m_suggestedFileName;
    QByteArray m_startupId;
    QVector<qint64> m_pids;
    QVector<KProcessRunner *> m_processRunners;
    int m_numProcessesPending = 0;
    KIO::ApplicationLauncherJob *q;
};

KIO::ApplicationLauncherJob::ApplicationLauncherJob(const KService::Ptr &service, QObject *parent)
    : KJob(parent), d(new ApplicationLauncherJobPrivate(this, service))
{
}

KIO::ApplicationLauncherJob::ApplicationLauncherJob(const KServiceAction &serviceAction, QObject *parent)
    : ApplicationLauncherJob(serviceAction.service(), parent)
{
    Q_ASSERT(d->m_service);
    d->m_service.detach();
    d->m_service->setExec(serviceAction.exec());
}

KIO::ApplicationLauncherJob::ApplicationLauncherJob(QObject *parent)
    : KJob(parent), d(new ApplicationLauncherJobPrivate(this, {}))
{
}

KIO::ApplicationLauncherJob::~ApplicationLauncherJob()
{
    // Do *NOT* delete the KProcessRunner instances here.
    // We need it to keep running so it can terminate startup notification on process exit.
}

void KIO::ApplicationLauncherJob::setUrls(const QList<QUrl> &urls)
{
    d->m_urls = urls;
}

void KIO::ApplicationLauncherJob::setRunFlags(RunFlags runFlags)
{
    d->m_runFlags = runFlags;
}

void KIO::ApplicationLauncherJob::setSuggestedFileName(const QString &suggestedFileName)
{
    d->m_suggestedFileName = suggestedFileName;
}

void KIO::ApplicationLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KIO::ApplicationLauncherJob::emitUnauthorizedError()
{
    setError(KJob::UserDefinedError);
    setErrorText(i18n("You are not authorized to execute this file."));
    emitResult();
}

void KIO::ApplicationLauncherJob::start()
{
    if (!d->m_service) {
        d->showOpenWithDialog();
        return;
    }
    emit description(this, i18nc("Launching application", "Launching %1", d->m_service->name()), {}, {});

    // First, the security checks
    if (!KAuthorized::authorize(QStringLiteral("run_desktop_files"))) {
        // KIOSK restriction, cannot be circumvented
        emitUnauthorizedError();
        return;
    }
    if (!d->m_service->entryPath().isEmpty() &&
            !KDesktopFile::isAuthorizedDesktopFile(d->m_service->entryPath())) {
        // We can use QStandardPaths::findExecutable to resolve relative pathnames
        // but that gets rid of the command line arguments.
        QString program = QFileInfo(d->m_service->exec()).canonicalFilePath();
        if (program.isEmpty()) { // e.g. due to command line arguments
            program = d->m_service->exec();
        }
        if (!s_untrustedProgramHandler) {
            emitUnauthorizedError();
            return;
        }
        connect(s_untrustedProgramHandler, &KIO::UntrustedProgramHandlerInterface::result, this, [this](bool result) {
            if (result) {
                // Assume that service is an absolute path since we're being called (relative paths
                // would have been allowed unless Kiosk said no, therefore we already know where the
                // .desktop file is.  Now add a header to it if it doesn't already have one
                // and add the +x bit.

                QString errorString;
                if (s_untrustedProgramHandler->makeServiceFileExecutable(d->m_service->entryPath(), errorString)) {
                    proceedAfterSecurityChecks();
                } else {
                    QString serviceName = d->m_service->name();
                    if (serviceName.isEmpty()) {
                        serviceName = d->m_service->genericName();
                    }
                    setError(KJob::UserDefinedError);
                    setErrorText(i18n("Unable to make the service %1 executable, aborting execution.\n%2.",
                                      serviceName, errorString));
                    emitResult();
                }
            } else {
                setError(KIO::ERR_USER_CANCELED);
                emitResult();
            }
        });
        s_untrustedProgramHandler->showUntrustedProgramWarning(this, d->m_service->name());
        return;
    }
    proceedAfterSecurityChecks();
}

void KIO::ApplicationLauncherJob::proceedAfterSecurityChecks()
{
    if (d->m_urls.count() > 1 && !d->m_service->allowMultipleFiles()) {
        // We need to launch the application N times.
        // We ignore the result for application 2 to N.
        // For the first file we launch the application in the
        // usual way. The reported result is based on this application.
        d->m_numProcessesPending = d->m_urls.count();
        d->m_processRunners.reserve(d->m_numProcessesPending);
        for (int i = 1; i < d->m_urls.count(); ++i) {
            auto *processRunner = new KProcessRunner(d->m_service, { d->m_urls.at(i) },
                                                     d->m_runFlags, d->m_suggestedFileName, QByteArray());
            d->m_processRunners.push_back(processRunner);
            connect(processRunner, &KProcessRunner::processStarted, this, [this, processRunner]() {
                d->slotStarted(processRunner);
            });
        }
        d->m_urls = { d->m_urls.at(0) };
    } else {
        d->m_numProcessesPending = 1;
    }

    auto *processRunner = new KProcessRunner(d->m_service, d->m_urls,
                                             d->m_runFlags, d->m_suggestedFileName, d->m_startupId);
    d->m_processRunners.push_back(processRunner);
    connect(processRunner, &KProcessRunner::error, this, [this](const QString &errorText) {
        setError(KJob::UserDefinedError);
        setErrorText(errorText);
        emitResult();
    });
    connect(processRunner, &KProcessRunner::processStarted, this, [this, processRunner]() {
        d->slotStarted(processRunner);
    });
}

// For KRun
bool KIO::ApplicationLauncherJob::waitForStarted()
{
    if (error() != KJob::NoError) {
        return false;
    }
    if (d->m_processRunners.isEmpty()) {
        // Maybe we're in the security prompt...
        // Can't avoid the nested event loop
        // This fork of KJob::exec doesn't set QEventLoop::ExcludeUserInputEvents
        const bool wasAutoDelete = isAutoDelete();
        setAutoDelete(false);
        QEventLoop loop;
        connect(this, &KJob::result, this, [&](KJob *job) {
            loop.exit(job->error());
        });
        const int ret = loop.exec();
        if (wasAutoDelete) {
            deleteLater();
        }
        return ret != KJob::NoError;
    }
    const bool ret = std::all_of(d->m_processRunners.cbegin(),
                                 d->m_processRunners.cend(),
                                 [](KProcessRunner *r) { return r->waitForStarted(); });
    for (KProcessRunner *r : qAsConst(d->m_processRunners)) {
        qApp->sendPostedEvents(r); // so slotStarted gets called
    }
    return ret;
}

qint64 KIO::ApplicationLauncherJob::pid() const
{
    return d->m_pids.at(0);
}

QVector<qint64> KIO::ApplicationLauncherJob::pids() const
{
    return d->m_pids;
}

void KIO::ApplicationLauncherJobPrivate::showOpenWithDialog()
{
    if (!KAuthorized::authorizeAction(QStringLiteral("openwith"))) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("You are not authorized to select an application to open this file."));
        q->emitResult();
        return;
    }
    if (!s_openWithHandler) {
        q->setError(KJob::UserDefinedError);
        q->setErrorText(i18n("Internal error: could not prompt the user for which application to start"));
        q->emitResult();
        return;
    }

    QObject::connect(s_openWithHandler, &KIO::OpenWithHandlerInterface::canceled, q, [this]() {
        q->setError(KIO::ERR_USER_CANCELED);
        q->emitResult();
    });

    QObject::connect(s_openWithHandler, &KIO::OpenWithHandlerInterface::serviceSelected, q, [this](const KService::Ptr &service) {
        Q_ASSERT(service);
        m_service = service;
        q->start();
    });

    QObject::connect(s_openWithHandler, &KIO::OpenWithHandlerInterface::handled, q, [this]() {
        q->emitResult();
    });

    s_openWithHandler->promptUserForApplication(q, m_urls, QString() /* mimetype name unknown */);
}
