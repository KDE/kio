/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "simplejob.h"
#include "job_p.h"
#include "kprotocolinfo.h"
#include "scheduler.h"
#include "worker_p.h"
#include <QDebug>
#include <QTimer>
#include <kdirnotify.h>

using namespace KIO;

SimpleJob::SimpleJob(SimpleJobPrivate &dd)
    : Job(dd)
{
    d_func()->simpleJobInit();
}

void SimpleJobPrivate::simpleJobInit()
{
    Q_Q(SimpleJob);
    if (!m_url.isValid() || m_url.scheme().isEmpty()) {
        qCWarning(KIO_CORE) << "Invalid URL:" << m_url;
        q->setError(ERR_MALFORMED_URL);
        q->setErrorText(m_url.toString());
        QTimer::singleShot(0, q, &SimpleJob::slotFinished);
        return;
    }

    Scheduler::doJob(q);
}

bool SimpleJob::doKill()
{
    Q_D(SimpleJob);
    if ((d->m_extraFlags & JobPrivate::EF_KillCalled) == 0) {
        d->m_extraFlags |= JobPrivate::EF_KillCalled;
        Scheduler::cancelJob(this); // deletes the worker if not 0
    } else {
        qCWarning(KIO_CORE) << this << "killed twice, this is overkill";
    }
    return Job::doKill();
}

bool SimpleJob::doSuspend()
{
    Q_D(SimpleJob);
    if (d->m_worker) {
        d->m_worker->suspend();
    }
    return Job::doSuspend();
}

bool SimpleJob::doResume()
{
    Q_D(SimpleJob);
    if (d->m_worker) {
        d->m_worker->resume();
    }
    return Job::doResume();
}

const QUrl &SimpleJob::url() const
{
    return d_func()->m_url;
}

void SimpleJob::putOnHold()
{
    Q_D(SimpleJob);
    Q_ASSERT(d->m_worker);
    if (d->m_worker) {
        Scheduler::putWorkerOnHold(this, d->m_url);
    }
    // we should now be disassociated from the worker
    Q_ASSERT(!d->m_worker);
    kill(Quietly);
}

void SimpleJob::removeOnHold()
{
    Scheduler::removeWorkerOnHold();
}

bool SimpleJob::isRedirectionHandlingEnabled() const
{
    return d_func()->m_redirectionHandlingEnabled;
}

void SimpleJob::setRedirectionHandlingEnabled(bool handle)
{
    Q_D(SimpleJob);
    d->m_redirectionHandlingEnabled = handle;
}

SimpleJob::~SimpleJob()
{
    Q_D(SimpleJob);
    // last chance to remove this job from the scheduler!
    if (d->m_schedSerial) {
        // qDebug() << "Killing job" << this << "in destructor!"/* << qBacktrace()*/;
        Scheduler::cancelJob(this);
    }
}

