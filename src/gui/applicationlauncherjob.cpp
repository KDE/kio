/*
    This file is part of the KDE libraries
    Copyright (c) 2020 David Faure <faure@kde.org>

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License or ( at
    your option ) version 3 or, at the discretion of KDE e.V. ( which shall
    act as a proxy as in section 14 of the GPLv3 ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "applicationlauncherjob.h"
#include "kprocessrunner_p.h"
#include "untrustedprogramhandlerinterface.h"
#include "kiogui_debug.h"
#include "../core/global.h"

#include <KAuthorized>
#include <KDesktopFile>
#include <KLocalizedString>
#include <QFileInfo>

static KIO::UntrustedProgramHandlerInterface *s_untrustedProgramHandler = nullptr;
namespace KIO {
// Hidden API because in KF6 we'll just check if the job's uiDelegate implements UntrustedProgramHandlerInterface.
KIOGUI_EXPORT void setDefaultUntrustedProgramHandler(KIO::UntrustedProgramHandlerInterface *iface) { s_untrustedProgramHandler = iface; }
}

class KIO::ApplicationLauncherJobPrivate
{
public:
    explicit ApplicationLauncherJobPrivate(const KService::Ptr &service)
        : m_service(service) {}

    void slotStarted(KIO::ApplicationLauncherJob *q, KProcessRunner *processRunner) {
        m_pids.append(processRunner->pid());
        if (--m_numProcessesPending == 0) {
            q->emitResult();
        }
    }
    KService::Ptr m_service;
    QList<QUrl> m_urls;
    KIO::ApplicationLauncherJob::RunFlags m_runFlags;
    QString m_suggestedFileName;
    QByteArray m_startupId;
    QVector<qint64> m_pids;
    QVector<KProcessRunner *> m_processRunners;
    int m_numProcessesPending = 0;
};

KIO::ApplicationLauncherJob::ApplicationLauncherJob(const KService::Ptr &service, QObject *parent)
    : KJob(parent), d(new ApplicationLauncherJobPrivate(service))
{
}

KIO::ApplicationLauncherJob::ApplicationLauncherJob(const KServiceAction &serviceAction, QObject *parent)
    : ApplicationLauncherJob(serviceAction.service(), parent)
{
    Q_ASSERT(d->m_service);
    d->m_service.detach();
    d->m_service->setExec(serviceAction.exec());
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
                d->slotStarted(this, processRunner);
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
        d->slotStarted(this, processRunner);
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
