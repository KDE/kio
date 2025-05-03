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
    BatchRenameJobPrivate(const QList<QUrl> &src, const std::function<QString(const QStringView view, int index)> renamefunction, int index, JobFlags flags)
        : JobPrivate()
        , m_srcList(src)
        , m_index(index)
        , m_renamefunction(renamefunction)
        , m_listIterator(m_srcList.constBegin())
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
    }

    QList<QUrl> m_srcList;
    int m_index;
    const std::function<QString(const QStringView view, int index)> m_renamefunction;
    QList<QUrl>::const_iterator m_listIterator;
    QUrl m_oldUrl;
    QUrl m_newUrl; // for fileRenamed signal
    const JobFlags m_flags;
    QTimer m_reportTimer;

    Q_DECLARE_PUBLIC(BatchRenameJob)

    void slotStart();
    void slotReport();

    QString indexedName(const QString &name, int index, QChar placeHolder) const;

    static inline BatchRenameJob *
    newJob(const QList<QUrl> &src, const std::function<QString(const QStringView view, int index)> renamefunction, int index, JobFlags flags)
    {
        BatchRenameJob *job = new BatchRenameJob(*new BatchRenameJobPrivate(src, renamefunction, index, flags));
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
        QMimeDatabase db;
        const QUrl oldUrl = *m_listIterator;
        const QString oldFileName = oldUrl.fileName();
        const QString extension = db.suffixForFileName(oldFileName);
        int lastPoint = oldFileName.lastIndexOf(QLatin1Char('.'));
        QString fileNameNoExt = oldFileName.left(lastPoint);

        QString newName = m_renamefunction(fileNameNoExt, m_index);

        const auto suffix = QLatin1Char('.') + extension;
        if (!extension.isEmpty() && !newName.endsWith(suffix)) {
            newName += suffix;
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
    bool allExtensionsDifferent = true;

    // Check for extensions.
    std::set<QString> extensions;
    QMimeDatabase db;
    for (const QUrl &url : std::as_const(src)) {
        const QString extension = db.suffixForFileName(url.path());
        const auto [it, isInserted] = extensions.insert(extension);
        if (!isInserted) {
            allExtensionsDifferent = false;
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

    bool appendIndex = false;
    if (!validPlaceholder) {
        if (!allExtensionsDifferent) {
            appendIndex = true;
        }
    }

    std::function<QString(const QStringView view, int index)> function;
    if (appendIndex) {
        function = [newName](const QStringView view, int index) {
            Q_UNUSED(view);
            return QString(newName).append(QString::number(index));
        };
    } else {
        function = [newName, placeHolder](const QStringView view, int index) {
            Q_UNUSED(view);
            return QString(newName).replace(placeHolder, QString::number(index));
        };
    }

    return BatchRenameJobPrivate::newJob(src, std::move(function), index, flags);
}

BatchRenameJob *
KIO::batchRename(const QList<QUrl> &src, const std::function<QString(const QStringView view, int index)> function, int index, KIO::JobFlags flags)
{
    return BatchRenameJobPrivate::newJob(src, function, index, flags);
}

#include "moc_batchrenamejob.cpp"
