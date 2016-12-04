/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                  2000-2013 David Faure <faure@kde.org>

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

#include "simplejob.h"
#include "job_p.h"
#include "scheduler.h"
#include "slave.h"
#include "kprotocolinfo.h"
#include <kdirnotify.h>
#include <QTimer>
#include <QDebug>

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
        QTimer::singleShot(0, q, SLOT(slotFinished()));
        return;
    }

    Scheduler::doJob(q);
}

bool SimpleJob::doKill()
{
    Q_D(SimpleJob);
    if ((d->m_extraFlags & JobPrivate::EF_KillCalled) == 0) {
        d->m_extraFlags |= JobPrivate::EF_KillCalled;
        Scheduler::cancelJob(this); // deletes the slave if not 0
    } else {
        qCWarning(KIO_CORE) << this << "This is overkill.";
    }
    return Job::doKill();
}

bool SimpleJob::doSuspend()
{
    Q_D(SimpleJob);
    if (d->m_slave) {
        d->m_slave->suspend();
    }
    return Job::doSuspend();
}

bool SimpleJob::doResume()
{
    Q_D(SimpleJob);
    if (d->m_slave) {
        d->m_slave->resume();
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
    Q_ASSERT(d->m_slave);
    if (d->m_slave) {
        Scheduler::putSlaveOnHold(this, d->m_url);
    }
    // we should now be disassociated from the slave
    Q_ASSERT(!d->m_slave);
    kill(Quietly);
}

void SimpleJob::removeOnHold()
{
    Scheduler::removeSlaveOnHold();
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
        //qDebug() << "Killing job" << this << "in destructor!"/* << qBacktrace()*/;
        Scheduler::cancelJob(this);
    }
}

