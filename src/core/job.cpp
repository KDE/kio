/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2009 David Faure <faure@kde.org>
                       Waldo Bastian <bastian@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "job.h"
#include "job_p.h"

#include <time.h>

#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QLinkedList>

#include <klocalizedstring.h>

#include <kio/jobuidelegateextension.h>
#include "slave.h"
#include "scheduler.h"

using namespace KIO;

//this will update the report dialog with 5 Hz, I think this is fast enough, aleXXX
#define REPORT_TIMEOUT 200

Job::Job() : KCompositeJob(0)
    , d_ptr(new JobPrivate)
{
    d_ptr->q_ptr = this;
    setCapabilities(KJob::Killable | KJob::Suspendable);
}

Job::Job(JobPrivate &dd) : KCompositeJob(0)
    , d_ptr(&dd)
{
    d_ptr->q_ptr = this;
    setCapabilities(KJob::Killable | KJob::Suspendable);
}

Job::~Job()
{
    delete d_ptr;
}

// Exists for historical reasons only
KJobUiDelegate *Job::ui() const
{
    return uiDelegate();
}

JobUiDelegateExtension *Job::uiDelegateExtension() const
{
    Q_D(const Job);
    return d->m_uiDelegateExtension;
}

void Job::setUiDelegateExtension(JobUiDelegateExtension *extension)
{
    Q_D(Job);
    d->m_uiDelegateExtension = extension;
}

bool Job::addSubjob(KJob *jobBase)
{
    //qDebug() << "addSubjob(" << jobBase << ") this=" << this;

    bool ok = KCompositeJob::addSubjob(jobBase);
    KIO::Job *job = dynamic_cast<KIO::Job *>(jobBase);
    if (ok && job) {
        // Copy metadata into subjob (e.g. window-id, user-timestamp etc.)
        Q_D(Job);
        job->mergeMetaData(d->m_outgoingMetaData);

        // Forward information from that subjob.
        connect(job, SIGNAL(speed(KJob*,ulong)),
                SLOT(slotSpeed(KJob*,ulong)));

        job->setProperty("window", property("window")); // see KJobWidgets
        job->setProperty("userTimestamp", property("userTimestamp")); // see KJobWidgets
        job->setUiDelegateExtension(d->m_uiDelegateExtension);
    }
    return ok;
}

bool Job::removeSubjob(KJob *jobBase)
{
    //qDebug() << "removeSubjob(" << jobBase << ") this=" << this << "subjobs=" << subjobs().count();
    return KCompositeJob::removeSubjob(jobBase);
}

KIO::JobPrivate::~JobPrivate()
{
}

