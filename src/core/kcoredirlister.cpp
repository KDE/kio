/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 2000 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2003-2005 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kcoredirlister.h"
#include "kcoredirlister_p.h"

#include <KLocalizedString>
#include <kio/listjob.h>
#include "kprotocolmanager.h"
#include "kmountpoint.h"
#include "kiocoredebug.h"
#include "../pathhelpers_p.h"

#include <QRegularExpression>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMimeDatabase>

#include <list>

#include <QLoggingCategory>
Q_DECLARE_LOGGING_CATEGORY(KIO_CORE_DIRLISTER)
Q_LOGGING_CATEGORY(KIO_CORE_DIRLISTER, "kf.kio.core.dirlister", QtWarningMsg)

// Enable this to get printDebug() called often, to see the contents of the cache
//#define DEBUG_CACHE

// Make really sure it doesn't get activated in the final build
#ifdef NDEBUG
#undef DEBUG_CACHE
#endif

Q_GLOBAL_STATIC(KCoreDirListerCache, kDirListerCache)

KCoreDirListerCache::KCoreDirListerCache()
    : itemsCached(10),       // keep the last 10 directories around
      m_cacheHiddenFiles(10) // keep the last 10 ".hidden" files around
{
    qCDebug(KIO_CORE_DIRLISTER);

    connect(&pendingUpdateTimer, &QTimer::timeout, this, &KCoreDirListerCache::processPendingUpdates);
    pendingUpdateTimer.setSingleShot(true);

    connect(KDirWatch::self(), &KDirWatch::dirty,   this, &KCoreDirListerCache::slotFileDirty);
    connect(KDirWatch::self(), &KDirWatch::created, this, &KCoreDirListerCache::slotFileCreated);
    connect(KDirWatch::self(), &KDirWatch::deleted, this, &KCoreDirListerCache::slotFileDeleted);

    kdirnotify = new org::kde::KDirNotify(QString(), QString(), QDBusConnection::sessionBus(), this);
    connect(kdirnotify, &org::kde::KDirNotify::FileRenamedWithLocalPath, this, &KCoreDirListerCache::slotFileRenamed);
    connect(kdirnotify, &org::kde::KDirNotify::FilesAdded  , this, &KCoreDirListerCache::slotFilesAdded);
    connect(kdirnotify, &org::kde::KDirNotify::FilesChanged, this, &KCoreDirListerCache::slotFilesChanged);
    connect(kdirnotify, &org::kde::KDirNotify::FilesRemoved, this, QOverload<const QStringList&>::of(&KCoreDirListerCache::slotFilesRemoved));

    // Probably not needed in KF5 anymore:
    // The use of KUrl::url() in ~DirItem (sendSignal) crashes if the static for QRegExpEngine got deleted already,
    // so we need to destroy the KCoreDirListerCache before that.
    //qAddPostRoutine(kDirListerCache.destroy);
}

KCoreDirListerCache::~KCoreDirListerCache()
{
    qCDebug(KIO_CORE_DIRLISTER);

    qDeleteAll(itemsInUse);
    itemsInUse.clear();

    itemsCached.clear();
    directoryData.clear();
    m_cacheHiddenFiles.clear();

    if (KDirWatch::exists()) {
        KDirWatch::self()->disconnect(this);
    }
}

// setting _reload to true will emit the old files and
// call updateDirectory
bool KCoreDirListerCache::listDir(KCoreDirLister *lister, const QUrl &dirUrl,
                                  bool _keep, bool _reload)
{
    QUrl _url(dirUrl);
    _url.setPath(QDir::cleanPath(_url.path())); // kill consecutive slashes

    // like this we don't have to worry about trailing slashes any further
    _url = _url.adjusted(QUrl::StripTrailingSlash);

    QString resolved;
    if (_url.isLocalFile()) {
        // Resolve symlinks (#213799)
        const QString local = _url.toLocalFile();
        resolved = QFileInfo(local).canonicalFilePath();
        if (local != resolved) {
            canonicalUrls[QUrl::fromLocalFile(resolved)].append(_url);
        }
        // TODO: remove entry from canonicalUrls again in forgetDirs
        // Note: this is why we use a QStringList value in there rather than a QSet:
        // we can just remove one entry and not have to worry about other dirlisters
        // (the non-unicity of the stringlist gives us the refcounting, basically).
    }

    if (!validUrl(lister, _url)) {
        qCDebug(KIO_CORE_DIRLISTER) << lister << "url=" << _url << "not a valid url";
        return false;
    }

    qCDebug(KIO_CORE_DIRLISTER) << lister << "url=" << _url << "keep=" << _keep << "reload=" << _reload;
#ifdef DEBUG_CACHE
    printDebug();
#endif

    if (!_keep) {
        // stop any running jobs for lister
        stop(lister, true /*silent*/);

        // clear our internal list for lister
        forgetDirs(lister);

        lister->d->rootFileItem = KFileItem();
    } else if (lister->d->lstDirs.contains(_url)) {
        // stop the job listing _url for this lister
        stopListingUrl(lister, _url, true /*silent*/);

        // remove the _url as well, it will be added in a couple of lines again!
        // forgetDirs with three args does not do this
        // TODO: think about moving this into forgetDirs
        lister->d->lstDirs.removeAll(_url);

        // clear _url for lister
        forgetDirs(lister, _url, true);

        if (lister->d->url == _url) {
            lister->d->rootFileItem = KFileItem();
        }
    }

    lister->d->complete = false;

    lister->d->lstDirs.append(_url);

    if (lister->d->url.isEmpty() || !_keep) { // set toplevel URL only if not set yet
        lister->d->url = _url;
    }

    DirItem *itemU = itemsInUse.value(_url);

    KCoreDirListerCacheDirectoryData &dirData = directoryData[_url]; // find or insert

    if (dirData.listersCurrentlyListing.isEmpty()) {
        // if there is an update running for _url already we get into
        // the following case - it will just be restarted by updateDirectory().

        dirData.listersCurrentlyListing.append(lister);

        DirItem *itemFromCache = nullptr;
        if (itemU || (!_reload && (itemFromCache = itemsCached.take(_url)))) {
            if (itemU) {
                qCDebug(KIO_CORE_DIRLISTER) << "Entry already in use:" << _url;
                // if _reload is set, then we'll emit cached items and then updateDirectory.
            } else {
                qCDebug(KIO_CORE_DIRLISTER) << "Entry in cache:" << _url;
                itemsInUse.insert(_url, itemFromCache);
                itemU = itemFromCache;
            }
            if (lister->d->autoUpdate) {
                itemU->incAutoUpdate();
            }
            if (itemFromCache && itemFromCache->watchedWhileInCache) {
                itemFromCache->watchedWhileInCache = false;;
                itemFromCache->decAutoUpdate();
            }

            Q_EMIT lister->started(_url);

            // List items from the cache in a delayed manner, just like things would happen
            // if we were not using the cache.
            new KCoreDirListerPrivate::CachedItemsJob(lister, _url, _reload);

        } else {
            // dir not in cache or _reload is true
            if (_reload) {
                qCDebug(KIO_CORE_DIRLISTER) << "Reloading directory:" << _url;
                itemsCached.remove(_url);
            } else {
                qCDebug(KIO_CORE_DIRLISTER) << "Listing directory:" << _url;
            }

            itemU = new DirItem(_url, resolved);
            itemsInUse.insert(_url, itemU);
            if (lister->d->autoUpdate) {
                itemU->incAutoUpdate();
            }

//        // we have a limit of MAX_JOBS_PER_LISTER concurrently running jobs
//        if ( lister->d->numJobs() >= MAX_JOBS_PER_LISTER )
//        {
//          pendingUpdates.insert( _url );
//        }
//        else
            {
                KIO::ListJob *job = KIO::listDir(_url, KIO::HideProgressInfo);
                runningListJobs.insert(job, KIO::UDSEntryList());

                lister->jobStarted(job);
                lister->d->connectJob(job);

                connect(job, &KIO::ListJob::entries, this, &KCoreDirListerCache::slotEntries);
                connect(job, &KJob::result, this, &KCoreDirListerCache::slotResult);
                connect(job, &KIO::ListJob::redirection, this, &KCoreDirListerCache::slotRedirection);

                Q_EMIT lister->started(_url);
            }
            qCDebug(KIO_CORE_DIRLISTER) << "Entry now being listed by" << dirData.listersCurrentlyListing;
        }
    } else {

        qCDebug(KIO_CORE_DIRLISTER) << "Entry currently being listed:" << _url << "by" << dirData.listersCurrentlyListing;
#ifdef DEBUG_CACHE
        printDebug();
#endif

        Q_EMIT lister->started(_url);

        // Maybe listersCurrentlyListing/listersCurrentlyHolding should be QSets?
        Q_ASSERT(!dirData.listersCurrentlyListing.contains(lister));
        dirData.listersCurrentlyListing.append(lister);

        KIO::ListJob *job = jobForUrl(_url);
        // job will be 0 if we were listing from cache rather than listing from a kio job.
        if (job) {
            lister->jobStarted(job);
            lister->d->connectJob(job);
        }
        Q_ASSERT(itemU);

        // List existing items in a delayed manner, just like things would happen
        // if we were not using the cache.
        qCDebug(KIO_CORE_DIRLISTER) << "Listing" << itemU->lstItems.count() << "cached items soon";
        auto *cachedItemsJob = new KCoreDirListerPrivate::CachedItemsJob(lister, _url, _reload);
        if (job) {
            // The ListJob will take care of emitting completed.
            // ### If it finishes before the CachedItemsJob, then we'll emit cached items after completed(), not sure how bad this is.
            cachedItemsJob->setEmitCompleted(false);
        }

#ifdef DEBUG_CACHE
        printDebug();
#endif
    }

    return true;
}

KCoreDirListerPrivate::CachedItemsJob *KCoreDirListerPrivate::cachedItemsJobForUrl(const QUrl &url) const
{
    for (CachedItemsJob *job : m_cachedItemsJobs) {
        if (job->url() == url) {
            return job;
        }
    }
    return nullptr;
}

KCoreDirListerPrivate::CachedItemsJob::CachedItemsJob(KCoreDirLister *lister, const QUrl &url, bool reload)
    : KJob(lister),
      m_lister(lister), m_url(url),
      m_reload(reload), m_emitCompleted(true)
{
    qCDebug(KIO_CORE_DIRLISTER) << "Creating CachedItemsJob" << this << "for lister" << lister << url;
    if (lister->d->cachedItemsJobForUrl(url)) {
        qCWarning(KIO_CORE) << "Lister" << lister << "has a cached items job already for" << url;
    }
    lister->d->m_cachedItemsJobs.append(this);
    setAutoDelete(true);
    start();
}

// Called by start() via QueuedConnection
void KCoreDirListerPrivate::CachedItemsJob::done()
{
    if (!m_lister) { // job was already killed, but waiting deletion due to deleteLater
        return;
    }
    kDirListerCache()->emitItemsFromCache(this, m_lister, m_url, m_reload, m_emitCompleted);
    emitResult();
}

bool KCoreDirListerPrivate::CachedItemsJob::doKill()
{
    qCDebug(KIO_CORE_DIRLISTER) << this;
    kDirListerCache()->forgetCachedItemsJob(this, m_lister, m_url);
    if (!property("_kdlc_silent").toBool()) {
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT m_lister->canceled(m_url);
#endif
        Q_EMIT m_lister->listingDirCanceled(m_url);

        Q_EMIT m_lister->canceled();
    }
    m_lister = nullptr;
    return true;
}

void KCoreDirListerCache::emitItemsFromCache(KCoreDirListerPrivate::CachedItemsJob *cachedItemsJob,
                                             KCoreDirLister *lister, const QUrl &_url, bool _reload, bool _emitCompleted)
{
    lister->d->complete = false;

    DirItem *itemU = kDirListerCache()->itemsInUse.value(_url);
    if (!itemU) {
        qCWarning(KIO_CORE) << "Can't find item for directory" << _url << "anymore";
    } else {
        const QList<KFileItem> items = itemU->lstItems;
        const KFileItem rootItem = itemU->rootItem;
        _reload = _reload || !itemU->complete;

        if (lister->d->rootFileItem.isNull() && !rootItem.isNull() && lister->d->url == _url) {
            lister->d->rootFileItem = rootItem;
        }
        if (!items.isEmpty()) {
            qCDebug(KIO_CORE_DIRLISTER) << "emitting" << items.count() << "for lister" << lister;
            lister->d->addNewItems(_url, items);
            lister->d->emitItems();
        }
    }

    forgetCachedItemsJob(cachedItemsJob, lister, _url);

    // Emit completed, unless we were told not to,
    // or if listDir() was called while another directory listing for this dir was happening,
    // so we "joined" it. We detect that using jobForUrl to ensure it's a real ListJob,
    // not just a lister-specific CachedItemsJob (which wouldn't emit completed for us).
    if (_emitCompleted) {

        lister->d->complete = true;

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT lister->completed(_url);
#endif
        Q_EMIT lister->listingDirCompleted(_url);
        Q_EMIT lister->completed();

        if (_reload) {
            updateDirectory(_url);
        }
    }
}

