/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "faviconrequestjob.h"
#include <faviconscache_p.h>

#include "favicons_debug.h"

#include <KConfig>
#include <KIO/TransferJob>
#include <KLocalizedString>

#include <QBuffer>
#include <QCache>
#include <QDate>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QUrl>
#include <QStandardPaths>
#include <QSaveFile>

using namespace KIO;

static bool isIconOld(const QString &icon)
{
    const QFileInfo info(icon);
    if (!info.exists()) {
        qCDebug(FAVICONS_LOG) << "isIconOld" << icon << "yes, no such file";
        return true; // Trigger a new download on error
    }
    const QDate date = info.lastModified().date();

    qCDebug(FAVICONS_LOG) << "isIconOld" << icon << "?";
    return date.daysTo(QDate::currentDate()) > 7; // arbitrary value (one week)
}

class KIO::FavIconRequestJobPrivate
{
public:
    FavIconRequestJobPrivate(const QUrl &hostUrl, KIO::LoadType reload)
        : m_hostUrl(hostUrl), m_reload(reload) {}

    // slots
    void slotData(KIO::Job *job, const QByteArray &data);

    QUrl m_hostUrl;
    QUrl m_iconUrl;
    QString m_iconFile;
    QByteArray m_iconData;
    KIO::LoadType m_reload;
};


FavIconRequestJob::FavIconRequestJob(const QUrl &hostUrl, LoadType reload, QObject *parent)
    : KCompositeJob(parent), d(new FavIconRequestJobPrivate(hostUrl, reload))
{
    QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
}

FavIconRequestJob::~FavIconRequestJob()
{
    delete d;
}

void FavIconRequestJob::setIconUrl(const QUrl &iconUrl)
{
    d->m_iconUrl = iconUrl;
}

QString FavIconRequestJob::iconFile() const
{
    return d->m_iconFile;
}

QUrl FavIconRequestJob::hostUrl() const
{
    return d->m_hostUrl;
}

void FavIconRequestJob::doStart()
{
    KIO::FavIconsCache *cache = KIO::FavIconsCache::instance();
    QUrl iconUrl = d->m_iconUrl;
    const bool isNewIconUrl = !iconUrl.isEmpty();
    if (isNewIconUrl) {
        cache->setIconForUrl(d->m_hostUrl, d->m_iconUrl);
    } else {
        iconUrl = cache->iconUrlForUrl(d->m_hostUrl);
    }
    if (d->m_reload == NoReload) {
        const QString iconFile = cache->cachePathForIconUrl(iconUrl);
        if (!isIconOld(iconFile)) {
            qCDebug(FAVICONS_LOG) << "existing icon not old, reload not requested -> doing nothing";
            d->m_iconFile = iconFile;
            emitResult();
            return;
        }

        if (cache->isFailedDownload(iconUrl)) {
            qCDebug(FAVICONS_LOG) << iconUrl << "already in failedDownloads, emitting error";
            setError(KIO::ERR_DOES_NOT_EXIST);
            setErrorText(i18n("No favicon found for %1", d->m_hostUrl.host()));
            emitResult();
            return;
        }
    }

    qCDebug(FAVICONS_LOG) << "downloading" << iconUrl;
    KIO::TransferJob *job = KIO::get(iconUrl, d->m_reload, KIO::HideProgressInfo);
    QMap<QString, QString> metaData;
    metaData.insert(QStringLiteral("ssl_no_client_cert"), QStringLiteral("true"));
    metaData.insert(QStringLiteral("ssl_no_ui"), QStringLiteral("true"));
    metaData.insert(QStringLiteral("UseCache"), QStringLiteral("false"));
    metaData.insert(QStringLiteral("cookies"), QStringLiteral("none"));
    metaData.insert(QStringLiteral("no-www-auth"), QStringLiteral("true"));
    metaData.insert(QStringLiteral("errorPage"), QStringLiteral("false"));
    job->addMetaData(metaData);
    QObject::connect(job, &KIO::TransferJob::data,
                     this, [this](KIO::Job *job, const QByteArray &data) { d->slotData(job, data); });
    addSubjob(job);
}

void FavIconRequestJob::slotResult(KJob *job)
{
    KIO::TransferJob *tjob = static_cast<KIO::TransferJob *>(job);
    const QUrl &iconUrl = tjob->url();
    KIO::FavIconsCache *cache = KIO::FavIconsCache::instance();
    if (!job->error()) {
        QBuffer buffer(&d->m_iconData);
        buffer.open(QIODevice::ReadOnly);
        QImageReader ir(&buffer);
        QSize desired(16, 16);
        if (ir.canRead()) {
            while (ir.imageCount() > 1
                    && ir.currentImageRect() != QRect(0, 0, desired.width(), desired.height())) {
                if (!ir.jumpToNextImage()) {
                    break;
                }
            }
            ir.setScaledSize(desired);
            const QImage img = ir.read();
            if (!img.isNull()) {
                cache->ensureCacheExists();
                const QString localPath = cache->cachePathForIconUrl(iconUrl);
                qCDebug(FAVICONS_LOG) << "Saving image to" << localPath;
                QSaveFile saveFile(localPath);
                if (saveFile.open(QIODevice::WriteOnly) &&
                        img.save(&saveFile, "PNG") && saveFile.commit()) {
                    d->m_iconFile = localPath;
                } else {
                    setError(KIO::ERR_CANNOT_WRITE);
                    setErrorText(i18n("Error saving image to %1", localPath));
                }
            } else {
                qCDebug(FAVICONS_LOG) << "QImageReader read() returned a null image";
            }
        } else {
            qCDebug(FAVICONS_LOG) << "QImageReader canRead returned false";
        }
    } else if (job->error() == KJob::KilledJobError) { // we killed it in slotData
        setError(KIO::ERR_SLAVE_DEFINED);
        setErrorText(i18n("Icon file too big, download aborted"));
    } else {
        setError(job->error());
        setErrorText(job->errorString()); // not errorText(), because "this" is a KJob, with no errorString building logic
    }
    d->m_iconData.clear(); // release memory
    if (d->m_iconFile.isEmpty()) {
        qCDebug(FAVICONS_LOG) << "adding" << iconUrl << "to failed downloads due to error:" << errorString();
        cache->addFailedDownload(iconUrl);
    } else {
        cache->removeFailedDownload(iconUrl);
    }
    KCompositeJob::removeSubjob(job);
    emitResult();
}

void FavIconRequestJobPrivate::slotData(Job *job, const QByteArray &data)
{
    KIO::TransferJob *tjob = static_cast<KIO::TransferJob *>(job);
    unsigned int oldSize = m_iconData.size();
    // Size limit. Stop downloading if the file is huge.
    // Testcase (as of june 2008, at least): http://planet-soc.com/favicon.ico, 136K and strange format.
    // Another case: sites which redirect from "/favicon.ico" to "/" and return the main page.
    if (oldSize > 0x10000) { // 65K
        qCDebug(FAVICONS_LOG) << "Favicon too big, aborting download of" << tjob->url();
        const QUrl iconUrl = tjob->url();
        KIO::FavIconsCache::instance()->addFailedDownload(iconUrl);
        tjob->kill(KJob::EmitResult);
    } else {
        m_iconData.resize(oldSize + data.size());
        memcpy(m_iconData.data() + oldSize, data.data(), data.size());
    }
}

#include "moc_faviconrequestjob.cpp"
