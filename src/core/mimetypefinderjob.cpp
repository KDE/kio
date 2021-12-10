/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "mimetypefinderjob.h"

#include "global.h"
#include "job.h" // for buildErrorString
#include "kiocoredebug.h"
#include "scheduler.h"
#include "statjob.h"

#include <KLocalizedString>
#include <KProtocolManager>

#include <QMimeDatabase>
#include <QTimer>
#include <QUrl>

class KIO::MimeTypeFinderJobPrivate
{
public:
    explicit MimeTypeFinderJobPrivate(const QUrl &url, MimeTypeFinderJob *qq)
        : m_url(url)
        , q(qq)
    {
        q->setCapabilities(KJob::Killable);
    }

    void statFile();
    void scanFileWithGet();

    QUrl m_url;
    KIO::MimeTypeFinderJob *const q;
    QString m_mimeTypeName;
    QString m_suggestedFileName;
    bool m_followRedirections = true;
    bool m_authPrompts = true;
};

KIO::MimeTypeFinderJob::MimeTypeFinderJob(const QUrl &url, QObject *parent)
    : KCompositeJob(parent)
    , d(new MimeTypeFinderJobPrivate(url, this))
{
}

KIO::MimeTypeFinderJob::~MimeTypeFinderJob() = default;

void KIO::MimeTypeFinderJob::start()
{
    if (!d->m_url.isValid() || d->m_url.scheme().isEmpty()) {
        const QString error = !d->m_url.isValid() ? d->m_url.errorString() : d->m_url.toDisplayString();
        setError(KIO::ERR_MALFORMED_URL);
        setErrorText(i18n("Malformed URL\n%1", error));
        emitResult();
        return;
    }

    if (!KProtocolManager::supportsListing(d->m_url)) {
        // No support for listing => it can't be a directory (example: http)
        d->scanFileWithGet();
        return;
    }

    // It may be a directory or a file, let's use stat to find out
    d->statFile();
}

void KIO::MimeTypeFinderJob::setFollowRedirections(bool b)
{
    d->m_followRedirections = b;
}

void KIO::MimeTypeFinderJob::setSuggestedFileName(const QString &suggestedFileName)
{
    d->m_suggestedFileName = suggestedFileName;
}

QString KIO::MimeTypeFinderJob::suggestedFileName() const
{
    return d->m_suggestedFileName;
}

QString KIO::MimeTypeFinderJob::mimeType() const
{
    return d->m_mimeTypeName;
}

void KIO::MimeTypeFinderJob::setAuthenticationPromptEnabled(bool enable)
{
    d->m_authPrompts = enable;
}

bool KIO::MimeTypeFinderJob::isAuthenticationPromptEnabled() const
{
    return d->m_authPrompts;
}

bool KIO::MimeTypeFinderJob::doKill()
{
    // This should really be in KCompositeJob...
    const QList<KJob *> jobs = subjobs();
    for (KJob *job : jobs) {
        job->kill(); // ret val ignored, see below
    }
    // Even if for some reason killing a subjob fails,
    // we can still consider this job as killed.
    // The stat() or get() subjob has no side effects.
    return true;
}

void KIO::MimeTypeFinderJob::slotResult(KJob *job)
{
    // We do the error handling elsewhere, just do the bookkeeping here
    removeSubjob(job);
}