void KCoreDirListerCache::forgetCachedItemsJob(KCoreDirListerPrivate::CachedItemsJob *cachedItemsJob,
                                               KCoreDirLister *lister, const QUrl &_url)
{
    // Modifications to data structures only below this point;
    // so that addNewItems is called with a consistent state

    lister->d->m_cachedItemsJobs.removeAll(cachedItemsJob);

    KCoreDirListerCacheDirectoryData &dirData = directoryData[_url];
    Q_ASSERT(dirData.listersCurrentlyListing.contains(lister));

    KIO::ListJob *listJob = jobForUrl(_url);
    if (!listJob) {
        Q_ASSERT(!dirData.listersCurrentlyHolding.contains(lister));
        qCDebug(KIO_CORE_DIRLISTER) << "Moving from listing to holding, because no more job" << lister << _url;
        dirData.listersCurrentlyHolding.append(lister);
        dirData.listersCurrentlyListing.removeAll(lister);
    } else {
        qCDebug(KIO_CORE_DIRLISTER) << "Still having a listjob" << listJob << ", so not moving to currently-holding.";
    }
}

bool KCoreDirListerCache::validUrl(KCoreDirLister *lister, const QUrl &url) const
{
    if (!url.isValid()) {
        qCWarning(KIO_CORE) << url.errorString();
        lister->handleErrorMessage(i18n("Malformed URL\n%1", url.errorString()));
        return false;
    }

    if (!KProtocolManager::supportsListing(url)) {
        lister->handleErrorMessage(i18n("URL cannot be listed\n%1", url.toString()));
        return false;
    }

    return true;
}

void KCoreDirListerCache::stop(KCoreDirLister *lister, bool silent)
{
    qCDebug(KIO_CORE_DIRLISTER) << "lister:" << lister << "silent=" << silent;

    const QList<QUrl> urls = lister->d->lstDirs;
    for (const QUrl &url : urls) {
        stopListingUrl(lister, url, silent);
    }
}

void KCoreDirListerCache::stopListingUrl(KCoreDirLister *lister, const QUrl &_u, bool silent)
{
    QUrl url(_u);
    url = url.adjusted(QUrl::StripTrailingSlash);

    KCoreDirListerPrivate::CachedItemsJob *cachedItemsJob = lister->d->cachedItemsJobForUrl(url);
    if (cachedItemsJob) {
        if (silent) {
            cachedItemsJob->setProperty("_kdlc_silent", true);
        }
        cachedItemsJob->kill(); // removes job from list, too
    }

    // TODO: consider to stop all the "child jobs" of url as well
    qCDebug(KIO_CORE_DIRLISTER) << lister << " url=" << url;

    const auto dirit = directoryData.find(url);
    if (dirit == directoryData.end()) {
        return;
    }
    KCoreDirListerCacheDirectoryData &dirData = dirit.value();
    if (dirData.listersCurrentlyListing.contains(lister)) {
        qCDebug(KIO_CORE_DIRLISTER) << " found lister" << lister << "in list - for" << url;
        if (dirData.listersCurrentlyListing.count() == 1) {
            // This was the only dirlister interested in the list job -> kill the job
            stopListJob(url, silent);
        } else {
            // Leave the job running for the other dirlisters, just unsubscribe us.
            dirData.listersCurrentlyListing.removeAll(lister);
            if (!silent) {
                Q_EMIT lister->canceled();

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
                Q_EMIT lister->canceled(url);
#endif
                Q_EMIT lister->listingDirCanceled(url);
            }
        }
    }
}

// Helper for stop() and stopListingUrl()
void KCoreDirListerCache::stopListJob(const QUrl &url, bool silent)
{
    // Old idea: if it's an update job, let's just leave the job running.
    // After all, update jobs do run for "listersCurrentlyHolding",
    // so there's no reason to kill them just because @p lister is now a holder.

    // However it could be a long-running non-local job (e.g. filenamesearch), which
    // the user wants to abort, and which will never be used for updating...
    // And in any case slotEntries/slotResult is not meant to be called by update jobs.
    // So, change of plan, let's kill it after all, in a way that triggers slotResult/slotUpdateResult.

    KIO::ListJob *job = jobForUrl(url);
    if (job) {
        qCDebug(KIO_CORE_DIRLISTER) << "Killing list job" << job << "for" << url;
        if (silent) {
            job->setProperty("_kdlc_silent", true);
        }
        job->kill(KJob::EmitResult);
    }
}

void KCoreDirListerCache::setAutoUpdate(KCoreDirLister *lister, bool enable)
{
    // IMPORTANT: this method does not check for the current autoUpdate state!

    for (auto it = lister->d->lstDirs.constBegin(), cend = lister->d->lstDirs.constEnd();
            it != cend; ++it) {
        DirItem *dirItem = itemsInUse.value(*it);
        Q_ASSERT(dirItem);
        if (enable) {
            dirItem->incAutoUpdate();
        } else {
            dirItem->decAutoUpdate();
        }
    }
}

void KCoreDirListerCache::forgetDirs(KCoreDirLister *lister)
{
    qCDebug(KIO_CORE_DIRLISTER) << lister;

    Q_EMIT lister->clear();
    // clear lister->d->lstDirs before calling forgetDirs(), so that
    // it doesn't contain things that itemsInUse doesn't. When emitting
    // the canceled signals, lstDirs must not contain anything that
    // itemsInUse does not contain. (otherwise it might crash in findByName()).
    const QList<QUrl> lstDirsCopy = lister->d->lstDirs;
    lister->d->lstDirs.clear();

    qCDebug(KIO_CORE_DIRLISTER) << "Iterating over dirs" << lstDirsCopy;
    for (const QUrl &dir : lstDirsCopy) {
        forgetDirs(lister, dir, false);
    }
}

static bool manually_mounted(const QString &path, const KMountPoint::List &possibleMountPoints)
{
    KMountPoint::Ptr mp = possibleMountPoints.findByPath(path);
    if (!mp) { // not listed in fstab -> yes, manually mounted
        if (possibleMountPoints.isEmpty()) { // no fstab at all -> don't assume anything
            return false;
        }
        return true;
    }
    // noauto -> manually mounted. Otherwise, mounted at boot time, won't be unmounted any time soon hopefully.
    return mp->mountOptions().contains(QLatin1String("noauto"));
}

void KCoreDirListerCache::forgetDirs(KCoreDirLister *lister, const QUrl &_url, bool notify)
{
    qCDebug(KIO_CORE_DIRLISTER) << lister << " _url: " << _url;

    const QUrl url = _url.adjusted(QUrl::StripTrailingSlash);

    DirectoryDataHash::iterator dit = directoryData.find(url);
    if (dit == directoryData.end()) {
        return;
    }
    KCoreDirListerCacheDirectoryData &dirData = *dit;
    dirData.listersCurrentlyHolding.removeAll(lister);

    // This lister doesn't care for updates running in <url> anymore
    KIO::ListJob *job = jobForUrl(url);
    if (job) {
        lister->d->jobDone(job);
    }

    DirItem *item = itemsInUse.value(url);
    Q_ASSERT(item);
    bool insertIntoCache = false;

    if (dirData.listersCurrentlyHolding.isEmpty() && dirData.listersCurrentlyListing.isEmpty()) {
        // item not in use anymore -> move into cache if complete
        directoryData.erase(dit);
        itemsInUse.remove(url);

        // this job is a running update which nobody cares about anymore
        if (job) {
            killJob(job);
            qCDebug(KIO_CORE_DIRLISTER) << "Killing update job for " << url;

            // Well, the user of KCoreDirLister doesn't really care that we're stopping
            // a background-running job from a previous URL (in listDir) -> commented out.
            // stop() already emitted canceled.
            //emit lister->canceled( url );
            if (lister->d->numJobs() == 0) {
                lister->d->complete = true;
                //emit lister->canceled();
            }
        }

        if (notify) {
            lister->d->lstDirs.removeAll(url);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
            Q_EMIT lister->clear(url);
#endif
            Q_EMIT lister->clearDir(url);
        }

        insertIntoCache = item->complete;
        if (insertIntoCache) {
            // TODO(afiestas): remove use of KMountPoint+manually_mounted and port to Solid:
            // 1) find Volume for the local path "item->url.toLocalFile()" (which could be anywhere
            // under the mount point) -- probably needs a new operator in libsolid query parser
            // 2) [**] becomes: if (Drive is hotpluggable or Volume is removable) "set to dirty" else "keep watch"
            const KMountPoint::List possibleMountPoints = KMountPoint::possibleMountPoints(KMountPoint::NeedMountOptions);

            // Should we forget the dir for good, or keep a watch on it?
            // Generally keep a watch, except when it would prevent
            // unmounting a removable device (#37780)
            const bool isLocal = item->url.isLocalFile();
            bool isManuallyMounted = false;
            bool containsManuallyMounted = false;
            if (isLocal) {
                isManuallyMounted = manually_mounted(item->url.toLocalFile(), possibleMountPoints);
                if (!isManuallyMounted) {
                    // Look for a manually-mounted directory inside
                    // If there's one, we can't keep a watch either, FAM would prevent unmounting the CDROM
                    // I hope this isn't too slow
                    auto kit = item->lstItems.constBegin();
                    const auto kend = item->lstItems.constEnd();
                    for (; kit != kend && !containsManuallyMounted; ++kit)
                        if ((*kit).isDir() && manually_mounted((*kit).url().toLocalFile(), possibleMountPoints)) {
                            containsManuallyMounted = true;
                        }
                }
            }

            if (isManuallyMounted || containsManuallyMounted) { // [**]
                qCDebug(KIO_CORE_DIRLISTER) << "Not adding a watch on " << item->url << " because it " <<
                    ( isManuallyMounted ? "is manually mounted" : "contains a manually mounted subdir" );
                item->complete = false; // set to "dirty"
            } else {
                item->incAutoUpdate(); // keep watch
                item->watchedWhileInCache = true;
            }
        } else {
            delete item;
            item = nullptr;
        }
    }

    if (item && lister->d->autoUpdate) {
        item->decAutoUpdate();
    }

    // Inserting into QCache must be done last, since it might delete the item
    if (item && insertIntoCache) {
        qCDebug(KIO_CORE_DIRLISTER) << lister << "item moved into cache:" << url;
        itemsCached.insert(url, item);
    }
}

void KCoreDirListerCache::updateDirectory(const QUrl &_dir)
{
    qCDebug(KIO_CORE_DIRLISTER) << _dir;

    const QUrl dir = _dir.adjusted(QUrl::StripTrailingSlash);
    if (!checkUpdate(dir)) {
        return;
    }

    // A job can be running to
    //   - only list a new directory: the listers are in listersCurrentlyListing
    //   - only update a directory: the listers are in listersCurrentlyHolding
    //   - update a currently running listing: the listers are in both

    KCoreDirListerCacheDirectoryData &dirData = directoryData[dir];
    const QList<KCoreDirLister *> listers = dirData.listersCurrentlyListing;
    const QList<KCoreDirLister *> holders = dirData.listersCurrentlyHolding;

    qCDebug(KIO_CORE_DIRLISTER) << dir << "listers=" << listers << "holders=" << holders;

    bool killed = false;
    KIO::ListJob *job = jobForUrl(dir);
    if (job) {
        // the job is running already, tell it to do another update at the end
        // (don't kill it, we would keep doing that during a long download to a slow sshfs mount)
        job->setProperty("need_another_update", true);
        return;
    } else {
        // Emit any cached items.
        // updateDirectory() is about the diff compared to the cached items...
        for (const KCoreDirLister *kdl : listers) {
            KCoreDirListerPrivate::CachedItemsJob *cachedItemsJob = kdl->d->cachedItemsJobForUrl(dir);
            if (cachedItemsJob) {
                cachedItemsJob->setEmitCompleted(false);
                cachedItemsJob->done(); // removes from cachedItemsJobs list
                delete cachedItemsJob;
                killed = true;
            }
        }
    }
    qCDebug(KIO_CORE_DIRLISTER) << "Killed=" << killed;

    // we don't need to emit canceled signals since we only replaced the job,
    // the listing is continuing.

    if (!(listers.isEmpty() || killed)) {
        qCWarning(KIO_CORE) << "The unexpected happened.";
        qCWarning(KIO_CORE) << "listers for" << dir << "=" << listers;
        qCWarning(KIO_CORE) << "job=" << job;
        for (const KCoreDirLister *kdl : listers) {
            qCDebug(KIO_CORE_DIRLISTER) << "lister" << kdl << "m_cachedItemsJobs=" << kdl->d->m_cachedItemsJobs;
        }
#ifndef NDEBUG
        printDebug();
#endif
    }
    Q_ASSERT(listers.isEmpty() || killed);

    job = KIO::listDir(dir, KIO::HideProgressInfo);
    runningListJobs.insert(job, KIO::UDSEntryList());

    connect(job, &KIO::ListJob::entries, this, &KCoreDirListerCache::slotUpdateEntries);
    connect(job, &KJob::result, this, &KCoreDirListerCache::slotUpdateResult);

    qCDebug(KIO_CORE_DIRLISTER) << "update started in" << dir;

    for (KCoreDirLister *kdl : listers) {
        kdl->jobStarted(job);
    }

    if (!holders.isEmpty()) {
        if (!killed) {
            for (KCoreDirLister *kdl : holders) {
                kdl->jobStarted(job);
                Q_EMIT kdl->started(dir);
            }
        } else {
            for (KCoreDirLister *kdl : holders) {
                kdl->jobStarted(job);
            }
        }
    }
}