void SimpleJobPrivate::start(Slave *slave)
{
    Q_Q(SimpleJob);
    m_slave = slave;

    // Slave::setJob can send us SSL metadata if there is a persistent connection
    q->connect(slave, SIGNAL(metaData(KIO::MetaData)),
               SLOT(slotMetaData(KIO::MetaData)));

    slave->setJob(q);

    q->connect(slave, SIGNAL(error(int,QString)),
               SLOT(slotError(int,QString)));

    q->connect(slave, SIGNAL(warning(QString)),
               SLOT(slotWarning(QString)));

    q->connect(slave, SIGNAL(infoMessage(QString)),
               SLOT(_k_slotSlaveInfoMessage(QString)));

    q->connect(slave, SIGNAL(connected()),
               SLOT(slotConnected()));

    q->connect(slave, SIGNAL(finished()),
               SLOT(slotFinished()));

    if ((m_extraFlags & EF_TransferJobDataSent) == 0) { // this is a "get" job
        q->connect(slave, SIGNAL(totalSize(KIO::filesize_t)),
                   SLOT(slotTotalSize(KIO::filesize_t)));

        q->connect(slave, SIGNAL(processedSize(KIO::filesize_t)),
                   SLOT(slotProcessedSize(KIO::filesize_t)));

        q->connect(slave, SIGNAL(speed(ulong)),
                   SLOT(slotSpeed(ulong)));
    }

    const QVariant windowIdProp = q->property("window-id"); // see KJobWidgets::setWindow
    if (windowIdProp.isValid()) {
        m_outgoingMetaData.insert(QStringLiteral("window-id"), QString::number(windowIdProp.toULongLong()));
    }

    const QVariant userTimestampProp = q->property("userTimestamp"); // see KJobWidgets::updateUserTimestamp
    if (userTimestampProp.isValid()) {
        m_outgoingMetaData.insert(QStringLiteral("user-timestamp"), QString::number(userTimestampProp.toULongLong()));
    }

    if (q->uiDelegate() == 0) {            // not interactive
        m_outgoingMetaData.insert(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    }

    if (!m_outgoingMetaData.isEmpty()) {
        KIO_ARGS << m_outgoingMetaData;
        slave->send(CMD_META_DATA, packedArgs);
    }

    if (!m_subUrl.isEmpty()) {
        KIO_ARGS << m_subUrl;
        slave->send(CMD_SUBURL, packedArgs);
    }

    slave->send(m_command, m_packedArgs);
    if (q->isSuspended()) {
       slave->suspend();
    }
}

void SimpleJobPrivate::slaveDone()
{
    Q_Q(SimpleJob);
    if (m_slave) {
        if (m_command == CMD_OPEN) {
            m_slave->send(CMD_CLOSE);
        }
        q->disconnect(m_slave); // Remove all signals between slave and job
    }
    // only finish a job once; Scheduler::jobFinished() resets schedSerial to zero.
    if (m_schedSerial) {
        Scheduler::jobFinished(q, m_slave);
    }
}

void SimpleJob::slotFinished()
{
    Q_D(SimpleJob);
    // Return slave to the scheduler
    d->slaveDone();

    if (!hasSubjobs()) {
        if (!error() && (d->m_command == CMD_MKDIR || d->m_command == CMD_RENAME)) {
            if (d->m_command == CMD_MKDIR) {
                const QUrl urlDir = url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
                org::kde::KDirNotify::emitFilesAdded(urlDir);
            } else { /*if ( m_command == CMD_RENAME )*/
                QUrl src, dst;
                QDataStream str(d->m_packedArgs);
                str >> src >> dst;
                if (src.adjusted(QUrl::RemoveFilename) == dst.adjusted(QUrl::RemoveFilename) // For the user, moving isn't renaming. Only renaming is.
                    && !KProtocolInfo::slaveHandlesNotify(dst.scheme()).contains(QLatin1String("Rename"))) {
                    org::kde::KDirNotify::emitFileRenamed(src, dst);
                }

                org::kde::KDirNotify::emitFileMoved(src, dst);
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
    emit warning(this, errorText);
}

void SimpleJobPrivate::_k_slotSlaveInfoMessage(const QString &msg)
{
    emit q_func()->infoMessage(q_func(), msg);
}

void SimpleJobPrivate::slotConnected()
{
    emit q_func()->connected(q_func());
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
    //qDebug() << KIO::number(size);
    q->setProcessedAmount(KJob::Bytes, size);
}

void SimpleJobPrivate::slotSpeed(unsigned long speed)
{
    //qDebug() << speed;
    q_func()->emitSpeed(speed);
}

void SimpleJobPrivate::restartAfterRedirection(QUrl *redirectionUrl)
{
    Q_Q(SimpleJob);
    // Return slave to the scheduler while we still have the old URL in place; the scheduler
    // requires a job URL to stay invariant while the job is running.
    slaveDone();

    m_url = *redirectionUrl;
    redirectionUrl->clear();
    if ((m_extraFlags & EF_KillCalled) == 0) {
        Scheduler::doJob(q);
    }
}

int SimpleJobPrivate::requestMessageBox(int _type, const QString &text, const QString &caption,
                                        const QString &buttonYes, const QString &buttonNo,
                                        const QString &iconYes, const QString &iconNo,
                                        const QString &dontAskAgainName,
                                        const KIO::MetaData &sslMetaData)
{
    if (m_uiDelegateExtension) {
        const JobUiDelegateExtension::MessageBoxType type = static_cast<JobUiDelegateExtension::MessageBoxType>(_type);
        return m_uiDelegateExtension->requestMessageBox(type, text, caption, buttonYes, buttonNo,
                iconYes, iconNo, dontAskAgainName, sslMetaData);
    }
    qCWarning(KIO_CORE) << "JobUiDelegate not set! Returing -1";
    return -1;
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
    // the ioslave is finished has unintended consequences if the client starts
    // a new connection without waiting for the ioslave to finish.
    if (!d->m_internalMetaData.isEmpty()) {
        Scheduler::updateInternalMetaData(this);
    }
}

void SimpleJob::storeSSLSessionFromJob(const QUrl &redirectionURL)
{
    Q_UNUSED(redirectionURL);
}

//////////
SimpleJob *KIO::rmdir(const QUrl &url)
{
    //qDebug() << "rmdir " << url;
    KIO_ARGS << url << qint8(false); // isFile is false
    return SimpleJobPrivate::newJob(url, CMD_DEL, packedArgs);
}

SimpleJob *KIO::chmod(const QUrl &url, int permissions)
{
    //qDebug() << "chmod " << url;
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
    //qDebug() << "setModificationTime " << url << " " << mtime;
    KIO_ARGS << url << mtime;
    return SimpleJobPrivate::newJobNoUi(url, CMD_SETMODIFICATIONTIME, packedArgs);
}

SimpleJob *KIO::rename(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    //qDebug() << "rename " << src << " " << dest;
    KIO_ARGS << src << dest << (qint8)(flags & Overwrite);
    return SimpleJobPrivate::newJob(src, CMD_RENAME, packedArgs);
}

SimpleJob *KIO::symlink(const QString &target, const QUrl &dest, JobFlags flags)
{
    //qDebug() << "symlink target=" << target << " " << dest;
    KIO_ARGS << target << dest << (qint8)(flags & Overwrite);
    return SimpleJobPrivate::newJob(dest, CMD_SYMLINK, packedArgs, flags);
}

SimpleJob *KIO::special(const QUrl &url, const QByteArray &data, JobFlags flags)
{
    //qDebug() << "special " << url;
    return SimpleJobPrivate::newJob(url, CMD_SPECIAL, data, flags);
}

SimpleJob *KIO::mount(bool ro, const QByteArray &fstype, const QString &dev, const QString &point, JobFlags flags)
{
    KIO_ARGS << int(1) << qint8(ro ? 1 : 0)
             << QString::fromLatin1(fstype) << dev << point;
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

SimpleJob *KIO::http_update_cache(const QUrl &url, bool no_cache, const QDateTime &expireDate)
{
    Q_ASSERT(url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https"));
    // Send http update_cache command (2)
    KIO_ARGS << (int)2 << url << no_cache << qlonglong(expireDate.toMSecsSinceEpoch() / 1000);
    SimpleJob *job = SimpleJobPrivate::newJob(url, CMD_SPECIAL, packedArgs);
    Scheduler::setJobPriority(job, 1);
    return job;
}

#include "moc_simplejob.cpp"
