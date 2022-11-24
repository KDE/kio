/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2004 Kevin Ottens <ervin@ipsquad.net>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "forwardingworkerbase.h"
#include "../utils_p.h"

#include "deletejob.h"
#include "job.h"
#include "kiocoredebug.h"
#include "mkdirjob.h"

#include <QEventLoop>
#include <QMimeDatabase>

namespace KIO
{
class ForwardingWorkerBasePrivate
{
public:
    ForwardingWorkerBasePrivate(const QByteArray &protocol, QObject *eventLoopParent, ForwardingWorkerBase *qq)
        : q(qq)
        , m_protocol(QString::fromUtf8(protocol))
        , eventLoop(eventLoopParent)
    {
    }
    ForwardingWorkerBase *const q;

    const QString m_protocol;
    QUrl m_processedURL;
    QUrl m_requestedURL;

    bool internalRewriteUrl(const QUrl &url, QUrl &newURL);

    void connectJob(Job *job);
    void connectSimpleJob(SimpleJob *job);
    void connectListJob(ListJob *job);
    void connectTransferJob(TransferJob *job);

    void _k_slotResult(KJob *job);
    void _k_slotWarning(KJob *job, const QString &msg) const;
    void _k_slotInfoMessage(KJob *job, const QString &msg) const;
    void _k_slotTotalSize(KJob *job, qulonglong size) const;
    void _k_slotProcessedSize(KJob *job, qulonglong size) const;
    void _k_slotSpeed(KJob *job, unsigned long bytesPerSecond) const;

    // KIO::SimpleJob subclasses
    void _k_slotRedirection(KIO::Job *job, const QUrl &url);

    // KIO::ListJob
    void _k_slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries) const;

    // KIO::TransferJob
    void _k_slotData(KIO::Job *job, const QByteArray &data) const;
    void _k_slotDataReq(KIO::Job *job, QByteArray &data) const;
    void _k_slotMimetype(KIO::Job *job, const QString &type) const;
    void _k_slotCanResume(KIO::Job *job, KIO::filesize_t offset) const;

    [[nodiscard]] WorkerResult loopResult()
    {
        eventLoop.exec();
        return m_pendingResult;
    }

private:
    // These are intentionally private to force us to go through [[nodiscard]] helper functions, lest we forget to retrieve the result.
    QEventLoop eventLoop;
    WorkerResult m_pendingResult = WorkerResult::pass();
};

ForwardingWorkerBase::ForwardingWorkerBase(const QByteArray &protocol, const QByteArray &poolSocket, const QByteArray &appSocket)
    : WorkerBase(protocol, poolSocket, appSocket)
    , d(new ForwardingWorkerBasePrivate(protocol, this, this))
{
}

ForwardingWorkerBase::~ForwardingWorkerBase() = default;

bool ForwardingWorkerBasePrivate::internalRewriteUrl(const QUrl &url, QUrl &newURL)
{
    bool result = true;

    if (url.scheme() == m_protocol) {
        result = q->rewriteUrl(url, newURL);
    } else {
        newURL = url;
    }

    m_processedURL = newURL;
    m_requestedURL = url;
    return result;
}

void ForwardingWorkerBase::adjustUDSEntry(KIO::UDSEntry &entry, UDSEntryCreationMode creationMode) const
{
    const bool listing = (creationMode == UDSEntryCreationInListDir);
    // qDebug() << "listing==" << listing;

    const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
    QString mimetype = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
    QUrl url;
    const QString urlStr = entry.stringValue(KIO::UDSEntry::UDS_URL);
    const bool url_found = !urlStr.isEmpty();
    if (url_found) {
        url = QUrl(urlStr);
        QUrl new_url(d->m_requestedURL);
        if (listing) {
            new_url.setPath(Utils::concatPaths(new_url.path(), url.fileName()));
        }
        // ## Didn't find a way to use an iterator instead of re-doing a key lookup
        entry.replace(KIO::UDSEntry::UDS_URL, new_url.toString());
        // qDebug() << "URL =" << url;
        // qDebug() << "New URL =" << new_url;
    }

    if (mimetype.isEmpty()) {
        QUrl new_url(d->m_processedURL);
        if (url_found && listing) {
            new_url.setPath(Utils::concatPaths(new_url.path(), url.fileName()));
        } else if (listing) {
            new_url.setPath(Utils::concatPaths(new_url.path(), name));
        }

        QMimeDatabase db;
        mimetype = db.mimeTypeForUrl(new_url).name();

        entry.replace(KIO::UDSEntry::UDS_MIME_TYPE, mimetype);

        // qDebug() << "New MIME type = " << mimetype;
    }

    if (d->m_processedURL.isLocalFile()) {
        QUrl new_url(d->m_processedURL);
        if (listing) {
            new_url.setPath(Utils::concatPaths(new_url.path(), name));
        }

        entry.replace(KIO::UDSEntry::UDS_LOCAL_PATH, new_url.toLocalFile());
    }
}