bool KCoreDirListerCache::checkUpdate(const QUrl &_dir)
{
    if (!itemsInUse.contains(_dir)) {
        DirItem *item = itemsCached[_dir];
        if (item && item->complete) {
            item->complete = false;
            item->watchedWhileInCache = false;
            item->decAutoUpdate();
            qCDebug(KIO_CORE_DIRLISTER) << "directory " << _dir << " not in use, marked dirty.";
        }
        //else
        qCDebug(KIO_CORE_DIRLISTER) << "aborted, directory " << _dir << " not in cache.";
        return false;
    } else {
        return true;
    }
}

KFileItem KCoreDirListerCache::itemForUrl(const QUrl &url) const
{
    return findByUrl(nullptr, url);
}

KCoreDirListerCache::DirItem *KCoreDirListerCache::dirItemForUrl(const QUrl &dir) const
{
    const QUrl url = dir.adjusted(QUrl::StripTrailingSlash);
    DirItem *item = itemsInUse.value(url);
    if (!item) {
        item = itemsCached[url];
    }
    return item;
}

QList<KFileItem> *KCoreDirListerCache::itemsForDir(const QUrl &dir) const
{
    DirItem *item = dirItemForUrl(dir);
    return item ? &item->lstItems : nullptr;
}

KFileItem KCoreDirListerCache::findByName(const KCoreDirLister *lister, const QString &_name) const
{
    Q_ASSERT(lister);

    for (QList<QUrl>::const_iterator it = lister->d->lstDirs.constBegin();
            it != lister->d->lstDirs.constEnd(); ++it) {
        DirItem *dirItem = itemsInUse.value(*it);
        Q_ASSERT(dirItem);

        auto lit = dirItem->lstItems.constBegin();
        const auto litend = dirItem->lstItems.constEnd();
        for (; lit != litend; ++lit) {
            if ((*lit).name() == _name) {
                return *lit;
            }
        }
    }

    return KFileItem();
}

KFileItem KCoreDirListerCache::findByUrl(const KCoreDirLister *lister, const QUrl &_u) const
{
    QUrl url(_u);
    url = url.adjusted(QUrl::StripTrailingSlash);

    const QUrl parentDir = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    DirItem *dirItem = dirItemForUrl(parentDir);
    if (dirItem) {
        // If lister is set, check that it contains this dir
        if (!lister || lister->d->lstDirs.contains(parentDir)) {
            // Binary search
            auto it = std::lower_bound(dirItem->lstItems.begin(), dirItem->lstItems.end(), url);
            if (it != dirItem->lstItems.end() && it->url() == url) {
                return *it;
            }
        }
    }

    // Maybe _u is a directory itself? (see KDirModelTest::testChmodDirectory)
    // We check this last, though, we prefer returning a kfileitem with an actual
    // name if possible (and we make it '.' for root items later).
    dirItem = dirItemForUrl(url);
    if (dirItem && !dirItem->rootItem.isNull() && dirItem->rootItem.url() == url) {
        // If lister is set, check that it contains this dir
        if (!lister || lister->d->lstDirs.contains(url)) {
            return dirItem->rootItem;
        }
    }

    return KFileItem();
}

void KCoreDirListerCache::slotFilesAdded(const QString &dir /*url*/)   // from KDirNotify signals
{
    QUrl urlDir(dir);
    itemsAddedInDirectory(urlDir);
}

void KCoreDirListerCache::itemsAddedInDirectory(const QUrl &urlDir)
{
    qCDebug(KIO_CORE_DIRLISTER) << urlDir;
    const QList<QUrl> urls = directoriesForCanonicalPath(urlDir);
    for (const QUrl &u : urls) {
        updateDirectory(u);
    }
}

void KCoreDirListerCache::slotFilesRemoved(const QStringList &fileList)   // from KDirNotify signals
{
    // TODO: handling of symlinks-to-directories isn't done here,
    // because I'm not sure how to do it and keep the performance ok...

    slotFilesRemoved(QUrl::fromStringList(fileList));
}

void KCoreDirListerCache::slotFilesRemoved(const QList<QUrl> &fileList)
{
    qCDebug(KIO_CORE_DIRLISTER) << fileList.count();
    // Group notifications by parent dirs (usually there would be only one parent dir)
    QMap<QUrl, KFileItemList> removedItemsByDir;
    QList<QUrl> deletedSubdirs;

    for (const QUrl &url : fileList) {
        DirItem *dirItem = dirItemForUrl(url); // is it a listed directory?
        if (dirItem) {
            deletedSubdirs.append(url);
            if (!dirItem->rootItem.isNull()) {
                removedItemsByDir[url].append(dirItem->rootItem);
            }
        }

        const QUrl parentDir = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        dirItem = dirItemForUrl(parentDir);
        if (!dirItem) {
            continue;
        }
        for (auto fit = dirItem->lstItems.begin(), fend = dirItem->lstItems.end(); fit != fend; ++fit) {
            if ((*fit).url() == url) {
                const KFileItem fileitem = *fit;
                removedItemsByDir[parentDir].append(fileitem);
                // If we found a fileitem, we can test if it's a dir. If not, we'll go to deleteDir just in case.
                if (fileitem.isNull() || fileitem.isDir()) {
                    deletedSubdirs.append(url);
                }
                dirItem->lstItems.erase(fit); // remove fileitem from list
                break;
            }
        }
    }

    for (auto rit = removedItemsByDir.constBegin(), cend = removedItemsByDir.constEnd(); rit != cend; ++rit) {
        // Tell the views about it before calling deleteDir.
        // They might need the subdirs' file items (see the dirtree).
        auto dit = directoryData.constFind(rit.key());
        if (dit != directoryData.constEnd()) {
            itemsDeleted((*dit).listersCurrentlyHolding, rit.value());
        }
    }

    for (const QUrl &url : qAsConst(deletedSubdirs)) {
        // in case of a dir, check if we have any known children, there's much to do in that case
        // (stopping jobs, removing dirs from cache etc.)
        deleteDir(url);
    }
}

void KCoreDirListerCache::slotFilesChanged(const QStringList &fileList)   // from KDirNotify signals
{
    qCDebug(KIO_CORE_DIRLISTER) << fileList;
    QList<QUrl> dirsToUpdate;
    for (const QString &fileUrl : fileList) {
        const QUrl url(fileUrl);
        const KFileItem &fileitem = findByUrl(nullptr, url);
        if (fileitem.isNull()) {
            qCDebug(KIO_CORE_DIRLISTER) << "item not found for" << url;
            continue;
        }
        if (url.isLocalFile()) {
            pendingUpdates.insert(url.toLocalFile()); // delegate the work to processPendingUpdates
        } else {
            pendingRemoteUpdates.insert(fileitem);
            // For remote files, we won't be able to figure out the new information,
            // we have to do a update (directory listing)
            const QUrl dir = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
            if (!dirsToUpdate.contains(dir)) {
                dirsToUpdate.prepend(dir);
            }
        }
    }

    for (const QUrl &dirUrl : qAsConst(dirsToUpdate)) {
        updateDirectory(dirUrl);
    }
    // ## TODO problems with current jobs listing/updating that dir
    // ( see kde-2.2.2's kdirlister )

    processPendingUpdates();
}

void KCoreDirListerCache::slotFileRenamed(const QString &_src, const QString &_dst, const QString &dstPath)   // from KDirNotify signals
{
    QUrl src(_src);
    QUrl dst(_dst);
    qCDebug(KIO_CORE_DIRLISTER) << src << "->" << dst;
#ifdef DEBUG_CACHE
    printDebug();
#endif

    QUrl oldurl = src.adjusted(QUrl::StripTrailingSlash);
    KFileItem fileitem = findByUrl(nullptr, oldurl);
    if (fileitem.isNull()) {
        qCDebug(KIO_CORE_DIRLISTER) << "Item not found:" << oldurl;
        return;
    }

    const KFileItem oldItem = fileitem;

    // Dest already exists? Was overwritten then (testcase: #151851)
    // We better emit it as deleted -before- doing the renaming, otherwise
    // the "update" mechanism will emit the old one as deleted and
    // kdirmodel will delete the new (renamed) one!
    const KFileItem &existingDestItem = findByUrl(nullptr, dst);
    if (!existingDestItem.isNull()) {
        qCDebug(KIO_CORE_DIRLISTER) << dst << "already existed, let's delete it";
        slotFilesRemoved(QList<QUrl>{dst});
    }

    // If the item had a UDS_URL as well as UDS_NAME set, the user probably wants
    // to be updating the name only (since they can't see the URL).
    // Check to see if a URL exists, and if so, if only the file part has changed,
    // only update the name and not the underlying URL.
    bool nameOnly = !fileitem.entry().stringValue(KIO::UDSEntry::UDS_URL).isEmpty();
    nameOnly = nameOnly && src.adjusted(QUrl::RemoveFilename) == dst.adjusted(QUrl::RemoveFilename);

    if (!nameOnly && fileitem.isDir()) {
        renameDir(oldurl, dst);
        // #172945 - if the fileitem was the root item of a DirItem that was just removed from the cache,
        // then it's a dangling pointer now...
        fileitem = findByUrl(nullptr, oldurl);
        if (fileitem.isNull()) { //deleted from cache altogether, #188807
            return;
        }
    }

    // Now update the KFileItem representing that file or dir (not exclusive with the above!)
    if (!oldItem.isLocalFile() && !oldItem.localPath().isEmpty() && dstPath.isEmpty()) { // it uses UDS_LOCAL_PATH and we don't know the new path? needs an update then
        slotFilesChanged(QStringList{src.toString()});
    } else {
        const QUrl &itemOldUrl = fileitem.url();
        if (nameOnly) {
            fileitem.setName(dst.fileName());
        } else {
            fileitem.setUrl(dst);
        }

        if (!dstPath.isEmpty()) {
            fileitem.setLocalPath(dstPath);
        }

        fileitem.refreshMimeType();
        fileitem.determineMimeType();
        reinsert(fileitem, itemOldUrl);

        const QSet<KCoreDirLister *> listers = emitRefreshItem(oldItem, fileitem);
        for (KCoreDirLister *kdl : listers) {
            kdl->d->emitItems();
        }
    }

#ifdef DEBUG_CACHE
    printDebug();
#endif
}

QSet<KCoreDirLister *> KCoreDirListerCache::emitRefreshItem(const KFileItem &oldItem, const KFileItem &fileitem)
{
    qCDebug(KIO_CORE_DIRLISTER) << "old:" << oldItem.name() << oldItem.url()
                                << "new:" << fileitem.name() << fileitem.url();
    // Look whether this item was shown in any view, i.e. held by any dirlister
    const QUrl parentDir = oldItem.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    DirectoryDataHash::iterator dit = directoryData.find(parentDir);
    QList<KCoreDirLister *> listers;
    // Also look in listersCurrentlyListing, in case the user manages to rename during a listing
    if (dit != directoryData.end()) {
        listers += (*dit).listersCurrentlyHolding + (*dit).listersCurrentlyListing;
    }
    if (oldItem.isDir()) {
        // For a directory, look for dirlisters where it's the root item.
        dit = directoryData.find(oldItem.url());
        if (dit != directoryData.end()) {
            listers += (*dit).listersCurrentlyHolding + (*dit).listersCurrentlyListing;
        }
    }
    QSet<KCoreDirLister *> listersToRefresh;
    for (KCoreDirLister *kdl : qAsConst(listers)) {
        // For a directory, look for dirlisters where it's the root item.
        QUrl directoryUrl(oldItem.url());
        if (oldItem.isDir() && kdl->d->rootFileItem == oldItem) {
            const KFileItem oldRootItem = kdl->d->rootFileItem;
            kdl->d->rootFileItem = fileitem;
            kdl->d->addRefreshItem(directoryUrl, oldRootItem, fileitem);
        } else {
            directoryUrl = directoryUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
            kdl->d->addRefreshItem(directoryUrl, oldItem, fileitem);
        }
        listersToRefresh.insert(kdl);
    }
    return listersToRefresh;
}

