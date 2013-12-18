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

#include <kurlauthorized.h>
#include <klocalizedstring.h>
#include <kconfig.h>
#include <kconfiggroup.h>

#include <kdirnotify.h>
#include "kprotocolmanager.h"

#include <kio/jobuidelegateextension.h>
#include "slave.h"
#include "scheduler.h"

using namespace KIO;

#define MAX_READ_BUF_SIZE  (64 * 1024)       // 64 KB at a time seems reasonable...

static inline Slave *jobSlave(SimpleJob *job)
{
    return SimpleJobPrivate::get(job)->m_slave;
}

//this will update the report dialog with 5 Hz, I think this is fast enough, aleXXX
#define REPORT_TIMEOUT 200

Job::Job() : KCompositeJob(0)
    , d_ptr(new JobPrivate)
{
    d_ptr->q_ptr = this;
    setCapabilities( KJob::Killable | KJob::Suspendable );
}

Job::Job(JobPrivate &dd) : KCompositeJob(0)
    , d_ptr(&dd)
{
    d_ptr->q_ptr = this;
    setCapabilities( KJob::Killable | KJob::Suspendable );
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

    bool ok = KCompositeJob::addSubjob( jobBase );
    KIO::Job *job = dynamic_cast<KIO::Job*>( jobBase );
    if (ok && job) {
        // Copy metadata into subjob (e.g. window-id, user-timestamp etc.)
        Q_D(Job);
        job->mergeMetaData(d->m_outgoingMetaData);

        // Forward information from that subjob.
        connect(job, SIGNAL(speed(KJob*,ulong)),
                SLOT(slotSpeed(KJob*,ulong)));

        job->setProperty("window", property("window")); // see KJobWidgets
        job->setProperty("userTimestamp", property("userTimestamp")); // see KJobWidgets
    }
    return ok;
}

bool Job::removeSubjob( KJob *jobBase )
{
    //qDebug() << "removeSubjob(" << jobBase << ") this=" << this << "subjobs=" << subjobs().count();
    return KCompositeJob::removeSubjob( jobBase );
}

