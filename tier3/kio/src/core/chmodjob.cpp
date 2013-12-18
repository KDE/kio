/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>
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

#include "chmodjob.h"

#include <klocalizedstring.h>
#include <kio/jobuidelegatefactory.h>
#include <QtCore/QFile>
#include <QtCore/QLinkedList>
#include <QDebug>

#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include "job_p.h"

namespace KIO {

    struct ChmodInfo
    {
        QUrl url;
        int permissions;
    };

    enum ChmodJobState {
        CHMODJOB_STATE_LISTING,
        CHMODJOB_STATE_CHMODING
    };

    class ChmodJobPrivate: public KIO::JobPrivate
    {
    public:
        ChmodJobPrivate(const KFileItemList& lstItems, int permissions, int mask,
                        int newOwner, int newGroup, bool recursive)
            : state( CHMODJOB_STATE_LISTING )
            , m_permissions( permissions )
            , m_mask( mask )
            , m_newOwner( newOwner )
            , m_newGroup( newGroup )
            , m_recursive( recursive )
            , m_bAutoSkipFiles( false )
            , m_lstItems( lstItems )
        {
        }

        ChmodJobState state;
        int m_permissions;
        int m_mask;
        int m_newOwner;
        int m_newGroup;
        bool m_recursive;
        bool m_bAutoSkipFiles;
        KFileItemList m_lstItems;
        QLinkedList<ChmodInfo> m_infos; // linkedlist since we keep removing the first item

        void _k_chmodNextFile();
        void _k_slotEntries( KIO::Job * , const KIO::UDSEntryList & );
        void _k_processList();

        Q_DECLARE_PUBLIC(ChmodJob)

        static inline ChmodJob *newJob(const KFileItemList& lstItems, int permissions, int mask,
                                       int newOwner, int newGroup, bool recursive, JobFlags flags)
        {
            ChmodJob *job = new ChmodJob(*new ChmodJobPrivate(lstItems,permissions,mask,
                                                              newOwner,newGroup,recursive));
            job->setUiDelegate(KIO::createDefaultJobUiDelegate());
            if (!(flags & HideProgressInfo))
                KIO::getJobTracker()->registerJob(job);
            return job;
        }
    };

} // namespace KIO

using namespace KIO;

ChmodJob::ChmodJob(ChmodJobPrivate &dd)
    : KIO::Job(dd)
{
    QMetaObject::invokeMethod( this, "_k_processList", Qt::QueuedConnection );
}

ChmodJob::~ChmodJob()
{
}

void ChmodJobPrivate::_k_processList()
{
    Q_Q(ChmodJob);
    while ( !m_lstItems.isEmpty() )
    {
        const KFileItem item = m_lstItems.first();
        if ( !item.isLink() ) // don't do anything with symlinks
        {
            // File or directory -> remember to chmod
            ChmodInfo info;
            info.url = item.url();
            // This is a toplevel file, we apply changes directly (no +X emulation here)
            const mode_t permissions = item.permissions() & 0777; // get rid of "set gid" and other special flags
            info.permissions = ( m_permissions & m_mask ) | ( permissions & ~m_mask );
            /*//qDebug() << "toplevel url:" << info.url << "\n current permissions=" << QString::number(permissions,8)
                          << "\n wanted permission=" << QString::number(m_permissions,8)
                          << "\n with mask=" << QString::number(m_mask,8)
                          << "\n with ~mask (mask bits we keep) =" << QString::number((uint)~m_mask,8)
                          << "\n bits we keep =" << QString::number(permissions & ~m_mask,8)
                          << "\n new permissions = " << QString::number(info.permissions,8);*/
            m_infos.prepend( info );
            //qDebug() << "processList : Adding info for " << info.url;
            // Directory and recursive -> list
            if ( item.isDir() && m_recursive )
            {
                //qDebug() << "ChmodJob::processList dir -> listing";
                KIO::ListJob * listJob = KIO::listRecursive( item.url(), KIO::HideProgressInfo );
                q->connect( listJob, SIGNAL(entries( KIO::Job *,
                                                     const KIO::UDSEntryList& )),
                            SLOT(_k_slotEntries(KIO::Job*,KIO::UDSEntryList)));
                q->addSubjob( listJob );
                return; // we'll come back later, when this one's finished
            }
        }
        m_lstItems.removeFirst();
    }
    //qDebug() << "ChmodJob::processList -> going to STATE_CHMODING";
    // We have finished, move on
    state = CHMODJOB_STATE_CHMODING;
    _k_chmodNextFile();
}