QList<QUrl> KCoreDirListerCache::directoriesForCanonicalPath(const QUrl &dir) const
{
    QList<QUrl> urlList = canonicalUrls.value(dir);
    // make unique
    if (urlList.size() > 1) {
        std::sort(urlList.begin(), urlList.end());
        auto end_unique = std::unique(urlList.begin(), urlList.end());
        urlList.erase(end_unique, urlList.end());
    }

    QList<QUrl> dirs({dir});
    dirs.append(urlList);

    if (dirs.count() > 1) {
        qCDebug(KIO_CORE_DIRLISTER) << dir << "known as" << dirs;
    }
    return dirs;
}

// private slots

// Called by KDirWatch - usually when a dir we're watching has been modified,
// but it can also be called for a file.
void KCoreDirListerCache::slotFileDirty(const QString &path)
{
    qCDebug(KIO_CORE_DIRLISTER) << path;
    QUrl url = QUrl::fromLocalFile(path).adjusted(QUrl::StripTrailingSlash);
    // File or dir?
    bool isDir;
    const KFileItem item = itemForUrl(url);

    if (!item.isNull()) {
        isDir = item.isDir();
    } else {
        QFileInfo info(path);
        if (!info.exists()) {
            return;    // error
        }
        isDir = info.isDir();
    }

    if (isDir) {
        const QList<QUrl> urls = directoriesForCanonicalPath(url);
        for (const QUrl &dir : urls) {
            handleFileDirty(dir); // e.g. for permission changes
            handleDirDirty(dir);
        }
    } else {
        const QList<QUrl> urls = directoriesForCanonicalPath(url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
        for (const QUrl &dir : urls) {
            QUrl aliasUrl(dir);
            aliasUrl.setPath(concatPaths(aliasUrl.path(), url.fileName()));
            handleFileDirty(aliasUrl);
        }
    }
}

// Called by slotFileDirty
void KCoreDirListerCache::handleDirDirty(const QUrl &url)
{
    // A dir: launch an update job if anyone cares about it

    // This also means we can forget about pending updates to individual files in that dir
    const QString dir = url.toLocalFile();
    QString dirPath = dir;
    if (!dirPath.endsWith(QLatin1Char('/'))) {
        dirPath += QLatin1Char('/');
    }
    QMutableSetIterator<QString> pendingIt(pendingUpdates);
    while (pendingIt.hasNext()) {
        const QString updPath = pendingIt.next();
        qCDebug(KIO_CORE_DIRLISTER) << "had pending update" << updPath;
        if (updPath.startsWith(dirPath) &&
                updPath.indexOf(QLatin1Char('/'), dirPath.length()) == -1) { // direct child item
            qCDebug(KIO_CORE_DIRLISTER) << "forgetting about individual update to" << updPath;
            pendingIt.remove();
        }
    }

    if (checkUpdate(url) && !pendingDirectoryUpdates.contains(dir)) {
        pendingDirectoryUpdates.insert(dir);
        if (!pendingUpdateTimer.isActive()) {
            pendingUpdateTimer.start(200);
        }
    }
}

// Called by slotFileDirty
void KCoreDirListerCache::handleFileDirty(const QUrl &url)
{
    // A file: do we know about it already?
    const KFileItem &existingItem = findByUrl(nullptr, url);
    const QUrl dir = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
    QString filePath = url.toLocalFile();
    if (existingItem.isNull()) {
        // No - update the parent dir then
        handleDirDirty(dir);
    }

    // Delay updating the file, FAM is flooding us with events
    if (checkUpdate(dir) && !pendingUpdates.contains(filePath)) {
        pendingUpdates.insert(filePath);
        if (!pendingUpdateTimer.isActive()) {
            pendingUpdateTimer.start(200);
        }
    }
}

void KCoreDirListerCache::slotFileCreated(const QString &path)   // from KDirWatch
{
    qCDebug(KIO_CORE_DIRLISTER) << path;
    // XXX: how to avoid a complete rescan here?
    // We'd need to stat that one file separately and refresh the item(s) for it.
    QUrl fileUrl(QUrl::fromLocalFile(path));
    itemsAddedInDirectory(fileUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
}

void KCoreDirListerCache::slotFileDeleted(const QString &path)   // from KDirWatch
{
    qCDebug(KIO_CORE_DIRLISTER) << path;
    const QString fileName = QFileInfo(path).fileName();
    QUrl dirUrl(QUrl::fromLocalFile(path));
    QStringList fileUrls;
    const QList<QUrl> urls = directoriesForCanonicalPath(dirUrl.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash));
    for (const QUrl &url : urls) {
        QUrl urlInfo(url);
        urlInfo.setPath(concatPaths(urlInfo.path(), fileName));
        fileUrls << urlInfo.toString();
    }
    slotFilesRemoved(fileUrls);
}

void KCoreDirListerCache::slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries)
{
    QUrl url(joburl(static_cast<KIO::ListJob *>(job)));
    url = url.adjusted(QUrl::StripTrailingSlash);

    qCDebug(KIO_CORE_DIRLISTER) << "new entries for " << url;

    DirItem *dir = itemsInUse.value(url);
    if (!dir) {
        qCWarning(KIO_CORE) << "Internal error: job is listing" << url << "but itemsInUse only knows about" << itemsInUse.keys();
        Q_ASSERT(dir);
        return;
    }

    DirectoryDataHash::iterator dit = directoryData.find(url);
    if (dit == directoryData.end()) {
        qCWarning(KIO_CORE) << "Internal error: job is listing" << url << "but directoryData doesn't know about that url, only about:" << directoryData.keys();
        Q_ASSERT(dit != directoryData.end());
        return;
    }
    KCoreDirListerCacheDirectoryData &dirData = *dit;
    const QList<KCoreDirLister *> listers = dirData.listersCurrentlyListing;
    if (listers.isEmpty()) {
        qCWarning(KIO_CORE) << "Internal error: job is listing" << url << "but directoryData says no listers are currently listing " << url;
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT(!listers.isEmpty());
        return;
    }

    // check if anyone wants the MIME types immediately
    bool delayedMimeTypes = true;
    for  (const KCoreDirLister *kdl : listers) {
        delayedMimeTypes &= kdl->d->delayedMimeTypes;
    }

    QSet<QString> filesToHide;
    bool dotHiddenChecked = false;
    KIO::UDSEntryList::const_iterator it = entries.begin();
    const KIO::UDSEntryList::const_iterator end = entries.end();
    for (; it != end; ++it) {
        const QString name = (*it).stringValue(KIO::UDSEntry::UDS_NAME);

        Q_ASSERT(!name.isEmpty());
        if (name.isEmpty()) {
            continue;
        }

        if (name == QLatin1Char('.')) {
            Q_ASSERT(dir->rootItem.isNull());
            // Try to reuse an existing KFileItem (if we listed the parent dir)
            // rather than creating a new one. There are many reasons:
            // 1) renames and permission changes to the item would have to emit the signals
            // twice, otherwise, so that both views manage to recognize the item.
            // 2) with kio_ftp we can only know that something is a symlink when
            // listing the parent, so prefer that item, which has more info.
            // Note that it gives a funky name() to the root item, rather than "." ;)
            dir->rootItem = itemForUrl(url);
            if (dir->rootItem.isNull()) {
                dir->rootItem = KFileItem(*it, url, delayedMimeTypes, true);
            }

            for (KCoreDirLister *kdl : listers) {
                if (kdl->d->rootFileItem.isNull() && kdl->d->url == url) {
                    kdl->d->rootFileItem = dir->rootItem;
                }
            }
        } else if (name != QLatin1String("..")) {
            KFileItem item(*it, url, delayedMimeTypes, true);

            // get the names of the files listed in ".hidden", if it exists and is a local file
            if (!dotHiddenChecked) {
                const QString localPath = item.localPath();
                if (!localPath.isEmpty()) {
                    const QString rootItemPath = QFileInfo(localPath).absolutePath();
                    filesToHide = filesInDotHiddenForDir(rootItemPath);
                }
                dotHiddenChecked = true;
            }

            // hide file if listed in ".hidden"
            if (filesToHide.contains(name)) {
                item.setHidden();
            }

            qCDebug(KIO_CORE_DIRLISTER)<< "Adding item: " << item.url();
            // Add the items sorted by url, needed by findByUrl
            dir->insert(item);

            for (KCoreDirLister *kdl : listers) {
                kdl->d->addNewItem(url, item);
            }
        }
    }

    for (KCoreDirLister *kdl : listers) {
        kdl->d->emitItems();
    }
}

void KCoreDirListerCache::slotResult(KJob *j)
{
#ifdef DEBUG_CACHE
    //printDebug();
#endif

    Q_ASSERT(j);
    KIO::ListJob *job = static_cast<KIO::ListJob *>(j);
    runningListJobs.remove(job);

    QUrl jobUrl(joburl(job));
    jobUrl = jobUrl.adjusted(QUrl::StripTrailingSlash);  // need remove trailing slashes again, in case of redirections

    qCDebug(KIO_CORE_DIRLISTER) << "finished listing" << jobUrl;

    const auto dit = directoryData.find(jobUrl);
    if (dit == directoryData.end()) {
        qCWarning(KIO_CORE) << "Nothing found in directoryData for URL" << jobUrl;
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT(dit != directoryData.end());
        return;
    }
    KCoreDirListerCacheDirectoryData &dirData = *dit;
    if (dirData.listersCurrentlyListing.isEmpty()) {
        qCWarning(KIO_CORE) << "OOOOPS, nothing in directoryData.listersCurrentlyListing for" << jobUrl;
        // We're about to assert; dump the current state...
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT(!dirData.listersCurrentlyListing.isEmpty());
    }
    const QList<KCoreDirLister *> listers = dirData.listersCurrentlyListing;

    // move all listers to the holding list, do it before emitting
    // the signals to make sure it exists in KCoreDirListerCache in case someone
    // calls listDir during the signal emission
    Q_ASSERT(dirData.listersCurrentlyHolding.isEmpty());
    dirData.moveListersWithoutCachedItemsJob(jobUrl);

    if (job->error()) {
        for (KCoreDirLister *kdl : listers) {
            kdl->d->jobDone(job);
            if (job->error() != KJob::KilledJobError) {
                kdl->handleError(job);
            }
            const bool silent = job->property("_kdlc_silent").toBool();
            if (!silent) {
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
                Q_EMIT kdl->canceled(jobUrl);
#endif
                Q_EMIT kdl->listingDirCanceled(jobUrl);
            }

            if (kdl->d->numJobs() == 0) {
                kdl->d->complete = true;
                if (!silent) {
                    Q_EMIT kdl->canceled();
                }
            }
        }
    } else {
        DirItem *dir = itemsInUse.value(jobUrl);
        Q_ASSERT(dir);
        dir->complete = true;

        for (KCoreDirLister *kdl : listers) {
            kdl->d->jobDone(job);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
            Q_EMIT kdl->completed(jobUrl);
#endif
            Q_EMIT kdl->listingDirCompleted(jobUrl);
            if (kdl->d->numJobs() == 0) {
                kdl->d->complete = true;
                Q_EMIT kdl->completed();
            }
        }
    }

    // TODO: hmm, if there was an error and job is a parent of one or more
    // of the pending urls we should cancel it/them as well
    processPendingUpdates();

    if (job->property("need_another_update").toBool()) {
        updateDirectory(jobUrl);
    }

#ifdef DEBUG_CACHE
    printDebug();
#endif
}