void JobPrivate::emitMoving(KIO::Job * job, const QUrl &src, const QUrl &dest)
{
    emit job->description(job, i18nc("@title job","Moving"),
                          qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString(QUrl::PreferLocalFile)),
                          qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitCopying(KIO::Job * job, const QUrl &src, const QUrl &dest)
{
    emit job->description(job, i18nc("@title job","Copying"),
                          qMakePair(i18nc("The source of a file operation", "Source"), src.toDisplayString(QUrl::PreferLocalFile)),
                          qMakePair(i18nc("The destination of a file operation", "Destination"), dest.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitCreatingDir(KIO::Job * job, const QUrl &dir)
{
    emit job->description(job, i18nc("@title job","Creating directory"),
                          qMakePair(i18n("Directory"), dir.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitDeleting(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job","Deleting"),
                          qMakePair(i18n("File"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitStating(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job","Examining"),
                          qMakePair(i18n("File"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitTransferring(KIO::Job *job, const QUrl &url)
{
    emit job->description(job, i18nc("@title job","Transferring"),
                          qMakePair(i18nc("The source of a file operation", "Source"), url.toDisplayString(QUrl::PreferLocalFile)));
}

void JobPrivate::emitMounting(KIO::Job * job, const QString &dev, const QString &point)
{
    emit job->description(job, i18nc("@title job","Mounting"),
                          qMakePair(i18n("Device"), dev),
                          qMakePair(i18n("Mountpoint"), point));
}

void JobPrivate::emitUnmounting(KIO::Job * job, const QString &point)
{
    emit job->description(job, i18nc("@title job","Unmounting"),
                          qMakePair(i18n("Mountpoint"), point));
}

bool Job::doKill()
{
  // kill all subjobs, without triggering their result slot
  Q_FOREACH( KJob* it, subjobs()) {
      it->kill( KJob::Quietly );
  }
  clearSubjobs();

  return true;
}

bool Job::doSuspend()
{
    Q_FOREACH(KJob* it, subjobs()) {
        if (!it->suspend())
            return false;
    }

    return true;
}

bool Job::doResume()
{
    Q_FOREACH ( KJob* it, subjobs() )
    {
        if (!it->resume())
            return false;
    }

    return true;
}

void JobPrivate::slotSpeed( KJob*, unsigned long speed )
{
    //qDebug() << speed;
    q_func()->emitSpeed( speed );
}

//Job::errorString is implemented in job_error.cpp

void Job::setParentJob(Job* job)
{
  Q_D(Job);
  Q_ASSERT(d->m_parentJob == 0L);
  Q_ASSERT(job);
  d->m_parentJob = job;
}

Job* Job::parentJob() const
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

void Job::setMetaData( const KIO::MetaData &_metaData)
{
    Q_D(Job);
    d->m_outgoingMetaData = _metaData;
}

void Job::addMetaData( const QString &key, const QString &value)
{
    d_func()->m_outgoingMetaData.insert(key, value);
}

void Job::addMetaData( const QMap<QString,QString> &values)
{
    Q_D(Job);
    QMap<QString,QString>::const_iterator it = values.begin();
    for(;it != values.end(); ++it)
        d->m_outgoingMetaData.insert(it.key(), it.value());
}

void Job::mergeMetaData( const QMap<QString,QString> &values)
{
    Q_D(Job);
    QMap<QString,QString>::const_iterator it = values.begin();
    for(;it != values.end(); ++it)
        // there's probably a faster way
        if ( !d->m_outgoingMetaData.contains( it.key() ) )
            d->m_outgoingMetaData.insert( it.key(), it.value() );
}

MetaData Job::outgoingMetaData() const
{
    return d_func()->m_outgoingMetaData;
}

SimpleJob::SimpleJob(SimpleJobPrivate &dd)
  : Job(dd)
{
    d_func()->simpleJobInit();
}

void SimpleJobPrivate::simpleJobInit()
{
    Q_Q(SimpleJob);
    if (!m_url.isValid())
    {
        q->setError( ERR_MALFORMED_URL );
        q->setErrorText(m_url.toString());
        QTimer::singleShot(0, q, SLOT(slotFinished()) );
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
        qWarning() << this << "This is overkill.";
    }
    return Job::doKill();
}

bool SimpleJob::doSuspend()
{
    Q_D(SimpleJob);
    if ( d->m_slave )
        d->m_slave->suspend();
    return Job::doSuspend();
}

bool SimpleJob::doResume()
{
    Q_D(SimpleJob);
    if ( d->m_slave )
        d->m_slave->resume();
    return Job::doResume();
}

const QUrl& SimpleJob::url() const
{
    return d_func()->m_url;
}

void SimpleJob::putOnHold()
{
    Q_D(SimpleJob);
    Q_ASSERT( d->m_slave );
    if ( d->m_slave )
    {
        Scheduler::putSlaveOnHold(this, d->m_url);
    }
    // we should now be disassociated from the slave
    Q_ASSERT(!d->m_slave);
    kill( Quietly );
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
    q->connect( slave, SIGNAL(metaData(KIO::MetaData)),
                SLOT(slotMetaData(KIO::MetaData)) );

    slave->setJob(q);

    q->connect( slave, SIGNAL(error(int,QString)),
                SLOT(slotError(int,QString)) );

    q->connect( slave, SIGNAL(warning(QString)),
                SLOT(slotWarning(QString)) );

    q->connect( slave, SIGNAL(infoMessage(QString)),
                SLOT(_k_slotSlaveInfoMessage(QString)) );

    q->connect( slave, SIGNAL(connected()),
                SLOT(slotConnected()));

    q->connect( slave, SIGNAL(finished()),
                SLOT(slotFinished()) );

    if ((m_extraFlags & EF_TransferJobDataSent) == 0) // this is a "get" job
    {
        q->connect( slave, SIGNAL(totalSize(KIO::filesize_t)),
                    SLOT(slotTotalSize(KIO::filesize_t)) );

        q->connect( slave, SIGNAL(processedSize(KIO::filesize_t)),
                    SLOT(slotProcessedSize(KIO::filesize_t)) );

        q->connect( slave, SIGNAL(speed(ulong)),
                    SLOT(slotSpeed(ulong)) );
    }

    const QVariant windowIdProp = q->property("window-id"); // see KJobWidgets::setWindow
    if (windowIdProp.isValid()) {
        m_outgoingMetaData.insert("window-id", QString::number(windowIdProp.toULongLong()));
    }

    const QVariant userTimestampProp = q->property("userTimestamp"); // see KJobWidgets::updateUserTimestamp
    if (userTimestampProp.isValid()) {
        m_outgoingMetaData.insert("user-timestamp", QString::number(userTimestampProp.toULongLong()));
    }

    if (q->uiDelegate() == 0)              // not interactive
    {
        m_outgoingMetaData.insert("no-auth-prompt", "true");
    }

    if (!m_outgoingMetaData.isEmpty())
    {
        KIO_ARGS << m_outgoingMetaData;
        slave->send( CMD_META_DATA, packedArgs );
    }

    if (!m_subUrl.isEmpty())
    {
       KIO_ARGS << m_subUrl;
       slave->send( CMD_SUBURL, packedArgs );
    }

    slave->send( m_command, m_packedArgs );
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

void SimpleJob::slotFinished( )
{
    Q_D(SimpleJob);
    // Return slave to the scheduler
    d->slaveDone();

    if (!hasSubjobs())
    {
        if ( !error() && (d->m_command == CMD_MKDIR || d->m_command == CMD_RENAME ) )
        {
            if ( d->m_command == CMD_MKDIR )
            {
                const QUrl urlDir = url().adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
                org::kde::KDirNotify::emitFilesAdded(urlDir);
            }
            else /*if ( m_command == CMD_RENAME )*/
            {
                QUrl src, dst;
                QDataStream str( d->m_packedArgs );
                str >> src >> dst;
                if(src.adjusted(QUrl::RemoveFilename) == dst.adjusted(QUrl::RemoveFilename)) // For the user, moving isn't renaming. Only renaming is.
                    org::kde::KDirNotify::emitFileRenamed(src, dst);

                org::kde::KDirNotify::emitFileMoved(src, dst);
                if (d->m_uiDelegateExtension)
                    d->m_uiDelegateExtension->updateUrlInClipboard(src, dst);
            }
        }
        emitResult();
    }
}

void SimpleJob::slotError( int err, const QString & errorText )
{
    Q_D(SimpleJob);
    setError( err );
    setErrorText( errorText );
    if ((error() == ERR_UNKNOWN_HOST) && d->m_url.host().isEmpty())
       setErrorText( QString() );
    // error terminates the job
    slotFinished();
}

void SimpleJob::slotWarning( const QString & errorText )
{
    emit warning( this, errorText );
}

void SimpleJobPrivate::_k_slotSlaveInfoMessage( const QString & msg )
{
    emit q_func()->infoMessage( q_func(), msg );
}

void SimpleJobPrivate::slotConnected()
{
    emit q_func()->connected( q_func() );
}

void SimpleJobPrivate::slotTotalSize( KIO::filesize_t size )
{
    Q_Q(SimpleJob);
    if (size != q->totalAmount(KJob::Bytes))
    {
        q->setTotalAmount(KJob::Bytes, size);
    }
}

void SimpleJobPrivate::slotProcessedSize( KIO::filesize_t size )
{
    Q_Q(SimpleJob);
    //qDebug() << KIO::number(size);
    q->setProcessedAmount(KJob::Bytes, size);
}

void SimpleJobPrivate::slotSpeed( unsigned long speed )
{
    //qDebug() << speed;
    q_func()->emitSpeed( speed );
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

int SimpleJobPrivate::requestMessageBox(int _type, const QString& text, const QString& caption,
                                       const QString& buttonYes, const QString& buttonNo,
                                       const QString& iconYes, const QString& iconNo,
                                       const QString& dontAskAgainName,
                                       const KIO::MetaData& sslMetaData)
{
    if (m_uiDelegateExtension) {
        const JobUiDelegateExtension::MessageBoxType type = static_cast<JobUiDelegateExtension::MessageBoxType>(_type);
        return m_uiDelegateExtension->requestMessageBox(type, text, caption, buttonYes, buttonNo,
                                                        iconYes, iconNo, dontAskAgainName, sslMetaData);
    }
    qWarning() << "JobUiDelegate not set! Returing -1";
    return -1;
}

void SimpleJob::slotMetaData( const KIO::MetaData &_metaData )
{
    Q_D(SimpleJob);
    QMapIterator<QString,QString> it (_metaData);
    while (it.hasNext()) {
        it.next();
        if (it.key().startsWith(QLatin1String("{internal~"), Qt::CaseInsensitive))
            d->m_internalMetaData.insert(it.key(), it.value());
        else
            d->m_incomingMetaData.insert(it.key(), it.value());
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
SimpleJob *KIO::rmdir( const QUrl& url )
{
    //qDebug() << "rmdir " << url;
    KIO_ARGS << url << qint8(false); // isFile is false
    return SimpleJobPrivate::newJob(url, CMD_DEL, packedArgs);
}

SimpleJob *KIO::chmod( const QUrl& url, int permissions )
{
    //qDebug() << "chmod " << url;
    KIO_ARGS << url << permissions;
    return SimpleJobPrivate::newJob(url, CMD_CHMOD, packedArgs);
}

SimpleJob *KIO::chown( const QUrl& url, const QString& owner, const QString& group )
{
    KIO_ARGS << url << owner << group;
    return SimpleJobPrivate::newJob(url, CMD_CHOWN, packedArgs);
}

SimpleJob *KIO::setModificationTime( const QUrl& url, const QDateTime& mtime )
{
    //qDebug() << "setModificationTime " << url << " " << mtime;
    KIO_ARGS << url << mtime;
    return SimpleJobPrivate::newJobNoUi(url, CMD_SETMODIFICATIONTIME, packedArgs);
}

SimpleJob *KIO::rename( const QUrl& src, const QUrl & dest, JobFlags flags )
{
    //qDebug() << "rename " << src << " " << dest;
    KIO_ARGS << src << dest << (qint8) (flags & Overwrite);
    return SimpleJobPrivate::newJob(src, CMD_RENAME, packedArgs);
}

SimpleJob *KIO::symlink( const QString& target, const QUrl & dest, JobFlags flags )
{
    //qDebug() << "symlink target=" << target << " " << dest;
    KIO_ARGS << target << dest << (qint8) (flags & Overwrite);
    return SimpleJobPrivate::newJob(dest, CMD_SYMLINK, packedArgs, flags);
}

SimpleJob *KIO::special(const QUrl& url, const QByteArray & data, JobFlags flags)
{
    //qDebug() << "special " << url;
    return SimpleJobPrivate::newJob(url, CMD_SPECIAL, data, flags);
}

SimpleJob *KIO::mount( bool ro, const QByteArray& fstype, const QString& dev, const QString& point, JobFlags flags )
{
    KIO_ARGS << int(1) << qint8( ro ? 1 : 0 )
             << QString::fromLatin1(fstype) << dev << point;
    SimpleJob *job = special(QUrl("file:///"), packedArgs, flags);
    if (!(flags & HideProgressInfo)) {
        KIO::JobPrivate::emitMounting(job, dev, point);
    }
    return job;
}

SimpleJob *KIO::unmount( const QString& point, JobFlags flags )
{
    KIO_ARGS << int(2) << point;
    SimpleJob *job = special(QUrl("file:///"), packedArgs, flags);
    if (!(flags & HideProgressInfo)) {
        KIO::JobPrivate::emitUnmounting(job, point);
    }
    return job;
}



//////////

class KIO::StatJobPrivate: public SimpleJobPrivate
{
public:
    inline StatJobPrivate(const QUrl& url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs), m_bSource(true), m_details(2)
        {}

    UDSEntry m_statResult;
    QUrl m_redirectionURL;
    bool m_bSource;
    short int m_details;
    void slotStatEntry( const KIO::UDSEntry & entry );
    void slotRedirection(const QUrl &url);

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    virtual void start( Slave *slave );

    Q_DECLARE_PUBLIC(StatJob)

    static inline StatJob *newJob(const QUrl& url, int command, const QByteArray &packedArgs,
        JobFlags flags )
    {
        StatJob *job = new StatJob(*new StatJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
            emitStating(job, url);
        }
        return job;
    }
};

StatJob::StatJob(StatJobPrivate &dd)
    : SimpleJob(dd)
{
}

StatJob::~StatJob()
{
}

#ifndef KDE_NO_DEPRECATED
void StatJob::setSide( bool source )
{
    d_func()->m_bSource = source;
}
#endif

void StatJob::setSide( StatSide side )
{
    d_func()->m_bSource = side == SourceSide;
}

void StatJob::setDetails( short int details )
{
    d_func()->m_details = details;
}

const UDSEntry & StatJob::statResult() const
{
    return d_func()->m_statResult;
}

QUrl StatJob::mostLocalUrl() const
{
    if (!url().isLocalFile()) {
        const UDSEntry& udsEntry = d_func()->m_statResult;
        const QString path = udsEntry.stringValue( KIO::UDSEntry::UDS_LOCAL_PATH );
        if (!path.isEmpty())
            return QUrl::fromLocalFile(path);
    }
    return url();
}

void StatJobPrivate::start(Slave *slave)
{
    Q_Q(StatJob);
    m_outgoingMetaData.insert( "statSide", m_bSource ? "source" : "dest" );
    m_outgoingMetaData.insert( "details", QString::number(m_details) );

    q->connect( slave, SIGNAL(statEntry(KIO::UDSEntry)),
             SLOT(slotStatEntry(KIO::UDSEntry)) );
    q->connect(slave, SIGNAL(redirection(QUrl)),
               SLOT(slotRedirection(QUrl)));

    SimpleJobPrivate::start(slave);
}

void StatJobPrivate::slotStatEntry( const KIO::UDSEntry & entry )
{
    //qDebug();
    m_statResult = entry;
}

// Slave got a redirection request
void StatJobPrivate::slotRedirection(const QUrl &url)
{
     Q_Q(StatJob);
     //qDebug() << m_url << "->" << url;
     if (!KUrlAuthorized::authorizeUrlAction("redirect", m_url, url))
     {
       qWarning() << "Redirection from" << m_url << "to" << url << "REJECTED!";
       q->setError( ERR_ACCESS_DENIED );
       q->setErrorText(url.toDisplayString());
       return;
     }
     m_redirectionURL = url; // We'll remember that when the job finishes
     // Tell the user that we haven't finished yet
     emit q->redirection(q, m_redirectionURL);
}

void StatJob::slotFinished()
{
    Q_D(StatJob);

    if ( !d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() )
    {
        //qDebug() << "StatJob: Redirection to " << m_redirectionURL;
        if (queryMetaData("permanent-redirect")=="true")
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);

        if ( d->m_redirectionHandlingEnabled )
        {
            d->m_packedArgs.truncate(0);
            QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
            stream << d->m_redirectionURL;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return slave to the scheduler
    SimpleJob::slotFinished();
}

void StatJob::slotMetaData( const KIO::MetaData &_metaData)
{
    Q_D(StatJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

StatJob *KIO::stat(const QUrl& url, JobFlags flags)
{
    // Assume sideIsSource. Gets are more common than puts.
    return stat( url, StatJob::SourceSide, 2, flags );
}

StatJob *KIO::mostLocalUrl(const QUrl& url, JobFlags flags)
{
    StatJob* job = stat( url, StatJob::SourceSide, 2, flags );
    if (url.isLocalFile()) {
        QTimer::singleShot(0, job, SLOT(slotFinished()));
        Scheduler::cancelJob(job); // deletes the slave if not 0
    }
    return job;
}

#ifndef KDE_NO_DEPRECATED
StatJob *KIO::stat(const QUrl& url, bool sideIsSource, short int details, JobFlags flags )
{
    //qDebug() << "stat" << url;
    KIO_ARGS << url;
    StatJob * job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide( sideIsSource ? StatJob::SourceSide : StatJob::DestinationSide );
    job->setDetails( details );
    return job;
}
#endif

StatJob *KIO::stat(const QUrl& url, KIO::StatJob::StatSide side, short int details, JobFlags flags )
{
    //qDebug() << "stat" << url;
    KIO_ARGS << url;
    StatJob * job = StatJobPrivate::newJob(url, CMD_STAT, packedArgs, flags);
    job->setSide( side );
    job->setDetails( details );
    return job;
}

SimpleJob *KIO::http_update_cache( const QUrl& url, bool no_cache, const QDateTime &expireDate)
{
    Q_ASSERT(url.scheme() == "http" || url.scheme() == "https");
    // Send http update_cache command (2)
    KIO_ARGS << (int)2 << url << no_cache << qlonglong(expireDate.toMSecsSinceEpoch() / 1000);
    SimpleJob * job = SimpleJobPrivate::newJob(url, CMD_SPECIAL, packedArgs);
    Scheduler::setJobPriority(job, 1);
    return job;
}

//////////

TransferJob::TransferJob(TransferJobPrivate &dd)
    : SimpleJob(dd)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_PUT) {
        d->m_extraFlags |= JobPrivate::EF_TransferJobDataSent;
    }
}

TransferJob::~TransferJob()
{
}

// Slave sends data
void TransferJob::slotData( const QByteArray &_data)
{
    Q_D(TransferJob);
    if (d->m_command == CMD_GET && !d->m_isMimetypeEmitted) {
        qWarning() << "mimeType() not emitted when sending first data!; job URL ="
                       << d->m_url << "data size =" << _data.size();
    }
    // shut up the warning, HACK: downside is that it changes the meaning of the variable
    d->m_isMimetypeEmitted = true;

    if (d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error()) {
        emit data(this, _data);
    }
}

void KIO::TransferJob::setTotalSize(KIO::filesize_t bytes)
{
    setTotalAmount(KJob::Bytes, bytes);
}

// Slave got a redirection request
void TransferJob::slotRedirection(const QUrl &url)
{
    Q_D(TransferJob);
    //qDebug() << url;
    if (!KUrlAuthorized::authorizeUrlAction("redirect", d->m_url, url))
    {
        qWarning() << "Redirection from" << d->m_url << "to" << url << "REJECTED!";
        return;
    }

    // Some websites keep redirecting to themselves where each redirection
    // acts as the stage in a state-machine. We define "endless redirections"
    // as 5 redirections to the same URL.
    if (d->m_redirectionList.count(url) > 5)
    {
       //qDebug() << "CYCLIC REDIRECTION!";
       setError( ERR_CYCLIC_LINK );
       setErrorText(d->m_url.toDisplayString());
    }
    else
    {
       d->m_redirectionURL = url; // We'll remember that when the job finishes
       d->m_redirectionList.append(url);
       d->m_outgoingMetaData["ssl_was_in_use"] = d->m_incomingMetaData["ssl_in_use"];
       // Tell the user that we haven't finished yet
       emit redirection(this, d->m_redirectionURL);
    }
}

void TransferJob::slotFinished()
{
    Q_D(TransferJob);

    //qDebug() << d->m_url;
    if (!d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid()) {

        //qDebug() << "Redirection to" << m_redirectionURL;
        if (queryMetaData("permanent-redirect")=="true")
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);

        if (d->m_redirectionHandlingEnabled) {
            // Honour the redirection
            // We take the approach of "redirecting this same job"
            // Another solution would be to create a subjob, but the same problem
            // happens (unpacking+repacking)
            d->staticData.truncate(0);
            d->m_incomingMetaData.clear();
            if (queryMetaData("cache") != "reload")
                addMetaData("cache","refresh");
            d->m_internalSuspended = false;
            // The very tricky part is the packed arguments business
            QString dummyStr;
            QUrl dummyUrl;
            QDataStream istream( d->m_packedArgs );
            switch( d->m_command ) {
                case CMD_GET: {
                    d->m_packedArgs.truncate(0);
                    QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
                    stream << d->m_redirectionURL;
                    break;
                }
                case CMD_PUT: {
                    int permissions;
                    qint8 iOverwrite, iResume;
                    istream >> dummyUrl >> iOverwrite >> iResume >> permissions;
                    d->m_packedArgs.truncate(0);
                    QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
                    stream << d->m_redirectionURL << iOverwrite << iResume << permissions;
                    break;
                }
                case CMD_SPECIAL: {
                    int specialcmd;
                    istream >> specialcmd;
                    if (specialcmd == 1) // HTTP POST
                    {
                      d->m_outgoingMetaData.remove(QLatin1String("content-type"));
                      addMetaData("cache","reload");
                      d->m_packedArgs.truncate(0);
                      QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
                      stream << d->m_redirectionURL;
                      d->m_command = CMD_GET;
                    }
                    break;
                }
            }
            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    SimpleJob::slotFinished();
}

void TransferJob::setAsyncDataEnabled(bool enabled)
{
    Q_D(TransferJob);
    if (enabled)
        d->m_extraFlags |= JobPrivate::EF_TransferJobAsync;
    else
        d->m_extraFlags &= ~JobPrivate::EF_TransferJobAsync;
}

void TransferJob::sendAsyncData(const QByteArray &dataForSlave)
{
    Q_D(TransferJob);
    if (d->m_extraFlags & JobPrivate::EF_TransferJobNeedData)
    {
       d->m_slave->send( MSG_DATA, dataForSlave );
       if (d->m_extraFlags & JobPrivate::EF_TransferJobDataSent) // put job -> emit progress
       {
           KIO::filesize_t size = processedAmount(KJob::Bytes)+dataForSlave.size();
           setProcessedAmount(KJob::Bytes, size);
       }
    }

    d->m_extraFlags &= ~JobPrivate::EF_TransferJobNeedData;
}

#ifndef KDE_NO_DEPRECATED
void TransferJob::setReportDataSent(bool enabled)
{
    Q_D(TransferJob);
    if (enabled)
       d->m_extraFlags |= JobPrivate::EF_TransferJobDataSent;
    else
       d->m_extraFlags &= ~JobPrivate::EF_TransferJobDataSent;
}
#endif

#ifndef KDE_NO_DEPRECATED
bool TransferJob::reportDataSent() const
{
    return (d_func()->m_extraFlags & JobPrivate::EF_TransferJobDataSent);
}
#endif

QString TransferJob::mimetype() const
{
    return d_func()->m_mimetype;
}

QUrl TransferJob::redirectUrl() const
{
    return d_func()->m_redirectionURL;
}

// Slave requests data
void TransferJob::slotDataReq()
{
    Q_D(TransferJob);
    QByteArray dataForSlave;

    d->m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (!d->staticData.isEmpty())
    {
       dataForSlave = d->staticData;
       d->staticData.clear();
    }
    else
    {
       emit dataReq( this, dataForSlave);

       if (d->m_extraFlags & JobPrivate::EF_TransferJobAsync)
          return;
    }

    static const int max_size = 14 * 1024 * 1024;
    if (dataForSlave.size() > max_size)
    {
       //qDebug() << "send" << dataForSlave.size() / 1024 / 1024 << "MB of data in TransferJob::dataReq. This needs to be split, which requires a copy. Fix the application.";
       d->staticData = QByteArray(dataForSlave.data() + max_size ,  dataForSlave.size() - max_size);
       dataForSlave.truncate(max_size);
    }

    sendAsyncData(dataForSlave);

    if (d->m_subJob)
    {
       // Bitburger protocol in action
       d->internalSuspend(); // Wait for more data from subJob.
       d->m_subJob->d_func()->internalResume(); // Ask for more!
    }
}

void TransferJob::slotMimetype( const QString& type )
{
    Q_D(TransferJob);
    d->m_mimetype = type;
    if (d->m_command == CMD_GET && d->m_isMimetypeEmitted) {
        qWarning() << "mimetype() emitted again, or after sending first data!; job URL =" << d->m_url;
    }
    d->m_isMimetypeEmitted = true;
    emit mimetype( this, type );
}


void TransferJobPrivate::internalSuspend()
{
    m_internalSuspended = true;
    if (m_slave)
       m_slave->suspend();
}

void TransferJobPrivate::internalResume()
{
    m_internalSuspended = false;
    if ( m_slave && !q_func()->isSuspended() )
        m_slave->resume();
}

bool TransferJob::doResume()
{
    Q_D(TransferJob);
    if ( !SimpleJob::doResume() )
        return false;
    if ( d->m_internalSuspended )
        d->internalSuspend();
    return true;
}

bool TransferJob::isErrorPage() const
{
    return d_func()->m_errorPage;
}

void TransferJobPrivate::start(Slave *slave)
{
    Q_Q(TransferJob);
    Q_ASSERT(slave);
    JobPrivate::emitTransferring(q, m_url);
    q->connect( slave, SIGNAL(data(QByteArray)),
             SLOT(slotData(QByteArray)) );

    if (m_outgoingDataSource)
        q->connect( slave, SIGNAL(dataReq()),
                SLOT(slotDataReqFromDevice()) );
    else
        q->connect( slave, SIGNAL(dataReq()),
                SLOT(slotDataReq()) );

    q->connect(slave, SIGNAL(redirection(QUrl)),
               SLOT(slotRedirection(QUrl)));

    q->connect( slave, SIGNAL(mimeType(QString)),
             SLOT(slotMimetype(QString)) );

    q->connect( slave, SIGNAL(errorPage()),
             SLOT(slotErrorPage()) );

    q->connect( slave, SIGNAL(needSubUrlData()),
             SLOT(slotNeedSubUrlData()) );

    q->connect( slave, SIGNAL(canResume(KIO::filesize_t)),
             SLOT(slotCanResume(KIO::filesize_t)) );

    if (slave->suspended())
    {
       m_mimetype = "unknown";
       // WABA: The slave was put on hold. Resume operation.
       slave->resume();
    }

    SimpleJobPrivate::start(slave);
    if (m_internalSuspended)
       slave->suspend();
}

void TransferJobPrivate::slotNeedSubUrlData()
{
    Q_Q(TransferJob);
    // Job needs data from subURL.
    m_subJob = KIO::get( m_subUrl, NoReload, HideProgressInfo);
    internalSuspend(); // Put job on hold until we have some data.
    q->connect(m_subJob, SIGNAL(data(KIO::Job*,QByteArray)),
            SLOT(slotSubUrlData(KIO::Job*,QByteArray)));
    q->addSubjob(m_subJob);
}

void TransferJobPrivate::slotSubUrlData(KIO::Job*, const QByteArray &data)
{
    // The Alternating Bitburg protocol in action again.
    staticData = data;
    m_subJob->d_func()->internalSuspend(); // Put job on hold until we have delivered the data.
    internalResume(); // Activate ourselves again.
}

void TransferJob::slotMetaData( const KIO::MetaData &_metaData)
{
    Q_D(TransferJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

void TransferJobPrivate::slotErrorPage()
{
    m_errorPage = true;
}

void TransferJobPrivate::slotCanResume( KIO::filesize_t offset )
{
    Q_Q(TransferJob);
    emit q->canResume(q, offset);
}

void TransferJobPrivate::slotDataReqFromDevice()
{
    Q_Q(TransferJob);

    QByteArray dataForSlave;

    m_extraFlags |= JobPrivate::EF_TransferJobNeedData;

    if (m_outgoingDataSource)
        dataForSlave = m_outgoingDataSource.data()->read(MAX_READ_BUF_SIZE);

    if (dataForSlave.isEmpty())
    {
        emit q->dataReq(q, dataForSlave);
        if (m_extraFlags & JobPrivate::EF_TransferJobAsync)
            return;
    }

    q->sendAsyncData(dataForSlave);

    if (m_subJob)
    {
       // Bitburger protocol in action
       internalSuspend(); // Wait for more data from subJob.
       m_subJob->d_func()->internalResume(); // Ask for more!
    }
}

void TransferJob::slotResult( KJob *job)
{
    Q_D(TransferJob);
    // This can only be our suburl.
    Q_ASSERT(job == d->m_subJob);

   SimpleJob::slotResult( job );

   if (!error() && job == d->m_subJob)
   {
      d->m_subJob = 0; // No action required
      d->internalResume(); // Make sure we get the remaining data.
   }
}

void TransferJob::setModificationTime( const QDateTime& mtime )
{
    addMetaData( "modified", mtime.toString( Qt::ISODate ) );
}

TransferJob *KIO::get( const QUrl& url, LoadType reload, JobFlags flags )
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    TransferJob * job = TransferJobPrivate::newJob(url, CMD_GET, packedArgs,
                                                   QByteArray(), flags);
    if (reload == Reload)
       job->addMetaData("cache", "reload");
    return job;
}

class KIO::StoredTransferJobPrivate: public TransferJobPrivate
{
public:
    StoredTransferJobPrivate(const QUrl& url, int command,
                             const QByteArray &packedArgs,
                             const QByteArray &_staticData)
        : TransferJobPrivate(url, command, packedArgs, _staticData),
          m_uploadOffset( 0 )
        {}
    StoredTransferJobPrivate(const QUrl& url, int command,
                             const QByteArray &packedArgs,
                             QIODevice* ioDevice)
        : TransferJobPrivate(url, command, packedArgs, ioDevice),
          m_uploadOffset( 0 )
        {}

    QByteArray m_data;
    int m_uploadOffset;

    void slotStoredData( KIO::Job *job, const QByteArray &data );
    void slotStoredDataReq( KIO::Job *job, QByteArray &data );

    Q_DECLARE_PUBLIC(StoredTransferJob)

    static inline StoredTransferJob *newJob(const QUrl &url, int command,
                                            const QByteArray &packedArgs,
                                            const QByteArray &staticData, JobFlags flags)
    {
        StoredTransferJob *job = new StoredTransferJob(
            *new StoredTransferJobPrivate(url, command, packedArgs, staticData));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo))
            KIO::getJobTracker()->registerJob(job);
        return job;
    }

    static inline StoredTransferJob *newJob(const QUrl &url, int command,
                                            const QByteArray &packedArgs,
                                            QIODevice* ioDevice, JobFlags flags)
    {
        StoredTransferJob *job = new StoredTransferJob(
            *new StoredTransferJobPrivate(url, command, packedArgs, ioDevice));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo))
            KIO::getJobTracker()->registerJob(job);
        return job;
    }
};

namespace KIO {
    class PostErrorJob : public StoredTransferJob
    {
    public:

        PostErrorJob(int _error, const QString& url, const QByteArray &packedArgs, const QByteArray &postData)
            : StoredTransferJob(*new StoredTransferJobPrivate(QUrl(), CMD_SPECIAL, packedArgs, postData))
            {
                setError( _error );
                setErrorText( url );
            }

        PostErrorJob(int _error, const QString& url, const QByteArray &packedArgs, QIODevice* ioDevice)
            : StoredTransferJob(*new StoredTransferJobPrivate(QUrl(), CMD_SPECIAL, packedArgs, ioDevice))
            {
                setError( _error );
                setErrorText( url );
            }
    };
}

static int isUrlPortBad(const QUrl& url)
{
    int _error = 0;

    // filter out some malicious ports
    static const int bad_ports[] = {
        1,   // tcpmux
        7,   // echo
        9,   // discard
        11,   // systat
        13,   // daytime
        15,   // netstat
        17,   // qotd
        19,   // chargen
        20,   // ftp-data
        21,   // ftp-cntl
        22,   // ssh
        23,   // telnet
        25,   // smtp
        37,   // time
        42,   // name
        43,   // nicname
        53,   // domain
        77,   // priv-rjs
        79,   // finger
        87,   // ttylink
        95,   // supdup
        101,  // hostriame
        102,  // iso-tsap
        103,  // gppitnp
        104,  // acr-nema
        109,  // pop2
        110,  // pop3
        111,  // sunrpc
        113,  // auth
        115,  // sftp
        117,  // uucp-path
        119,  // nntp
        123,  // NTP
        135,  // loc-srv / epmap
        139,  // netbios
        143,  // imap2
        179,  // BGP
        389,  // ldap
        512,  // print / exec
        513,  // login
        514,  // shell
        515,  // printer
        526,  // tempo
        530,  // courier
        531,  // Chat
        532,  // netnews
        540,  // uucp
        556,  // remotefs
        587,  // sendmail
        601,  //
        989,  // ftps data
        990,  // ftps
        992,  // telnets
        993,  // imap/SSL
        995,  // pop3/SSL
        1080, // SOCKS
        2049, // nfs
        4045, // lockd
        6000, // x11
        6667, // irc
        0};
    if (url.port() != 80)
    {
        const int port = url.port();
        for (int cnt=0; bad_ports[cnt] && bad_ports[cnt] <= port; ++cnt)
            if (port == bad_ports[cnt])
            {
                _error = KIO::ERR_POST_DENIED;
                break;
            }
    }

    if ( _error )
    {
        static bool override_loaded = false;
        static QList< int >* overriden_ports = NULL;
        if( !override_loaded ) {
            KConfig cfg( "kio_httprc" );
            overriden_ports = new QList< int >;
            *overriden_ports = cfg.group(QString()).readEntry( "OverriddenPorts", QList<int>() );
            override_loaded = true;
        }
        for( QList< int >::ConstIterator it = overriden_ports->constBegin();
                it != overriden_ports->constEnd();
                ++it ) {
            if( overriden_ports->contains( url.port())) {
                _error = 0;
            }
        }
    }

    // filter out non https? protocols
    if ((url.scheme() != "http") && (url.scheme() != "https" ))
        _error = KIO::ERR_POST_DENIED;

    if (!_error && !KUrlAuthorized::authorizeUrlAction("open", QUrl(), url))
        _error = KIO::ERR_ACCESS_DENIED;

    return _error;
}

static KIO::PostErrorJob* precheckHttpPost(const QUrl& url, QIODevice* ioDevice, JobFlags flags)
{
    // if request is not valid, return an invalid transfer job
    const int _error = isUrlPortBad(url);

    if (_error)
    {
        KIO_ARGS << (int)1 << url;
        PostErrorJob * job = new PostErrorJob(_error, url.toString(), packedArgs, ioDevice);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

    // all is ok, return 0
    return 0;
}

static KIO::PostErrorJob* precheckHttpPost(const QUrl& url, const QByteArray& postData, JobFlags flags)
{
    // if request is not valid, return an invalid transfer job
    const int _error = isUrlPortBad(url);

    if (_error)
    {
        KIO_ARGS << (int)1 << url;
        PostErrorJob * job = new PostErrorJob(_error, url.toString(), packedArgs, postData);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        return job;
    }

    // all is ok, return 0
    return 0;
}

TransferJob *KIO::http_post( const QUrl& url, const QByteArray &postData, JobFlags flags )
{
    bool redirection = false;
    QUrl _url(url);
    if (_url.path().isEmpty())
    {
      redirection = true;
      _url.setPath("/");
    }

    TransferJob* job = precheckHttpPost(_url, postData, flags);
    if (job)
        return job;

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << static_cast<qint64>(postData.size());
    job = TransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, postData, flags);

    if (redirection)
      QTimer::singleShot(0, job, SLOT(slotPostRedirection()) );

    return job;
}

TransferJob *KIO::http_post( const QUrl& url, QIODevice* ioDevice, qint64 size, JobFlags flags )
{
    bool redirection = false;
    QUrl _url(url);
    if (_url.path().isEmpty())
    {
      redirection = true;
      _url.setPath("/");
    }

    TransferJob* job = precheckHttpPost(_url, ioDevice, flags);
    if (job)
        return job;

    // If no size is specified and the QIODevice is not a sequential one,
    // attempt to obtain the size information from it.
    Q_ASSERT(ioDevice);
    if (size < 0)
        size = ((ioDevice && !ioDevice->isSequential()) ? ioDevice->size() : -1);

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << size;
    job = TransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, ioDevice, flags);

    if (redirection)
      QTimer::singleShot(0, job, SLOT(slotPostRedirection()) );

    return job;
}

TransferJob* KIO::http_delete(const QUrl& url, JobFlags flags)
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    TransferJob * job = TransferJobPrivate::newJob(url, CMD_DEL, packedArgs,
                                                   QByteArray(), flags);
    return job;
}

StoredTransferJob *KIO::storedHttpPost( const QByteArray& postData, const QUrl& url, JobFlags flags )
{
    QUrl _url(url);
    if (_url.path().isEmpty())
    {
      _url.setPath("/");
    }

    StoredTransferJob* job = precheckHttpPost(_url, postData, flags);
    if (job)
        return job;

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << static_cast<qint64>(postData.size());
    job = StoredTransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, postData, flags );
    return job;
}

StoredTransferJob *KIO::storedHttpPost( QIODevice* ioDevice, const QUrl& url, qint64 size, JobFlags flags )
{
    QUrl _url(url);
    if (_url.path().isEmpty())
    {
      _url.setPath("/");
    }

    StoredTransferJob* job = precheckHttpPost(_url, ioDevice, flags);
    if (job)
        return job;

    // If no size is specified and the QIODevice is not a sequential one,
    // attempt to obtain the size information from it.
    Q_ASSERT(ioDevice);
    if (size < 0)
        size = ((ioDevice && !ioDevice->isSequential()) ? ioDevice->size() : -1);

    // Send http post command (1), decoded path and encoded query
    KIO_ARGS << (int)1 << _url << size;
    job = StoredTransferJobPrivate::newJob(_url, CMD_SPECIAL, packedArgs, ioDevice, flags );
    return job;
}

// http post got redirected from http://host to http://host/ by TransferJob
// We must do this redirection ourselves because redirections by the
// slave change post jobs into get jobs.
void TransferJobPrivate::slotPostRedirection()
{
    Q_Q(TransferJob);
    //qDebug() << m_url;
    // Tell the user about the new url.
    emit q->redirection(q, m_url);
}


TransferJob *KIO::put( const QUrl& url, int permissions, JobFlags flags )
{
    KIO_ARGS << url << qint8( (flags & Overwrite) ? 1 : 0 ) << qint8( (flags & Resume) ? 1 : 0 ) << permissions;
    return TransferJobPrivate::newJob(url, CMD_PUT, packedArgs, QByteArray(), flags);
}

//////////

StoredTransferJob::StoredTransferJob(StoredTransferJobPrivate &dd)
    : TransferJob(dd)
{
    connect( this, SIGNAL(data(KIO::Job*,QByteArray)),
             SLOT(slotStoredData(KIO::Job*,QByteArray)) );
    connect( this, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
             SLOT(slotStoredDataReq(KIO::Job*,QByteArray&)) );
}

StoredTransferJob::~StoredTransferJob()
{
}

void StoredTransferJob::setData( const QByteArray& arr )
{
    Q_D(StoredTransferJob);
    Q_ASSERT( d->m_data.isNull() ); // check that we're only called once
    Q_ASSERT( d->m_uploadOffset == 0 ); // no upload started yet
    d->m_data = arr;
    setTotalSize( d->m_data.size() );
}

QByteArray StoredTransferJob::data() const
{
    return d_func()->m_data;
}

void StoredTransferJobPrivate::slotStoredData( KIO::Job *, const QByteArray &data )
{
  // check for end-of-data marker:
  if ( data.size() == 0 )
    return;
  unsigned int oldSize = m_data.size();
  m_data.resize( oldSize + data.size() );
  memcpy( m_data.data() + oldSize, data.data(), data.size() );
}

void StoredTransferJobPrivate::slotStoredDataReq( KIO::Job *, QByteArray &data )
{
  // Inspired from kmail's KMKernel::byteArrayToRemoteFile
  // send the data in 64 KB chunks
  const int MAX_CHUNK_SIZE = 64*1024;
  int remainingBytes = m_data.size() - m_uploadOffset;
  if( remainingBytes > MAX_CHUNK_SIZE ) {
    // send MAX_CHUNK_SIZE bytes to the receiver (deep copy)
    data = QByteArray( m_data.data() + m_uploadOffset, MAX_CHUNK_SIZE );
    m_uploadOffset += MAX_CHUNK_SIZE;
    //qDebug() << "Sending " << MAX_CHUNK_SIZE << " bytes ("
    //                << remainingBytes - MAX_CHUNK_SIZE << " bytes remain)\n";
  } else {
    // send the remaining bytes to the receiver (deep copy)
    data = QByteArray( m_data.data() + m_uploadOffset, remainingBytes );
    m_data = QByteArray();
    m_uploadOffset = 0;
    //qDebug() << "Sending " << remainingBytes << " bytes\n";
  }
}

StoredTransferJob *KIO::storedGet( const QUrl& url, LoadType reload, JobFlags flags )
{
    // Send decoded path and encoded query
    KIO_ARGS << url;
    StoredTransferJob * job = StoredTransferJobPrivate::newJob(url, CMD_GET, packedArgs, QByteArray(), flags);
    if (reload == Reload)
       job->addMetaData("cache", "reload");
    return job;
}

StoredTransferJob *KIO::storedPut( const QByteArray& arr, const QUrl& url, int permissions,
                                   JobFlags flags )
{
    KIO_ARGS << url << qint8( (flags & Overwrite) ? 1 : 0 ) << qint8( (flags & Resume) ? 1 : 0 ) << permissions;
    StoredTransferJob * job = StoredTransferJobPrivate::newJob(url, CMD_PUT, packedArgs, QByteArray(), flags );
    job->setData( arr );
    return job;
}

//////////

class KIO::MimetypeJobPrivate: public KIO::TransferJobPrivate
{
public:
    MimetypeJobPrivate(const QUrl& url, int command, const QByteArray &packedArgs)
        : TransferJobPrivate(url, command, packedArgs, QByteArray())
        {}

    Q_DECLARE_PUBLIC(MimetypeJob)

    static inline MimetypeJob *newJob(const QUrl& url, int command, const QByteArray &packedArgs,
                                      JobFlags flags)
    {
        MimetypeJob *job = new MimetypeJob(*new MimetypeJobPrivate(url, command, packedArgs));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
            emitStating(job, url);
        }
        return job;
    }
};

MimetypeJob::MimetypeJob(MimetypeJobPrivate &dd)
    : TransferJob(dd)
{
}

MimetypeJob::~MimetypeJob()
{
}

void MimetypeJob::slotFinished( )
{
    Q_D(MimetypeJob);
    //qDebug();
    if ( error() == KIO::ERR_IS_DIRECTORY )
    {
        // It is in fact a directory. This happens when HTTP redirects to FTP.
        // Due to the "protocol doesn't support listing" code in KRun, we
        // assumed it was a file.
        //qDebug() << "It is in fact a directory!";
        d->m_mimetype = QString::fromLatin1("inode/directory");
        emit TransferJob::mimetype( this, d->m_mimetype );
        setError( 0 );
    }

    if ( !d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() && !error() )
    {
        //qDebug() << "Redirection to " << m_redirectionURL;
        if (queryMetaData("permanent-redirect")=="true")
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);

        if (d->m_redirectionHandlingEnabled)
        {
            d->staticData.truncate(0);
            d->m_internalSuspended = false;
            d->m_packedArgs.truncate(0);
            QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
            stream << d->m_redirectionURL;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return slave to the scheduler
    TransferJob::slotFinished();
}

MimetypeJob *KIO::mimetype(const QUrl& url, JobFlags flags)
{
    KIO_ARGS << url;
    return MimetypeJobPrivate::newJob(url, CMD_MIMETYPE, packedArgs, flags);
}

//////////////////////////

class KIO::DirectCopyJobPrivate: public KIO::SimpleJobPrivate
{
public:
    DirectCopyJobPrivate(const QUrl& url, int command, const QByteArray &packedArgs)
        : SimpleJobPrivate(url, command, packedArgs)
        {}

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    virtual void start(Slave *slave);

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

void DirectCopyJobPrivate::start( Slave* slave )
{
    Q_Q(DirectCopyJob);
    q->connect( slave, SIGNAL(canResume(KIO::filesize_t)),
             SLOT(slotCanResume(KIO::filesize_t)) );
    SimpleJobPrivate::start(slave);
}

void DirectCopyJob::slotCanResume( KIO::filesize_t offset )
{
    emit canResume(this, offset);
}

//////////////////////////

/** @internal */
class KIO::FileCopyJobPrivate: public KIO::JobPrivate
{
public:
    FileCopyJobPrivate(const QUrl& src, const QUrl& dest, int permissions,
                       bool move, JobFlags flags)
        : m_sourceSize(filesize_t(-1)), m_src(src), m_dest(dest), m_moveJob(0), m_copyJob(0), m_delJob(0),
          m_chmodJob(0), m_getJob(0), m_putJob(0), m_permissions(permissions),
          m_move(move), m_mustChmod(0), m_flags(flags)
        {
        }
    KIO::filesize_t m_sourceSize;
    QDateTime m_modificationTime;
    QUrl m_src;
    QUrl m_dest;
    QByteArray m_buffer;
    SimpleJob *m_moveJob;
    SimpleJob *m_copyJob;
    SimpleJob *m_delJob;
    SimpleJob *m_chmodJob;
    TransferJob *m_getJob;
    TransferJob *m_putJob;
    int m_permissions;
    bool m_move:1;
    bool m_canResume:1;
    bool m_resumeAnswerSent:1;
    bool m_mustChmod:1;
    JobFlags m_flags;

    void startBestCopyMethod();
    void startCopyJob();
    void startCopyJob(const QUrl &slave_url);
    void startRenameJob(const QUrl &slave_url);
    void startDataPump();
    void connectSubjob( SimpleJob * job );

    void slotStart();
    void slotData( KIO::Job *, const QByteArray &data);
    void slotDataReq( KIO::Job *, QByteArray &data);
    void slotMimetype( KIO::Job*, const QString& type );
    /**
     * Forward signal from subjob
     * @param job the job that emitted this signal
     * @param size the processed size in bytes
     */
    void slotProcessedSize( KJob *job, qulonglong size );
    /**
     * Forward signal from subjob
     * @param job the job that emitted this signal
     * @param size the total size
     */
    void slotTotalSize( KJob *job, qulonglong size );
    /**
     * Forward signal from subjob
     * @param job the job that emitted this signal
     * @param pct the percentage
     */
    void slotPercent( KJob *job, unsigned long pct );
    /**
     * Forward signal from subjob
     * @param job the job that emitted this signal
     * @param offset the offset to resume from
     */
    void slotCanResume( KIO::Job *job, KIO::filesize_t offset );

    Q_DECLARE_PUBLIC(FileCopyJob)

    static inline FileCopyJob* newJob(const QUrl& src, const QUrl& dest, int permissions, bool move,
                                      JobFlags flags)
    {
        //qDebug() << src << "->" << dest;
        FileCopyJob *job = new FileCopyJob(
            *new FileCopyJobPrivate(src, dest, permissions, move, flags));
        job->setProperty("destUrl", dest.toString());
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo))
            KIO::getJobTracker()->registerJob(job);
        return job;
    }
};

/*
 * The FileCopyJob works according to the famous Bavarian
 * 'Alternating Bitburger Protocol': we either drink a beer or we
 * we order a beer, but never both at the same time.
 * Translated to io-slaves: We alternate between receiving a block of data
 * and sending it away.
 */
FileCopyJob::FileCopyJob(FileCopyJobPrivate &dd)
    : Job(dd)
{
    //qDebug();
    QTimer::singleShot(0, this, SLOT(slotStart()));
}

void FileCopyJobPrivate::slotStart()
{
    Q_Q(FileCopyJob);
    if (!m_move)
        JobPrivate::emitCopying( q, m_src, m_dest );
    else
        JobPrivate::emitMoving( q, m_src, m_dest );

   if ( m_move )
   {
      // The if() below must be the same as the one in startBestCopyMethod
      if ((m_src.scheme() == m_dest.scheme()) &&
          (m_src.host() == m_dest.host()) &&
          (m_src.port() == m_dest.port()) &&
          (m_src.userName() == m_dest.userName()) &&
          (m_src.password() == m_dest.password()))
      {
         startRenameJob(m_src);
         return;
      }
      else if (m_src.isLocalFile() && KProtocolManager::canRenameFromFile(m_dest))
      {
         startRenameJob(m_dest);
         return;
      }
      else if (m_dest.isLocalFile() && KProtocolManager::canRenameToFile(m_src))
      {
         startRenameJob(m_src);
         return;
      }
      // No fast-move available, use copy + del.
   }
   startBestCopyMethod();
}

void FileCopyJobPrivate::startBestCopyMethod()
{
   if ((m_src.scheme() == m_dest.scheme()) &&
       (m_src.host() == m_dest.host()) &&
       (m_src.port() == m_dest.port()) &&
       (m_src.userName() == m_dest.userName()) &&
       (m_src.password() == m_dest.password()))
   {
      startCopyJob();
   }
   else if (m_src.isLocalFile() && KProtocolManager::canCopyFromFile(m_dest))
   {
      startCopyJob(m_dest);
   }
   else if (m_dest.isLocalFile() && KProtocolManager::canCopyToFile(m_src) &&
            !KIO::Scheduler::isSlaveOnHoldFor(m_src))
   {
      startCopyJob(m_src);
   }
   else
   {
      startDataPump();
   }
}

FileCopyJob::~FileCopyJob()
{
}

void FileCopyJob::setSourceSize( KIO::filesize_t size )
{
    Q_D(FileCopyJob);
    d->m_sourceSize = size;
    if (size != (KIO::filesize_t) -1)
        setTotalAmount(KJob::Bytes, size);
}

void FileCopyJob::setModificationTime( const QDateTime& mtime )
{
    Q_D(FileCopyJob);
    d->m_modificationTime = mtime;
}

QUrl FileCopyJob::srcUrl() const
{
    return d_func()->m_src;
}

QUrl FileCopyJob::destUrl() const
{
    return d_func()->m_dest;
}

void FileCopyJobPrivate::startCopyJob()
{
    startCopyJob(m_src);
}

void FileCopyJobPrivate::startCopyJob(const QUrl &slave_url)
{
    Q_Q(FileCopyJob);
    //qDebug();
    KIO_ARGS << m_src << m_dest << m_permissions << (qint8) (m_flags & Overwrite);
    m_copyJob = new DirectCopyJob(slave_url, packedArgs);
    if (m_modificationTime.isValid()) {
      m_copyJob->addMetaData( "modified", m_modificationTime.toString( Qt::ISODate ) ); // #55804
    }
    q->addSubjob( m_copyJob );
    connectSubjob( m_copyJob );
    q->connect( m_copyJob, SIGNAL(canResume(KIO::Job*,KIO::filesize_t)),
                SLOT(slotCanResume(KIO::Job*,KIO::filesize_t)));
}

void FileCopyJobPrivate::startRenameJob(const QUrl &slave_url)
{
    Q_Q(FileCopyJob);
    m_mustChmod = true;  // CMD_RENAME by itself doesn't change permissions
    KIO_ARGS << m_src << m_dest << (qint8) (m_flags & Overwrite);
    m_moveJob = SimpleJobPrivate::newJobNoUi(slave_url, CMD_RENAME, packedArgs);
    if (m_modificationTime.isValid()) {
      m_moveJob->addMetaData( "modified", m_modificationTime.toString( Qt::ISODate ) ); // #55804
    }
    q->addSubjob( m_moveJob );
    connectSubjob( m_moveJob );
}

void FileCopyJobPrivate::connectSubjob( SimpleJob * job )
{
    Q_Q(FileCopyJob);
    q->connect( job, SIGNAL(totalSize(KJob*,qulonglong)),
                SLOT(slotTotalSize(KJob*,qulonglong)) );

    q->connect( job, SIGNAL(processedSize(KJob*,qulonglong)),
                SLOT(slotProcessedSize(KJob*,qulonglong)) );

    q->connect( job, SIGNAL(percent(KJob*,ulong)),
                SLOT(slotPercent(KJob*,ulong)) );

}

bool FileCopyJob::doSuspend()
{
    Q_D(FileCopyJob);
    if (d->m_moveJob)
        d->m_moveJob->suspend();

    if (d->m_copyJob)
        d->m_copyJob->suspend();

    if (d->m_getJob)
        d->m_getJob->suspend();

    if (d->m_putJob)
        d->m_putJob->suspend();

    Job::doSuspend();
    return true;
}

bool FileCopyJob::doResume()
{
    Q_D(FileCopyJob);
    if (d->m_moveJob)
        d->m_moveJob->resume();

    if (d->m_copyJob)
        d->m_copyJob->resume();

    if (d->m_getJob)
        d->m_getJob->resume();

    if (d->m_putJob)
        d->m_putJob->resume();

    Job::doResume();
    return true;
}

void FileCopyJobPrivate::slotProcessedSize( KJob *, qulonglong size )
{
    Q_Q(FileCopyJob);
    q->setProcessedAmount(KJob::Bytes, size);
}

void FileCopyJobPrivate::slotTotalSize( KJob*, qulonglong size )
{
    Q_Q(FileCopyJob);
    if (size != q->totalAmount(KJob::Bytes))
    {
        q->setTotalAmount(KJob::Bytes, size);
    }
}

void FileCopyJobPrivate::slotPercent( KJob*, unsigned long pct )
{
  Q_Q(FileCopyJob);
  if ( pct > q->percent() ) {
      q->setPercent( pct );
  }
}

void FileCopyJobPrivate::startDataPump()
{
    Q_Q(FileCopyJob);
    //qDebug();

    m_canResume = false;
    m_resumeAnswerSent = false;
    m_getJob = 0L; // for now
    m_putJob = put( m_dest, m_permissions, (m_flags | HideProgressInfo) /* no GUI */);
    //qDebug() << "m_putJob=" << m_putJob << "m_dest=" << m_dest;
    if ( m_modificationTime.isValid() ) {
        m_putJob->setModificationTime( m_modificationTime );
    }

    // The first thing the put job will tell us is whether we can
    // resume or not (this is always emitted)
    q->connect( m_putJob, SIGNAL(canResume(KIO::Job*,KIO::filesize_t)),
                SLOT(slotCanResume(KIO::Job*,KIO::filesize_t)));
    q->connect( m_putJob, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
                SLOT(slotDataReq(KIO::Job*,QByteArray&)));
    q->addSubjob( m_putJob );
}

void FileCopyJobPrivate::slotCanResume( KIO::Job* job, KIO::filesize_t offset )
{
    Q_Q(FileCopyJob);
    if ( job == m_putJob || job == m_copyJob )
    {
        //qDebug() << "'can resume' from PUT job. offset=" << KIO::number(offset);
        if (offset)
        {
            RenameDialog_Result res = R_RESUME;

            if (!KProtocolManager::autoResume() && !(m_flags & Overwrite) && m_uiDelegateExtension)
            {
                QString newPath;
                KIO::Job* job = ( q->parentJob() ) ? q->parentJob() : q;
                // Ask confirmation about resuming previous transfer
                res = m_uiDelegateExtension->askFileRename(
                      job, i18n("File Already Exists"),
                      m_src,
                      m_dest,
                      (RenameDialog_Mode) (M_OVERWRITE | M_RESUME | M_NORENAME), newPath,
                      m_sourceSize, offset );
            }

            if ( res == R_OVERWRITE || (m_flags & Overwrite) )
              offset = 0;
            else if ( res == R_CANCEL )
            {
                if ( job == m_putJob ) {
                    m_putJob->kill( FileCopyJob::Quietly );
                    q->removeSubjob(m_putJob);
                    m_putJob = 0;
                } else {
                    m_copyJob->kill( FileCopyJob::Quietly );
                    q->removeSubjob(m_copyJob);
                    m_copyJob = 0;
                }
                q->setError( ERR_USER_CANCELED );
                q->emitResult();
                return;
            }
        }
        else
            m_resumeAnswerSent = true; // No need for an answer

        if ( job == m_putJob )
        {
            m_getJob = KIO::get( m_src, NoReload, HideProgressInfo /* no GUI */ );
            //qDebug() << "m_getJob=" << m_getJob << m_src;
            m_getJob->addMetaData( "errorPage", "false" );
            m_getJob->addMetaData( "AllowCompressedPage", "false" );
            // Set size in subjob. This helps if the slave doesn't emit totalSize.
            if ( m_sourceSize != (KIO::filesize_t)-1 )
                m_getJob->setTotalAmount(KJob::Bytes, m_sourceSize);
            if (offset)
            {
                //qDebug() << "Setting metadata for resume to" << (unsigned long) offset;
                // TODO KDE4: rename to seek or offset and document it
                // This isn't used only for resuming, but potentially also for extracting (#72302).
                m_getJob->addMetaData( "resume", KIO::number(offset) );

                // Might or might not get emitted
                q->connect( m_getJob, SIGNAL(canResume(KIO::Job*,KIO::filesize_t)),
                            SLOT(slotCanResume(KIO::Job*,KIO::filesize_t)));
            }
            jobSlave(m_putJob)->setOffset( offset );

            m_putJob->d_func()->internalSuspend();
            q->addSubjob( m_getJob );
            connectSubjob( m_getJob ); // Progress info depends on get
            m_getJob->d_func()->internalResume(); // Order a beer

            q->connect( m_getJob, SIGNAL(data(KIO::Job*,QByteArray)),
                        SLOT(slotData(KIO::Job*,QByteArray)) );
            q->connect( m_getJob, SIGNAL(mimetype(KIO::Job*,QString)),
                        SLOT(slotMimetype(KIO::Job*,QString)) );
        }
        else // copyjob
        {
            jobSlave(m_copyJob)->sendResumeAnswer( offset != 0 );
        }
    }
    else if ( job == m_getJob )
    {
        // Cool, the get job said ok, we can resume
        m_canResume = true;
        //qDebug() << "'can resume' from the GET job -> we can resume";

        jobSlave(m_getJob)->setOffset( jobSlave(m_putJob)->offset() );
    }
    else
        qWarning() << "unknown job=" << job << "m_getJob=" << m_getJob << "m_putJob=" << m_putJob;
}

void FileCopyJobPrivate::slotData( KIO::Job * , const QByteArray &data)
{
   //qDebug() << "data size:" << data.size();
   Q_ASSERT(m_putJob);
   if (!m_putJob) return; // Don't crash
   m_getJob->d_func()->internalSuspend();
   m_putJob->d_func()->internalResume(); // Drink the beer
   m_buffer += data;

   // On the first set of data incoming, we tell the "put" slave about our
   // decision about resuming
   if (!m_resumeAnswerSent)
   {
       m_resumeAnswerSent = true;
       //qDebug() << "(first time) -> send resume answer " << m_canResume;
       jobSlave(m_putJob)->sendResumeAnswer( m_canResume );
   }
}

void FileCopyJobPrivate::slotDataReq( KIO::Job * , QByteArray &data)
{
   Q_Q(FileCopyJob);
   //qDebug();
   if (!m_resumeAnswerSent && !m_getJob) {
       // This can't happen
       q->setError( ERR_INTERNAL );
       q->setErrorText( "'Put' job did not send canResume or 'Get' job did not send data!" );
       m_putJob->kill( FileCopyJob::Quietly );
       q->removeSubjob(m_putJob);
       m_putJob = 0;
       q->emitResult();
       return;
   }
   if (m_getJob)
   {
       m_getJob->d_func()->internalResume(); // Order more beer
       m_putJob->d_func()->internalSuspend();
   }
   data = m_buffer;
   m_buffer = QByteArray();
}

void FileCopyJobPrivate::slotMimetype( KIO::Job*, const QString& type )
{
    Q_Q(FileCopyJob);
    emit q->mimetype( q, type );
}

void FileCopyJob::slotResult( KJob *job)
{
   Q_D(FileCopyJob);
   //qDebug() << "this=" << this << "job=" << job;
   removeSubjob(job);
   // Did job have an error ?
   if ( job->error() )
   {
      if ((job == d->m_moveJob) && (job->error() == ERR_UNSUPPORTED_ACTION))
      {
         d->m_moveJob = 0;
         d->startBestCopyMethod();
         return;
      }
      else if ((job == d->m_copyJob) && (job->error() == ERR_UNSUPPORTED_ACTION))
      {
         d->m_copyJob = 0;
         d->startDataPump();
         return;
      }
      else if (job == d->m_getJob)
      {
        d->m_getJob = 0L;
        if (d->m_putJob)
        {
          d->m_putJob->kill( Quietly );
          removeSubjob( d->m_putJob );
        }
      }
      else if (job == d->m_putJob)
      {
        d->m_putJob = 0L;
        if (d->m_getJob)
        {
          d->m_getJob->kill( Quietly );
          removeSubjob( d->m_getJob );
        }
      }
      setError( job->error() );
      setErrorText( job->errorText() );
      emitResult();
      return;
   }

   if (d->m_mustChmod)
   {
       // If d->m_permissions == -1, keep the default permissions
       if (d->m_permissions != -1)
       {
           d->m_chmodJob = chmod(d->m_dest, d->m_permissions);
       }
       d->m_mustChmod = false;
   }

   if (job == d->m_moveJob)
   {
      d->m_moveJob = 0; // Finished
   }

   if (job == d->m_copyJob)
   {
      d->m_copyJob = 0;
      if (d->m_move)
      {
         d->m_delJob = file_delete( d->m_src, HideProgressInfo/*no GUI*/ ); // Delete source
         addSubjob(d->m_delJob);
      }
   }

   if (job == d->m_getJob)
   {
       //qDebug() << "m_getJob finished";
      d->m_getJob = 0; // No action required
      if (d->m_putJob)
          d->m_putJob->d_func()->internalResume();
   }

   if (job == d->m_putJob)
   {
       //qDebug() << "m_putJob finished";
      d->m_putJob = 0;
      if (d->m_getJob)
      {
          // The get job is still running, probably after emitting data(QByteArray())
          // and before we receive its finished().
         d->m_getJob->d_func()->internalResume();
      }
      if (d->m_move)
      {
         d->m_delJob = file_delete( d->m_src, HideProgressInfo/*no GUI*/ ); // Delete source
         addSubjob(d->m_delJob);
      }
   }

   if (job == d->m_delJob)
   {
      d->m_delJob = 0; // Finished
   }

   if (job == d->m_chmodJob)
   {
       d->m_chmodJob = 0; // Finished
   }

   if ( !hasSubjobs() )
       emitResult();
}

FileCopyJob *KIO::file_copy( const QUrl& src, const QUrl& dest, int permissions,
                             JobFlags flags )
{
    return FileCopyJobPrivate::newJob(src, dest, permissions, false, flags);
}

FileCopyJob *KIO::file_move( const QUrl& src, const QUrl& dest, int permissions,
                             JobFlags flags )
{
    FileCopyJob* job = FileCopyJobPrivate::newJob(src, dest, permissions, true, flags);
    if (job->uiDelegateExtension())
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::UpdateContent);
    return job;
}

SimpleJob *KIO::file_delete( const QUrl& src, JobFlags flags )
{
    KIO_ARGS << src << qint8(true); // isFile
    SimpleJob* job = SimpleJobPrivate::newJob(src, CMD_DEL, packedArgs, flags);
    if (job->uiDelegateExtension())
        job->uiDelegateExtension()->createClipboardUpdater(job, JobUiDelegateExtension::RemoveContent);
    return job;
}

//////////

class KIO::ListJobPrivate: public KIO::SimpleJobPrivate
{
public:
    ListJobPrivate(const QUrl& url, bool _recursive,
                   const QString &prefix, const QString &displayPrefix,
                   bool _includeHidden)
        : SimpleJobPrivate(url, CMD_LISTDIR, QByteArray()),
          recursive(_recursive), includeHidden(_includeHidden),
          m_prefix(prefix), m_displayPrefix(displayPrefix), m_processedEntries(0)
    {}
    bool recursive;
    bool includeHidden;
    QString m_prefix;
    QString m_displayPrefix;
    unsigned long m_processedEntries;
    QUrl m_redirectionURL;

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    virtual void start( Slave *slave );

    void slotListEntries( const KIO::UDSEntryList& list );
    void slotRedirection(const QUrl &url);
    void gotEntries( KIO::Job * subjob, const KIO::UDSEntryList& list );

    Q_DECLARE_PUBLIC(ListJob)

    static inline ListJob *newJob(const QUrl& u, bool _recursive,
                                  const QString &prefix, const QString &displayPrefix,
                                  bool _includeHidden, JobFlags flags = HideProgressInfo)
    {
        ListJob *job = new ListJob(*new ListJobPrivate(u, _recursive, prefix, displayPrefix, _includeHidden));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo))
            KIO::getJobTracker()->registerJob(job);
        return job;
    }
    static inline ListJob *newJobNoUi(const QUrl& u, bool _recursive,
                                      const QString &prefix, const QString &displayPrefix,
                                      bool _includeHidden)
    {
        return new ListJob(*new ListJobPrivate(u, _recursive, prefix, displayPrefix, _includeHidden));
    }
};

ListJob::ListJob(ListJobPrivate &dd)
    : SimpleJob(dd)
{
    Q_D(ListJob);
    // We couldn't set the args when calling the parent constructor,
    // so do it now.
    QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
    stream << d->m_url;
}

ListJob::~ListJob()
{
}

void ListJobPrivate::slotListEntries( const KIO::UDSEntryList& list )
{
    Q_Q(ListJob);
    // Emit progress info (takes care of emit processedSize and percent)
    m_processedEntries += list.count();
    slotProcessedSize( m_processedEntries );

    if (recursive) {
        UDSEntryList::ConstIterator it = list.begin();
        const UDSEntryList::ConstIterator end = list.end();

        for (; it != end; ++it) {

            const UDSEntry& entry = *it;

            QUrl itemURL;
            const QString udsUrl = entry.stringValue(KIO::UDSEntry::UDS_URL);
            QString filename;
            if (!udsUrl.isEmpty()) {
                itemURL = QUrl(udsUrl);
                filename = itemURL.fileName();
            } else { // no URL, use the name
                itemURL = q->url();
                filename = entry.stringValue(KIO::UDSEntry::UDS_NAME);
                Q_ASSERT(!filename.isEmpty()); // we'll recurse forever otherwise :)
                itemURL.setPath(itemURL.path() + '/' + filename);
            }

            if (entry.isDir() && !entry.isLink()) {
                Q_ASSERT(!filename.isEmpty());
                QString displayName = entry.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
                if (displayName.isEmpty())
                    displayName = filename;
                // skip hidden dirs when listing if requested
                if (filename != ".." && filename != "." && (includeHidden || filename[0] != '.')) {
                    ListJob *job = ListJobPrivate::newJobNoUi(itemURL,
                                               true /*recursive*/,
                                               m_prefix + filename + '/',
                                               m_displayPrefix + displayName + '/',
                                               includeHidden);
                    Scheduler::setJobPriority(job, 1);
                    q->connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
                               SLOT(gotEntries(KIO::Job*,KIO::UDSEntryList)));
                    q->addSubjob(job);
                }
            }
        }
    }

    // Not recursive, or top-level of recursive listing : return now (send . and .. as well)
    // exclusion of hidden files also requires the full sweep, but the case for full-listing
    // a single dir is probably common enough to justify the shortcut
    if (m_prefix.isNull() && includeHidden) {
        emit q->entries(q, list);
    } else {
        // cull the unwanted hidden dirs and/or parent dir references from the listing, then emit that
        UDSEntryList newlist;

        UDSEntryList::const_iterator it = list.begin();
        const UDSEntryList::const_iterator end = list.end();
        for (; it != end; ++it) {

            // Modify the name in the UDSEntry
            UDSEntry newone = *it;
            const QString filename = newone.stringValue( KIO::UDSEntry::UDS_NAME );
            QString displayName = newone.stringValue(KIO::UDSEntry::UDS_DISPLAY_NAME);
            if (displayName.isEmpty())
                displayName = filename;
            // Avoid returning entries like subdir/. and subdir/.., but include . and .. for
            // the toplevel dir, and skip hidden files/dirs if that was requested
            if (  (m_prefix.isNull() || (filename != ".." && filename != ".") )
                  && (includeHidden || (filename[0] != '.') )  )
            {
                // ## Didn't find a way to use the iterator instead of re-doing a key lookup
                newone.insert( KIO::UDSEntry::UDS_NAME, m_prefix + filename );
                newone.insert(KIO::UDSEntry::UDS_DISPLAY_NAME, m_displayPrefix + displayName);
                newlist.append(newone);
            }
        }

        emit q->entries(q, newlist);
    }
}

void ListJobPrivate::gotEntries(KIO::Job *, const KIO::UDSEntryList& list )
{
    // Forward entries received by subjob - faking we received them ourselves
    Q_Q(ListJob);
    emit q->entries(q, list);
}

void ListJob::slotResult( KJob * job )
{
    if (job->error()) {
	// If we can't list a subdir, the result is still ok
	// This is why we override KCompositeJob::slotResult - to not set
	// an error on parent job.
	// Let's emit a signal about this though
	emit subError(this, static_cast<KIO::ListJob*>(job));
    }
    removeSubjob(job);
    if (!hasSubjobs())
        emitResult();
}

void ListJobPrivate::slotRedirection(const QUrl & url)
{
    Q_Q(ListJob);
    if (!KUrlAuthorized::authorizeUrlAction("redirect", m_url, url))
    {
        qWarning() << "Redirection from" << m_url << "to" << url << "REJECTED!";
        return;
    }
    m_redirectionURL = url; // We'll remember that when the job finishes
    emit q->redirection( q, m_redirectionURL );
}

void ListJob::slotFinished()
{
    Q_D(ListJob);

    // Support for listing archives as directories
    if ( error() == KIO::ERR_IS_FILE && d->m_url.isLocalFile() ) {
#if 0   // TODO port this KDE 3 code to KProtocolManager::protocolForArchiveMimetype.
        // Note however that this code never worked in KDE 4.
        KMimeType::Ptr ptr = KMimeType::findByUrl( d->m_url, 0, true, true );
        if ( ptr ) {
            QString proto = ptr->property("X-KDE-LocalProtocol").toString();
            if ( !proto.isEmpty() && KProtocolInfo::isKnownProtocol( proto) ) {
                d->m_redirectionURL = d->m_url;
                d->m_redirectionURL.setScheme(proto);
                setError( 0 );
                emit redirection(this,d->m_redirectionURL);
            }
        }
#endif
    }

    if ( !d->m_redirectionURL.isEmpty() && d->m_redirectionURL.isValid() && !error() ) {

        //qDebug() << "Redirection to " << d->m_redirectionURL;
        if (queryMetaData("permanent-redirect")=="true")
            emit permanentRedirection(this, d->m_url, d->m_redirectionURL);

        if ( d->m_redirectionHandlingEnabled ) {
            d->m_packedArgs.truncate(0);
            QDataStream stream( &d->m_packedArgs, QIODevice::WriteOnly );
            stream << d->m_redirectionURL;

            d->restartAfterRedirection(&d->m_redirectionURL);
            return;
        }
    }

    // Return slave to the scheduler
    SimpleJob::slotFinished();
}

void ListJob::slotMetaData( const KIO::MetaData &_metaData)
{
    Q_D(ListJob);
    SimpleJob::slotMetaData(_metaData);
    storeSSLSessionFromJob(d->m_redirectionURL);
}

ListJob *KIO::listDir( const QUrl& url, JobFlags flags, bool includeHidden )
{
    return ListJobPrivate::newJob(url, false, QString(), QString(), includeHidden, flags);
}

ListJob *KIO::listRecursive( const QUrl& url, JobFlags flags, bool includeHidden )
{
    return ListJobPrivate::newJob(url, true, QString(), QString(), includeHidden, flags);
}

void ListJob::setUnrestricted(bool unrestricted)
{
    Q_D(ListJob);
    if (unrestricted)
        d->m_extraFlags |= JobPrivate::EF_ListJobUnrestricted;
    else
        d->m_extraFlags &= ~JobPrivate::EF_ListJobUnrestricted;
}

void ListJobPrivate::start(Slave *slave)
{
    Q_Q(ListJob);
    if (!KUrlAuthorized::authorizeUrlAction("list", m_url, m_url) &&
        !(m_extraFlags & EF_ListJobUnrestricted))
    {
        q->setError( ERR_ACCESS_DENIED );
        q->setErrorText( m_url.toDisplayString() );
        QTimer::singleShot(0, q, SLOT(slotFinished()) );
        return;
    }
    q->connect( slave, SIGNAL(listEntries(KIO::UDSEntryList)),
             SLOT(slotListEntries(KIO::UDSEntryList)));
    q->connect( slave, SIGNAL(totalSize(KIO::filesize_t)),
             SLOT(slotTotalSize(KIO::filesize_t)) );
    q->connect( slave, SIGNAL(redirection(QUrl)),
             SLOT(slotRedirection(QUrl)) );

    SimpleJobPrivate::start(slave);
}

const QUrl& ListJob::redirectionUrl() const
{
    return d_func()->m_redirectionURL;
}

////

class KIO::MultiGetJobPrivate: public KIO::TransferJobPrivate
{
public:
    MultiGetJobPrivate(const QUrl& url)
        : TransferJobPrivate(url, 0, QByteArray(), QByteArray()),
          m_currentEntry( 0, QUrl(), MetaData() )
    {}
    struct GetRequest {
        GetRequest(long _id, const QUrl &_url, const MetaData &_metaData)
            : id(_id), url(_url), metaData(_metaData) { }
        long id;
        QUrl url;
        MetaData metaData;

        inline bool operator==( const GetRequest& req ) const
            { return req.id == id; }
    };
    typedef QLinkedList<GetRequest> RequestQueue;

    RequestQueue m_waitQueue;
    RequestQueue m_activeQueue;
    GetRequest m_currentEntry;
    bool b_multiGetActive;

    /**
     * @internal
     * Called by the scheduler when a @p slave gets to
     * work on this job.
     * @param slave the slave that starts working on this job
     */
    virtual void start(Slave *slave);

    bool findCurrentEntry();
    void flushQueue(QLinkedList<GetRequest> &queue);

    Q_DECLARE_PUBLIC(MultiGetJob)

    static inline MultiGetJob *newJob(const QUrl &url)
    {
        MultiGetJob *job = new MultiGetJob(*new MultiGetJobPrivate(url));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        return job;
    }
};

MultiGetJob::MultiGetJob(MultiGetJobPrivate &dd)
    : TransferJob(dd)
{
}

MultiGetJob::~MultiGetJob()
{
}

void MultiGetJob::get(long id, const QUrl &url, const MetaData &metaData)
{
   Q_D(MultiGetJob);
   MultiGetJobPrivate::GetRequest entry(id, url, metaData);
   entry.metaData["request-id"] = QString::number(id);
   d->m_waitQueue.append(entry);
}

void MultiGetJobPrivate::flushQueue(RequestQueue &queue)
{
   // Use multi-get
   // Scan all jobs in m_waitQueue
   RequestQueue::iterator wqit = m_waitQueue.begin();
   const RequestQueue::iterator wqend = m_waitQueue.end();
   while ( wqit != wqend )
   {
      const GetRequest& entry = *wqit;
      if ((m_url.scheme() == entry.url.scheme()) &&
          (m_url.host() == entry.url.host()) &&
          (m_url.port() == entry.url.port()) &&
          (m_url.userName() == entry.url.userName()))
      {
         queue.append( entry );
         wqit = m_waitQueue.erase( wqit );
      }
      else
      {
         ++wqit;
      }
   }
   // Send number of URLs, (URL, metadata)*
   KIO_ARGS << (qint32) queue.count();
   RequestQueue::const_iterator qit = queue.begin();
   const RequestQueue::const_iterator qend = queue.end();
   for( ; qit != qend; ++qit )
   {
      stream << (*qit).url << (*qit).metaData;
   }
   m_packedArgs = packedArgs;
   m_command = CMD_MULTI_GET;
   m_outgoingMetaData.clear();
}

void MultiGetJobPrivate::start(Slave *slave)
{
   // Add first job from m_waitQueue and add it to m_activeQueue
   GetRequest entry = m_waitQueue.takeFirst();
   m_activeQueue.append(entry);

   m_url = entry.url;

   if (!entry.url.scheme().startsWith(QLatin1String("http")))
   {
      // Use normal get
      KIO_ARGS << entry.url;
      m_packedArgs = packedArgs;
      m_outgoingMetaData = entry.metaData;
      m_command = CMD_GET;
      b_multiGetActive = false;
   }
   else
   {
      flushQueue(m_activeQueue);
      b_multiGetActive = true;
   }

   TransferJobPrivate::start(slave); // Anything else to do??
}

bool MultiGetJobPrivate::findCurrentEntry()
{
   if (b_multiGetActive)
   {
      long id = m_incomingMetaData["request-id"].toLong();
      RequestQueue::const_iterator qit = m_activeQueue.begin();
      const RequestQueue::const_iterator qend = m_activeQueue.end();
      for( ; qit != qend; ++qit )
      {
         if ((*qit).id == id)
         {
            m_currentEntry = *qit;
            return true;
         }
      }
      m_currentEntry.id = 0;
      return false;
   }
   else
   {
      if ( m_activeQueue.isEmpty() )
        return false;
      m_currentEntry = m_activeQueue.first();
      return true;
   }
}

void MultiGetJob::slotRedirection(const QUrl &url)
{
  Q_D(MultiGetJob);
  if (!d->findCurrentEntry()) return; // Error
  if (!KUrlAuthorized::authorizeUrlAction("redirect", d->m_url, url))
  {
     qWarning() << "Redirection from" << d->m_currentEntry.url << "to" << url << "REJECTED!";
     return;
  }
  d->m_redirectionURL = url;
  get(d->m_currentEntry.id, d->m_redirectionURL, d->m_currentEntry.metaData); // Try again
}


void MultiGetJob::slotFinished()
{
  Q_D(MultiGetJob);
  if (!d->findCurrentEntry()) return;
  if (d->m_redirectionURL.isEmpty())
  {
     // No redirection, tell the world that we are finished.
     emit result(d->m_currentEntry.id);
  }
  d->m_redirectionURL = QUrl();
  setError( 0 );
  d->m_incomingMetaData.clear();
  d->m_activeQueue.removeAll(d->m_currentEntry);
  if (d->m_activeQueue.count() == 0)
  {
     if (d->m_waitQueue.count() == 0)
     {
        // All done
        TransferJob::slotFinished();
     }
     else
     {
        // return slave to pool
        // fetch new slave for first entry in d->m_waitQueue and call start
        // again.
        d->slaveDone();

        d->m_url = d->m_waitQueue.first().url;
        if ((d->m_extraFlags & JobPrivate::EF_KillCalled) == 0) {
            Scheduler::doJob(this);
        }
     }
  }
}

void MultiGetJob::slotData( const QByteArray &_data)
{
    Q_D(MultiGetJob);
    if(d->m_redirectionURL.isEmpty() || !d->m_redirectionURL.isValid() || error())
        emit data(d->m_currentEntry.id, _data);
}

void MultiGetJob::slotMimetype( const QString &_mimetype )
{
  Q_D(MultiGetJob);
  if (d->b_multiGetActive)
  {
     MultiGetJobPrivate::RequestQueue newQueue;
     d->flushQueue(newQueue);
     if (!newQueue.isEmpty())
     {
        d->m_activeQueue += newQueue;
        d->m_slave->send( d->m_command, d->m_packedArgs );
     }
  }
  if (!d->findCurrentEntry()) return; // Error, unknown request!
  emit mimetype(d->m_currentEntry.id, _mimetype);
}

MultiGetJob *KIO::multi_get(long id, const QUrl &url, const MetaData &metaData)
{
    MultiGetJob * job = MultiGetJobPrivate::newJob(url);
    job->get(id, url, metaData);
    return job;
}

class KIO::SpecialJobPrivate: public TransferJobPrivate
{
    SpecialJobPrivate(const QUrl& url, int command,
                             const QByteArray &packedArgs,
                             const QByteArray &_staticData)
        : TransferJobPrivate(url, command, packedArgs, _staticData)
    {}
};

SpecialJob::SpecialJob(const QUrl &url, const QByteArray &packedArgs)
    : TransferJob(*new TransferJobPrivate(url, CMD_SPECIAL, packedArgs, QByteArray()))
{
}

SpecialJob::~SpecialJob()
{
}

void SpecialJob::setArguments(const QByteArray &data)
{
    Q_D(SpecialJob);
    d->m_packedArgs = data;
}

QByteArray SpecialJob::arguments() const
{
    return d_func()->m_packedArgs;
}

#include "moc_jobclasses.cpp"
#include "moc_job_p.cpp"
