/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2000 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "chmodjob.h"
#include "../pathhelpers_p.h"

#include <KLocalizedString>
#include <KUser>
#include <QDebug>


#include "listjob.h"
#include "job_p.h"
#include "jobuidelegatefactory.h"
#include "kioglobal_p.h"

#include <stack>

namespace KIO
{

struct ChmodInfo {
    QUrl url;
    int permissions;
};

enum ChmodJobState {
    CHMODJOB_STATE_LISTING,
    CHMODJOB_STATE_CHMODING,
};

class ChmodJobPrivate: public KIO::JobPrivate
{
public:
    ChmodJobPrivate(const KFileItemList &lstItems, int permissions, int mask,
                    KUserId newOwner, KGroupId newGroup, bool recursive)
        : state(CHMODJOB_STATE_LISTING)
        , m_permissions(permissions)
        , m_mask(mask)
        , m_newOwner(newOwner)
        , m_newGroup(newGroup)
        , m_recursive(recursive)
        , m_bAutoSkipFiles(false)
        , m_lstItems(lstItems)
    {
    }

    ChmodJobState state;
    int m_permissions;
    int m_mask;
    KUserId m_newOwner;
    KGroupId m_newGroup;
    bool m_recursive;
    bool m_bAutoSkipFiles;
    KFileItemList m_lstItems;
    std::stack<ChmodInfo> m_infos;

    void _k_chmodNextFile();
    void _k_slotEntries(KIO::Job *, const KIO::UDSEntryList &);
    void _k_processList();

    Q_DECLARE_PUBLIC(ChmodJob)

    static inline ChmodJob *newJob(const KFileItemList &lstItems, int permissions, int mask,
                                   KUserId newOwner, KGroupId newGroup, bool recursive, JobFlags flags)
    {
        ChmodJob *job = new ChmodJob(*new ChmodJobPrivate(lstItems, permissions, mask,
                                     newOwner, newGroup, recursive));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = ChangeAttr;
        }
        return job;
    }
};

} // namespace KIO

using namespace KIO;

ChmodJob::ChmodJob(ChmodJobPrivate &dd)
    : KIO::Job(dd)
{
    QMetaObject::invokeMethod(this, "_k_processList", Qt::QueuedConnection);
}

ChmodJob::~ChmodJob()
{
}