void KCoreDirListerCache::slotRedirection(KIO::Job *j, const QUrl &url)
{
    Q_ASSERT(j);
    KIO::ListJob *job = static_cast<KIO::ListJob *>(j);

    QUrl oldUrl(job->url());  // here we really need the old url!
    QUrl newUrl(url);

    // strip trailing slashes
    oldUrl = oldUrl.adjusted(QUrl::StripTrailingSlash);
    newUrl = newUrl.adjusted(QUrl::StripTrailingSlash);

    if (oldUrl == newUrl) {
        qCDebug(KIO_CORE_DIRLISTER) << "New redirection url same as old, giving up.";
        return;
    } else if (newUrl.isEmpty()) {
        qCDebug(KIO_CORE_DIRLISTER) << "New redirection url is empty, giving up.";
        return;
    }

    qCDebug(KIO_CORE_DIRLISTER) << oldUrl << "->" << newUrl;

#ifdef DEBUG_CACHE
    // Can't do that here. KCoreDirListerCache::joburl() will use the new url already,
    // while our data structures haven't been updated yet -> assert fail.
    //printDebug();
#endif

    // I don't think there can be dirItems that are children of oldUrl.
    // Am I wrong here? And even if so, we don't need to delete them, right?
    // DF: redirection happens before listDir emits any item. Makes little sense otherwise.

    // oldUrl cannot be in itemsCached because only completed items are moved there
    DirItem *dir = itemsInUse.take(oldUrl);
    Q_ASSERT(dir);

    DirectoryDataHash::iterator dit = directoryData.find(oldUrl);
    Q_ASSERT(dit != directoryData.end());
    KCoreDirListerCacheDirectoryData oldDirData = *dit;
    directoryData.erase(dit);
    Q_ASSERT(!oldDirData.listersCurrentlyListing.isEmpty());
    const QList<KCoreDirLister *> listers = oldDirData.listersCurrentlyListing;
    Q_ASSERT(!listers.isEmpty());

    for (KCoreDirLister *kdl : listers) {
        kdl->d->redirect(oldUrl, newUrl, false /*clear items*/);
    }

    // when a lister was stopped before the job emits the redirection signal, the old url will
    // also be in listersCurrentlyHolding
    const QList<KCoreDirLister *> holders = oldDirData.listersCurrentlyHolding;
    for (KCoreDirLister *kdl : holders) {
        kdl->jobStarted(job);
        // do it like when starting a new list-job that will redirect later
        // TODO: maybe don't emit started if there's an update running for newUrl already?
        Q_EMIT kdl->started(oldUrl);

        kdl->d->redirect(oldUrl, newUrl, false /*clear items*/);
    }

    const QList<KCoreDirLister *> allListers = listers + holders;

    DirItem *newDir = itemsInUse.value(newUrl);
    if (newDir) {
        qCDebug(KIO_CORE_DIRLISTER) << newUrl << "already in use";

        // only in this case there can newUrl already be in listersCurrentlyListing or listersCurrentlyHolding
        delete dir;

        // get the job if one's running for newUrl already (can be a list-job or an update-job), but
        // do not return this 'job', which would happen because of the use of redirectionURL()
        KIO::ListJob *oldJob = jobForUrl(newUrl, job);

        // listers of newUrl with oldJob: forget about the oldJob and use the already running one
        // which will be converted to an updateJob
        KCoreDirListerCacheDirectoryData &newDirData = directoryData[newUrl];

        QList<KCoreDirLister *> &curListers = newDirData.listersCurrentlyListing;
        if (!curListers.isEmpty()) {
            qCDebug(KIO_CORE_DIRLISTER) << "and it is currently listed";

            Q_ASSERT(oldJob);    // ?!

            for (KCoreDirLister *kdl : qAsConst(curListers)) {   // listers of newUrl
                kdl->d->jobDone(oldJob);

                kdl->jobStarted(job);
                kdl->d->connectJob(job);
            }

            // append listers of oldUrl with newJob to listers of newUrl with oldJob
            for (KCoreDirLister *kdl : listers) {
                curListers.append(kdl);
            }
        } else {
            curListers = listers;
        }

        if (oldJob) {         // kill the old job, be it a list-job or an update-job
            killJob(oldJob);
        }

        // holders of newUrl: use the already running job which will be converted to an updateJob
        QList<KCoreDirLister *> &curHolders = newDirData.listersCurrentlyHolding;
        if (!curHolders.isEmpty()) {
            qCDebug(KIO_CORE_DIRLISTER) << "and it is currently held.";

            for (KCoreDirLister *kdl : qAsConst(curHolders)) {    // holders of newUrl
                kdl->jobStarted(job);
                Q_EMIT kdl->started(newUrl);
            }

            // append holders of oldUrl to holders of newUrl
            for (KCoreDirLister *kdl : holders) {
                curHolders.append(kdl);
            }
        } else {
            curHolders = holders;
        }

        // emit old items: listers, holders. NOT: newUrlListers/newUrlHolders, they already have them listed
        // TODO: make this a separate method?
        for (KCoreDirLister *kdl : allListers) {
            if (kdl->d->rootFileItem.isNull() && kdl->d->url == newUrl) {
                kdl->d->rootFileItem = newDir->rootItem;
            }

            kdl->d->addNewItems(newUrl, newDir->lstItems);
            kdl->d->emitItems();
        }
    } else if ((newDir = itemsCached.take(newUrl))) {
        qCDebug(KIO_CORE_DIRLISTER) << newUrl << "is unused, but already in the cache.";

        delete dir;
        itemsInUse.insert(newUrl, newDir);
        KCoreDirListerCacheDirectoryData &newDirData = directoryData[newUrl];
        newDirData.listersCurrentlyListing = listers;
        newDirData.listersCurrentlyHolding = holders;

        // emit old items: listers, holders
        for (KCoreDirLister *kdl : allListers) {
            if (kdl->d->rootFileItem.isNull() && kdl->d->url == newUrl) {
                kdl->d->rootFileItem = newDir->rootItem;
            }

            kdl->d->addNewItems(newUrl, newDir->lstItems);
            kdl->d->emitItems();
        }
    } else {
        qCDebug(KIO_CORE_DIRLISTER) << newUrl << "has not been listed yet.";

        dir->rootItem = KFileItem();
        dir->lstItems.clear();
        dir->redirect(newUrl);
        itemsInUse.insert(newUrl, dir);
        KCoreDirListerCacheDirectoryData &newDirData = directoryData[newUrl];
        newDirData.listersCurrentlyListing = listers;
        newDirData.listersCurrentlyHolding = holders;

        if (holders.isEmpty()) {
#ifdef DEBUG_CACHE
            printDebug();
#endif
            return; // only in this case the job doesn't need to be converted,
        }
    }

    // make the job an update job
    job->disconnect(this);

    connect(job, &KIO::ListJob::entries, this, &KCoreDirListerCache::slotUpdateEntries);
    connect(job, &KJob::result, this, &KCoreDirListerCache::slotUpdateResult);

    // FIXME: autoUpdate-Counts!!

#ifdef DEBUG_CACHE
    printDebug();
#endif
}

struct KCoreDirListerCache::ItemInUseChange {
    ItemInUseChange(const QUrl &old, const QUrl &newU, DirItem *di)
        : oldUrl(old), newUrl(newU), dirItem(di) {}
    QUrl oldUrl;
    QUrl newUrl;
    DirItem *dirItem;
};

void KCoreDirListerCache::renameDir(const QUrl &oldUrl, const QUrl &newUrl)
{
    qCDebug(KIO_CORE_DIRLISTER) << oldUrl << "->" << newUrl;
    //const QString oldUrlStr = oldUrl.url(KUrl::RemoveTrailingSlash);
    //const QString newUrlStr = newUrl.url(KUrl::RemoveTrailingSlash);

    // Not enough. Also need to look at any child dir, even sub-sub-sub-dir.
    //DirItem *dir = itemsInUse.take( oldUrlStr );
    //emitRedirections( oldUrl, url );

    std::vector<ItemInUseChange> itemsToChange;
    QSet<KCoreDirLister *> listers;

    // Look at all dirs being listed/shown
    for (auto itu = itemsInUse.begin(), ituend = itemsInUse.end(); itu != ituend; ++itu) {
        DirItem *dir = itu.value();
        const QUrl &oldDirUrl = itu.key();
        qCDebug(KIO_CORE_DIRLISTER) << "itemInUse:" << oldDirUrl;
        // Check if this dir is oldUrl, or a subfolder of it
        if (oldDirUrl == oldUrl || oldUrl.isParentOf(oldDirUrl)) {
            // TODO should use KUrl::cleanpath like isParentOf does
            QString relPath = oldDirUrl.path().mid(oldUrl.path().length()+1);

            QUrl newDirUrl(newUrl); // take new base
            if (!relPath.isEmpty()) {
                newDirUrl.setPath(concatPaths(newDirUrl.path(), relPath));    // add unchanged relative path
            }
            qCDebug(KIO_CORE_DIRLISTER) << "new url=" << newDirUrl;

            // Update URL in dir item and in itemsInUse
            dir->redirect(newDirUrl);

            itemsToChange.emplace_back(oldDirUrl.adjusted(QUrl::StripTrailingSlash),
                                       newDirUrl.adjusted(QUrl::StripTrailingSlash),
                                       dir);
            // Rename all items under that dir
            // If all items of the directory change the same part of their url, the order is not
            // changed, therefore just change it in the list.
            for (KFileItem &item : dir->lstItems) {
                const KFileItem oldItem = item;
                KFileItem newItem = oldItem;
                const QUrl &oldItemUrl = oldItem.url();
                QUrl newItemUrl(oldItemUrl);
                newItemUrl.setPath(concatPaths(newDirUrl.path(), oldItemUrl.fileName()));
                qCDebug(KIO_CORE_DIRLISTER) << "renaming" << oldItemUrl << "to" << newItemUrl;
                newItem.setUrl(newItemUrl);

                listers |= emitRefreshItem(oldItem, newItem);
                // Change the item
                item.setUrl(newItemUrl);
            }
        }
    }

    for (KCoreDirLister *kdl : qAsConst(listers)) {
        kdl->d->emitItems();
    }

    // Do the changes to itemsInUse out of the loop to avoid messing up iterators,
    // and so that emitRefreshItem can find the stuff in the hash.
    for (const ItemInUseChange &i : itemsToChange) {
        itemsInUse.remove(i.oldUrl);
        itemsInUse.insert(i.newUrl, i.dirItem);
    }
    //Now that all the caches are updated and consistent, emit the redirection.
    for (const ItemInUseChange& i : itemsToChange) {
        emitRedirections(QUrl(i.oldUrl), QUrl(i.newUrl));
    }
    // Is oldUrl a directory in the cache?
    // Remove any child of oldUrl from the cache - even if the renamed dir itself isn't in it!
    removeDirFromCache(oldUrl);
    // TODO rename, instead.
}

// helper for renameDir, not used for redirections from KIO::listDir().
void KCoreDirListerCache::emitRedirections(const QUrl &_oldUrl, const QUrl &_newUrl)
{
    qCDebug(KIO_CORE_DIRLISTER) << _oldUrl << "->" << _newUrl;
    const QUrl oldUrl = _oldUrl.adjusted(QUrl::StripTrailingSlash);
    const QUrl newUrl = _newUrl.adjusted(QUrl::StripTrailingSlash);

    KIO::ListJob *job = jobForUrl(oldUrl);
    if (job) {
        killJob(job);
    }

    // Check if we were listing this dir. Need to abort and restart with new name in that case.
    DirectoryDataHash::iterator dit = directoryData.find(oldUrl);
    if (dit == directoryData.end()) {
        return;
    }
    const QList<KCoreDirLister *> listers = (*dit).listersCurrentlyListing;
    const QList<KCoreDirLister *> holders = (*dit).listersCurrentlyHolding;

    KCoreDirListerCacheDirectoryData &newDirData = directoryData[newUrl];

    // Tell the world that the job listing the old url is dead.
    for (KCoreDirLister *kdl : listers) {
        if (job) {
            kdl->d->jobDone(job);
        }
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT kdl->canceled(oldUrl);
#endif
        Q_EMIT kdl->listingDirCanceled(oldUrl);
    }
    newDirData.listersCurrentlyListing += listers;

    // Check if we are currently displaying this directory (odds opposite wrt above)
    for (KCoreDirLister *kdl : holders) {
        if (job) {
            kdl->d->jobDone(job);
        }
    }
    newDirData.listersCurrentlyHolding += holders;
    directoryData.erase(dit);

    if (!listers.isEmpty()) {
        updateDirectory(newUrl);

        // Tell the world about the new url
        for (KCoreDirLister *kdl : listers) {
            Q_EMIT kdl->started(newUrl);
        }
    }

    // And notify the dirlisters of the redirection
    for (KCoreDirLister *kdl : holders) {
        kdl->d->redirect(oldUrl, newUrl, true /*keep items*/);
    }
}

void KCoreDirListerCache::removeDirFromCache(const QUrl &dir)
{
    qCDebug(KIO_CORE_DIRLISTER) << dir;
    const QList<QUrl> cachedDirs = itemsCached.keys(); // seems slow, but there's no qcache iterator...
    for (const QUrl &cachedDir : cachedDirs) {
        if (dir == cachedDir || dir.isParentOf(cachedDir)) {
            itemsCached.remove(cachedDir);
        }
    }
}