void KIO::MimeTypeFinderJobPrivate::statFile()
{
    Q_ASSERT(m_mimeTypeName.isEmpty());

    constexpr auto statFlags = KIO::StatBasic | KIO::StatResolveSymlink | KIO::StatMimeType;

    KIO::StatJob *job = KIO::statDetails(m_url, KIO::StatJob::SourceSide, statFlags, KIO::HideProgressInfo);
    if (!m_authPrompts) {
        job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    }
    job->setUiDelegate(nullptr);
    q->addSubjob(job);
    QObject::connect(job, &KJob::result, q, [=]() {
        const int errCode = job->error();
        if (errCode) {
            // ERR_NO_CONTENT is not an error, but an indication no further
            // actions need to be taken.
            if (errCode != KIO::ERR_NO_CONTENT) {
                q->setError(errCode);
                // We're a KJob, not a KIO::Job, so build the error string here
                q->setErrorText(KIO::buildErrorString(errCode, job->errorText()));
            }
            q->emitResult();
            return;
        }
        if (m_followRedirections) { // Update our URL in case of a redirection
            m_url = job->url();
        }

        const KIO::UDSEntry entry = job->statResult();

        qCDebug(KIO_CORE) << "UDSEntry from StatJob in MimeTypeFinderJob" << entry;

        const QString localPath = entry.stringValue(KIO::UDSEntry::UDS_LOCAL_PATH);
        if (!localPath.isEmpty()) {
            m_url = QUrl::fromLocalFile(localPath);
        }

        // MIME type already known? (e.g. print:/manager)
        m_mimeTypeName = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
        if (!m_mimeTypeName.isEmpty()) {
            q->emitResult();
            return;
        }

        if (entry.isDir()) {
            m_mimeTypeName = QStringLiteral("inode/directory");
            q->emitResult();
        } else { // It's a file
            // Start the timer. Once we get the timer event this
            // protocol server is back in the pool and we can reuse it.
            // This gives better performance than starting a new slave
            QTimer::singleShot(0, q, [this] {
                scanFileWithGet();
            });
        }
    });
}

static QMimeType fixupMimeType(const QString &mimeType, const QString &fileName)
{
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForName(mimeType);
    if ((!mime.isValid() || mime.isDefault()) && !fileName.isEmpty()) {
        mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchExtension);
    }
    return mime;
}

void KIO::MimeTypeFinderJobPrivate::scanFileWithGet()
{
    Q_ASSERT(m_mimeTypeName.isEmpty());

    if (!KProtocolManager::supportsReading(m_url)) {
        qCDebug(KIO_CORE) << "No support for reading from" << m_url.scheme();
        q->setError(KIO::ERR_CANNOT_READ);
        q->setErrorText(m_url.toDisplayString());
        q->emitResult();
        return;
    }
    // qDebug() << this << "Scanning file" << url;

    KIO::TransferJob *job = KIO::get(m_url, KIO::NoReload /*reload*/, KIO::HideProgressInfo);
    if (!m_authPrompts) {
        job->addMetaData(QStringLiteral("no-auth-prompt"), QStringLiteral("true"));
    }
    job->setUiDelegate(nullptr);
    q->addSubjob(job);
    QObject::connect(job, &KJob::result, q, [=]() {
        const int errCode = job->error();
        if (errCode) {
            // ERR_NO_CONTENT is not an error, but an indication no further
            // actions need to be taken.
            if (errCode != KIO::ERR_NO_CONTENT) {
                q->setError(errCode);
                q->setErrorText(job->errorText());
            }
            q->emitResult();
        }
        // if the job succeeded, we certainly hope it emitted mimeTypeFound()...
        if (m_mimeTypeName.isEmpty()) {
            qCWarning(KIO_CORE) << "KIO::get didn't emit a mimetype! Please fix the ioslave for URL" << m_url;
            q->setError(KIO::ERR_INTERNAL);
            q->setErrorText(i18n("Unable to determine the type of file for %1", m_url.toDisplayString()));
            q->emitResult();
        }
    });
    QObject::connect(job, &KIO::TransferJob::mimeTypeFound, q, [=](KIO::Job *, const QString &mimetype) {
        if (m_followRedirections) { // Update our URL in case of a redirection
            m_url = job->url();
        }
        if (mimetype.isEmpty()) {
            qCWarning(KIO_CORE) << "get() didn't emit a MIME type! Probably a kioslave bug, please check the implementation of" << m_url.scheme();
        }
        m_mimeTypeName = mimetype;

        // If the current MIME type is the default MIME type, then attempt to
        // determine the "real" MIME type from the file name (bug #279675)
        const QMimeType mime = fixupMimeType(m_mimeTypeName, m_suggestedFileName.isEmpty() ? m_url.fileName() : m_suggestedFileName);
        if (mime.isValid()) {
            m_mimeTypeName = mime.name();
        }

        if (m_suggestedFileName.isEmpty()) {
            m_suggestedFileName = job->queryMetaData(QStringLiteral("content-disposition-filename"));
        }

        if (!m_url.isLocalFile()) { // #434455
            job->putOnHold();
        }
        q->emitResult();
    });
}
