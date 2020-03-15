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

#include "processlauncherjob.h"
#include "kprocessrunner_p.h"
#include "kiogui_debug.h"

class KIO::ProcessLauncherJobPrivate
{
public:
    ProcessLauncherJobPrivate(const KService::Ptr &service, WId windowId)
        : m_service(service), m_windowId(windowId) {}

    void slotStarted(KIO::ProcessLauncherJob *q, KProcessRunner *processRunner) {
        m_pids.append(processRunner->pid());
        if (--m_numProcessesPending == 0) {
            q->emitResult();
        }
    }
    const KService::Ptr m_service;
    const WId m_windowId;
    QList<QUrl> m_urls;
    KIO::ProcessLauncherJob::RunFlags m_runFlags;
    QString m_suggestedFileName;
    QByteArray m_startupId;
    QVector<qint64> m_pids;
    QVector<KProcessRunner *> m_processRunners;
    int m_numProcessesPending = 0;
};

KIO::ProcessLauncherJob::ProcessLauncherJob(const KService::Ptr &service, WId windowId, QObject *parent)
    : KJob(parent), d(new ProcessLauncherJobPrivate(service, windowId))
{
}

KIO::ProcessLauncherJob::~ProcessLauncherJob()
{
    // Do *NOT* delete the KProcessRunner instances here.
    // We need it to keep running so it can do terminate startup notification on process exit.
}

void KIO::ProcessLauncherJob::setUrls(const QList<QUrl> &urls)
{
    d->m_urls = urls;
}

void KIO::ProcessLauncherJob::setRunFlags(RunFlags runFlags)
{
    d->m_runFlags = runFlags;
}

void KIO::ProcessLauncherJob::setSuggestedFileName(const QString &suggestedFileName)
{
    d->m_suggestedFileName = suggestedFileName;
}

void KIO::ProcessLauncherJob::setStartupId(const QByteArray &startupId)
{
    d->m_startupId = startupId;
}

void KIO::ProcessLauncherJob::start()
{
    if (d->m_urls.count() > 1 && !d->m_service->allowMultipleFiles()) {
        // We need to launch the application N times.
        // We ignore the result for application 2 to N.
        // For the first file we launch the application in the
        // usual way. The reported result is based on this application.
        d->m_numProcessesPending = d->m_urls.count();
        d->m_processRunners.reserve(d->m_numProcessesPending);
        for (int i = 1; i < d->m_urls.count(); ++i) {
            auto *processRunner = new KProcessRunner(d->m_service, { d->m_urls.at(i) }, d->m_windowId,
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

    auto *processRunner = new KProcessRunner(d->m_service, d->m_urls, d->m_windowId,
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

bool KIO::ProcessLauncherJob::waitForStarted()
{
    const bool ret = std::all_of(d->m_processRunners.cbegin(),
                                 d->m_processRunners.cend(),
                                 [](KProcessRunner *r) { return r->waitForStarted(); });
    for (KProcessRunner *r : qAsConst(d->m_processRunners)) {
        qApp->sendPostedEvents(r); // so slotStarted gets called
    }
    return ret;
}

qint64 KIO::ProcessLauncherJob::pid() const
{
    return d->m_pids.at(0);
}

QVector<qint64> KIO::ProcessLauncherJob::pids() const
{
    return d->m_pids;
}