void KCoreDirListerCache::slotUpdateEntries(KIO::Job *job, const KIO::UDSEntryList &list)
{
    runningListJobs[static_cast<KIO::ListJob *>(job)] += list;
}

void KCoreDirListerCache::slotUpdateResult(KJob *j)
{
    Q_ASSERT(j);
    KIO::ListJob *job = static_cast<KIO::ListJob *>(j);

    QUrl jobUrl(joburl(job));
    jobUrl = jobUrl.adjusted(QUrl::StripTrailingSlash);  // need remove trailing slashes again, in case of redirections

    qCDebug(KIO_CORE_DIRLISTER) << "finished update" << jobUrl;

    KCoreDirListerCacheDirectoryData &dirData = directoryData[jobUrl];
    // Collect the dirlisters which were listing the URL using that ListJob
    // plus those that were already holding that URL - they all get updated.
    dirData.moveListersWithoutCachedItemsJob(jobUrl);
    const QList<KCoreDirLister *> listers = dirData.listersCurrentlyHolding +
                                            dirData.listersCurrentlyListing;

    // once we are updating dirs that are only in the cache this will fail!
    Q_ASSERT(!listers.isEmpty());

    if (job->error()) {
        for (KCoreDirLister *kdl : listers) {
            kdl->d->jobDone(job);

            //don't bother the user
            //kdl->handleError( job );

            const bool silent = job->property("_kdlc_silent").toBool();
            if (!silent) {
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
                Q_EMIT kdl->canceled(jobUrl);
#endif
                Q_EMIT kdl->listingDirCanceled(jobUrl);
            }
            if (kdl->d->numJobs() == 0) {
                kdl->d->complete = true;
                if (!silent) {
                    Q_EMIT kdl->canceled();
                }
            }
        }

        runningListJobs.remove(job);

        // TODO: if job is a parent of one or more
        // of the pending urls we should cancel them
        processPendingUpdates();
        return;
    }

    DirItem *dir = itemsInUse.value(jobUrl, nullptr);
    if (!dir) {
        qCWarning(KIO_CORE) << "Internal error: itemsInUse did not contain" << jobUrl;
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT(dir);
    } else {
        dir->complete = true;
    }

    // check if anyone wants the MIME types immediately
    bool delayedMimeTypes = true;
    for (const KCoreDirLister *kdl : listers) {
        delayedMimeTypes &= kdl->d->delayedMimeTypes;
    }

    typedef QHash<QString, KFileItem> FileItemHash; // fileName -> KFileItem
    FileItemHash fileItems;

    // Fill the hash from the old list of items. We'll remove entries as we see them
    // in the new listing, and the resulting hash entries will be the deleted items.
    for (const KFileItem &item : qAsConst(dir->lstItems)) {
        fileItems.insert(item.name(), item);
    }

    QSet<QString> filesToHide;
    bool dotHiddenChecked = false;
    const KIO::UDSEntryList &buf = runningListJobs.value(job);
    KIO::UDSEntryList::const_iterator it = buf.constBegin();
    const KIO::UDSEntryList::const_iterator end = buf.constEnd();
    for (; it != end; ++it) {
        // Form the complete url
        KFileItem item(*it, jobUrl, delayedMimeTypes, true);

        const QString name = item.name();
        Q_ASSERT(!name.isEmpty()); // A kioslave setting an empty UDS_NAME is utterly broken, fix the kioslave!

        // we duplicate the check for dotdot here, to avoid iterating over
        // all items again and checking in matchesFilter() that way.
        if (name.isEmpty() || name == QLatin1String("..")) {
            continue;
        }

        if (name == QLatin1Char('.')) {
            // if the update was started before finishing the original listing
            // there is no root item yet
            if (dir->rootItem.isNull()) {
                dir->rootItem = item;

                for (KCoreDirLister *kdl : listers) {
                    if (kdl->d->rootFileItem.isNull() && kdl->d->url == jobUrl) {
                        kdl->d->rootFileItem = dir->rootItem;
                    }
                }
            }
            continue;
        } else {
            // get the names of the files listed in ".hidden", if it exists and is a local file
            if (!dotHiddenChecked) {
                const QString localPath = item.localPath();
                if (!localPath.isEmpty()) {
                    const QString rootItemPath = QFileInfo(localPath).absolutePath();
                    filesToHide = filesInDotHiddenForDir(rootItemPath);
                }
                dotHiddenChecked = true;
            }
        }

        // hide file if listed in ".hidden"
        if (filesToHide.contains(name)) {
            item.setHidden();
        }

        // Find this item
        FileItemHash::iterator fiit = fileItems.find(item.name());
        if (fiit != fileItems.end()) {
            const KFileItem tmp = fiit.value();
            QSet<KFileItem>::iterator pru_it = pendingRemoteUpdates.find(tmp);
            const bool inPendingRemoteUpdates = (pru_it != pendingRemoteUpdates.end());

            // check if something changed for this file, using KFileItem::cmp()
            if (!tmp.cmp(item) || inPendingRemoteUpdates) {

                if (inPendingRemoteUpdates) {
                    pendingRemoteUpdates.erase(pru_it);
                }

                qCDebug(KIO_CORE_DIRLISTER) << "file changed:" << tmp.name();

                reinsert(item, tmp.url());
                for (KCoreDirLister *kdl : listers) {
                    kdl->d->addRefreshItem(jobUrl, tmp, item);
                }
            }
            // Seen, remove
            fileItems.erase(fiit);
        } else { // this is a new file
            qCDebug(KIO_CORE_DIRLISTER) << "new file:" << name;
            dir->insert(item);

            for (KCoreDirLister *kdl : listers) {
                kdl->d->addNewItem(jobUrl, item);
            }
        }
    }

    runningListJobs.remove(job);

    if (!fileItems.isEmpty()) {
        deleteUnmarkedItems(listers, dir->lstItems, fileItems);
    }

    for (KCoreDirLister *kdl : listers) {
        kdl->d->emitItems();

        kdl->d->jobDone(job);
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
        Q_EMIT kdl->completed(jobUrl);
#endif
        Q_EMIT kdl->listingDirCompleted(jobUrl);
        if (kdl->d->numJobs() == 0) {
            kdl->d->complete = true;
            Q_EMIT kdl->completed();
        }
    }

    // TODO: hmm, if there was an error and job is a parent of one or more
    // of the pending urls we should cancel it/them as well
    processPendingUpdates();

    if (job->property("need_another_update").toBool()) {
        updateDirectory(jobUrl);
    }
}

// private

KIO::ListJob *KCoreDirListerCache::jobForUrl(const QUrl &url, KIO::ListJob *not_job)
{
    auto it = runningListJobs.constBegin();
    while (it != runningListJobs.constEnd()) {
        KIO::ListJob *job = it.key();
        const QUrl jobUrl = joburl(job).adjusted(QUrl::StripTrailingSlash);

        if (jobUrl == url && job != not_job) {
            return job;
        }
        ++it;
    }
    return nullptr;
}

const QUrl &KCoreDirListerCache::joburl(KIO::ListJob *job)
{
    if (job->redirectionUrl().isValid()) {
        return job->redirectionUrl();
    } else {
        return job->url();
    }
}

void KCoreDirListerCache::killJob(KIO::ListJob *job)
{
    runningListJobs.remove(job);
    job->disconnect(this);
    job->kill();
}

void KCoreDirListerCache::deleteUnmarkedItems(const QList<KCoreDirLister *> &listers, QList<KFileItem> &lstItems, const QHash<QString, KFileItem> &itemsToDelete)
{
    // Make list of deleted items (for emitting)
    KFileItemList deletedItems;
    QHashIterator<QString, KFileItem> kit(itemsToDelete);
    while (kit.hasNext()) {
        const KFileItem item = kit.next().value();
        deletedItems.append(item);
        qCDebug(KIO_CORE_DIRLISTER) << "deleted:" << item.name() << item;
    }

    // Delete all remaining items
    QMutableListIterator<KFileItem> it(lstItems);
    while (it.hasNext()) {
        if (itemsToDelete.contains(it.next().name())) {
            it.remove();
         }
     }
    itemsDeleted(listers, deletedItems);
}

void KCoreDirListerCache::itemsDeleted(const QList<KCoreDirLister *> &listers, const KFileItemList &deletedItems)
{
    for (KCoreDirLister *kdl : listers) {
        kdl->d->emitItemsDeleted(deletedItems);
    }

    for (const KFileItem &item : deletedItems) {
        if (item.isDir()) {
            deleteDir(item.url());
        }
    }
}

void KCoreDirListerCache::deleteDir(const QUrl &_dirUrl)
{
    qCDebug(KIO_CORE_DIRLISTER) << _dirUrl;
    // unregister and remove the children of the deleted item.
    // Idea: tell all the KCoreDirListers that they should forget the dir
    //       and then remove it from the cache.

    QUrl dirUrl(_dirUrl.adjusted(QUrl::StripTrailingSlash));

    // Separate itemsInUse iteration and calls to forgetDirs (which modify itemsInUse)
    QList<QUrl> affectedItems;

    auto itu = itemsInUse.begin();
    const auto ituend = itemsInUse.end();
    for (; itu != ituend; ++itu) {
        const QUrl &deletedUrl = itu.key();
        if (dirUrl == deletedUrl || dirUrl.isParentOf(deletedUrl)) {
            affectedItems.append(deletedUrl);
        }
    }

    for (const QUrl &deletedUrl : qAsConst(affectedItems)) {
        // stop all jobs for deletedUrlStr
        DirectoryDataHash::iterator dit = directoryData.find(deletedUrl);
        if (dit != directoryData.end()) {
            // we need a copy because stop modifies the list
            const QList<KCoreDirLister *> listers = (*dit).listersCurrentlyListing;
            for (KCoreDirLister *kdl : listers) {
                stopListingUrl(kdl, deletedUrl);
            }
            // tell listers holding deletedUrl to forget about it
            // this will stop running updates for deletedUrl as well

            // we need a copy because forgetDirs modifies the list
            const QList<KCoreDirLister *> holders = (*dit).listersCurrentlyHolding;
            for (KCoreDirLister *kdl : holders) {
                // lister's root is the deleted item
                if (kdl->d->url == deletedUrl) {
                    // tell the view first. It might need the subdirs' items (which forgetDirs will delete)
                    if (!kdl->d->rootFileItem.isNull()) {
                        Q_EMIT kdl->itemsDeleted(KFileItemList{kdl->d->rootFileItem});
                    }
                    forgetDirs(kdl);
                    kdl->d->rootFileItem = KFileItem();
                } else {
                    const bool treeview = kdl->d->lstDirs.count() > 1;
                    if (!treeview) {
                        Q_EMIT kdl->clear();
                        kdl->d->lstDirs.clear();
                    } else {
                        kdl->d->lstDirs.removeAll(deletedUrl);
                    }

                    forgetDirs(kdl, deletedUrl, treeview);
                }
            }
        }

        // delete the entry for deletedUrl - should not be needed, it's in
        // items cached now
        int count = itemsInUse.remove(deletedUrl);
        Q_ASSERT(count == 0);
        Q_UNUSED(count);   //keep gcc "unused variable" complaining quiet when in release mode
    }

    // remove the children from the cache
    removeDirFromCache(dirUrl);
}

// delayed updating of files, FAM is flooding us with events
void KCoreDirListerCache::processPendingUpdates()
{
    QSet<KCoreDirLister *> listers;
    for (const QString &file : qAsConst(pendingUpdates)) { // always a local path
        qCDebug(KIO_CORE_DIRLISTER) << file;
        QUrl u = QUrl::fromLocalFile(file);
        KFileItem item = findByUrl(nullptr, u);   // search all items
        if (!item.isNull()) {
            // we need to refresh the item, because e.g. the permissions can have changed.
            KFileItem oldItem = item;
            item.refresh();

            if (!oldItem.cmp(item)) {
                reinsert(item, oldItem.url());
                listers |= emitRefreshItem(oldItem, item);
            }
        }
    }
    pendingUpdates.clear();
    for (KCoreDirLister *kdl : qAsConst(listers)) {
        kdl->d->emitItems();
    }

    // Directories in need of updating
    for (const QString &dir : qAsConst(pendingDirectoryUpdates)) {
        updateDirectory(QUrl::fromLocalFile(dir));
    }
    pendingDirectoryUpdates.clear();
}