void SimpleJobPrivate::start(Worker *worker)
{
    Q_Q(SimpleJob);
    m_worker = worker;

    // Worker::setJob can send us SSL metadata if there is a persistent connection
    QObject::connect(worker, &Worker::metaData, q, &SimpleJob::slotMetaData);

    worker->setJob(q);

    QObject::connect(worker, &Worker::error, q, &SimpleJob::slotError);

    QObject::connect(worker, &Worker::warning, q, &SimpleJob::slotWarning);

    QObject::connect(worker, &Worker::finished, q, &SimpleJob::slotFinished);

    QObject::connect(worker, &Worker::infoMessage, q, [this](const QString &message) {
        _k_slotWorkerInfoMessage(message);
    });

    QObject::connect(worker, &Worker::connected, q, [this]() {
        slotConnected();
    });

    if ((m_extraFlags & EF_TransferJobDataSent) == 0) { // this is a "get" job
        QObject::connect(worker, &Worker::totalSize, q, [this](KIO::filesize_t size) {
            slotTotalSize(size);
        });

        QObject::connect(worker, &Worker::processedSize, q, [this](KIO::filesize_t size) {
            slotProcessedSize(size);
        });

        QObject::connect(worker, &Worker::speed, q, [this](ulong speed) {
            slotSpeed(speed);
        });
    }

    const QVariant windowIdProp = q->property("window-id"); // see KJobWidgets::setWindow
    if (windowIdProp.isValid()) {
        m_outgoingMetaData.insert(QStringLiteral("window-id"), QString::number(windowIdProp.toULongLong()));
    }

    const QVariant userTimestampProp = q->property("userTimestamp"); // see KJobWidgets::updateUserTimestamp
    if (userTimestampProp.isValid()) {
        m_outgoingMetaData.insert(QStringLiteral("user-timestamp"), QString::number(userTimestampProp.toULongLong()));
    }

    if (q->uiDelegate() == nullptr) { // not interactive
        m_outgoingMetaData.insert(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    }

    if (!m_outgoingMetaData.isEmpty()) {
        KIO_ARGS << m_outgoingMetaData;
        worker->send(CMD_META_DATA, packedArgs);
    }

    worker->send(m_command, m_packedArgs);
    if (q->isSuspended()) {
        worker->suspend();
    }
}

void SimpleJobPrivate::workerDone()
{
    Q_Q(SimpleJob);
    if (m_worker) {
        if (m_command == CMD_OPEN) {
            m_worker->send(CMD_CLOSE);
        }
        q->disconnect(m_worker); // Remove all signals between worker and job
    }
    // only finish a job once; Scheduler::jobFinished() resets schedSerial to zero.
    if (m_schedSerial) {
        Scheduler::jobFinished(q, m_worker);
    }
}

void SimpleJob::slotFinished()
{
    Q_D(SimpleJob);
    // Return worker to the scheduler
    d->workerDone();

    if (!hasSubjobs()) {
        if (!error() && (d->m_command == CMD_MKDIR || d->m_command == CMD_RENAME)) {
            if (d->m_command == CMD_MKDIR) {
                const QUrl urlDir = url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
#ifdef WITH_QTDBUS
                org::kde::KDirNotify::emitFilesAdded(urlDir);
#endif
            } else { /*if ( m_command == CMD_RENAME )*/
                QUrl src;
                QUrl dst;
                QDataStream str(d->m_packedArgs);
                str >> src >> dst;
                if (src.adjusted(QUrl::RemoveFilename) == dst.adjusted(QUrl::RemoveFilename) // For the user, moving isn't
                                                                                             // renaming. Only renaming is.
                ) {
#ifdef WITH_QTDBUS
                    org::kde::KDirNotify::emitFileRenamed(src, dst);
#endif
                }

#ifdef WITH_QTDBUS
                org::kde::KDirNotify::emitFileMoved(src, dst);
#endif
                if (d->m_uiDelegateExtension) {
                    d->m_uiDelegateExtension->updateUrlInClipboard(src, dst);
                }
            }
        }
        emitResult();
    }
}

void SimpleJob::slotError(int err, const QString &errorText)
{
    Q_D(SimpleJob);
    setError(err);
    setErrorText(errorText);
    if ((error() == ERR_UNKNOWN_HOST) && d->m_url.host().isEmpty()) {
        setErrorText(QString());
    }
    // error terminates the job
    slotFinished();
}

void SimpleJob::slotWarning(const QString &errorText)
{
    Q_EMIT warning(this, errorText);
}

void SimpleJobPrivate::_k_slotWorkerInfoMessage(const QString &msg)
{
    Q_EMIT q_func()->infoMessage(q_func(), msg);
}

void SimpleJobPrivate::slotConnected()
{
    Q_EMIT q_func()->connected(q_func());
}

void SimpleJobPrivate::slotTotalSize(KIO::filesize_t size)
{
    Q_Q(SimpleJob);
    if (size != q->totalAmount(KJob::Bytes)) {
        q->setTotalAmount(KJob::Bytes, size);
    }
}

void SimpleJobPrivate::slotProcessedSize(KIO::filesize_t size)
{
    Q_Q(SimpleJob);
    // qDebug() << KIO::number(size);
    q->setProcessedAmount(KJob::Bytes, size);
}

void SimpleJobPrivate::slotSpeed(unsigned long speed)
{
    // qDebug() << speed;
    q_func()->emitSpeed(speed);
}

void SimpleJobPrivate::restartAfterRedirection(QUrl *redirectionUrl)
{
    Q_Q(SimpleJob);
    // Return worker to the scheduler while we still have the old URL in place; the scheduler
    // requires a job URL to stay invariant while the job is running.
    workerDone();

    m_url = *redirectionUrl;
    redirectionUrl->clear();
    if ((m_extraFlags & EF_KillCalled) == 0) {
        Scheduler::doJob(q);
    }
}

void SimpleJob::slotMetaData(const KIO::MetaData &_metaData)
{
    Q_D(SimpleJob);
    QMapIterator<QString, QString> it(_metaData);
    while (it.hasNext()) {
        it.next();
        if (it.key().startsWith(QLatin1String("{internal~"), Qt::CaseInsensitive)) {
            d->m_internalMetaData.insert(it.key(), it.value());
        } else {
            d->m_incomingMetaData.insert(it.key(), it.value());
        }
    }

    // Update the internal meta-data values as soon as possible. Waiting until
    // the KIO worker is finished has unintended consequences if the client starts
    // a new connection without waiting for the KIO worker to finish.
    if (!d->m_internalMetaData.isEmpty()) {
        Scheduler::updateInternalMetaData(this);
    }
}

//////////
SimpleJob *KIO::rmdir(const QUrl &url)
{
    // qDebug() << "rmdir " << url;
    KIO_ARGS << url << qint8(false); // isFile is false
    return SimpleJobPrivate::newJob(url, CMD_DEL, packedArgs);
}

SimpleJob *KIO::chmod(const QUrl &url, int permissions)
{
    // qDebug() << "chmod " << url;
    KIO_ARGS << url << permissions;
    return SimpleJobPrivate::newJob(url, CMD_CHMOD, packedArgs);
}

SimpleJob *KIO::chown(const QUrl &url, const QString &owner, const QString &group)
{
    KIO_ARGS << url << owner << group;
    return SimpleJobPrivate::newJob(url, CMD_CHOWN, packedArgs);
}

SimpleJob *KIO::setModificationTime(const QUrl &url, const QDateTime &mtime)
{
    // qDebug() << "setModificationTime " << url << " " << mtime;
    KIO_ARGS << url << mtime;
    return SimpleJobPrivate::newJobNoUi(url, CMD_SETMODIFICATIONTIME, packedArgs);
}

SimpleJob *KIO::rename(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    // qDebug() << "rename " << src << " " << dest;
    KIO_ARGS << src << dest << (qint8)(flags & Overwrite);
    return SimpleJobPrivate::newJob(src, CMD_RENAME, packedArgs, flags);
}

SimpleJob *KIO::symlink(const QString &target, const QUrl &dest, JobFlags flags)
{
    // qDebug() << "symlink target=" << target << " " << dest;
    KIO_ARGS << target << dest << (qint8)(flags & Overwrite);
    return SimpleJobPrivate::newJob(dest, CMD_SYMLINK, packedArgs, flags);
}

SimpleJob *KIO::special(const QUrl &url, const QByteArray &data, JobFlags flags)
{
    // qDebug() << "special " << url;
    return SimpleJobPrivate::newJob(url, CMD_SPECIAL, data, flags);
}

SimpleJob *KIO::mount(bool ro, const QByteArray &fstype, const QString &dev, const QString &point, JobFlags flags)
{
    KIO_ARGS << int(1) << qint8(ro ? 1 : 0) << QString::fromLatin1(fstype) << dev << point;
    SimpleJob *job = special(QUrl(QStringLiteral("file:///")), packedArgs, flags);
    if (!(flags & HideProgressInfo)) {
        KIO::JobPrivate::emitMounting(job, dev, point);
    }
    return job;
}

SimpleJob *KIO::unmount(const QString &point, JobFlags flags)
{
    KIO_ARGS << int(2) << point;
    SimpleJob *job = special(QUrl(QStringLiteral("file:///")), packedArgs, flags);
    if (!(flags & HideProgressInfo)) {
        KIO::JobPrivate::emitUnmounting(job, point);
    }
    return job;
}

//////////

#if KIOCORE_BUILD_DEPRECATED_SINCE(6, 9)
SimpleJob *KIO::http_update_cache(const QUrl &url, bool no_cache, const QDateTime &expireDate)
{
    Q_ASSERT(url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
    // Send http update_cache command (2)
    KIO_ARGS << (int)2 << url << no_cache << qlonglong(expireDate.toMSecsSinceEpoch() / 1000);
    return SimpleJobPrivate::newJob(url, CMD_SPECIAL, packedArgs);
}
#endif

#include "moc_simplejob.cpp"
