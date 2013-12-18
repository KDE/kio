/* This file is part of the KDE project
   Copyright (c) 2004 Kevin Ottens <ervin@ipsquad.net>

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

#include "forwardingslavebase.h"

#include "deletejob.h"
#include "mkdirjob.h"
#include "job.h"

#include <qmimedatabase.h>

#include <QtCore/QEventLoop>


namespace KIO
{

class ForwardingSlaveBasePrivate
{
public:
    ForwardingSlaveBasePrivate(QObject * eventLoopParent) :
        eventLoop(eventLoopParent)
    {}
    ForwardingSlaveBase *q;

    QUrl m_processedURL;
    QUrl m_requestedURL;
    QEventLoop eventLoop;

    bool internalRewriteUrl(const QUrl &url, QUrl &newURL);

    void connectJob(Job *job);
    void connectSimpleJob(SimpleJob *job);
    void connectListJob(ListJob *job);
    void connectTransferJob(TransferJob *job);

    void _k_slotResult(KJob *job);
    void _k_slotWarning(KJob *job, const QString &msg);
    void _k_slotInfoMessage(KJob *job, const QString &msg);
    void _k_slotTotalSize(KJob *job, qulonglong size);
    void _k_slotProcessedSize(KJob *job, qulonglong size);
    void _k_slotSpeed(KJob *job, unsigned long bytesPerSecond);

    // KIO::SimpleJob subclasses
    void _k_slotRedirection(KIO::Job *job, const QUrl &url);

    // KIO::ListJob
    void _k_slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries);

    // KIO::TransferJob
    void _k_slotData(KIO::Job *job, const QByteArray &data);
    void _k_slotDataReq(KIO::Job *job, QByteArray &data);
    void _k_slotMimetype (KIO::Job *job, const QString &type);
    void _k_slotCanResume (KIO::Job *job, KIO::filesize_t offset);
};

ForwardingSlaveBase::ForwardingSlaveBase(const QByteArray &protocol,
                                         const QByteArray &poolSocket,
                                         const QByteArray &appSocket)
    : QObject(), SlaveBase(protocol, poolSocket, appSocket),
      d( new ForwardingSlaveBasePrivate(this) )
{
    d->q = this;
}

ForwardingSlaveBase::~ForwardingSlaveBase()
{
    delete d;
}

bool ForwardingSlaveBasePrivate::internalRewriteUrl(const QUrl &url, QUrl &newURL)
{
    bool result = true;

    if ( url.scheme() == q->mProtocol )
    {
        result = q->rewriteUrl(url, newURL);
    }
    else
    {
        newURL = url;
    }

    m_processedURL = newURL;
    m_requestedURL = url;
    return result;
}

void ForwardingSlaveBase::prepareUDSEntry(KIO::UDSEntry &entry,
                                          bool listing) const
{
    //qDebug() << "listing==" << listing;

    const QString name = entry.stringValue( KIO::UDSEntry::UDS_NAME );
    QString mimetype = entry.stringValue( KIO::UDSEntry::UDS_MIME_TYPE );
    QUrl url;
    const QString urlStr = entry.stringValue( KIO::UDSEntry::UDS_URL );
    const bool url_found = !urlStr.isEmpty();
    if ( url_found )
    {
        url = QUrl(urlStr);
        QUrl new_url(d->m_requestedURL);
        if (listing)
            new_url.setPath(new_url.path() + '/' + url.fileName());
        // ## Didn't find a way to use an iterator instead of re-doing a key lookup
        entry.insert(KIO::UDSEntry::UDS_URL, new_url.toString());
        //qDebug() << "URL =" << url;
        //qDebug() << "New URL =" << new_url;
    }

    if (mimetype.isEmpty())
    {
        QUrl new_url(d->m_processedURL);
        if (url_found && listing) {
            new_url.setPath(new_url.path() + '/' + url.fileName());
        } else if (listing) {
            new_url.setPath(new_url.path() + '/' + name);
        }

        QMimeDatabase db;
        mimetype = db.mimeTypeForUrl(new_url).name();

        entry.insert( KIO::UDSEntry::UDS_MIME_TYPE, mimetype );

        //qDebug() << "New Mimetype = " << mimetype;
    }

    if ( d->m_processedURL.isLocalFile() )
    {
        QUrl new_url(d->m_processedURL);
        if (listing) {
            new_url.setPath(new_url.path() + '/' + name);
        }

        entry.insert(KIO::UDSEntry::UDS_LOCAL_PATH, new_url.toLocalFile());
    }
}

QUrl ForwardingSlaveBase::processedUrl() const
{
    return d->m_processedURL;
}

QUrl ForwardingSlaveBase::requestedUrl() const
{
    return d->m_requestedURL;
}

void ForwardingSlaveBase::get(const QUrl &url)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::TransferJob *job = KIO::get(new_url, NoReload, HideProgressInfo);
        d->connectTransferJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::put(const QUrl &url, int permissions,
                              JobFlags flags)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::TransferJob *job = KIO::put(new_url, permissions,
                                         flags | HideProgressInfo);
        d->connectTransferJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error( KIO::ERR_MALFORMED_URL, url.toDisplayString() );
    }
}

void ForwardingSlaveBase::stat(const QUrl &url)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::SimpleJob *job = KIO::stat(new_url, KIO::HideProgressInfo);
        d->connectSimpleJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::mimetype(const QUrl &url)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::TransferJob *job = KIO::mimetype(new_url, KIO::HideProgressInfo);
        d->connectTransferJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::listDir(const QUrl &url)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::ListJob *job = KIO::listDir(new_url, KIO::HideProgressInfo);
        d->connectListJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::mkdir(const QUrl &url, int permissions)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::SimpleJob *job = KIO::mkdir(new_url, permissions);
        d->connectSimpleJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error( KIO::ERR_MALFORMED_URL, url.toDisplayString() );
    }
}

void ForwardingSlaveBase::rename(const QUrl &src, const QUrl &dest,
                                 JobFlags flags)
{
    //qDebug() << src << "," << dest;

    QUrl new_src, new_dest;
    if( !d->internalRewriteUrl(src, new_src) )
    {
        error(KIO::ERR_DOES_NOT_EXIST, src.toDisplayString());
    }
    else if ( d->internalRewriteUrl(dest, new_dest) )
    {
        KIO::Job *job = KIO::rename(new_src, new_dest, flags);
        d->connectJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error( KIO::ERR_MALFORMED_URL, dest.toDisplayString() );
    }
}

void ForwardingSlaveBase::symlink(const QString &target, const QUrl &dest,
                                  JobFlags flags)
{
    //qDebug() << target << ", " << dest;

    QUrl new_dest;
    if ( d->internalRewriteUrl(dest, new_dest) )
    {
        KIO::SimpleJob *job = KIO::symlink(target, new_dest, flags & HideProgressInfo);
        d->connectSimpleJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error( KIO::ERR_MALFORMED_URL, dest.toDisplayString() );
    }
}

void ForwardingSlaveBase::chmod(const QUrl &url, int permissions)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::SimpleJob *job = KIO::chmod(new_url, permissions);
        d->connectSimpleJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::setModificationTime(const QUrl& url, const QDateTime& mtime)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        KIO::SimpleJob *job = KIO::setModificationTime(new_url, mtime);
        d->connectSimpleJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}

void ForwardingSlaveBase::copy(const QUrl &src, const QUrl &dest,
                               int permissions, JobFlags flags)
{
    //qDebug() << src << "," << dest;

    QUrl new_src, new_dest;
    if ( !d->internalRewriteUrl(src, new_src) )
    {
        error(KIO::ERR_DOES_NOT_EXIST, src.toDisplayString());
    }
    else if( d->internalRewriteUrl(dest, new_dest) )
    {
      // Are you sure you want to display here a ProgressInfo ???
        KIO::Job *job = KIO::file_copy(new_src, new_dest, permissions,
                                       (flags & (~Overwrite) & (~HideProgressInfo)) );
        d->connectJob(job);

        d->eventLoop.exec();
    }
    else
    {
        error( KIO::ERR_MALFORMED_URL, dest.toDisplayString() );
    }
}

void ForwardingSlaveBase::del(const QUrl &url, bool isfile)
{
    //qDebug() << url;

    QUrl new_url;
    if ( d->internalRewriteUrl(url, new_url) )
    {
        if (isfile)
        {
            KIO::DeleteJob *job = KIO::del(new_url, HideProgressInfo);
            d->connectJob(job);
        }
        else
        {
            KIO::SimpleJob *job = KIO::rmdir(new_url);
            d->connectSimpleJob(job);
        }

        d->eventLoop.exec();
    }
    else
    {
        error(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
    }
}


//////////////////////////////////////////////////////////////////////////////

void ForwardingSlaveBasePrivate::connectJob(KIO::Job *job)
{
    // We will forward the warning message, no need to let the job
    // display it itself
    job->setUiDelegate( 0 );

    // Forward metadata (e.g. modification time for put())
    job->setMetaData( q->allMetaData() );
#if 0 // debug code
    //qDebug() << "transferring metadata:";
    const MetaData md = allMetaData();
    for ( MetaData::const_iterator it = md.begin(); it != md.end(); ++it )
        //qDebug() << it.key() << " = " << it.data();
#endif

    q->connect( job, SIGNAL(result(KJob*)),
                SLOT(_k_slotResult(KJob*)) );
    q->connect( job, SIGNAL(warning(KJob*,QString,QString)),
                SLOT(_k_slotWarning(KJob*,QString)) );
    q->connect( job, SIGNAL(infoMessage(KJob*,QString,QString)),
                SLOT(_k_slotInfoMessage(KJob*,QString)) );
    q->connect( job, SIGNAL(totalSize(KJob*,qulonglong)),
                SLOT(_k_slotTotalSize(KJob*,qulonglong)) );
    q->connect( job, SIGNAL(processedSize(KJob*,qulonglong)),
                SLOT(_k_slotProcessedSize(KJob*,qulonglong)) );
    q->connect( job, SIGNAL(speed(KJob*,ulong)),
                SLOT(_k_slotSpeed(KJob*,ulong)) );
}

void ForwardingSlaveBasePrivate::connectSimpleJob(KIO::SimpleJob *job)
{
    connectJob(job);
    q->connect( job, SIGNAL(redirection(KIO::Job*,QUrl)),
                SLOT(_k_slotRedirection(KIO::Job*,QUrl)) );
}

void ForwardingSlaveBasePrivate::connectListJob(KIO::ListJob *job)
{
    connectSimpleJob(job);
    q->connect( job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
                SLOT(_k_slotEntries(KIO::Job*,KIO::UDSEntryList)) );
}

void ForwardingSlaveBasePrivate::connectTransferJob(KIO::TransferJob *job)
{
    connectSimpleJob(job);
    q->connect( job, SIGNAL(data(KIO::Job*,QByteArray)),
                SLOT(_k_slotData(KIO::Job*,QByteArray)) );
    q->connect( job, SIGNAL(dataReq(KIO::Job*,QByteArray&)),
                SLOT(_k_slotDataReq(KIO::Job*,QByteArray&)) );
    q->connect( job, SIGNAL(mimetype(KIO::Job*,QString)),
                SLOT(_k_slotMimetype(KIO::Job*,QString)) );
    q->connect( job, SIGNAL(canResume(KIO::Job*,KIO::filesize_t)),
                SLOT(_k_slotCanResume(KIO::Job*,KIO::filesize_t)) );
}

//////////////////////////////////////////////////////////////////////////////

void ForwardingSlaveBasePrivate::_k_slotResult(KJob *job)
{
    if ( job->error() != 0)
    {
        q->error( job->error(), job->errorText() );
    }
    else
    {
        KIO::StatJob *stat_job = qobject_cast<KIO::StatJob *>(job);
        if ( stat_job!=0L )
        {
            KIO::UDSEntry entry = stat_job->statResult();
	    q->prepareUDSEntry(entry);
            q->statEntry( entry );
        }
        q->finished();
    }

    eventLoop.exit();
}

void ForwardingSlaveBasePrivate::_k_slotWarning(KJob* /*job*/, const QString &msg)
{
    q->warning(msg);
}