#ifndef NDEBUG
void KCoreDirListerCache::printDebug()
{
    qCDebug(KIO_CORE_DIRLISTER) << "Items in use:";
    auto itu = itemsInUse.constBegin();
    const auto ituend = itemsInUse.constEnd();
    for (; itu != ituend; ++itu) {
        qCDebug(KIO_CORE_DIRLISTER) << "   " << itu.key() << "URL:" << itu.value()->url
                 << "rootItem:" << (!itu.value()->rootItem.isNull() ? itu.value()->rootItem.url() : QUrl())
                 << "autoUpdates refcount:" << itu.value()->autoUpdates
                 << "complete:" << itu.value()->complete
                 << QStringLiteral("with %1 items.").arg(itu.value()->lstItems.count());
    }

    QList<KCoreDirLister *> listersWithoutJob;
    qCDebug(KIO_CORE_DIRLISTER) << "Directory data:";
    DirectoryDataHash::const_iterator dit = directoryData.constBegin();
    for (; dit != directoryData.constEnd(); ++dit) {
        QString list;
        const QList<KCoreDirLister *> listers = (*dit).listersCurrentlyListing;
        for (KCoreDirLister *listit : listers) {
            list += QLatin1String(" 0x") + QString::number(reinterpret_cast<qlonglong>(listit), 16);
        }
        qCDebug(KIO_CORE_DIRLISTER) << "  " << dit.key() << listers.count() << "listers:" << list;
        for (KCoreDirLister *listit : listers) {
            if (!listit->d->m_cachedItemsJobs.isEmpty()) {
                qCDebug(KIO_CORE_DIRLISTER) << "  Lister" << listit << "has CachedItemsJobs" << listit->d->m_cachedItemsJobs;
            } else if (KIO::ListJob *listJob = jobForUrl(dit.key())) {
                qCDebug(KIO_CORE_DIRLISTER) << "  Lister" << listit << "has ListJob" << listJob;
            } else {
                listersWithoutJob.append(listit);
            }
        }

        list.clear();
        const QList<KCoreDirLister *> holders = (*dit).listersCurrentlyHolding;
        for (KCoreDirLister *listit : holders) {
            list += QLatin1String(" 0x") + QString::number(reinterpret_cast<qlonglong>(listit), 16);
        }
        qCDebug(KIO_CORE_DIRLISTER) << "  " << dit.key() << holders.count() << "holders:" << list;
    }

    QMap< KIO::ListJob *, KIO::UDSEntryList >::Iterator jit = runningListJobs.begin();
    qCDebug(KIO_CORE_DIRLISTER) << "Jobs:";
    for (; jit != runningListJobs.end(); ++jit) {
        qCDebug(KIO_CORE_DIRLISTER) << "   " << jit.key() << "listing" << joburl(jit.key()) << ":" << (*jit).count() << "entries.";
    }

    qCDebug(KIO_CORE_DIRLISTER) << "Items in cache:";
    const QList<QUrl> cachedDirs = itemsCached.keys();
    for (const QUrl &cachedDir : cachedDirs) {
        DirItem *dirItem = itemsCached.object(cachedDir);
        qCDebug(KIO_CORE_DIRLISTER) << "   " << cachedDir << "rootItem:"
                 << (!dirItem->rootItem.isNull() ? dirItem->rootItem.url().toString() : QStringLiteral("NULL"))
                 << "with" << dirItem->lstItems.count() << "items.";
    }

    // Abort on listers without jobs -after- showing the full dump. Easier debugging.
    for (KCoreDirLister *listit : qAsConst(listersWithoutJob)) {
        qCWarning(KIO_CORE) << "Fatal Error: HUH? Lister" << listit << "is supposed to be listing, but has no job!";
        abort();
    }
}
#endif

KCoreDirLister::KCoreDirLister(QObject *parent)
    : QObject(parent), d(new KCoreDirListerPrivate(this))
{
    qCDebug(KIO_CORE_DIRLISTER) << "+KCoreDirLister";

    d->complete = true;

    setAutoUpdate(true);
    setDirOnlyMode(false);
    setShowingDotFiles(false);
}

KCoreDirLister::~KCoreDirLister()
{
    qCDebug(KIO_CORE_DIRLISTER) << "~KCoreDirLister" << this;

    // Stop all running jobs, remove lister from lists
    if (!kDirListerCache.isDestroyed()) {
        stop();
        kDirListerCache()->forgetDirs(this);
    }
}

bool KCoreDirLister::openUrl(const QUrl &_url, OpenUrlFlags _flags)
{
    // emit the current changes made to avoid an inconsistent treeview
    if (d->hasPendingChanges && (_flags & Keep)) {
        emitChanges();
    }

    d->hasPendingChanges = false;

    return kDirListerCache()->listDir(this, _url, _flags & Keep, _flags & Reload);
}

void KCoreDirLister::stop()
{
    kDirListerCache()->stop(this);
}

void KCoreDirLister::stop(const QUrl &_url)
{
    kDirListerCache()->stopListingUrl(this, _url);
}

bool KCoreDirLister::autoUpdate() const
{
    return d->autoUpdate;
}

void KCoreDirLister::setAutoUpdate(bool enable)
{
    if (d->autoUpdate == enable) {
        return;
    }

    d->autoUpdate = enable;
    kDirListerCache()->setAutoUpdate(this, enable);
}

bool KCoreDirLister::showingDotFiles() const
{
    return d->settings.isShowingDotFiles;
}

void KCoreDirLister::setShowingDotFiles(bool showDotFiles)
{
    if (d->settings.isShowingDotFiles == showDotFiles) {
        return;
    }

    d->prepareForSettingsChange();
    d->settings.isShowingDotFiles = showDotFiles;
}

bool KCoreDirLister::dirOnlyMode() const
{
    return d->settings.dirOnlyMode;
}

void KCoreDirLister::setDirOnlyMode(bool dirsOnly)
{
    if (d->settings.dirOnlyMode == dirsOnly) {
        return;
    }

    d->prepareForSettingsChange();
    d->settings.dirOnlyMode = dirsOnly;
}

QUrl KCoreDirLister::url() const
{
    return d->url;
}

QList<QUrl> KCoreDirLister::directories() const
{
    return d->lstDirs;
}

void KCoreDirLister::emitChanges()
{
    d->emitChanges();
}

void KCoreDirListerPrivate::emitChanges()
{
    if (!hasPendingChanges) {
        return;
    }

    // reset 'hasPendingChanges' now, in case of recursion
    // (testcase: enabling recursive scan in ktorrent, #174920)
    hasPendingChanges = false;

    const KCoreDirListerPrivate::FilterSettings newSettings = settings;
    settings = oldSettings; // temporarily

    // Fill hash with all items that are currently visible
    QSet<QString> oldVisibleItems;
    for (const QUrl &dir : qAsConst(lstDirs)) {
        const QList<KFileItem> *itemList = kDirListerCache()->itemsForDir(dir);
        if (!itemList) {
            continue;
        }

        for (const KFileItem &item : *itemList) {
            if (isItemVisible(item) && q->matchesMimeFilter(item)) {
                oldVisibleItems.insert(item.name());
            }
        }
    }

    settings = newSettings;

    const QList<QUrl> dirs = lstDirs;
    for (const QUrl &dir : dirs) {
        KFileItemList deletedItems;

        const QList<KFileItem> *itemList = kDirListerCache()->itemsForDir(dir);
        if (!itemList) {
            continue;
        }

        auto kit = itemList->begin();
        const auto kend = itemList->end();
        for (; kit != kend; ++kit) {
            const KFileItem &item = *kit;
            const QString text = item.text();
            if (text == QLatin1Char('.') || text == QLatin1String("..")) {
                continue;
            }
            const bool wasVisible = oldVisibleItems.contains(item.name());
            const bool nowVisible = isItemVisible(item) && q->matchesMimeFilter(item);
            if (nowVisible && !wasVisible) {
                addNewItem(dir, item);    // takes care of emitting newItem or itemsFilteredByMime
            } else if (!nowVisible && wasVisible) {
                deletedItems.append(*kit);
            }
        }
        if (!deletedItems.isEmpty()) {
            Q_EMIT q->itemsDeleted(deletedItems);
        }
        emitItems();
    }
    oldSettings = settings;
}

void KCoreDirLister::updateDirectory(const QUrl &dirUrl)
{
    kDirListerCache()->updateDirectory(dirUrl);
}

bool KCoreDirLister::isFinished() const
{
    return d->complete;
}

KFileItem KCoreDirLister::rootItem() const
{
    return d->rootFileItem;
}

KFileItem KCoreDirLister::findByUrl(const QUrl &url) const
{
    return kDirListerCache()->findByUrl(this, url);
}

KFileItem KCoreDirLister::findByName(const QString &name) const
{
    return kDirListerCache()->findByName(this, name);
}

// ================ public filter methods ================ //

void KCoreDirLister::setNameFilter(const QString &nameFilter)
{
    if (d->nameFilter == nameFilter) {
        return;
    }

    d->prepareForSettingsChange();

    d->settings.lstFilters.clear();
    d->nameFilter = nameFilter;
    // Split on white space
    const QStringList list = nameFilter.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &filter : list) {
        d->settings.lstFilters.append(
            QRegularExpression(QRegularExpression::wildcardToRegularExpression(filter),
                               QRegularExpression::CaseInsensitiveOption));
    }
}

QString KCoreDirLister::nameFilter() const
{
    return d->nameFilter;
}

void KCoreDirLister::setMimeFilter(const QStringList &mimeFilter)
{
    if (d->settings.mimeFilter == mimeFilter) {
        return;
    }

    d->prepareForSettingsChange();
    if (mimeFilter.contains(QLatin1String("application/octet-stream")) || mimeFilter.contains(QLatin1String("all/allfiles"))) { // all files
        d->settings.mimeFilter.clear();
    } else {
        d->settings.mimeFilter = mimeFilter;
    }
}

void KCoreDirLister::setMimeExcludeFilter(const QStringList &mimeExcludeFilter)
{
    if (d->settings.mimeExcludeFilter == mimeExcludeFilter) {
        return;
    }

    d->prepareForSettingsChange();
    d->settings.mimeExcludeFilter = mimeExcludeFilter;
}

void KCoreDirLister::clearMimeFilter()
{
    d->prepareForSettingsChange();
    d->settings.mimeFilter.clear();
    d->settings.mimeExcludeFilter.clear();
}

QStringList KCoreDirLister::mimeFilters() const
{
    return d->settings.mimeFilter;
}

bool KCoreDirLister::matchesFilter(const QString &name) const
{
    return std::any_of(d->settings.lstFilters.cbegin(), d->settings.lstFilters.cend(), [&name](const QRegularExpression &filter) {
        return filter.match(name).hasMatch();
    });
}

bool KCoreDirLister::matchesMimeFilter(const QString &mime) const
{
    return doMimeFilter(mime, d->settings.mimeFilter) &&
           d->doMimeExcludeFilter(mime, d->settings.mimeExcludeFilter);
}

// ================ protected methods ================ //

bool KCoreDirLister::matchesFilter(const KFileItem &item) const
{
    Q_ASSERT(!item.isNull());

    if (item.text() == QLatin1String("..")) {
        return false;
    }

    if (!d->settings.isShowingDotFiles && item.isHidden()) {
        return false;
    }

    if (item.isDir() || d->settings.lstFilters.isEmpty()) {
        return true;
    }

    return matchesFilter(item.text());
}

bool KCoreDirLister::matchesMimeFilter(const KFileItem &item) const
{
    Q_ASSERT(!item.isNull());
    // Don't lose time determining the MIME type if there is no filter
    if (d->settings.mimeFilter.isEmpty() && d->settings.mimeExcludeFilter.isEmpty()) {
        return true;
    }
    return matchesMimeFilter(item.mimetype());
}

bool KCoreDirLister::doNameFilter(const QString &name, const QList<QRegExp> &filters) const
{
    return std::any_of(filters.cbegin(), filters.cend(), [&name](const QRegExp &filter) {
        return filter.exactMatch(name);
    });
}

bool KCoreDirLister::doMimeFilter(const QString &mime, const QStringList &filters) const
{
    if (filters.isEmpty()) {
        return true;
    }

    QMimeDatabase db;
    const QMimeType mimeptr = db.mimeTypeForName(mime);
    if (!mimeptr.isValid()) {
        return false;
    }

    qCDebug(KIO_CORE_DIRLISTER) << "doMimeFilter: investigating:" << mimeptr.name();
    return std::any_of(filters.cbegin(), filters.cend(), [&mimeptr](const QString &filter) {
        return mimeptr.inherits(filter);
    });
}

bool KCoreDirListerPrivate::doMimeExcludeFilter(const QString &mime, const QStringList &filters) const
{
    return !std::any_of(filters.cbegin(), filters.cend(), [&mime](const QString &filter) {
        return mime == filter;
    });
}

