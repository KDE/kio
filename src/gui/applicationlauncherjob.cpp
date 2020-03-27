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
#include "kiogui_debug.h"

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

void KIO::ApplicationLauncherJob::start()
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

bool KIO::ApplicationLauncherJob::waitForStarted()
{
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