void ForwardingSlaveBasePrivate::_k_slotInfoMessage(KJob* /*job*/, const QString &msg)
{
    q->infoMessage(msg);
}

void ForwardingSlaveBasePrivate::_k_slotTotalSize(KJob* /*job*/, qulonglong size)
{
    q->totalSize(size);
}

void ForwardingSlaveBasePrivate::_k_slotProcessedSize(KJob* /*job*/, qulonglong size)
{
    q->processedSize(size);
}

void ForwardingSlaveBasePrivate::_k_slotSpeed(KJob* /*job*/, unsigned long bytesPerSecond)
{
    q->speed(bytesPerSecond);
}

void ForwardingSlaveBasePrivate::_k_slotRedirection(KIO::Job *job, const QUrl &url)
{
    q->redirection(url);

    // We've been redirected stop everything.
    job->kill( KJob::Quietly );
    q->finished();

    eventLoop.exit();
}

void ForwardingSlaveBasePrivate::_k_slotEntries(KIO::Job* /*job*/,
                                      const KIO::UDSEntryList &entries)
{
    KIO::UDSEntryList final_entries = entries;

    KIO::UDSEntryList::iterator it = final_entries.begin();
    const KIO::UDSEntryList::iterator end = final_entries.end();

    for(; it!=end; ++it)
    {
        q->prepareUDSEntry(*it, true);
    }

    q->listEntries( final_entries );
}

void ForwardingSlaveBasePrivate::_k_slotData(KIO::Job* /*job*/, const QByteArray &_data)
{
    q->data(_data);
}

void ForwardingSlaveBasePrivate::_k_slotDataReq(KIO::Job* /*job*/, QByteArray &data)
{
    q->dataReq();
    q->readData(data);
}

void ForwardingSlaveBasePrivate::_k_slotMimetype (KIO::Job* /*job*/, const QString &type)
{
    q->mimeType(type);
}

void ForwardingSlaveBasePrivate::_k_slotCanResume (KIO::Job* /*job*/, KIO::filesize_t offset)
{
    q->canResume(offset);
}

}

#include "moc_forwardingslavebase.cpp"