void ChmodJobPrivate::_k_processList()
{
    Q_Q(ChmodJob);
    while (!m_lstItems.isEmpty()) {
        const KFileItem item = m_lstItems.first();
        if (!item.isLink()) { // don't do anything with symlinks
            // File or directory -> remember to chmod
            ChmodInfo info;
            info.url = item.url();
            // This is a toplevel file, we apply changes directly (no +X emulation here)
            const mode_t permissions = item.permissions() & 0777; // get rid of "set gid" and other special flags
            info.permissions = (m_permissions & m_mask) | (permissions & ~m_mask);
            /*//qDebug() << "toplevel url:" << info.url << "\n current permissions=" << QString::number(permissions,8)
                          << "\n wanted permission=" << QString::number(m_permissions,8)
                          << "\n with mask=" << QString::number(m_mask,8)
                          << "\n with ~mask (mask bits we keep) =" << QString::number((uint)~m_mask,8)
                          << "\n bits we keep =" << QString::number(permissions & ~m_mask,8)
                          << "\n new permissions = " << QString::number(info.permissions,8);*/
            m_infos.push(std::move(info));
            //qDebug() << "processList : Adding info for " << info.url;
            // Directory and recursive -> list
            if (item.isDir() && m_recursive) {
                //qDebug() << "ChmodJob::processList dir -> listing";
                KIO::ListJob *listJob = KIO::listRecursive(item.url(), KIO::HideProgressInfo);
                q->connect(listJob, &KIO::ListJob::entries,
                           q, [this](KIO::Job *job, const KIO::UDSEntryList &entries) { _k_slotEntries(job, entries); });
                q->addSubjob(listJob);
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

void ChmodJobPrivate::_k_slotEntries(KIO::Job *, const KIO::UDSEntryList &list)
{
    KIO::UDSEntryList::ConstIterator it = list.begin();
    KIO::UDSEntryList::ConstIterator end = list.end();
    for (; it != end; ++it) {
        const KIO::UDSEntry &entry = *it;
        const bool isLink = !entry.stringValue(KIO::UDSEntry::UDS_LINK_DEST).isEmpty();
        const QString relativePath = entry.stringValue(KIO::UDSEntry::UDS_NAME);
        if (!isLink && relativePath != QLatin1String("..")) {
            const mode_t permissions = entry.numberValue(KIO::UDSEntry::UDS_ACCESS)
                                       & 0777; // get rid of "set gid" and other special flags

            ChmodInfo info;
            info.url = m_lstItems.first().url(); // base directory
            info.url.setPath(concatPaths(info.url.path(), relativePath));
            int mask = m_mask;
            // Emulate -X: only give +x to files that had a +x bit already
            // So the check is the opposite : if the file had no x bit, don't touch x bits
            // For dirs this doesn't apply
            if (!entry.isDir()) {
                int newPerms = m_permissions & mask;
                if ((newPerms & 0111) && !(permissions & 0111)) {
                    // don't interfere with mandatory file locking
                    if (newPerms & 02000) {
                        mask = mask & ~0101;
                    } else {
                        mask = mask & ~0111;
                    }
                }
            }
            info.permissions = (m_permissions & mask) | (permissions & ~mask);
            /*//qDebug() << info.url << "\n current permissions=" << QString::number(permissions,8)
                          << "\n wanted permission=" << QString::number(m_permissions,8)
                          << "\n with mask=" << QString::number(mask,8)
                          << "\n with ~mask (mask bits we keep) =" << QString::number((uint)~mask,8)
                          << "\n bits we keep =" << QString::number(permissions & ~mask,8)
                          << "\n new permissions = " << QString::number(info.permissions,8);*/
            // Push this info on top of the stack so it's handled first.
            // This way, the toplevel dirs are done last.
            m_infos.push(std::move(info));
        }
    }
}

void ChmodJobPrivate::_k_chmodNextFile()
{
    Q_Q(ChmodJob);
    if (!m_infos.empty()) {
        ChmodInfo info = m_infos.top();
        m_infos.pop();
        // First update group / owner (if local file)
        // (permissions have to set after, in case of suid and sgid)
        if (info.url.isLocalFile() && (m_newOwner.isValid() || m_newGroup.isValid())) {
            QString path = info.url.toLocalFile();
            if (!KIOPrivate::changeOwnership(path, m_newOwner, m_newGroup)) {
                if (!m_uiDelegateExtension) {
                    Q_EMIT q->warning(q, i18n("Could not modify the ownership of file %1", path));
                } else if (!m_bAutoSkipFiles) {
                    const QString errMsg = i18n("<qt>Could not modify the ownership of file <b>%1</b>. You have insufficient access to the file to perform the change.</qt>", path);
                    SkipDialog_Options options;
                    if (m_infos.size() > 1) {
                        options |= SkipDialog_MultipleItems;
                    }
                    const SkipDialog_Result skipResult = m_uiDelegateExtension->askSkip(q, options, errMsg);
                    switch (skipResult) {
                    case Result_AutoSkip:
                        m_bAutoSkipFiles = true;
                    // fall through
                        Q_FALLTHROUGH();
                    case Result_Skip:
                        QMetaObject::invokeMethod(q, "_k_chmodNextFile", Qt::QueuedConnection);
                        return;
                    case Result_Retry:
                        m_infos.push(std::move(info));
                        QMetaObject::invokeMethod(q, "_k_chmodNextFile", Qt::QueuedConnection);
                        return;
                    case Result_Cancel:
                    default:
                        q->setError(ERR_USER_CANCELED);
                        q->emitResult();
                        return;
                    }
                }
            }
        }

        /*qDebug() << "chmod'ing" << info.url
                      << "to" << QString::number(info.permissions,8);*/
        KIO::SimpleJob *job = KIO::chmod(info.url, info.permissions);
        job->setParentJob(q);
        // copy the metadata for acl and default acl
        const QString aclString = q->queryMetaData(QStringLiteral("ACL_STRING"));
        const QString defaultAclString = q->queryMetaData(QStringLiteral("DEFAULT_ACL_STRING"));
        if (!aclString.isEmpty()) {
            job->addMetaData(QStringLiteral("ACL_STRING"), aclString);
        }
        if (!defaultAclString.isEmpty()) {
            job->addMetaData(QStringLiteral("DEFAULT_ACL_STRING"), defaultAclString);
        }
        q->addSubjob(job);
    } else
        // We have finished
    {
        q->emitResult();
    }
}

void ChmodJob::slotResult(KJob *job)
{
    Q_D(ChmodJob);
    removeSubjob(job);
    if (job->error()) {
        setError(job->error());
        setErrorText(job->errorText());
        emitResult();
        return;
    }
    //qDebug() << "d->m_lstItems:" << d->m_lstItems.count();
    switch (d->state) {
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
        Q_ASSERT(false);
        return;
    }
}

ChmodJob *KIO::chmod(const KFileItemList &lstItems, int permissions, int mask,
                     const QString &owner, const QString &group,
                     bool recursive, JobFlags flags)
{
    KUserId uid = KUserId::fromName(owner);
    KGroupId gid = KGroupId::fromName(group);
    return ChmodJobPrivate::newJob(lstItems, permissions, mask, uid,
                                   gid, recursive, flags);
}

#include "moc_chmodjob.cpp"