void KCoreDirLister::handleError(KIO::Job *job)
{
    qCWarning(KIO_CORE) << job->errorString();
}

void KCoreDirLister::handleErrorMessage(const QString &message)
{
    qCWarning(KIO_CORE) << message;
}

// ================= private methods ================= //

void KCoreDirListerPrivate::addNewItem(const QUrl &directoryUrl, const KFileItem &item)
{
    if (!isItemVisible(item)) {
        return;    // No reason to continue... bailing out here prevents a MIME type scan.
    }

    qCDebug(KIO_CORE_DIRLISTER) << "in" << directoryUrl << "item:" << item.url();

    if (q->matchesMimeFilter(item)) {
        Q_ASSERT(!item.isNull());
        lstNewItems[directoryUrl].append(item);              // items not filtered
    } else {
        Q_ASSERT(!item.isNull());
        lstMimeFilteredItems.append(item);     // only filtered by MIME type
    }
}

void KCoreDirListerPrivate::addNewItems(const QUrl &directoryUrl, const QList<KFileItem> &items)
{
    // TODO: make this faster - test if we have a filter at all first
    // DF: was this profiled? The matchesFoo() functions should be fast, w/o filters...
    // Of course if there is no filter and we can do a range-insertion instead of a loop, that might be good.
    auto kit = items.cbegin();
    const auto kend = items.cend();
    for (; kit != kend; ++kit) {
        addNewItem(directoryUrl, *kit);
    }
}

void KCoreDirListerPrivate::addRefreshItem(const QUrl &directoryUrl, const KFileItem &oldItem, const KFileItem &item)
{
    const bool refreshItemWasFiltered = !isItemVisible(oldItem) ||
                                        !q->matchesMimeFilter(oldItem);
    if (isItemVisible(item) && q->matchesMimeFilter(item)) {
        if (refreshItemWasFiltered) {
            Q_ASSERT(!item.isNull());
            lstNewItems[directoryUrl].append(item);
        } else {
            Q_ASSERT(!item.isNull());
            lstRefreshItems.append(qMakePair(oldItem, item));
        }
    } else if (!refreshItemWasFiltered) {
        // notify the user that the MIME type of a file changed that doesn't match
        // a filter or does match an exclude filter
        // This also happens when renaming foo to .foo and dot files are hidden (#174721)
        Q_ASSERT(!oldItem.isNull());
        lstRemoveItems.append(oldItem);
    }
}

void KCoreDirListerPrivate::emitItems()
{
    if (!lstNewItems.empty()) {
        QHashIterator<QUrl, KFileItemList> it(lstNewItems);
        while (it.hasNext()) {
            it.next();
            Q_EMIT q->itemsAdded(it.key(), it.value());
            Q_EMIT q->newItems(it.value()); // compat
        }
        lstNewItems.clear();
    }

    if (!lstMimeFilteredItems.empty()) {
        Q_EMIT q->itemsFilteredByMime(lstMimeFilteredItems);
        lstMimeFilteredItems.clear();
    }

    if (!lstRefreshItems.empty()) {
        Q_EMIT q->refreshItems(lstRefreshItems);
        lstRefreshItems.clear();
    }

    if (!lstRemoveItems.empty()) {
        Q_EMIT q->itemsDeleted(lstRemoveItems);
        lstRemoveItems.clear();
    }
}

bool KCoreDirListerPrivate::isItemVisible(const KFileItem &item) const
{
    // Note that this doesn't include MIME type filters, because
    // of the itemsFilteredByMime signal. Filtered-by-MIME-type items are
    // considered "visible", they are just visible via a different signal...
    return (!settings.dirOnlyMode || item.isDir())
           && q->matchesFilter(item);
}

void KCoreDirListerPrivate::emitItemsDeleted(const KFileItemList &itemsList)
{
    KFileItemList items = itemsList;
    QMutableListIterator<KFileItem> it(items);
    while (it.hasNext()) {
        const KFileItem &item = it.next();
        if (!isItemVisible(item) || !q->matchesMimeFilter(item)) {
            it.remove();
        }
    }
    if (!items.isEmpty()) {
        Q_EMIT q->itemsDeleted(items);
    }
}

// ================ private slots ================ //

void KCoreDirListerPrivate::slotInfoMessage(KJob *, const QString &message)
{
    Q_EMIT q->infoMessage(message);
}

void KCoreDirListerPrivate::slotPercent(KJob *job, unsigned long pcnt)
{
    jobData[static_cast<KIO::ListJob *>(job)].percent = pcnt;

    int result = 0;

    KIO::filesize_t size = 0;

    auto dataIt = jobData.cbegin();
    while (dataIt != jobData.cend()) {
        const auto data = dataIt.value();
        result += data.percent * data.totalSize;
        size += data.totalSize;
        ++dataIt;
    }

    if (size != 0) {
        result /= size;
    } else {
        result = 100;
    }
    Q_EMIT q->percent(result);
}

void KCoreDirListerPrivate::slotTotalSize(KJob *job, qulonglong size)
{
    jobData[static_cast<KIO::ListJob *>(job)].totalSize = size;

    KIO::filesize_t result = 0;
    auto dataIt = jobData.cbegin();
    while (dataIt != jobData.cend()) {
        result += dataIt.value().totalSize;
        ++dataIt;
    }

    Q_EMIT q->totalSize(result);
}

void KCoreDirListerPrivate::slotProcessedSize(KJob *job, qulonglong size)
{
    jobData[static_cast<KIO::ListJob *>(job)].processedSize = size;

    KIO::filesize_t result = 0;
    auto dataIt = jobData.cbegin();
    while (dataIt != jobData.cend()) {
        result += dataIt.value().processedSize;
        ++dataIt;
    }

    Q_EMIT q->processedSize(result);
}

void KCoreDirListerPrivate::slotSpeed(KJob *job, unsigned long spd)
{
    jobData[static_cast<KIO::ListJob *>(job)].speed = spd;

    int result = 0;
    auto dataIt = jobData.cbegin();
    while (dataIt != jobData.cend()) {
        result += dataIt.value().speed;
        ++dataIt;
    }

    Q_EMIT q->speed(result);
}

uint KCoreDirListerPrivate::numJobs()
{
#ifdef DEBUG_CACHE
    // This code helps detecting stale entries in the jobData map.
    qCDebug(KIO_CORE_DIRLISTER) << q << "numJobs:" << jobData.count();
    QMapIterator<KIO::ListJob *, JobData> it(jobData);
    while (it.hasNext()) {
        it.next();
        qCDebug(KIO_CORE_DIRLISTER) << (void*)it.key();
        qCDebug(KIO_CORE_DIRLISTER) << it.key();
    }
#endif

    return jobData.count();
}

void KCoreDirListerPrivate::jobDone(KIO::ListJob *job)
{
    jobData.remove(job);
}

void KCoreDirLister::jobStarted(KIO::ListJob *job)
{
    KCoreDirListerPrivate::JobData data;
    data.speed = 0;
    data.percent = 0;
    data.processedSize = 0;
    data.totalSize = 0;

    d->jobData.insert(job, data);
    d->complete = false;
}

void KCoreDirListerPrivate::connectJob(KIO::ListJob *job)
{
    q->connect(job, &KJob::infoMessage,
               q, [this](KJob *job, const QString &plain) { slotInfoMessage(job, plain); });
    q->connect(job, QOverload<KJob*, ulong>::of(&KJob::percent),
               q, [this](KJob *job, ulong _percent) { slotPercent(job, _percent); });
    q->connect(job, &KJob::totalSize,
               q, [this](KJob *job, qulonglong _size) { slotTotalSize(job, _size); });
    q->connect(job, &KJob::processedSize,
               q, [this](KJob *job, qulonglong _psize) { slotProcessedSize(job, _psize); });
    q->connect(job, &KJob::speed,
               q, [this](KJob *job, qulonglong _speed) { slotSpeed(job, _speed); });
}

KFileItemList KCoreDirLister::items(WhichItems which) const
{
    return itemsForDir(url(), which);
}

KFileItemList KCoreDirLister::itemsForDir(const QUrl &dir, WhichItems which) const
{
    QList<KFileItem> *allItems = kDirListerCache()->itemsForDir(dir);
    KFileItemList result;
    if (!allItems) {
        return result;
    }

    if (which == AllItems) {
        return KFileItemList(*allItems);
    } else { // only items passing the filters
        auto kit = allItems->constBegin();
        const auto kend = allItems->constEnd();
        for (; kit != kend; ++kit) {
            const KFileItem &item = *kit;
            if (d->isItemVisible(item) && matchesMimeFilter(item)) {
                result.append(item);
            }
        }
    }
    return result;
}

bool KCoreDirLister::delayedMimeTypes() const
{
    return d->delayedMimeTypes;
}

void KCoreDirLister::setDelayedMimeTypes(bool delayedMimeTypes)
{
    d->delayedMimeTypes = delayedMimeTypes;
}

// called by KCoreDirListerCache::slotRedirection
void KCoreDirListerPrivate::redirect(const QUrl &oldUrl, const QUrl &newUrl, bool keepItems)
{
    if (url.matches(oldUrl, QUrl::StripTrailingSlash)) {
        if (!keepItems) {
            rootFileItem = KFileItem();
        } else {
            rootFileItem.setUrl(newUrl);
        }
        url = newUrl;
    }

    const int idx = lstDirs.indexOf(oldUrl);
    if (idx == -1) {
        qCWarning(KIO_CORE) << "Unexpected redirection from" << oldUrl << "to" << newUrl
                   << "but this dirlister is currently listing/holding" << lstDirs;
    } else {
        lstDirs[ idx ] = newUrl;
    }

    if (lstDirs.count() == 1) {
        if (!keepItems) {
            Q_EMIT q->clear();
        }
        Q_EMIT q->redirection(newUrl);
    } else {
        if (!keepItems) {
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 79)
            Q_EMIT q->clear(oldUrl);
#endif
            Q_EMIT q->clearDir(oldUrl);
        }
    }
    Q_EMIT q->redirection(oldUrl, newUrl);
}

void KCoreDirListerCacheDirectoryData::moveListersWithoutCachedItemsJob(const QUrl &url)
{
    // Move dirlisters from listersCurrentlyListing to listersCurrentlyHolding,
    // but not those that are still waiting on a CachedItemsJob...
    // Unit-testing note:
    // Run kdirmodeltest in valgrind to hit the case where an update
    // is triggered while a lister has a CachedItemsJob (different timing...)
    QMutableListIterator<KCoreDirLister *> lister_it(listersCurrentlyListing);
    while (lister_it.hasNext()) {
        KCoreDirLister *kdl = lister_it.next();
        if (!kdl->d->cachedItemsJobForUrl(url)) {
            // OK, move this lister from "currently listing" to "currently holding".

            // Huh? The KCoreDirLister was present twice in listersCurrentlyListing, or was in both lists?
            Q_ASSERT(!listersCurrentlyHolding.contains(kdl));
            if (!listersCurrentlyHolding.contains(kdl)) {
                listersCurrentlyHolding.append(kdl);
            }
            lister_it.remove();
        } else {
            qCDebug(KIO_CORE_DIRLISTER) << "Not moving" << kdl << "to listersCurrentlyHolding because it still has job" << kdl->d->m_cachedItemsJobs;
        }
    }
}

KFileItem KCoreDirLister::cachedItemForUrl(const QUrl &url)
{
    if (kDirListerCache.exists()) {
        return kDirListerCache()->itemForUrl(url);
    } else {
        return {};
    }
}

QSet<QString> KCoreDirListerCache::filesInDotHiddenForDir(const QString& dir)
{
    const QString path = dir + QLatin1String("/.hidden");
    QFile dotHiddenFile(path);

    if (dotHiddenFile.exists()) {
        const QDateTime mtime = QFileInfo(dotHiddenFile).lastModified();
        const CacheHiddenFile *cachedDotHiddenFile = m_cacheHiddenFiles.object(path);

        if (cachedDotHiddenFile && mtime <= cachedDotHiddenFile->mtime) {
            // ".hidden" is in cache and still valid (the file was not modified since then),
            // so return it
            return cachedDotHiddenFile->listedFiles;
        } else {
            // read the ".hidden" file, then cache it and return it
            if (dotHiddenFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QSet<QString> filesToHide;
                QTextStream stream(&dotHiddenFile);
                while (!stream.atEnd()) {
                    QString name = stream.readLine();
                    if (!name.isEmpty()) {
                        filesToHide.insert(name);
                    }
                }
                m_cacheHiddenFiles.insert(path, new CacheHiddenFile(mtime, filesToHide));
                return filesToHide;
            }
        }
    }

    return QSet<QString>();
}

#include "moc_kcoredirlister.cpp"
#include "moc_kcoredirlister_p.cpp"
