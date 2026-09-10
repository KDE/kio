/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2017 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "batchrenamejob.h"

#include "copyjob.h"
#include "job_p.h"
#include "kfileitem.h"

#include "../utils_p.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTimer>

#include <KLocalizedString>

#include <set>

using namespace KIO;

class KIO::BatchRenameJobPrivate : public KIO::JobPrivate
{
public:
    BatchRenameJobPrivate(const QList<QUrl> &src, const QList<bool> &isDir, const renameFunctionType renamefunction, JobFlags flags)
        : JobPrivate()
        , m_srcList(src)
        , m_isDir(isDir)
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
    const QList<bool> m_isDir;
    const renameFunctionType m_renamefunction;
    QList<QUrl>::const_iterator m_listIterator;
    QUrl m_oldUrl;
    QUrl m_newUrl; // for fileRenamed signal
    const JobFlags m_flags;
    QTimer m_reportTimer;

    Q_DECLARE_PUBLIC(BatchRenameJob)

    void slotStart();
    void slotReport();

    static inline BatchRenameJob *newJob(const QList<QUrl> &src, const QList<bool> &isDir, const renameFunctionType renamefunction, JobFlags flags)
    {
        BatchRenameJob *job = new BatchRenameJob(*new BatchRenameJobPrivate(src, isDir, renamefunction, flags));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate());
        if (!(flags & HideProgressInfo)) {
            KIO::getJobTracker()->registerJob(job);
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

void BatchRenameJobPrivate::slotStart()
{
    Q_Q(BatchRenameJob);

    if (m_listIterator == m_srcList.constBegin()) { //  emit total
        q->setTotalAmount(KJob::Items, m_srcList.count());
    }

    if (m_listIterator == m_srcList.constEnd()) {
        m_reportTimer.stop();
        slotReport();
        q->emitResult();
        return;
    }

    const QUrl oldUrl = *m_listIterator;
    const QString oldFileName = oldUrl.fileName();

    // Directories do not have a file name extension, so nothing may be split off one. A
    // folder called "Vol.1 Ch.3.1" must keep its name whatever the mime database happens to
    // know about ".1". Which items are directories was settled when the job was created, see
    // the batchRenameWithFunction() overloads.
    const bool isDir = m_isDir.at(m_listIterator - m_srcList.constBegin());

    // Only split off what is going to be put back afterwards. Splitting at the last dot instead
    // would silently drop everything after it whenever QMimeDatabase does not know the
    // suffix: "Vol.1 Ch.3.1" would reach the rename function as "Vol.1 Ch.3". It would also
    // mishandle multi-part suffixes, turning "archive.tar.gz" into "archive.tar.tar.gz".
    QString extension;
    const QString fileNameNoExt = Utils::splitFileNameExtension(oldFileName, isDir, &extension);
    const QString newName = Utils::appendFileNameExtension(m_renamefunction(fileNameNoExt), extension);

    m_oldUrl = oldUrl;
    m_newUrl = oldUrl.adjusted(QUrl::RemoveFilename);
    m_newUrl.setPath(m_newUrl.path() + KIO::encodeFileName(newName));

    if (m_newUrl == m_oldUrl) {
        // skip

        // We still must emit fileRenamed so users have
        // the corresponding number of files in the output
        Q_EMIT q->fileRenamed(*m_listIterator, m_newUrl);

        ++m_listIterator;
        slotStart();
        return;
    }

    KIO::Job *job = KIO::moveAs(oldUrl, m_newUrl, KIO::HideProgressInfo);
    job->setParentJob(q);
    q->addSubjob(job);
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
    d->slotStart();
}

BatchRenameJob *KIO::batchRename(const QList<QUrl> &srcList, const QString &newName, int startIndex, QChar placeHolder, KIO::JobFlags flags)
{
    bool allExtensionsDifferent = true;
    // Check for extensions.
    std::set<QString> extensions;
    QMimeDatabase db;
    for (const QUrl &url : std::as_const(srcList)) {
        const QString extension = db.suffixForFileName(url.path());
        const auto [it, isInserted] = extensions.insert(extension);
        if (!isInserted) {
            allExtensionsDifferent = false;
            break;
        }
    }

    // look for consecutive # groups
    static const QRegularExpression regex(QStringLiteral("%1+").arg(placeHolder));

    auto matchDashes = regex.globalMatch(newName);
    QRegularExpressionMatch lastMatchDashes;
    int matchCount = 0;
    while (matchDashes.hasNext()) {
        lastMatchDashes = matchDashes.next();
        matchCount++;
    }

    bool validPlaceholder = matchCount == 1;

    int placeHolderStart = lastMatchDashes.capturedStart(0);
    int placeHolderLength = lastMatchDashes.capturedLength(0);

    QString pattern(newName);

    if (!validPlaceholder) {
        if (allExtensionsDifferent) {
            // pattern: my-file
            // in: file-a.txt file-b.md
        } else {
            // pattern: my-file
            // in: file-a.txt file-b.txt
            // effective pattern: my-file#
            placeHolderLength = 1;
            placeHolderStart = pattern.length();
            pattern.append(placeHolder);
        }
    }

    renameFunctionType function =
        [pattern, allExtensionsDifferent, validPlaceholder, placeHolderStart, placeHolderLength, index = startIndex](const QStringView view) mutable {
            Q_UNUSED(view);

            QString indexString = QString::number(index);

            if (!validPlaceholder) {
                if (allExtensionsDifferent) {
                    // pattern: my-file
                    // in: file-a.txt file-b.md
                    return pattern;
                }
            }

            // Insert leading zeros if necessary
            indexString = indexString.prepend(QString(placeHolderLength - indexString.length(), QLatin1Char('0')));

            ++index;

            return QString(pattern).replace(placeHolderStart, placeHolderLength, indexString);
        };

    return batchRenameWithFunction(srcList, std::move(function), flags);
};

static bool isLocalDirectory(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return false;
    }
    const QFileInfo info(url.toLocalFile());
    return info.isDir() && !info.isSymLink();
}

BatchRenameJob *KIO::batchRenameWithFunction(const QList<QUrl> &srcList, const renameFunctionType renameFunction, KIO::JobFlags flags)
{
    QList<bool> isDir;
    isDir.reserve(srcList.size());
    for (const QUrl &url : srcList) {
        isDir.append(isLocalDirectory(url));
    }
    return BatchRenameJobPrivate::newJob(srcList, isDir, renameFunction, flags);
}

BatchRenameJob *KIO::batchRenameWithFunction(const KFileItemList &items, const renameFunctionType renameFunction, KIO::JobFlags flags)
{
    QList<bool> isDir;
    isDir.reserve(items.size());
    for (const KFileItem &item : items) {
        // The item knows what it is from the listing, for any protocol. A symlink is renamed as
        // a file even when it points at a directory, as with plain URLs.
        isDir.append(item.isDir() && !item.isLink());
    }
    return BatchRenameJobPrivate::newJob(items.urlList(), isDir, renameFunction, flags);
}

#include "moc_batchrenamejob.cpp"