QUrl ForwardingWorkerBase::processedUrl() const
{
    return d->m_processedURL;
}

QUrl ForwardingWorkerBase::requestedUrl() const
{
    return d->m_requestedURL;
}

WorkerResult ForwardingWorkerBase::get(const QUrl &url)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::TransferJob *job = KIO::get(new_url, NoReload, HideProgressInfo);
        d->connectTransferJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::put(const QUrl &url, int permissions, JobFlags flags)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::TransferJob *job = KIO::put(new_url, permissions, flags | HideProgressInfo);
        d->connectTransferJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_MALFORMED_URL, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::stat(const QUrl &url)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::SimpleJob *job = KIO::stat(new_url, KIO::HideProgressInfo);
        d->connectSimpleJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::mimetype(const QUrl &url)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::TransferJob *job = KIO::mimetype(new_url, KIO::HideProgressInfo);
        d->connectTransferJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::listDir(const QUrl &url)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::ListJob *job = KIO::listDir(new_url, KIO::HideProgressInfo);
        d->connectListJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::mkdir(const QUrl &url, int permissions)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::SimpleJob *job = KIO::mkdir(new_url, permissions);
        d->connectSimpleJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_MALFORMED_URL, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::rename(const QUrl &src, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_CORE) << "rename" << src << dest;

    QUrl new_src;
    QUrl new_dest;
    if (!d->internalRewriteUrl(src, new_src)) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, src.toDisplayString());
    }
    if (d->internalRewriteUrl(dest, new_dest)) {
        KIO::Job *job = KIO::rename(new_src, new_dest, flags);
        d->connectJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_MALFORMED_URL, dest.toDisplayString());
}

WorkerResult ForwardingWorkerBase::symlink(const QString &target, const QUrl &dest, JobFlags flags)
{
    qCDebug(KIO_CORE) << "symlink" << target << dest;

    QUrl new_dest;
    if (d->internalRewriteUrl(dest, new_dest)) {
        KIO::SimpleJob *job = KIO::symlink(target, new_dest, flags | HideProgressInfo);
        d->connectSimpleJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_MALFORMED_URL, dest.toDisplayString());
}

WorkerResult ForwardingWorkerBase::chmod(const QUrl &url, int permissions)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::SimpleJob *job = KIO::chmod(new_url, permissions);
        d->connectSimpleJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::setModificationTime(const QUrl &url, const QDateTime &mtime)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        KIO::SimpleJob *job = KIO::setModificationTime(new_url, mtime);
        d->connectSimpleJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

WorkerResult ForwardingWorkerBase::copy(const QUrl &src, const QUrl &dest, int permissions, JobFlags flags)
{
    qCDebug(KIO_CORE) << "copy" << src << dest;

    QUrl new_src;
    QUrl new_dest;
    if (!d->internalRewriteUrl(src, new_src)) {
        return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, src.toDisplayString());
    }
    if (d->internalRewriteUrl(dest, new_dest)) {
        KIO::Job *job = KIO::file_copy(new_src, new_dest, permissions, flags | HideProgressInfo);
        d->connectJob(job);

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_MALFORMED_URL, dest.toDisplayString());
}

WorkerResult ForwardingWorkerBase::del(const QUrl &url, bool isfile)
{
    QUrl new_url;
    if (d->internalRewriteUrl(url, new_url)) {
        if (isfile) {
            KIO::DeleteJob *job = KIO::del(new_url, HideProgressInfo);
            d->connectJob(job);
        } else {
            KIO::SimpleJob *job = KIO::rmdir(new_url);
            d->connectSimpleJob(job);
        }

        return d->loopResult();
    }
    return WorkerResult::fail(KIO::ERR_DOES_NOT_EXIST, url.toDisplayString());
}

//////////////////////////////////////////////////////////////////////////////

void ForwardingWorkerBasePrivate::connectJob(KIO::Job *job)
{
    // We will forward the warning message, no need to let the job
    // display it itself
    job->setUiDelegate(nullptr);

    // Forward metadata (e.g. modification time for put())
    job->setMetaData(q->allMetaData());

    q->connect(job, &KJob::result, q, [this](KJob *job) {
        _k_slotResult(job);
    });
    q->connect(job, &KJob::warning, q, [this](KJob *job, const QString &text) {
        _k_slotWarning(job, text);
    });
    q->connect(job, &KJob::infoMessage, q, [this](KJob *job, const QString &info) {
        _k_slotInfoMessage(job, info);
    });
    q->connect(job, &KJob::totalSize, q, [this](KJob *job, qulonglong size) {
        _k_slotTotalSize(job, size);
    });
    q->connect(job, &KJob::processedSize, q, [this](KJob *job, qulonglong size) {
        _k_slotProcessedSize(job, size);
    });
    q->connect(job, &KJob::speed, q, [this](KJob *job, ulong speed) {
        _k_slotSpeed(job, speed);
    });
}

