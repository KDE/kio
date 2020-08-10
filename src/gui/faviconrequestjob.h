/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2016 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Malte Starostik <malte@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_FAVICONREQUESTJOB_H
#define KIO_FAVICONREQUESTJOB_H

#include "kiogui_export.h"
#include <kio/job_base.h> // for LoadType

class QUrl;

namespace KIO
{

class FavIconRequestJobPrivate;
/**
 * @class FavIconRequestJob faviconrequestjob.h <KIO/FavIconRequestJob>
 *
 * FavIconRequestJob handles the retrieval of a favicon (either from the local cache or from the internet)
 *
 * For instance, the icon for http://www.google.com exists at http://www.google.com/favicon.ico
 * This job will (the first time) download the favicon, and make it available as a local PNG
 * for fast lookups afterwards.
 *
 * Usage:
 * Create a FavIconRequestJob, connect to result(KJob *), and from there use iconFile().
 *
 * @code
 * // Let's say we want to show the icon for QUrl m_url
 * KIO::FavIconRequestJob *job = new KIO::FavIconRequestJob(m_url);
 * connect(job, &KIO::FavIconRequestJob::result, this, [job, this](KJob *){
 *     if (!job->error()) {
 *         // show the icon using QIcon(job->iconFile())
 *     }
 * });
 * @endcode
 *
 * For a given HTTP URL, you can find out if a favicon is available by calling KIO::favIconForUrl() in KIOCore.
 * It is however not necessary to check this first, FavIconRequestJob will do this
 * first and emit result right away if a cached icon is available and not too old.
 *
 * In Web Browsers, additional information exists: the HTML for a given page can
 * specify something like
 *  &lt;link rel="shortcut icon" href="another_favicon.ico" /&gt;
 * To handle this, call job->setIconUrl(iconUrl).
 * (KParts-based web engines use the signal BrowserExtension::setIconUrl to call back
 * into the web browser application, which should then call this).
 * The signal urlIconChanged will be emitted once the icon has been downloaded.
 *
 * The on-disk cache is shared between processes.
 *
 * @since 5.19
 */
class KIOGUI_EXPORT FavIconRequestJob : public KCompositeJob
{
    Q_OBJECT
public:
    /**
     * @brief FavIconRequestJob constructor
     * @param hostUrl The web page URL. We only use the scheme and host.
     * @param reload set this to reload to skip the cache and force a refresh of the favicon.
     * @param parent parent object
     */
    explicit FavIconRequestJob(const QUrl &hostUrl, KIO::LoadType reload = KIO::NoReload, QObject *parent = nullptr);

    /**
     * Destructor. You do not need to delete the job, it will delete automatically,
     * unless you call setAutoDelete(false).
     */
    ~FavIconRequestJob();

    /**
     * @brief setIconUrl allows to set, for a specific URL, a different icon URL
     * than the default one for the host (http://host/favicon.ico)
     *
     * This information is stored in the on-disk cache, so that
     * other FavIconRequestJobs for this url and KIO::favIconForUrl
     * will return the icon specified here.
     *
     * @param iconUrl the URL to the icon, usually parsed from the HTML
     */
    void setIconUrl(const QUrl &iconUrl);

    /**
     * Returns the full local path to the icon from the cache.
     * Only call this in the slot connected to the result(KJob*) signal.
     * @return the path to the icon file
     */
    QString iconFile() const;

    /**
     * Returns the URL passed to the constructor
     * @since 5.20
     */
    QUrl hostUrl() const;

    /**
     * @internal
     * Do not call start(), KIO jobs are autostarted
     */
    void start() override {}

private Q_SLOTS:
    void doStart(); // not called start() so that exec() doesn't call it too
    void slotResult(KJob *job) override;

private:
    Q_PRIVATE_SLOT(d, void slotData(KIO::Job *, const QByteArray &))

    FavIconRequestJobPrivate *const d;
};

}
#endif