void ChmodJobPrivate::_k_slotEntries( KIO::Job*, const KIO::UDSEntryList & list )
{
    KIO::UDSEntryList::ConstIterator it = list.begin();
    KIO::UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it) {
        const KIO::UDSEntry& entry = *it;
        const bool isLink = !entry.stringValue( KIO::UDSEntry::UDS_LINK_DEST ).isEmpty();
        const QString relativePath = entry.stringValue( KIO::UDSEntry::UDS_NAME );
        if ( !isLink && relativePath != ".." )
        {
            const mode_t permissions = entry.numberValue( KIO::UDSEntry::UDS_ACCESS )
                                       & 0777; // get rid of "set gid" and other special flags

            ChmodInfo info;
            info.url = m_lstItems.first().url(); // base directory
            info.url.setPath(info.url.path() + '/' + relativePath);
            int mask = m_mask;
            // Emulate -X: only give +x to files that had a +x bit already
            // So the check is the opposite : if the file had no x bit, don't touch x bits
            // For dirs this doesn't apply
            if ( !entry.isDir() )
            {
                int newPerms = m_permissions & mask;
                if ( (newPerms & 0111) && !(permissions & 0111) )
                {
                    // don't interfere with mandatory file locking
                    if ( newPerms & 02000 )
                      mask = mask & ~0101;
                    else
                      mask = mask & ~0111;
                }
            }
            info.permissions = ( m_permissions & mask ) | ( permissions & ~mask );
            /*//qDebug() << info.url << "\n current permissions=" << QString::number(permissions,8)
                          << "\n wanted permission=" << QString::number(m_permissions,8)
                          << "\n with mask=" << QString::number(mask,8)
                          << "\n with ~mask (mask bits we keep) =" << QString::number((uint)~mask,8)
                          << "\n bits we keep =" << QString::number(permissions & ~mask,8)
                          << "\n new permissions = " << QString::number(info.permissions,8);*/
            // Prepend this info in our todo list.
            // This way, the toplevel dirs are done last.
            m_infos.prepend( info );
        }
    }
}

void ChmodJobPrivate::_k_chmodNextFile()
{
    Q_Q(ChmodJob);
    if ( !m_infos.isEmpty() )
    {
        ChmodInfo info = m_infos.takeFirst();
        // First update group / owner (if local file)
        // (permissions have to set after, in case of suid and sgid)
        if ( info.url.isLocalFile() && ( m_newOwner != -1 || m_newGroup != -1 ) )
        {
            QString path = info.url.toLocalFile();
            if ( chown( QFile::encodeName(path), m_newOwner, m_newGroup ) != 0 )
            {
                if (!m_uiDelegateExtension) {
                    emit q->warning(q, i18n("Could not modify the ownership of file %1", path));
                } else if (!m_bAutoSkipFiles) {
                    const QString errMsg = i18n( "<qt>Could not modify the ownership of file <b>%1</b>. You have insufficient access to the file to perform the change.</qt>", path);
                    const SkipDialog_Result skipResult = m_uiDelegateExtension->askSkip(q, m_infos.count() > 1, errMsg);
                    switch (skipResult) {
                    case S_AUTO_SKIP:
                        m_bAutoSkipFiles = true;
                        // fall through
                    case S_SKIP:
                        QMetaObject::invokeMethod(q, "_k_chmodNextFile", Qt::QueuedConnection);
                        return;
                    case S_RETRY:
                        m_infos.prepend(info);
                        QMetaObject::invokeMethod(q, "_k_chmodNextFile", Qt::QueuedConnection);
                        return;
                    case S_CANCEL:
                        q->setError( ERR_USER_CANCELED );
                        q->emitResult();
                        return;
                    }
                }
            }
        }

        /*qDebug() << "chmod'ing" << info.url
                      << "to" << QString::number(info.permissions,8);*/
        KIO::SimpleJob * job = KIO::chmod( info.url, info.permissions );
        // copy the metadata for acl and default acl
        const QString aclString = q->queryMetaData( QLatin1String("ACL_STRING") );
        const QString defaultAclString = q->queryMetaData( QLatin1String("DEFAULT_ACL_STRING") );
        if ( !aclString.isEmpty() )
            job->addMetaData( QLatin1String("ACL_STRING"), aclString );
        if ( !defaultAclString.isEmpty() )
            job->addMetaData( QLatin1String("DEFAULT_ACL_STRING"), defaultAclString );
        q->addSubjob(job);
    }
    else
        // We have finished
        q->emitResult();
}

void ChmodJob::slotResult( KJob * job )
{
    Q_D(ChmodJob);
    removeSubjob(job);
    if ( job->error() )
    {
        setError( job->error() );
        setErrorText( job->errorText() );
        emitResult();
        return;
    }
    //qDebug() << "d->m_lstItems:" << d->m_lstItems.count();
    switch ( d->state )
    {
        case CHMODJOB_STATE_LISTING:
            d->m_lstItems.removeFirst();
            //qDebug() << "-> processList";
            d->_k_processList();
            return;
        case CHMODJOB_STATE_CHMODING:
            //qDebug() << "-> chmodNextFile";
            d->_k_chmodNextFile();
            return;
        default:
            assert(0);
            return;
    }
}

ChmodJob *KIO::chmod( const KFileItemList& lstItems, int permissions, int mask,
                      const QString& owner, const QString& group,
                      bool recursive, JobFlags flags )
{
    uid_t newOwnerID = uid_t(-1); // chown(2) : -1 means no change
    if ( !owner.isEmpty() )
    {
        struct passwd* pw = getpwnam(QFile::encodeName(owner));
        if ( pw == 0L )
            qWarning() << " ERROR: No user" << owner;
        else
            newOwnerID = pw->pw_uid;
    }
    gid_t newGroupID = gid_t(-1); // chown(2) : -1 means no change
    if ( !group.isEmpty() )
    {
        struct group* g = getgrnam(QFile::encodeName(group));
        if ( g == 0L )
            qWarning() << " ERROR: No group" << group;
        else
            newGroupID = g->gr_gid;
    }
    return ChmodJobPrivate::newJob(lstItems, permissions, mask, newOwnerID,
                                   newGroupID, recursive, flags);
}

#include "moc_chmodjob.cpp"