void JobPrivate::emitMoving(KIO::Job *job, const QUrl &src, const QUrl &dest)
{
    emit job->description(job, i18nc("@title job", "Moving"),
                          qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString(QUrl::PreferLocalFile)),
                          qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitCopying(KIO::Job *job, const QUrl &src, const QUrl &dest)
{
    emit job->description(job, i18nc("@title job", "Copying"),
                          qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString(QUrl::PreferLocalFile)),
                          qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitCreatingDir(KIO::Job *job, const QUrl &dir)
{
    emit job->description(job, i18nc("@title job", "Creating directory"),
                          qMakePair(i18n("Directory"), dir.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitDeleting(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job", "Deleting"),
                          qMakePair(i18n("File"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitStating(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job", "Examining"),
                          qMakePair(i18n("File"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitTransferring(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job", "Transferring"),
                          qMakePair(i18nc("The source of a file operation", "Source"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitMounting(KIO::Job *job, const QString &dev, const QString &point)
{
    emit job->description(job, i18nc("@title job", "Mounting"),
                          qMakePair(i18n("Device"), dev),
                          qMakePair(i18n("Mountpoint"), point));
}

void JobPrivate::emitUnmounting(KIO::Job *job, const QString &point)
{
    emit job->description(job, i18nc("@title job", "Unmounting"),
                          qMakePair(i18n("Mountpoint"), point));
}

bool Job::doKill()
{
    // kill all subjobs, without triggering their result slot
    Q_FOREACH (KJob *it, subjobs()) {
        it->kill(KJob::Quietly);
    }
    clearSubjobs();

    return true;
}

bool Job::doSuspend()
{
    Q_FOREACH (KJob *it, subjobs()) {
        if (!it->suspend()) {
            return false;
        }
    }

    return true;
}

bool Job::doResume()
{
    Q_FOREACH (KJob *it, subjobs()) {
        if (!it->resume()) {
            return false;
        }
    }

    return true;
}

void JobPrivate::slotSpeed(KJob *, unsigned long speed)
{
    //qDebug() << speed;
    q_func()->emitSpeed(speed);
}

//Job::errorString is implemented in job_error.cpp

void Job::setParentJob(Job *job)
{
    Q_D(Job);
    Q_ASSERT(d->m_parentJob == 0L);
    Q_ASSERT(job);
    d->m_parentJob = job;
}

Job *Job::parentJob() const
{
    return d_func()->m_parentJob;
}

MetaData Job::metaData() const
{
    return d_func()->m_incomingMetaData;
}

QString Job::queryMetaData(const QString &key)
{
    return d_func()->m_incomingMetaData.value(key, QString());
}

void Job::setMetaData(const KIO::MetaData &_metaData)
{
    Q_D(Job);
    d->m_outgoingMetaData = _metaData;
}

void Job::addMetaData(const QString &key, const QString &value)
{
    d_func()->m_outgoingMetaData.insert(key, value);
}

void Job::addMetaData(const QMap<QString, QString> &values)
{
    Q_D(Job);
    QMap<QString, QString>::const_iterator it = values.begin();
    for (; it != values.end(); ++it) {
        d->m_outgoingMetaData.insert(it.key(), it.value());
    }
}

void Job::mergeMetaData(const QMap<QString, QString> &values)
{
    Q_D(Job);
    QMap<QString, QString>::const_iterator it = values.begin();
    for (; it != values.end(); ++it)
        // there's probably a faster way
        if (!d->m_outgoingMetaData.contains(it.key())) {
            d->m_outgoingMetaData.insert(it.key(), it.value());
        }
}

MetaData Job::outgoingMetaData() const
{
    return d_func()->m_outgoingMetaData;
}

//////////////////////////

class KIO::DirectCopyJobPrivate: public KIO::SimpleJobPrivate
{
public:
    DirectCopyJobPrivate(const QUrl &url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
    {}

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    void start(Slave *slave) Q_DECL_OVERRIDE;

    Q_DECLARE_PUBLIC(DirectCopyJob)
};

DirectCopyJob::DirectCopyJob(const QUrl &url, const QByteArray &packedArgs)
    : SimpleJob(*new DirectCopyJobPrivate(url, CMD_COPY, packedArgs))
{
    setUiDelegate(KIO::createDefaultJobUiDelegate());
}

DirectCopyJob::~DirectCopyJob()
{
}

void DirectCopyJobPrivate::start(Slave *slave)
{
    Q_Q(DirectCopyJob);
    q->connect(slave, SIGNAL(canResume(KIO::filesize_t)),
               SLOT(slotCanResume(KIO::filesize_t)));
    SimpleJobPrivate::start(slave);
}

void DirectCopyJob::slotCanResume(KIO::filesize_t offset)
{
    emit canResume(this, offset);
}

//////////////////////////

SimpleJob *KIO::file_delete(const QUrl &src, JobFlags flags)
{
    KIO_ARGS << src << qint8(true); // isFile
    SimpleJob *job = SimpleJobPrivate::newJob(src, CMD_DEL, packedArgs, flags);
    if (job->uiDelegateExtension()) {
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::RemoveContent);
    }
    return job;
}

//////////
////

#include "moc_job_base.cpp"
#include "moc_job_p.cpp"
