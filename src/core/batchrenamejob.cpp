/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "batchrenamejob.h"

#include "copyjob.h"
#include "job_p.h"

#include <QMimeDatabase>
#include <QTimer>

#include <KLocalizedString>

#include <set>

using namespace KIO;

class KIO::BatchRenameJobPrivate : public KIO::JobPrivate
{
public:
    BatchRenameJobPrivate(const QList<QUrl> &src, const QString &newName, int index, QChar placeHolder, JobFlags flags)
        : JobPrivate()
        , m_srcList(src)
        , m_newName(newName)
        , m_index(index)
        , m_placeHolder(placeHolder)
        , m_listIterator(m_srcList.constBegin())
        , m_allExtensionsDifferent(true)
        , m_useIndex(true)
        , m_appendIndex(false)
        , m_flags(flags)
    {
        // There occur four cases when renaming multiple files,
        // 1. All files have different extension and $newName contains a valid placeholder.
        // 2. At least two files have same extension and $newName contains a valid placeholder.
        // In these two cases the placeholder character will be replaced by an integer($index).
        // 3. All files have different extension and new name contains an invalid placeholder
        //    (this means either $newName doesn't contain the placeholder or the placeholders
        //     are not in a connected sequence).
        // In this case nothing is substituted and all files have the same $newName.
        // 4. At least two files have same extension and $newName contains an invalid placeholder.
        // In this case $index is appended to $newName.

        // Check for extensions.
        std::set<QString> extensions;
        QMimeDatabase db;
        for (const QUrl &url : std::as_const(m_srcList)) {
            const QString extension = db.suffixForFileName(url.path());
            const auto [it, isInserted] = extensions.insert(extension);
            if (!isInserted) {
                m_allExtensionsDifferent = false;
                break;
            }
        }

        // Check for exactly one placeholder character or exactly one sequence of placeholders.
        int pos = newName.indexOf(placeHolder);
        if (pos != -1) {
            while (pos < newName.size() && newName.at(pos) == placeHolder) {
                pos++;
            }
        }
        const bool validPlaceholder = (newName.indexOf(placeHolder, pos) == -1);

        if (!validPlaceholder) {
            if (!m_allExtensionsDifferent) {
                m_appendIndex = true;
            } else {
                m_useIndex = false;
            }
        }
    }

    QList<QUrl> m_srcList;
    QString m_newName;
    int m_index;
    QChar m_placeHolder;
    QList<QUrl>::const_iterator m_listIterator;
    bool m_allExtensionsDifferent;
    bool m_useIndex;
    bool m_appendIndex;
    QUrl m_oldUrl;
    QUrl m_newUrl; // for fileRenamed signal
    const JobFlags m_flags;
    QTimer m_reportTimer;

    Q_DECLARE_PUBLIC(BatchRenameJob)

    void slotStart();
    void slotReport();

    QString indexedName(const QString &name, int index, QChar placeHolder) const;

    static inline BatchRenameJob *newJob(const QList<QUrl> &src, const QString &newName, int index, QChar placeHolder, JobFlags flags)
    {
        BatchRenameJob *job = new BatchRenameJob(*new BatchRenameJobPrivate(src, newName, index, placeHolder, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
        }
        if (!(flags & NoPrivilegeExecution)) {
            job->d_func()->m_privilegeExecutionEnabled = true;
            job->d_func()->m_operationType = Rename;
        }
        return job;
    }
};

BatchRenameJob::BatchRenameJob(BatchRenameJobPrivate &dd)
    : Job(dd)
{
    Q_D(BatchRenameJob);
    connect(&d->m_reportTimer, &QTimer::timeout, this, [this]() {
        d_func()->slotReport();
    });
    d->m_reportTimer.start(200);

    QTimer::singleShot(0, this, [this] {
        d_func()->slotStart();
    });
}

BatchRenameJob::~BatchRenameJob()
{
}

QString BatchRenameJobPrivate::indexedName(const QString &name, int index, QChar placeHolder) const
{
    if (!m_useIndex) {
        return name;
    }

    QString newName = name;
    QString indexString = QString::number(index);

    if (m_appendIndex) {
        newName.append(indexString);
        return newName;
    }

    // Insert leading zeros if necessary
    const int minIndexLength = name.count(placeHolder);
    indexString.prepend(QString(minIndexLength - indexString.length(), QLatin1Char('0')));

    // Replace the index placeholders by the indexString
    const int placeHolderStart = newName.indexOf(placeHolder);
    newName.replace(placeHolderStart, minIndexLength, indexString);

    return newName;
}

void BatchRenameJobPrivate::slotStart()
{
    Q_Q(BatchRenameJob);

    if (m_listIterator == m_srcList.constBegin()) { //  emit total
        q->setTotalAmount(KJob::Items, m_srcList.count());
    }

    if (m_listIterator != m_srcList.constEnd()) {
        QString newName = indexedName(m_newName, m_index, m_placeHolder);
        const QUrl oldUrl = *m_listIterator;
        QMimeDatabase db;
        const QString extension = db.suffixForFileName(oldUrl.path());
        if (!extension.isEmpty()) {
            newName += QLatin1Char('.') + extension;
        }

        m_oldUrl = oldUrl;
        m_newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
        m_newUrl.setPath(m_newUrl.path() + KIO::encodeFileName(newName));

        KIO::Job *job = KIO::moveAs(oldUrl, m_newUrl, KIO::HideProgressInfo);
        job->setParentJob(q);
        q->addSubjob(job);
    } else {
        m_reportTimer.stop();
        slotReport();
        q->emitResult();
    }
}

void BatchRenameJobPrivate::slotReport()
{
    Q_Q(BatchRenameJob);

    const auto processed = m_listIterator - m_srcList.constBegin();

    q->setProcessedAmount(KJob::Items, processed);
    q->emitPercent(processed, m_srcList.count());

    emitRenaming(q, m_oldUrl, m_newUrl);
}

void BatchRenameJob::slotResult(KJob *job)
{
    Q_D(BatchRenameJob);
    if (job->error()) {
        d->m_reportTimer.stop();
        d->slotReport();
        KIO::Job::slotResult(job);
        return;
    }

    removeSubjob(job);

    Q_EMIT fileRenamed(*d->m_listIterator, d->m_newUrl);
    ++d->m_listIterator;
    ++d->m_index;
    d->slotStart();
}

BatchRenameJob *KIO::batchRename(const QList<QUrl> &src, const QString &newName, int index, QChar placeHolder, KIO::JobFlags flags)
{
    return BatchRenameJobPrivate::newJob(src, newName, index, placeHolder, flags);
}

#include "moc_batchrenamejob.cpp"