void ForwardingWorkerBasePrivate::connectSimpleJob(KIO::SimpleJob *job)
{
    connectJob(job);
    if (job->metaObject()->indexOfSignal("redirection(KIO::Job*,QUrl)") > -1) {
        q->connect(job, SIGNAL(redirection(KIO::Job *, QUrl)), SLOT(_k_slotRedirection(KIO::Job *, QUrl)));
    }
}

void ForwardingWorkerBasePrivate::connectListJob(KIO::ListJob *job)
{
    connectSimpleJob(job);
    q->connect(job, &KIO::ListJob::entries, q, [this](KIO::Job *job, const KIO::UDSEntryList &entries) {
        _k_slotEntries(job, entries);
    });
}

void ForwardingWorkerBasePrivate::connectTransferJob(KIO::TransferJob *job)
{
    connectSimpleJob(job);
    q->connect(job, &KIO::TransferJob::data, q, [this](KIO::Job *job, const QByteArray &data) {
        _k_slotData(job, data);
    });
    q->connect(job, &KIO::TransferJob::dataReq, q, [this](KIO::Job *job, QByteArray &data) {
        _k_slotDataReq(job, data);
    });
    q->connect(job, &KIO::TransferJob::mimeTypeFound, q, [this](KIO::Job *job, const QString &mimeType) {
        _k_slotMimetype(job, mimeType);
    });
    q->connect(job, &KIO::TransferJob::canResume, q, [this](KIO::Job *job, KIO::filesize_t offset) {
        _k_slotCanResume(job, offset);
    });
}

//////////////////////////////////////////////////////////////////////////////

void ForwardingWorkerBasePrivate::_k_slotResult(KJob *job)
{
    if (job->error() != 0) {
        m_pendingResult = WorkerResult::fail(job->error(), job->errorText());
    } else {
        if (auto stat_job = qobject_cast<KIO::StatJob *>(job)) {
            KIO::UDSEntry entry = stat_job->statResult();
            q->adjustUDSEntry(entry, ForwardingWorkerBase::UDSEntryCreationInStat);
            q->statEntry(entry);
        }
        m_pendingResult = WorkerResult::pass();
    }

    eventLoop.exit();
}

void ForwardingWorkerBasePrivate::_k_slotWarning(KJob * /*job*/, const QString &msg) const
{
    q->warning(msg);
}

void ForwardingWorkerBasePrivate::_k_slotInfoMessage(KJob * /*job*/, const QString &msg) const
{
    q->infoMessage(msg);
}

void ForwardingWorkerBasePrivate::_k_slotTotalSize(KJob * /*job*/, qulonglong size) const
{
    q->totalSize(size);
}

void ForwardingWorkerBasePrivate::_k_slotProcessedSize(KJob * /*job*/, qulonglong size) const
{
    q->processedSize(size);
}

void ForwardingWorkerBasePrivate::_k_slotSpeed(KJob * /*job*/, unsigned long bytesPerSecond) const
{
    q->speed(bytesPerSecond);
}

void ForwardingWorkerBasePrivate::_k_slotRedirection(KIO::Job *job, const QUrl &url)
{
    q->redirection(url);

    // We've been redirected stop everything.
    job->kill(KJob::Quietly);
    m_pendingResult = WorkerResult::pass();

    eventLoop.exit();
}

void ForwardingWorkerBasePrivate::_k_slotEntries(KIO::Job * /*job*/, const KIO::UDSEntryList &entries) const
{
    KIO::UDSEntryList final_entries = entries;

    for (auto &entry : final_entries) {
        q->adjustUDSEntry(entry, ForwardingWorkerBase::UDSEntryCreationInListDir);
    }

    q->listEntries(final_entries);
}

void ForwardingWorkerBasePrivate::_k_slotData(KIO::Job * /*job*/, const QByteArray &_data) const
{
    q->data(_data);
}

void ForwardingWorkerBasePrivate::_k_slotDataReq(KIO::Job * /*job*/, QByteArray &data) const
{
    q->dataReq();
    q->readData(data);
}

void ForwardingWorkerBasePrivate::_k_slotMimetype(KIO::Job * /*job*/, const QString &type) const
{
    q->mimeType(type);
}

void ForwardingWorkerBasePrivate::_k_slotCanResume(KIO::Job * /*job*/, KIO::filesize_t offset) const
{
    q->canResume(offset);
}

} // namespace KIO
