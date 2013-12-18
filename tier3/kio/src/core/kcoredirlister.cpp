/* This file is part of the KDE project
   Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
                 2000 Carsten Pfeiffer <pfeiffer@kde.org>
                 2003-2005 David Faure <faure@kde.org>
                 2001-2006 Michael Brade <brade@kde.org>

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

#include "kcoredirlister.h"
#include "kcoredirlister_p.h"

#include <klocalizedstring.h>
#include <kio/job.h>
#include "kprotocolmanager.h"
#include "kmountpoint.h"

#include <QtCore/QRegExp>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QLinkedList>
#include <qmimedatabase.h>
#include <QDebug>

// Enable this to get printDebug() called often, to see the contents of the cache
//#define DEBUG_CACHE

// Make really sure it doesn't get activated in the final build
#ifdef NDEBUG
#undef DEBUG_CACHE
#endif

Q_GLOBAL_STATIC(KCoreDirListerCache, kDirListerCache)

KCoreDirListerCache::KCoreDirListerCache()
    : itemsCached( 10 ) // keep the last 10 directories around
{
    //qDebug();

  connect( &pendingUpdateTimer, SIGNAL(timeout()), this, SLOT(processPendingUpdates()) );
  pendingUpdateTimer.setSingleShot( true );

  connect( KDirWatch::self(), SIGNAL(dirty(QString)),
           this, SLOT(slotFileDirty(QString)) );
  connect( KDirWatch::self(), SIGNAL(created(QString)),
           this, SLOT(slotFileCreated(QString)) );
  connect( KDirWatch::self(), SIGNAL(deleted(QString)),
           this, SLOT(slotFileDeleted(QString)) );

  kdirnotify = new org::kde::KDirNotify(QString(), QString(), QDBusConnection::sessionBus(), this);
  connect(kdirnotify, SIGNAL(FileRenamed(QString,QString)), SLOT(slotFileRenamed(QString,QString)));
  connect(kdirnotify, SIGNAL(FilesAdded(QString)), SLOT(slotFilesAdded(QString)));
  connect(kdirnotify, SIGNAL(FilesChanged(QStringList)), SLOT(slotFilesChanged(QStringList)));
  connect(kdirnotify, SIGNAL(FilesRemoved(QStringList)), SLOT(slotFilesRemoved(QStringList)));

  // Probably not needed in KF5 anymore:
  // The use of KUrl::url() in ~DirItem (sendSignal) crashes if the static for QRegExpEngine got deleted already,
  // so we need to destroy the KCoreDirListerCache before that.
  //qAddPostRoutine(kDirListerCache.destroy);
}

KCoreDirListerCache::~KCoreDirListerCache()
{
    //qDebug();

    qDeleteAll(itemsInUse);
    itemsInUse.clear();

    itemsCached.clear();
    directoryData.clear();

    if ( KDirWatch::exists() )
        KDirWatch::self()->disconnect( this );
}

// setting _reload to true will emit the old files and
// call updateDirectory
bool KCoreDirListerCache::listDir( KCoreDirLister *lister, const QUrl& _u,
                               bool _keep, bool _reload )
{
  QUrl _url(_u);
  _url.setPath(QDir::cleanPath(_url.path())); // kill consecutive slashes

  if (!_url.host().isEmpty() && KProtocolInfo::protocolClass(_url.scheme()) == ":local"
      && _url.scheme() != "file") {
      // ":local" protocols ignore the hostname, so strip it out preventively - #160057
      // kio_file is special cased since it does honor the hostname (by redirecting to e.g. smb)
      _url.setHost(QString());
      if (_keep == false)
          emit lister->redirection(_url);
  }

  // like this we don't have to worry about trailing slashes any further
  _url = _url.adjusted(QUrl::StripTrailingSlash);

  const QString urlStr = _url.toString();

  QString resolved;
  if (_url.isLocalFile()) {
      // Resolve symlinks (#213799)
      const QString local = _url.toLocalFile();
      resolved = QFileInfo(local).canonicalFilePath();
      if (local != resolved)
          canonicalUrls[QUrl::fromLocalFile(resolved)].append(_url);
      // TODO: remove entry from canonicalUrls again in forgetDirs
      // Note: this is why we use a QStringList value in there rather than a QSet:
      // we can just remove one entry and not have to worry about other dirlisters
      // (the non-unicity of the stringlist gives us the refcounting, basically).
  }

    if (!validUrl(lister, _url)) {
        //qDebug() << lister << "url=" << _url << "not a valid url";
        return false;
    }

    //qDebug() << lister << "url=" << _url << "keep=" << _keep << "reload=" << _reload;
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

        if (lister->d->url == _url)
            lister->d->rootFileItem = KFileItem();
    }

    lister->d->complete = false;

    lister->d->lstDirs.append(_url);

    if (lister->d->url.isEmpty() || !_keep) // set toplevel URL only if not set yet
        lister->d->url = _url;

    DirItem *itemU = itemsInUse.value(urlStr);

    KCoreDirListerCacheDirectoryData& dirData = directoryData[urlStr]; // find or insert

    if (dirData.listersCurrentlyListing.isEmpty()) {
        // if there is an update running for _url already we get into
        // the following case - it will just be restarted by updateDirectory().

        dirData.listersCurrentlyListing.append(lister);

        DirItem *itemFromCache = 0;
        if (itemU || (!_reload && (itemFromCache = itemsCached.take(urlStr)) ) ) {
            if (itemU) {
                //qDebug() << "Entry already in use:" << _url;
                // if _reload is set, then we'll emit cached items and then updateDirectory.
            } else {
                //qDebug() << "Entry in cache:" << _url;
                itemsInUse.insert(urlStr, itemFromCache);
                itemU = itemFromCache;
            }
            if (lister->d->autoUpdate) {
                itemU->incAutoUpdate();
            }
            if (itemFromCache && itemFromCache->watchedWhileInCache) {
                itemFromCache->watchedWhileInCache = false;;
                itemFromCache->decAutoUpdate();
            }

            emit lister->started(_url);

            // List items from the cache in a delayed manner, just like things would happen
            // if we were not using the cache.
            new KCoreDirLister::Private::CachedItemsJob(lister, _url, _reload);

        } else {
            // dir not in cache or _reload is true
            if (_reload) {
                //qDebug() << "Reloading directory:" << _url;
                itemsCached.remove(urlStr);
            } else {
                //qDebug() << "Listing directory:" << _url;
            }

            itemU = new DirItem(_url, resolved);
            itemsInUse.insert(urlStr, itemU);
            if (lister->d->autoUpdate)
                itemU->incAutoUpdate();

//        // we have a limit of MAX_JOBS_PER_LISTER concurrently running jobs
//        if ( lister->d->numJobs() >= MAX_JOBS_PER_LISTER )
//        {
//          pendingUpdates.insert( _url );
//        }
//        else
            {
                KIO::ListJob* job = KIO::listDir(_url, KIO::HideProgressInfo);
                runningListJobs.insert(job, KIO::UDSEntryList());

                lister->jobStarted(job);
                lister->d->connectJob(job);

                connect(job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
                        this, SLOT(slotEntries(KIO::Job*,KIO::UDSEntryList)));
                connect(job, SIGNAL(result(KJob*)),
                        this, SLOT(slotResult(KJob*)));
                connect(job, SIGNAL(redirection(KIO::Job*,QUrl)),
                        this, SLOT(slotRedirection(KIO::Job*,QUrl)));

                emit lister->started(_url);
            }
            //qDebug() << "Entry now being listed by" << dirData.listersCurrentlyListing;
        }
    } else {

        //qDebug() << "Entry currently being listed:" << _url << "by" << dirData.listersCurrentlyListing;
#ifdef DEBUG_CACHE
        printDebug();
#endif

        emit lister->started( _url );

        // Maybe listersCurrentlyListing/listersCurrentlyHolding should be QSets?
        Q_ASSERT(!dirData.listersCurrentlyListing.contains(lister));
        dirData.listersCurrentlyListing.append( lister );

        KIO::ListJob *job = jobForUrl( urlStr );
        // job will be 0 if we were listing from cache rather than listing from a kio job.
        if( job ) {
            lister->jobStarted(job);
            lister->d->connectJob( job );
        }
        Q_ASSERT( itemU );

        // List existing items in a delayed manner, just like things would happen
        // if we were not using the cache.
        if (!itemU->lstItems.isEmpty()) {
            //qDebug() << "Listing" << itemU->lstItems.count() << "cached items soon";
            new KCoreDirLister::Private::CachedItemsJob(lister, _url, _reload);
        } else {
            // The other lister hasn't emitted anything yet. Good, we'll just listen to it.
            // One problem could be if we have _reload=true and the existing job doesn't, though.
        }

#ifdef DEBUG_CACHE
        printDebug();
#endif
    }

    return true;
}

KCoreDirLister::Private::CachedItemsJob* KCoreDirLister::Private::cachedItemsJobForUrl(const QUrl& url) const
{
    Q_FOREACH(CachedItemsJob* job, m_cachedItemsJobs) {
	if (job->url() == url)
	    return job;
    }
    return 0;
}

KCoreDirLister::Private::CachedItemsJob::CachedItemsJob(KCoreDirLister* lister, const QUrl& url, bool reload)
  : KJob(lister),
    m_lister(lister), m_url(url),
    m_reload(reload), m_emitCompleted(true)
{
    //qDebug() << "Creating CachedItemsJob" << this << "for lister" << lister << url;
    if (lister->d->cachedItemsJobForUrl(url)) {
      qWarning() << "Lister" << lister << "has a cached items job already for" << url;
    }
    lister->d->m_cachedItemsJobs.append(this);
    setAutoDelete(true);
    start();
}

// Called by start() via QueuedConnection
void KCoreDirLister::Private::CachedItemsJob::done()
{
    if (!m_lister) // job was already killed, but waiting deletion due to deleteLater
        return;
    kDirListerCache()->emitItemsFromCache(this, m_lister, m_url, m_reload, m_emitCompleted);
    emitResult();
}

bool KCoreDirLister::Private::CachedItemsJob::doKill()
{
    //qDebug() << this;
    kDirListerCache()->forgetCachedItemsJob(this, m_lister, m_url);
    if (!property("_kdlc_silent").toBool()) {
        emit m_lister->canceled(m_url);
        emit m_lister->canceled();
    }
    m_lister = 0;
    return true;
}

void KCoreDirListerCache::emitItemsFromCache(KCoreDirLister::Private::CachedItemsJob* cachedItemsJob, KCoreDirLister* lister, const QUrl& _url, bool _reload, bool _emitCompleted)
{
    const QString urlStr = _url.toString();
    KCoreDirLister::Private* kdl = lister->d;
    kdl->complete = false;

    DirItem *itemU = kDirListerCache()->itemsInUse.value(urlStr);
    if (!itemU) {
        qWarning() << "Can't find item for directory" << urlStr << "anymore";
    } else {
        const KFileItemList items = itemU->lstItems;
        const KFileItem rootItem = itemU->rootItem;
        _reload = _reload || !itemU->complete;

        if (kdl->rootFileItem.isNull() && !rootItem.isNull() && kdl->url == _url) {
            kdl->rootFileItem = rootItem;
        }
        if (!items.isEmpty()) {
            //qDebug() << "emitting" << items.count() << "for lister" << lister;
            kdl->addNewItems(_url, items);
            kdl->emitItems();
        }
    }

    forgetCachedItemsJob(cachedItemsJob, lister, _url);

    // Emit completed, unless we were told not to,
    // or if listDir() was called while another directory listing for this dir was happening,
    // so we "joined" it. We detect that using jobForUrl to ensure it's a real ListJob,
    // not just a lister-specific CachedItemsJob (which wouldn't emit completed for us).
    if (_emitCompleted) {

        kdl->complete = true;
        emit lister->completed( _url );
        emit lister->completed();

        if ( _reload ) {
            updateDirectory( _url );
        }
    }
}

void KCoreDirListerCache::forgetCachedItemsJob(KCoreDirLister::Private::CachedItemsJob* cachedItemsJob, KCoreDirLister* lister, const QUrl& _url)
{
    // Modifications to data structures only below this point;
    // so that addNewItems is called with a consistent state

    const QString urlStr = _url.toString();
    lister->d->m_cachedItemsJobs.removeAll(cachedItemsJob);

    KCoreDirListerCacheDirectoryData& dirData = directoryData[urlStr];
    Q_ASSERT(dirData.listersCurrentlyListing.contains(lister));

    KIO::ListJob *listJob = jobForUrl(urlStr);
    if (!listJob) {
        Q_ASSERT(!dirData.listersCurrentlyHolding.contains(lister));
        //qDebug() << "Moving from listing to holding, because no more job" << lister << urlStr;
        dirData.listersCurrentlyHolding.append( lister );
        dirData.listersCurrentlyListing.removeAll( lister );
    } else {
        //qDebug() << "Still having a listjob" << listJob << ", so not moving to currently-holding.";
    }
}

bool KCoreDirListerCache::validUrl(KCoreDirLister *lister, const QUrl& url) const
{
    if (!url.isValid()) {
        qWarning() << url.errorString();
        lister->handleErrorMessage(i18n("Malformed URL\n%1", url.errorString()));
        return false;
    }

    if (!KProtocolManager::supportsListing(url)) {
        lister->handleErrorMessage(i18n("URL cannot be listed\n%1", url.toString()));
        return false;
    }

    return true;
}

void KCoreDirListerCache::stop( KCoreDirLister *lister, bool silent )
{
#ifdef DEBUG_CACHE
    //printDebug();
#endif
    //qDebug() << "lister:" << lister << "silent=" << silent;

    const QList<QUrl> urls = lister->d->lstDirs;
    Q_FOREACH(const QUrl& url, urls) {
        stopListingUrl(lister, url, silent);
    }

#if 0 // test code
    QHash<QString,KCoreDirListerCacheDirectoryData>::iterator dirit = directoryData.begin();
    const QHash<QString,KCoreDirListerCacheDirectoryData>::iterator dirend = directoryData.end();
    for( ; dirit != dirend ; ++dirit ) {
        KCoreDirListerCacheDirectoryData& dirData = dirit.value();
        if (dirData.listersCurrentlyListing.contains(lister)) {
            //qDebug() << "ERROR: found lister" << lister << "in list - for" << dirit.key();
            Q_ASSERT(false);
        }
    }
#endif
}

void KCoreDirListerCache::stopListingUrl(KCoreDirLister *lister, const QUrl& _u, bool silent)
{
    QUrl url(_u);
    url = url.adjusted(QUrl::StripTrailingSlash);
    const QString urlStr = url.toString();

    KCoreDirLister::Private::CachedItemsJob* cachedItemsJob = lister->d->cachedItemsJobForUrl(url);
    if (cachedItemsJob) {
        if (silent) {
            cachedItemsJob->setProperty("_kdlc_silent", true);
        }
        cachedItemsJob->kill(); // removes job from list, too
    }

    // TODO: consider to stop all the "child jobs" of url as well
    //qDebug() << lister << " url=" << url;

    QHash<QString,KCoreDirListerCacheDirectoryData>::iterator dirit = directoryData.find(urlStr);
    if (dirit == directoryData.end())
        return;
    KCoreDirListerCacheDirectoryData& dirData = dirit.value();
    if (dirData.listersCurrentlyListing.contains(lister)) {
        //qDebug() << " found lister" << lister << "in list - for" << urlStr;
        if (dirData.listersCurrentlyListing.count() == 1) {
            // This was the only dirlister interested in the list job -> kill the job
            stopListJob(urlStr, silent);
        } else {
            // Leave the job running for the other dirlisters, just unsubscribe us.
            dirData.listersCurrentlyListing.removeAll(lister);
            if (!silent) {
                emit lister->canceled();
                emit lister->canceled(url);
            }
        }
    }
}

// Helper for stop() and stopListingUrl()
void KCoreDirListerCache::stopListJob(const QString& url, bool silent)
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
        //qDebug() << "Killing list job" << job << "for" << url;
        if (silent) {
            job->setProperty("_kdlc_silent", true);
        }
        job->kill(KJob::EmitResult);
    }
}

void KCoreDirListerCache::setAutoUpdate( KCoreDirLister *lister, bool enable )
{
    // IMPORTANT: this method does not check for the current autoUpdate state!

    for ( QList<QUrl>::const_iterator it = lister->d->lstDirs.constBegin();
          it != lister->d->lstDirs.constEnd(); ++it ) {
        DirItem* dirItem = itemsInUse.value((*it).toString());
        Q_ASSERT(dirItem);
        if ( enable )
            dirItem->incAutoUpdate();
        else
            dirItem->decAutoUpdate();
    }
}

void KCoreDirListerCache::forgetDirs( KCoreDirLister *lister )
{
    //qDebug() << lister;

    emit lister->clear();
    // clear lister->d->lstDirs before calling forgetDirs(), so that
    // it doesn't contain things that itemsInUse doesn't. When emitting
    // the canceled signals, lstDirs must not contain anything that
    // itemsInUse does not contain. (otherwise it might crash in findByName()).
    const QList<QUrl> lstDirsCopy = lister->d->lstDirs;
    lister->d->lstDirs.clear();

    //qDebug() << "Iterating over dirs" << lstDirsCopy;
    for ( QList<QUrl>::const_iterator it = lstDirsCopy.begin();
          it != lstDirsCopy.end(); ++it ) {
        forgetDirs( lister, *it, false );
    }
}

static bool manually_mounted(const QString& path, const KMountPoint::List& possibleMountPoints)
{
    KMountPoint::Ptr mp = possibleMountPoints.findByPath(path);
    if (!mp) { // not listed in fstab -> yes, manually mounted
        if (possibleMountPoints.isEmpty()) // no fstab at all -> don't assume anything
            return false;
        return true;
    }
    const bool supermount = mp->mountType() == "supermount";
    if (supermount) {
        return true;
    }
    // noauto -> manually mounted. Otherwise, mounted at boot time, won't be unmounted any time soon hopefully.
    return mp->mountOptions().contains("noauto");
}

void KCoreDirListerCache::forgetDirs( KCoreDirLister *lister, const QUrl& _url, bool notify )
{
    //qDebug() << lister << " _url: " << _url;

    QUrl url(_url);
    url = url.adjusted(QUrl::StripTrailingSlash);
    const QString urlStr = url.toString();

    DirectoryDataHash::iterator dit = directoryData.find(urlStr);
    if (dit == directoryData.end())
        return;
    KCoreDirListerCacheDirectoryData& dirData = *dit;
    dirData.listersCurrentlyHolding.removeAll(lister);

    // This lister doesn't care for updates running in <url> anymore
    KIO::ListJob *job = jobForUrl(urlStr);
    if (job)
        lister->d->jobDone(job);

    DirItem *item = itemsInUse.value(urlStr);
    Q_ASSERT(item);
    bool insertIntoCache = false;

    if ( dirData.listersCurrentlyHolding.isEmpty() && dirData.listersCurrentlyListing.isEmpty() ) {
        // item not in use anymore -> move into cache if complete
        directoryData.erase(dit);
        itemsInUse.remove( urlStr );

        // this job is a running update which nobody cares about anymore
        if ( job ) {
            killJob( job );
            //qDebug() << "Killing update job for " << urlStr;

            // Well, the user of KCoreDirLister doesn't really care that we're stopping
            // a background-running job from a previous URL (in listDir) -> commented out.
            // stop() already emitted canceled.
            //emit lister->canceled( url );
            if ( lister->d->numJobs() == 0 ) {
                lister->d->complete = true;
                //emit lister->canceled();
            }
        }

        if ( notify ) {
            lister->d->lstDirs.removeAll( url );
            emit lister->clear( url );
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
                isManuallyMounted = manually_mounted( item->url.toLocalFile(), possibleMountPoints );
                if ( !isManuallyMounted ) {
                    // Look for a manually-mounted directory inside
                    // If there's one, we can't keep a watch either, FAM would prevent unmounting the CDROM
                    // I hope this isn't too slow
                    KFileItemList::const_iterator kit = item->lstItems.constBegin();
                    KFileItemList::const_iterator kend = item->lstItems.constEnd();
                    for ( ; kit != kend && !containsManuallyMounted; ++kit )
                        if ( (*kit).isDir() && manually_mounted((*kit).url().toLocalFile(), possibleMountPoints) )
                            containsManuallyMounted = true;
                }
            }

            if ( isManuallyMounted || containsManuallyMounted ) // [**]
            {
                //qDebug() << "Not adding a watch on " << item->url << " because it " <<
                //    ( isManuallyMounted ? "is manually mounted" : "contains a manually mounted subdir" );
                item->complete = false; // set to "dirty"
            } else {
                item->incAutoUpdate(); // keep watch
                item->watchedWhileInCache = true;
            }
        }
        else
        {
            delete item;
            item = 0;
        }
    }

    if ( item && lister->d->autoUpdate )
        item->decAutoUpdate();

    // Inserting into QCache must be done last, since it might delete the item
    if (item && insertIntoCache) {
        //qDebug() << lister << "item moved into cache:" << url;
        itemsCached.insert(urlStr, item);
    }
}

void KCoreDirListerCache::updateDirectory( const QUrl& _dir )
{
    //qDebug() << _dir;

    const QUrl dir = _dir.adjusted(QUrl::StripTrailingSlash);
    if (!checkUpdate(dir)) {
        if (dir.isLocalFile() && findByUrl(0, dir)) {
            pendingUpdates.insert(dir.toLocalFile());
            if (!pendingUpdateTimer.isActive())
                pendingUpdateTimer.start(500);
        }
        return;
    }

    // A job can be running to
    //   - only list a new directory: the listers are in listersCurrentlyListing
    //   - only update a directory: the listers are in listersCurrentlyHolding
    //   - update a currently running listing: the listers are in both

    QString urlStr = dir.toString();
    KCoreDirListerCacheDirectoryData& dirData = directoryData[urlStr];
    QList<KCoreDirLister *> listers = dirData.listersCurrentlyListing;
    QList<KCoreDirLister *> holders = dirData.listersCurrentlyHolding;

    //qDebug() << urlStr << "listers=" << listers << "holders=" << holders;

    // restart the job for dir if it is running already
    bool killed = false;
    KIO::ListJob *job = jobForUrl( urlStr );
    if (job) {
        killJob( job );
        killed = true;

        foreach ( KCoreDirLister *kdl, listers )
            kdl->d->jobDone( job );

        foreach ( KCoreDirLister *kdl, holders )
            kdl->d->jobDone( job );
    } else {
        // Emit any cached items.
        // updateDirectory() is about the diff compared to the cached items...
        Q_FOREACH(KCoreDirLister *kdl, listers) {
	    KCoreDirLister::Private::CachedItemsJob* cachedItemsJob = kdl->d->cachedItemsJobForUrl(dir);
            if (cachedItemsJob) {
                cachedItemsJob->setEmitCompleted(false);
                cachedItemsJob->done(); // removes from cachedItemsJobs list
                delete cachedItemsJob;
                killed = true;
            }
        }
    }
    //qDebug() << "Killed=" << killed;

    // we don't need to emit canceled signals since we only replaced the job,
    // the listing is continuing.

    if (!(listers.isEmpty() || killed)) {
        qWarning() << "The unexpected happened.";
        qWarning() << "listers for" << dir << "=" << listers;
        qWarning() << "job=" << job;
        //Q_FOREACH(KCoreDirLister *kdl, listers) {
            //qDebug() << "lister" << kdl << "m_cachedItemsJobs=" << kdl->d->m_cachedItemsJobs;
        //}
#ifndef NDEBUG
        printDebug();
#endif
    }
    Q_ASSERT( listers.isEmpty() || killed );

    job = KIO::listDir( dir, KIO::HideProgressInfo );
    runningListJobs.insert( job, KIO::UDSEntryList() );

    connect( job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
             this, SLOT(slotUpdateEntries(KIO::Job*,KIO::UDSEntryList)) );
    connect( job, SIGNAL(result(KJob*)),
             this, SLOT(slotUpdateResult(KJob*)) );

    //qDebug() << "update started in" << dir;

    foreach ( KCoreDirLister *kdl, listers ) {
        kdl->jobStarted(job);
    }

    if ( !holders.isEmpty() ) {
        if ( !killed ) {
            foreach ( KCoreDirLister *kdl, holders ) {
                kdl->jobStarted(job);
                emit kdl->started( dir );
            }
        } else {
            foreach ( KCoreDirLister *kdl, holders ) {
                kdl->jobStarted(job);
            }
        }
    }
}

bool KCoreDirListerCache::checkUpdate(const QUrl& _dir)
{
    QString dir = _dir.toString();
    if (!itemsInUse.contains(dir)) {
        DirItem *item = itemsCached[dir];
        if (item && item->complete) {
            item->complete = false;
            item->watchedWhileInCache = false;
            item->decAutoUpdate();
            // Hmm, this debug output might include login/password from the _dir URL.
            //qDebug() << "directory " << _dir << " not in use, marked dirty.";
        }
        //else
        //qDebug() << "aborted, directory " << _dir << " not in cache.";
        return false;
    } else {
        return true;
    }
}

KFileItem KCoreDirListerCache::itemForUrl( const QUrl& url ) const
{
    KFileItem *item = findByUrl( 0, url );
    if (item) {
        return *item;
    } else {
        return KFileItem();
    }
}

KCoreDirListerCache::DirItem *KCoreDirListerCache::dirItemForUrl(const QUrl& dir) const
{
    const QString urlStr = dir.toString(QUrl::StripTrailingSlash);
    DirItem *item = itemsInUse.value(urlStr);
    if ( !item )
        item = itemsCached[urlStr];
    return item;
}

KFileItemList *KCoreDirListerCache::itemsForDir(const QUrl& dir) const
{
    DirItem *item = dirItemForUrl(dir);
    return item ? &item->lstItems : 0;
}

KFileItem KCoreDirListerCache::findByName( const KCoreDirLister *lister, const QString& _name ) const
{
    Q_ASSERT(lister);

    for (QList<QUrl>::const_iterator it = lister->d->lstDirs.constBegin();
         it != lister->d->lstDirs.constEnd(); ++it) {
        DirItem* dirItem = itemsInUse.value((*it).toString());
        Q_ASSERT(dirItem);
        const KFileItem item = dirItem->lstItems.findByName(_name);
        if (!item.isNull())
            return item;
    }

    return KFileItem();
}

KFileItem *KCoreDirListerCache::findByUrl( const KCoreDirLister *lister, const QUrl& _u ) const
{
    QUrl url(_u);
    url = url.adjusted(QUrl::StripTrailingSlash);

    const QUrl parentDir = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
    DirItem* dirItem = dirItemForUrl(parentDir);
    if (dirItem) {
        // If lister is set, check that it contains this dir
        if (!lister || lister->d->lstDirs.contains(parentDir)) {
            KFileItemList::iterator it = dirItem->lstItems.begin();
            const KFileItemList::iterator end = dirItem->lstItems.end();
            for (; it != end ; ++it) {
                if ((*it).url() == url) {
                    return &*it;
                }
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
            return &dirItem->rootItem;
        }
    }

    return 0;
}

void KCoreDirListerCache::slotFilesAdded( const QString &dir /*url*/ ) // from KDirNotify signals
{
    QUrl urlDir(dir);
    itemsAddedInDirectory(urlDir);
}

void KCoreDirListerCache::itemsAddedInDirectory(const QUrl& urlDir)
{
    //qDebug() << urlDir; // output urls, not qstrings, since they might contain a password
    Q_FOREACH(const QUrl& u, directoriesForCanonicalPath(urlDir)) {
        updateDirectory(u);
    }
}

void KCoreDirListerCache::slotFilesRemoved( const QStringList &fileList ) // from KDirNotify signals
{
    // TODO: handling of symlinks-to-directories isn't done here,
    // because I'm not sure how to do it and keep the performance ok...

    slotFilesRemoved(QUrl::fromStringList(fileList));
}

void KCoreDirListerCache::slotFilesRemoved(const QList<QUrl>& fileList)
{
    //qDebug() << fileList.count();
    // Group notifications by parent dirs (usually there would be only one parent dir)
    QMap<QString, KFileItemList> removedItemsByDir;
    QList<QUrl> deletedSubdirs;

    for (QList<QUrl>::const_iterator it = fileList.begin(); it != fileList.end() ; ++it) {
        QUrl url(*it);
        DirItem* dirItem = dirItemForUrl(url); // is it a listed directory?
        if (dirItem) {
            deletedSubdirs.append(url);
            if (!dirItem->rootItem.isNull()) {
                removedItemsByDir[url.toString()].append(dirItem->rootItem);
            }
        }

        const QUrl parentDir = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
        dirItem = dirItemForUrl(parentDir);
        if (!dirItem)
            continue;
        for (KFileItemList::iterator fit = dirItem->lstItems.begin(), fend = dirItem->lstItems.end(); fit != fend ; ++fit) {
            if ((*fit).url() == url) {
                const KFileItem fileitem = *fit;
                removedItemsByDir[parentDir.toString()].append(fileitem);
                // If we found a fileitem, we can test if it's a dir. If not, we'll go to deleteDir just in case.
                if (fileitem.isNull() || fileitem.isDir()) {
                    deletedSubdirs.append(url);
                }
                dirItem->lstItems.erase(fit); // remove fileitem from list
                break;
            }
        }
    }

    QMap<QString, KFileItemList>::const_iterator rit = removedItemsByDir.constBegin();
    for(; rit != removedItemsByDir.constEnd(); ++rit) {
        // Tell the views about it before calling deleteDir.
        // They might need the subdirs' file items (see the dirtree).
        DirectoryDataHash::const_iterator dit = directoryData.constFind(rit.key());
        if (dit != directoryData.constEnd()) {
            itemsDeleted((*dit).listersCurrentlyHolding, rit.value());
        }
    }

    Q_FOREACH(const QUrl& url, deletedSubdirs) {
        // in case of a dir, check if we have any known children, there's much to do in that case
        // (stopping jobs, removing dirs from cache etc.)
        deleteDir(url);
    }
}

void KCoreDirListerCache::slotFilesChanged( const QStringList &fileList ) // from KDirNotify signals
{
    //qDebug() << fileList;
    QList<QUrl> dirsToUpdate;
    QStringList::const_iterator it = fileList.begin();
    for (; it != fileList.end() ; ++it) {
        QUrl url(*it);
        KFileItem *fileitem = findByUrl(0, url);
        if (!fileitem) {
            //qDebug() << "item not found for" << url;
            continue;
        }
        if (url.isLocalFile()) {
            pendingUpdates.insert(url.toLocalFile()); // delegate the work to processPendingUpdates
        } else {
            pendingRemoteUpdates.insert(fileitem);
            // For remote files, we won't be able to figure out the new information,
            // we have to do a update (directory listing)
            const QUrl dir = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
            if (!dirsToUpdate.contains(dir))
                dirsToUpdate.prepend(dir);
        }
    }

    QList<QUrl>::const_iterator itdir = dirsToUpdate.constBegin();
    for (; itdir != dirsToUpdate.constEnd() ; ++itdir)
        updateDirectory( *itdir );
    // ## TODO problems with current jobs listing/updating that dir
    // ( see kde-2.2.2's kdirlister )

    processPendingUpdates();
}

void KCoreDirListerCache::slotFileRenamed( const QString &_src, const QString &_dst ) // from KDirNotify signals
{
    QUrl src(_src);
    QUrl dst(_dst);
    //qDebug() << src << "->" << dst;
#ifdef DEBUG_CACHE
  printDebug();
#endif

    QUrl oldurl = src.adjusted(QUrl::StripTrailingSlash);
    KFileItem *fileitem = findByUrl(0, oldurl);
    if (!fileitem) {
        //qDebug() << "Item not found:" << oldurl;
        return;
    }

    const KFileItem oldItem = *fileitem;

    // Dest already exists? Was overwritten then (testcase: #151851)
    // We better emit it as deleted -before- doing the renaming, otherwise
    // the "update" mechanism will emit the old one as deleted and
    // kdirmodel will delete the new (renamed) one!
    KFileItem* existingDestItem = findByUrl(0, dst);
    if (existingDestItem) {
        //qDebug() << dst << "already existed, let's delete it";
        slotFilesRemoved(QList<QUrl>() << dst);
    }

    // If the item had a UDS_URL as well as UDS_NAME set, the user probably wants
    // to be updating the name only (since they can't see the URL).
    // Check to see if a URL exists, and if so, if only the file part has changed,
    // only update the name and not the underlying URL.
    bool nameOnly = !fileitem->entry().stringValue( KIO::UDSEntry::UDS_URL ).isEmpty();
    nameOnly = nameOnly && src.adjusted(QUrl::RemoveFilename) == dst.adjusted(QUrl::RemoveFilename);

    if (!nameOnly && fileitem->isDir()) {
        renameDir(oldurl, dst);
        // #172945 - if the fileitem was the root item of a DirItem that was just removed from the cache,
        // then it's a dangling pointer now...
        fileitem = findByUrl(0, oldurl);
        if (!fileitem) //deleted from cache altogether, #188807
            return;
    }

    // Now update the KFileItem representing that file or dir (not exclusive with the above!)
    if (!oldItem.isLocalFile() && !oldItem.localPath().isEmpty()) { // it uses UDS_LOCAL_PATH? ouch, needs an update then
        slotFilesChanged(QStringList() << src.toString());
    } else {
        if( nameOnly )
            fileitem->setName(dst.fileName());
        else
            fileitem->setUrl( dst );
        fileitem->refreshMimeType();
        fileitem->determineMimeType();
        QSet<KCoreDirLister*> listers = emitRefreshItem( oldItem, *fileitem );
        Q_FOREACH(KCoreDirLister * kdl, listers) {
            kdl->d->emitItems();
        }
    }

#ifdef DEBUG_CACHE
    printDebug();
#endif
}

QSet<KCoreDirLister*> KCoreDirListerCache::emitRefreshItem(const KFileItem& oldItem, const KFileItem& fileitem)
{
    //qDebug() << "old:" << oldItem.name() << oldItem.url()
    //             << "new:" << fileitem.name() << fileitem.url();
    // Look whether this item was shown in any view, i.e. held by any dirlister
    const QUrl parentDir = oldItem.url().adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
    const QString parentDirURL = parentDir.toString();
    DirectoryDataHash::iterator dit = directoryData.find(parentDirURL);
    QList<KCoreDirLister *> listers;
    // Also look in listersCurrentlyListing, in case the user manages to rename during a listing
    if (dit != directoryData.end())
        listers += (*dit).listersCurrentlyHolding + (*dit).listersCurrentlyListing;
    if (oldItem.isDir()) {
        // For a directory, look for dirlisters where it's the root item.
        dit = directoryData.find(oldItem.url().toString());
        if (dit != directoryData.end())
            listers += (*dit).listersCurrentlyHolding + (*dit).listersCurrentlyListing;
    }
    QSet<KCoreDirLister*> listersToRefresh;
    Q_FOREACH(KCoreDirLister *kdl, listers) {
        // For a directory, look for dirlisters where it's the root item.
        QUrl directoryUrl(oldItem.url());
        if (oldItem.isDir() && kdl->d->rootFileItem == oldItem) {
            const KFileItem oldRootItem = kdl->d->rootFileItem;
            kdl->d->rootFileItem = fileitem;
            kdl->d->addRefreshItem(directoryUrl, oldRootItem, fileitem);
        } else {
            directoryUrl = directoryUrl.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
            kdl->d->addRefreshItem(directoryUrl, oldItem, fileitem);
        }
        listersToRefresh.insert(kdl);
    }
    return listersToRefresh;
}

QList<QUrl> KCoreDirListerCache::directoriesForCanonicalPath(const QUrl& dir) const
{
    QList<QUrl> dirs;
    dirs << dir;
    dirs << canonicalUrls.value(dir).toSet().toList(); /* make unique; there are faster ways, but this is really small anyway */

    if (dirs.count() > 1) {
        //qDebug() << dir << "known as" << dirs;
    }
    return dirs;
}

// private slots

// Called by KDirWatch - usually when a dir we're watching has been modified,
// but it can also be called for a file.
void KCoreDirListerCache::slotFileDirty( const QString& path )
{
    //qDebug() << path;
    QUrl url = QUrl::fromLocalFile(path).adjusted(QUrl::StripTrailingSlash);
    // File or dir?
    bool isDir;
    const KFileItem item = itemForUrl(url);

    if (!item.isNull()) {
        isDir = item.isDir();
    } else {
        QFileInfo info(path);
        if (!info.exists())
            return; // error
        isDir = info.isDir();
    }

    if (isDir) {
        Q_FOREACH(const QUrl& dir, directoriesForCanonicalPath(url)) {
            handleDirDirty(dir);
        }
    } else {
        Q_FOREACH(const QUrl& dir, directoriesForCanonicalPath(url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash))) {
            QUrl aliasUrl(dir);
            aliasUrl.setPath(aliasUrl.path() + '/' + url.fileName());
            handleFileDirty(aliasUrl);
        }
    }
}

// Called by slotFileDirty
void KCoreDirListerCache::handleDirDirty(const QUrl& url)
{
    // A dir: launch an update job if anyone cares about it

    // This also means we can forget about pending updates to individual files in that dir
    QString dirPath = url.toLocalFile();
    if (!dirPath.endsWith('/')) {
        dirPath += '/';
    }
    QMutableSetIterator<QString> pendingIt(pendingUpdates);
    while (pendingIt.hasNext()) {
        const QString updPath = pendingIt.next();
        //qDebug() << "had pending update" << updPath;
        if (updPath.startsWith(dirPath) &&
            updPath.indexOf('/', dirPath.length()) == -1) { // direct child item
            //qDebug() << "forgetting about individual update to" << updPath;
            pendingIt.remove();
        }
    }

    updateDirectory(url);
}

// Called by slotFileDirty
void KCoreDirListerCache::handleFileDirty(const QUrl& url)
{
    // A file: do we know about it already?
    KFileItem* existingItem = findByUrl(0, url);
    if (!existingItem) {
        // No - update the parent dir then
        const QUrl dir = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
        updateDirectory(dir);
    } else {
        // A known file: delay updating it, FAM is flooding us with events
        const QString filePath = url.toLocalFile();
        if (!pendingUpdates.contains(filePath)) {
            const QUrl dir = url.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash);
            if (checkUpdate(dir)) {
                pendingUpdates.insert(filePath);
                if (!pendingUpdateTimer.isActive())
                    pendingUpdateTimer.start(500);
            }
        }
    }
}

void KCoreDirListerCache::slotFileCreated( const QString& path ) // from KDirWatch
{
    //qDebug() << path;
    // XXX: how to avoid a complete rescan here?
    // We'd need to stat that one file separately and refresh the item(s) for it.
    QUrl fileUrl(QUrl::fromLocalFile(path));
    itemsAddedInDirectory(fileUrl.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash));
}

void KCoreDirListerCache::slotFileDeleted( const QString& path ) // from KDirWatch
{
    //qDebug() << path;
    const QString fileName = QFileInfo(path).fileName();
    QUrl dirUrl(QUrl::fromLocalFile(path));
    QStringList fileUrls;
    Q_FOREACH(const QUrl& url, directoriesForCanonicalPath(dirUrl.adjusted(QUrl::RemoveFilename|QUrl::StripTrailingSlash))) {
        QUrl urlInfo(url);
        urlInfo.setPath(urlInfo.path() + '/' + fileName);
        fileUrls << urlInfo.toString();
    }
    slotFilesRemoved(fileUrls);
}

void KCoreDirListerCache::slotEntries( KIO::Job *job, const KIO::UDSEntryList &entries )
{
    QUrl url(joburl(static_cast<KIO::ListJob *>(job)));
    url = url.adjusted(QUrl::StripTrailingSlash);
    QString urlStr = url.toString();

    //qDebug() << "new entries for " << url;

    DirItem *dir = itemsInUse.value(urlStr);
    if (!dir) {
        qWarning() << "Internal error: job is listing" << url << "but itemsInUse only knows about" << itemsInUse.keys();
        Q_ASSERT( dir );
        return;
    }

    DirectoryDataHash::iterator dit = directoryData.find(urlStr);
    if (dit == directoryData.end()) {
        qWarning() << "Internal error: job is listing" << url << "but directoryData doesn't know about that url, only about:" << directoryData.keys();
        Q_ASSERT(dit != directoryData.end());
        return;
    }
    KCoreDirListerCacheDirectoryData& dirData = *dit;
    if (dirData.listersCurrentlyListing.isEmpty()) {
        qWarning() << "Internal error: job is listing" << url << "but directoryData says no listers are currently listing " << urlStr;
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT( !dirData.listersCurrentlyListing.isEmpty() );
        return;
    }

    // check if anyone wants the mimetypes immediately
    bool delayedMimeTypes = true;
    foreach ( KCoreDirLister *kdl, dirData.listersCurrentlyListing )
        delayedMimeTypes &= kdl->d->delayedMimeTypes;

    KIO::UDSEntryList::const_iterator it = entries.begin();
    const KIO::UDSEntryList::const_iterator end = entries.end();
    for ( ; it != end; ++it )
    {
        const QString name = (*it).stringValue( KIO::UDSEntry::UDS_NAME );

        Q_ASSERT( !name.isEmpty() );
        if ( name.isEmpty() )
            continue;

        if ( name == "." )
        {
            Q_ASSERT( dir->rootItem.isNull() );
            // Try to reuse an existing KFileItem (if we listed the parent dir)
            // rather than creating a new one. There are many reasons:
            // 1) renames and permission changes to the item would have to emit the signals
            // twice, otherwise, so that both views manage to recognize the item.
            // 2) with kio_ftp we can only know that something is a symlink when
            // listing the parent, so prefer that item, which has more info.
            // Note that it gives a funky name() to the root item, rather than "." ;)
            dir->rootItem = itemForUrl(url);
            if (dir->rootItem.isNull())
                dir->rootItem = KFileItem( *it, url, delayedMimeTypes, true  );

            foreach ( KCoreDirLister *kdl, dirData.listersCurrentlyListing )
                if ( kdl->d->rootFileItem.isNull() && kdl->d->url == url )
                    kdl->d->rootFileItem = dir->rootItem;
        }
        else if ( name != ".." )
        {
            KFileItem item( *it, url, delayedMimeTypes, true );

            //qDebug()<< "Adding item: " << item.url();
            dir->lstItems.append( item );

            foreach ( KCoreDirLister *kdl, dirData.listersCurrentlyListing )
                kdl->d->addNewItem(url, item);
        }
    }

    foreach ( KCoreDirLister *kdl, dirData.listersCurrentlyListing )
        kdl->d->emitItems();
}

void KCoreDirListerCache::slotResult( KJob *j )
{
#ifdef DEBUG_CACHE
    //printDebug();
#endif

  Q_ASSERT( j );
  KIO::ListJob *job = static_cast<KIO::ListJob *>( j );
  runningListJobs.remove( job );

  QUrl jobUrl(joburl( job ));
  jobUrl = jobUrl.adjusted(QUrl::StripTrailingSlash);  // need remove trailing slashes again, in case of redirections
  QString jobUrlStr = jobUrl.toString();

  //qDebug() << "finished listing" << jobUrl;

  DirectoryDataHash::iterator dit = directoryData.find(jobUrlStr);
  if (dit == directoryData.end()) {
    qWarning() << "Nothing found in directoryData for URL" << jobUrlStr;
#ifndef NDEBUG
    printDebug();
#endif
    Q_ASSERT(dit != directoryData.end());
    return;
  }
  KCoreDirListerCacheDirectoryData& dirData = *dit;
  if ( dirData.listersCurrentlyListing.isEmpty() ) {
    qWarning() << "OOOOPS, nothing in directoryData.listersCurrentlyListing for" << jobUrlStr;
    // We're about to assert; dump the current state...
#ifndef NDEBUG
    printDebug();
#endif
    Q_ASSERT( !dirData.listersCurrentlyListing.isEmpty() );
  }
  QList<KCoreDirLister *> listers = dirData.listersCurrentlyListing;

  // move all listers to the holding list, do it before emitting
  // the signals to make sure it exists in KCoreDirListerCache in case someone
  // calls listDir during the signal emission
  Q_ASSERT( dirData.listersCurrentlyHolding.isEmpty() );
  dirData.moveListersWithoutCachedItemsJob(jobUrl);

  if ( job->error() )
  {
    foreach ( KCoreDirLister *kdl, listers )
    {
      kdl->d->jobDone( job );
      if (job->error() != KJob::KilledJobError) {
          kdl->handleError( job );
      }
      const bool silent = job->property("_kdlc_silent").toBool();
      if (!silent) {
          emit kdl->canceled( jobUrl );
      }

      if (kdl->d->numJobs() == 0) {
        kdl->d->complete = true;
        if (!silent) {
            emit kdl->canceled();
        }
      }
    }
  }
  else
  {
    DirItem *dir = itemsInUse.value(jobUrlStr);
    Q_ASSERT( dir );
    dir->complete = true;

    foreach ( KCoreDirLister* kdl, listers )
    {
      kdl->d->jobDone( job );
      emit kdl->completed( jobUrl );
      if ( kdl->d->numJobs() == 0 )
      {
        kdl->d->complete = true;
        emit kdl->completed();
      }
    }
  }

  // TODO: hmm, if there was an error and job is a parent of one or more
  // of the pending urls we should cancel it/them as well
  processPendingUpdates();

#ifdef DEBUG_CACHE
  printDebug();
#endif
}

void KCoreDirListerCache::slotRedirection( KIO::Job *j, const QUrl& url )
{
    Q_ASSERT( j );
    KIO::ListJob *job = static_cast<KIO::ListJob *>( j );

    QUrl oldUrl(job->url());  // here we really need the old url!
    QUrl newUrl(url);

    // strip trailing slashes
    oldUrl = oldUrl.adjusted(QUrl::StripTrailingSlash);
    newUrl = newUrl.adjusted(QUrl::StripTrailingSlash);

    if ( oldUrl == newUrl ) {
        //qDebug() << "New redirection url same as old, giving up.";
        return;
    } else if (newUrl.isEmpty()) {
        //qDebug() << "New redirection url is empty, giving up.";
        return;
    }

    const QString oldUrlStr = oldUrl.toString();
    const QString newUrlStr = newUrl.toString();

    //qDebug() << oldUrl << "->" << newUrl;

#ifdef DEBUG_CACHE
    // Can't do that here. KCoreDirListerCache::joburl() will use the new url already,
    // while our data structures haven't been updated yet -> assert fail.
    //printDebug();
#endif

    // I don't think there can be dirItems that are children of oldUrl.
    // Am I wrong here? And even if so, we don't need to delete them, right?
    // DF: redirection happens before listDir emits any item. Makes little sense otherwise.

    // oldUrl cannot be in itemsCached because only completed items are moved there
    DirItem *dir = itemsInUse.take(oldUrlStr);
    Q_ASSERT( dir );

    DirectoryDataHash::iterator dit = directoryData.find(oldUrlStr);
    Q_ASSERT(dit != directoryData.end());
    KCoreDirListerCacheDirectoryData oldDirData = *dit;
    directoryData.erase(dit);
    Q_ASSERT( !oldDirData.listersCurrentlyListing.isEmpty() );
    const QList<KCoreDirLister *> listers = oldDirData.listersCurrentlyListing;
    Q_ASSERT( !listers.isEmpty() );

    foreach ( KCoreDirLister *kdl, listers ) {
        kdl->d->redirect(oldUrl, newUrl, false /*clear items*/);
    }

    // when a lister was stopped before the job emits the redirection signal, the old url will
    // also be in listersCurrentlyHolding
    const QList<KCoreDirLister *> holders = oldDirData.listersCurrentlyHolding;
    foreach ( KCoreDirLister *kdl, holders ) {
        kdl->jobStarted(job);
        // do it like when starting a new list-job that will redirect later
        // TODO: maybe don't emit started if there's an update running for newUrl already?
        emit kdl->started( oldUrl );

        kdl->d->redirect(oldUrl, newUrl, false /*clear items*/);
    }

    DirItem *newDir = itemsInUse.value(newUrlStr);
    if ( newDir ) {
        //qDebug() << newUrl << "already in use";

        // only in this case there can newUrl already be in listersCurrentlyListing or listersCurrentlyHolding
        delete dir;

        // get the job if one's running for newUrl already (can be a list-job or an update-job), but
        // do not return this 'job', which would happen because of the use of redirectionURL()
        KIO::ListJob *oldJob = jobForUrl( newUrlStr, job );

        // listers of newUrl with oldJob: forget about the oldJob and use the already running one
        // which will be converted to an updateJob
        KCoreDirListerCacheDirectoryData& newDirData = directoryData[newUrlStr];

        QList<KCoreDirLister *>& curListers = newDirData.listersCurrentlyListing;
        if ( !curListers.isEmpty() ) {
            //qDebug() << "and it is currently listed";

            Q_ASSERT( oldJob );  // ?!

            foreach ( KCoreDirLister *kdl, curListers ) { // listers of newUrl
                kdl->d->jobDone( oldJob );

                kdl->jobStarted(job);
                kdl->d->connectJob( job );
            }

            // append listers of oldUrl with newJob to listers of newUrl with oldJob
            foreach ( KCoreDirLister *kdl, listers )
                curListers.append( kdl );
        } else {
            curListers = listers;
        }

        if ( oldJob )         // kill the old job, be it a list-job or an update-job
            killJob( oldJob );

        // holders of newUrl: use the already running job which will be converted to an updateJob
        QList<KCoreDirLister *>& curHolders = newDirData.listersCurrentlyHolding;
        if ( !curHolders.isEmpty() ) {
            //qDebug() << "and it is currently held.";

            foreach ( KCoreDirLister *kdl, curHolders ) {  // holders of newUrl
                kdl->jobStarted(job);
                emit kdl->started( newUrl );
            }

            // append holders of oldUrl to holders of newUrl
            foreach ( KCoreDirLister *kdl, holders )
                curHolders.append( kdl );
        } else {
            curHolders = holders;
        }


        // emit old items: listers, holders. NOT: newUrlListers/newUrlHolders, they already have them listed
        // TODO: make this a separate method?
        foreach ( KCoreDirLister *kdl, listers + holders ) {
            if ( kdl->d->rootFileItem.isNull() && kdl->d->url == newUrl )
                kdl->d->rootFileItem = newDir->rootItem;

            kdl->d->addNewItems(newUrl, newDir->lstItems);
            kdl->d->emitItems();
        }
    } else if ( (newDir = itemsCached.take( newUrlStr )) ) {
        //qDebug() << newUrl << "is unused, but already in the cache.";

        delete dir;
        itemsInUse.insert( newUrlStr, newDir );
        KCoreDirListerCacheDirectoryData& newDirData = directoryData[newUrlStr];
        newDirData.listersCurrentlyListing = listers;
        newDirData.listersCurrentlyHolding = holders;

        // emit old items: listers, holders
        foreach ( KCoreDirLister *kdl, listers + holders ) {
            if ( kdl->d->rootFileItem.isNull() && kdl->d->url == newUrl )
                kdl->d->rootFileItem = newDir->rootItem;

            kdl->d->addNewItems(newUrl, newDir->lstItems);
            kdl->d->emitItems();
        }
    } else {
        //qDebug() << newUrl << "has not been listed yet.";

        dir->rootItem = KFileItem();
        dir->lstItems.clear();
        dir->redirect( newUrl );
        itemsInUse.insert( newUrlStr, dir );
        KCoreDirListerCacheDirectoryData& newDirData = directoryData[newUrlStr];
        newDirData.listersCurrentlyListing = listers;
        newDirData.listersCurrentlyHolding = holders;

        if ( holders.isEmpty() ) {
#ifdef DEBUG_CACHE
            printDebug();
#endif
            return; // only in this case the job doesn't need to be converted,
        }
    }

    // make the job an update job
    job->disconnect( this );

    connect( job, SIGNAL(entries(KIO::Job*,KIO::UDSEntryList)),
             this, SLOT(slotUpdateEntries(KIO::Job*,KIO::UDSEntryList)) );
    connect( job, SIGNAL(result(KJob*)),
             this, SLOT(slotUpdateResult(KJob*)) );

    // FIXME: autoUpdate-Counts!!

#ifdef DEBUG_CACHE
    printDebug();
#endif
}

struct KCoreDirListerCache::ItemInUseChange
{
    ItemInUseChange(const QString& old, const QString& newU, DirItem* di)
        : oldUrl(old), newUrl(newU), dirItem(di) {}
    QString oldUrl;
    QString newUrl;
    DirItem* dirItem;
};

void KCoreDirListerCache::renameDir( const QUrl &oldUrl, const QUrl &newUrl )
{
    //qDebug() << oldUrl << "->" << newUrl;
    //const QString oldUrlStr = oldUrl.url(KUrl::RemoveTrailingSlash);
    //const QString newUrlStr = newUrl.url(KUrl::RemoveTrailingSlash);

    // Not enough. Also need to look at any child dir, even sub-sub-sub-dir.
    //DirItem *dir = itemsInUse.take( oldUrlStr );
    //emitRedirections( oldUrl, url );

    QLinkedList<ItemInUseChange> itemsToChange;
    QSet<KCoreDirLister *> listers;

    // Look at all dirs being listed/shown
    QHash<QString, DirItem *>::iterator itu = itemsInUse.begin();
    const QHash<QString, DirItem *>::iterator ituend = itemsInUse.end();
    for (; itu != ituend ; ++itu) {
        DirItem *dir = itu.value();
        QUrl oldDirUrl(itu.key());
        //qDebug() << "itemInUse:" << oldDirUrl;
        // Check if this dir is oldUrl, or a subfolder of it
        if ( oldDirUrl == oldUrl || oldUrl.isParentOf( oldDirUrl ) ) {
            // TODO should use KUrl::cleanpath like isParentOf does
            QString relPath = oldDirUrl.path().mid( oldUrl.path().length() );

            QUrl newDirUrl(newUrl); // take new base
            if ( !relPath.isEmpty() )
                newDirUrl.setPath(newDirUrl.path() + '/' + relPath); // add unchanged relative path
            //qDebug() << "new url=" << newDirUrl;

            // Update URL in dir item and in itemsInUse
            dir->redirect( newDirUrl );

            itemsToChange.append(ItemInUseChange(oldDirUrl.toString(QUrl::StripTrailingSlash),
                                                 newDirUrl.toString(QUrl::StripTrailingSlash),
                                                 dir));
            // Rename all items under that dir

            for ( KFileItemList::iterator kit = dir->lstItems.begin(), kend = dir->lstItems.end();
                  kit != kend ; ++kit )
            {
                const KFileItem oldItem = *kit;

                const QUrl oldItemUrl ((*kit).url());
                const QString oldItemUrlStr(oldItemUrl.toString(QUrl::StripTrailingSlash));
                QUrl newItemUrl(oldItemUrl);
                newItemUrl.setPath(newDirUrl.path() + '/' + oldItemUrl.fileName());
                //qDebug() << "renaming" << oldItemUrl << "to" << newItemUrl;
                (*kit).setUrl(newItemUrl);

                listers |= emitRefreshItem(oldItem, *kit);
            }
            emitRedirections( oldDirUrl, newDirUrl );
        }
    }

    Q_FOREACH(KCoreDirLister * kdl, listers) {
        kdl->d->emitItems();
    }

    // Do the changes to itemsInUse out of the loop to avoid messing up iterators,
    // and so that emitRefreshItem can find the stuff in the hash.
    foreach(const ItemInUseChange& i, itemsToChange) {
        itemsInUse.remove(i.oldUrl);
        itemsInUse.insert(i.newUrl, i.dirItem);
    }

    // Is oldUrl a directory in the cache?
    // Remove any child of oldUrl from the cache - even if the renamed dir itself isn't in it!
    removeDirFromCache( oldUrl );
    // TODO rename, instead.
}

// helper for renameDir, not used for redirections from KIO::listDir().
void KCoreDirListerCache::emitRedirections( const QUrl &oldUrl, const QUrl &newUrl )
{
    //qDebug() << oldUrl << "->" << newUrl;
    const QString oldUrlStr = oldUrl.toString(QUrl::StripTrailingSlash);
    const QString newUrlStr = newUrl.toString(QUrl::StripTrailingSlash);

    KIO::ListJob *job = jobForUrl( oldUrlStr );
    if ( job )
        killJob( job );

    // Check if we were listing this dir. Need to abort and restart with new name in that case.
    DirectoryDataHash::iterator dit = directoryData.find(oldUrlStr);
    if ( dit == directoryData.end() )
        return;
    const QList<KCoreDirLister *> listers = (*dit).listersCurrentlyListing;
    const QList<KCoreDirLister *> holders = (*dit).listersCurrentlyHolding;

    KCoreDirListerCacheDirectoryData& newDirData = directoryData[newUrlStr];

    // Tell the world that the job listing the old url is dead.
    foreach ( KCoreDirLister *kdl, listers ) {
        if ( job )
            kdl->d->jobDone( job );

        emit kdl->canceled( oldUrl );
    }
    newDirData.listersCurrentlyListing += listers;

    // Check if we are currently displaying this directory (odds opposite wrt above)
    foreach ( KCoreDirLister *kdl, holders ) {
        if ( job )
            kdl->d->jobDone( job );
    }
    newDirData.listersCurrentlyHolding += holders;
    directoryData.erase(dit);

    if ( !listers.isEmpty() ) {
        updateDirectory( newUrl );

        // Tell the world about the new url
        foreach ( KCoreDirLister *kdl, listers )
            emit kdl->started( newUrl );
    }

    // And notify the dirlisters of the redirection
    foreach ( KCoreDirLister *kdl, holders ) {
        kdl->d->redirect(oldUrl, newUrl, true /*keep items*/);
    }
}

void KCoreDirListerCache::removeDirFromCache( const QUrl& dir )
{
    //qDebug() << dir;
    const QList<QString> cachedDirs = itemsCached.keys(); // seems slow, but there's no qcache iterator...
    foreach(const QString& cachedDir, cachedDirs) {
        const QUrl cachedDirUrl(cachedDir);
        if (dir == cachedDirUrl || dir.isParentOf(cachedDirUrl))
            itemsCached.remove( cachedDir );
    }
}

void KCoreDirListerCache::slotUpdateEntries( KIO::Job* job, const KIO::UDSEntryList& list )
{
    runningListJobs[static_cast<KIO::ListJob*>(job)] += list;
}

void KCoreDirListerCache::slotUpdateResult( KJob * j )
{
    Q_ASSERT( j );
    KIO::ListJob *job = static_cast<KIO::ListJob *>( j );

    QUrl jobUrl(joburl( job ));
    jobUrl = jobUrl.adjusted(QUrl::StripTrailingSlash);  // need remove trailing slashes again, in case of redirections
    QString jobUrlStr(jobUrl.toString());

    //qDebug() << "finished update" << jobUrl;

    KCoreDirListerCacheDirectoryData& dirData = directoryData[jobUrlStr];
    // Collect the dirlisters which were listing the URL using that ListJob
    // plus those that were already holding that URL - they all get updated.
    dirData.moveListersWithoutCachedItemsJob(jobUrl);
    QList<KCoreDirLister *> listers = dirData.listersCurrentlyHolding;
    listers += dirData.listersCurrentlyListing;

    // once we are updating dirs that are only in the cache this will fail!
    Q_ASSERT( !listers.isEmpty() );

    if ( job->error() ) {
        foreach ( KCoreDirLister* kdl, listers ) {
            kdl->d->jobDone( job );

            //don't bother the user
            //kdl->handleError( job );

            const bool silent = job->property("_kdlc_silent").toBool();
            if (!silent) {
                emit kdl->canceled( jobUrl );
            }
            if ( kdl->d->numJobs() == 0 ) {
                kdl->d->complete = true;
                if (!silent) {
                    emit kdl->canceled();
                }
            }
        }

        runningListJobs.remove( job );

        // TODO: if job is a parent of one or more
        // of the pending urls we should cancel them
        processPendingUpdates();
        return;
    }

    DirItem *dir = itemsInUse.value(jobUrlStr, 0);
    if (!dir) {
        qWarning() << "Internal error: itemsInUse did not contain" << jobUrlStr;
#ifndef NDEBUG
        printDebug();
#endif
        Q_ASSERT(dir);
    } else {
        dir->complete = true;
    }

    // check if anyone wants the mimetypes immediately
    bool delayedMimeTypes = true;
    foreach ( KCoreDirLister *kdl, listers )
        delayedMimeTypes &= kdl->d->delayedMimeTypes;

    QHash<QString, KFileItem*> fileItems; // fileName -> KFileItem*

    // Unmark all items in url
    for ( KFileItemList::iterator kit = dir->lstItems.begin(), kend = dir->lstItems.end() ; kit != kend ; ++kit )
    {
        (*kit).unmark();
        fileItems.insert( (*kit).name(), &*kit );
    }

    const KIO::UDSEntryList& buf = runningListJobs.value( job );
    KIO::UDSEntryList::const_iterator it = buf.constBegin();
    const KIO::UDSEntryList::const_iterator end = buf.constEnd();
    for ( ; it != end; ++it )
    {
        // Form the complete url
        KFileItem item( *it, jobUrl, delayedMimeTypes, true );

        const QString name = item.name();
        Q_ASSERT( !name.isEmpty() );

        // we duplicate the check for dotdot here, to avoid iterating over
        // all items again and checking in matchesFilter() that way.
        if ( name.isEmpty() || name == ".." )
            continue;

        if ( name == "." )
        {
            // if the update was started before finishing the original listing
            // there is no root item yet
            if ( dir->rootItem.isNull() )
            {
                dir->rootItem = item;

                foreach ( KCoreDirLister *kdl, listers )
                    if ( kdl->d->rootFileItem.isNull() && kdl->d->url == jobUrl )
                        kdl->d->rootFileItem = dir->rootItem;
            }
            continue;
        }

        // Find this item
        if (KFileItem* tmp = fileItems.value(item.name()))
        {
            QSet<KFileItem*>::iterator pru_it = pendingRemoteUpdates.find(tmp);
            const bool inPendingRemoteUpdates = (pru_it != pendingRemoteUpdates.end());

            // check if something changed for this file, using KFileItem::cmp()
            if (!tmp->cmp( item ) || inPendingRemoteUpdates) {

                if (inPendingRemoteUpdates) {
                    pendingRemoteUpdates.erase(pru_it);
                }

                //qDebug() << "file changed:" << tmp->name();

                const KFileItem oldItem = *tmp;
                *tmp = item;
                foreach ( KCoreDirLister *kdl, listers )
                    kdl->d->addRefreshItem(jobUrl, oldItem, *tmp);
            }
            //qDebug() << "marking" << tmp;
            tmp->mark();
        }
        else // this is a new file
        {
            //qDebug() << "new file:" << name;

            KFileItem pitem(item);
            pitem.mark();
            dir->lstItems.append( pitem );

            foreach ( KCoreDirLister *kdl, listers )
                kdl->d->addNewItem(jobUrl, pitem);
        }
    }

    runningListJobs.remove( job );

    deleteUnmarkedItems( listers, dir->lstItems );

    foreach ( KCoreDirLister *kdl, listers ) {
        kdl->d->emitItems();

        kdl->d->jobDone( job );

        emit kdl->completed( jobUrl );
        if ( kdl->d->numJobs() == 0 )
        {
            kdl->d->complete = true;
            emit kdl->completed();
        }
    }

    // TODO: hmm, if there was an error and job is a parent of one or more
    // of the pending urls we should cancel it/them as well
    processPendingUpdates();
}

// private

KIO::ListJob *KCoreDirListerCache::jobForUrl( const QString& url, KIO::ListJob *not_job )
{
  QMap< KIO::ListJob *, KIO::UDSEntryList >::const_iterator it = runningListJobs.constBegin();
  while ( it != runningListJobs.constEnd() )
  {
    KIO::ListJob *job = it.key();
    QString jobUrlStr = joburl(job).toString(QUrl::StripTrailingSlash);

    if (jobUrlStr == url && job != not_job)
       return job;
    ++it;
  }
  return 0;
}

const QUrl& KCoreDirListerCache::joburl( KIO::ListJob *job )
{
  if ( job->redirectionUrl().isValid() )
     return job->redirectionUrl();
  else
     return job->url();
}

void KCoreDirListerCache::killJob( KIO::ListJob *job )
{
  runningListJobs.remove( job );
  job->disconnect( this );
  job->kill();
}

void KCoreDirListerCache::deleteUnmarkedItems( const QList<KCoreDirLister *>& listers, KFileItemList &lstItems )
{
    KFileItemList deletedItems;
    // Find all unmarked items and delete them
    QMutableListIterator<KFileItem> kit(lstItems);
    while (kit.hasNext()) {
        const KFileItem& item = kit.next();
        if (!item.isMarked()) {
            //qDebug() << "deleted:" << item.name() << &item;
            deletedItems.append(item);
            kit.remove();
        }
    }
    if (!deletedItems.isEmpty())
        itemsDeleted(listers, deletedItems);
}

void KCoreDirListerCache::itemsDeleted(const QList<KCoreDirLister *>& listers, const KFileItemList& deletedItems)
{
    Q_FOREACH(KCoreDirLister *kdl, listers) {
        kdl->d->emitItemsDeleted(deletedItems);
    }

    Q_FOREACH(const KFileItem& item, deletedItems) {
        if (item.isDir())
            deleteDir(item.url());
    }
}

void KCoreDirListerCache::deleteDir(const QUrl& _dirUrl)
{
    //qDebug() << dirUrl;
    // unregister and remove the children of the deleted item.
    // Idea: tell all the KCoreDirListers that they should forget the dir
    //       and then remove it from the cache.

    QUrl dirUrl(_dirUrl.adjusted(QUrl::StripTrailingSlash));

    // Separate itemsInUse iteration and calls to forgetDirs (which modify itemsInUse)
    QList<QUrl> affectedItems;

    QHash<QString, DirItem *>::iterator itu = itemsInUse.begin();
    const QHash<QString, DirItem *>::iterator ituend = itemsInUse.end();
    for ( ; itu != ituend; ++itu ) {
        const QUrl deletedUrl( itu.key() );
        if (dirUrl == deletedUrl || dirUrl.isParentOf(deletedUrl)) {
            affectedItems.append(deletedUrl);
        }
    }

    foreach(const QUrl& deletedUrl, affectedItems) {
        const QString deletedUrlStr = deletedUrl.toString();
        // stop all jobs for deletedUrlStr
        DirectoryDataHash::iterator dit = directoryData.find(deletedUrlStr);
        if (dit != directoryData.end()) {
            // we need a copy because stop modifies the list
            QList<KCoreDirLister *> listers = (*dit).listersCurrentlyListing;
            foreach ( KCoreDirLister *kdl, listers )
                stopListingUrl( kdl, deletedUrl );
            // tell listers holding deletedUrl to forget about it
            // this will stop running updates for deletedUrl as well

            // we need a copy because forgetDirs modifies the list
            QList<KCoreDirLister *> holders = (*dit).listersCurrentlyHolding;
            foreach ( KCoreDirLister *kdl, holders ) {
                // lister's root is the deleted item
                if ( kdl->d->url == deletedUrl )
                {
                    // tell the view first. It might need the subdirs' items (which forgetDirs will delete)
                    if ( !kdl->d->rootFileItem.isNull() ) {
                        emit kdl->deleteItem( kdl->d->rootFileItem );
                        emit kdl->itemsDeleted(KFileItemList() << kdl->d->rootFileItem);
                    }
                    forgetDirs( kdl );
                    kdl->d->rootFileItem = KFileItem();
                }
                else
                {
                    const bool treeview = kdl->d->lstDirs.count() > 1;
                    if ( !treeview )
                    {
                        emit kdl->clear();
                        kdl->d->lstDirs.clear();
                    }
                    else
                        kdl->d->lstDirs.removeAll( deletedUrl );

                    forgetDirs( kdl, deletedUrl, treeview );
                }
            }
        }

        // delete the entry for deletedUrl - should not be needed, it's in
        // items cached now
        int count = itemsInUse.remove( deletedUrlStr );
        Q_ASSERT( count == 0 );
        Q_UNUSED( count ); //keep gcc "unused variable" complaining quiet when in release mode
    }

    // remove the children from the cache
    removeDirFromCache( dirUrl );
}

// delayed updating of files, FAM is flooding us with events
void KCoreDirListerCache::processPendingUpdates()
{
    QSet<KCoreDirLister *> listers;
    foreach(const QString& file, pendingUpdates) { // always a local path
        //qDebug() << file;
        QUrl u = QUrl::fromLocalFile(file);
        KFileItem *item = findByUrl( 0, u ); // search all items
        if ( item ) {
            // we need to refresh the item, because e.g. the permissions can have changed.
            KFileItem oldItem = *item;
            item->refresh();
            listers |= emitRefreshItem( oldItem, *item );
        }
    }
    pendingUpdates.clear();
    Q_FOREACH(KCoreDirLister * kdl, listers) {
        kdl->d->emitItems();
    }
}

#ifndef NDEBUG
void KCoreDirListerCache::printDebug()
{
    qDebug() << "Items in use:";
    QHash<QString, DirItem *>::const_iterator itu = itemsInUse.constBegin();
    const QHash<QString, DirItem *>::const_iterator ituend = itemsInUse.constEnd();
    for ( ; itu != ituend ; ++itu ) {
        qDebug() << "   " << itu.key() << "URL:" << itu.value()->url
                     << "rootItem:" << ( !itu.value()->rootItem.isNull() ? itu.value()->rootItem.url() : QUrl() )
                     << "autoUpdates refcount:" << itu.value()->autoUpdates
                     << "complete:" << itu.value()->complete
                     << QString("with %1 items.").arg(itu.value()->lstItems.count());
    }

    QList<KCoreDirLister*> listersWithoutJob;
    qDebug() << "Directory data:";
    DirectoryDataHash::const_iterator dit = directoryData.constBegin();
    for ( ; dit != directoryData.constEnd(); ++dit )
    {
        QString list;
        foreach ( KCoreDirLister* listit, (*dit).listersCurrentlyListing )
            list += " 0x" + QString::number( (qlonglong)listit, 16 );
        qDebug() << "  " << dit.key() << (*dit).listersCurrentlyListing.count() << "listers:" << list;
        foreach ( KCoreDirLister* listit, (*dit).listersCurrentlyListing ) {
            if (!listit->d->m_cachedItemsJobs.isEmpty()) {
                qDebug() << "  Lister" << listit << "has CachedItemsJobs" << listit->d->m_cachedItemsJobs;
            } else if (KIO::ListJob* listJob = jobForUrl(dit.key())) {
                qDebug() << "  Lister" << listit << "has ListJob" << listJob;
            } else {
                listersWithoutJob.append(listit);
            }
        }

        list.clear();
        foreach ( KCoreDirLister* listit, (*dit).listersCurrentlyHolding )
            list += " 0x" + QString::number( (qlonglong)listit, 16 );
        qDebug() << "  " << dit.key() << (*dit).listersCurrentlyHolding.count() << "holders:" << list;
    }

    QMap< KIO::ListJob *, KIO::UDSEntryList >::Iterator jit = runningListJobs.begin();
    qDebug() << "Jobs:";
    for ( ; jit != runningListJobs.end() ; ++jit )
        qDebug() << "   " << jit.key() << "listing" << joburl( jit.key() ) << ":" << (*jit).count() << "entries.";

    qDebug() << "Items in cache:";
    const QList<QString> cachedDirs = itemsCached.keys();
    foreach(const QString& cachedDir, cachedDirs) {
        DirItem* dirItem = itemsCached.object(cachedDir);
        qDebug() << "   " << cachedDir << "rootItem:"
                     << (!dirItem->rootItem.isNull() ? dirItem->rootItem.url().toString() : QString("NULL") )
                     << "with" << dirItem->lstItems.count() << "items.";
    }

    // Abort on listers without jobs -after- showing the full dump. Easier debugging.
    Q_FOREACH(KCoreDirLister* listit, listersWithoutJob) {
        qWarning() << "Fatal Error: HUH? Lister" << listit << "is supposed to be listing, but has no job!";
        abort();
    }
}
#endif


KCoreDirLister::KCoreDirLister( QObject* parent )
    : QObject(parent), d(new Private(this))
{
    //qDebug() << "+KCoreDirLister";

    d->complete = true;

    setAutoUpdate( true );
    setDirOnlyMode( false );
    setShowingDotFiles( false );
}

KCoreDirLister::~KCoreDirLister()
{
    //qDebug() << "~KCoreDirLister" << this;

    // Stop all running jobs, remove lister from lists
    if (!kDirListerCache.isDestroyed()) {
        stop();
        kDirListerCache()->forgetDirs( this );
    }

    delete d;
}

bool KCoreDirLister::openUrl( const QUrl& _url, OpenUrlFlags _flags )
{
    // emit the current changes made to avoid an inconsistent treeview
    if (d->hasPendingChanges && (_flags & Keep))
        emitChanges();

    d->hasPendingChanges = false;

    return kDirListerCache()->listDir( this, _url, _flags & Keep, _flags & Reload );
}

void KCoreDirLister::stop()
{
    kDirListerCache()->stop( this );
}

void KCoreDirLister::stop( const QUrl& _url )
{
    kDirListerCache()->stopListingUrl( this, _url );
}

bool KCoreDirLister::autoUpdate() const
{
    return d->autoUpdate;
}

void KCoreDirLister::setAutoUpdate( bool _enable )
{
    if ( d->autoUpdate == _enable )
        return;

    d->autoUpdate = _enable;
    kDirListerCache()->setAutoUpdate( this, _enable );
}

bool KCoreDirLister::showingDotFiles() const
{
  return d->settings.isShowingDotFiles;
}

void KCoreDirLister::setShowingDotFiles( bool _showDotFiles )
{
  if ( d->settings.isShowingDotFiles == _showDotFiles )
    return;

  d->prepareForSettingsChange();
  d->settings.isShowingDotFiles = _showDotFiles;
}

bool KCoreDirLister::dirOnlyMode() const
{
  return d->settings.dirOnlyMode;
}

void KCoreDirLister::setDirOnlyMode( bool _dirsOnly )
{
  if ( d->settings.dirOnlyMode == _dirsOnly )
    return;

  d->prepareForSettingsChange();
  d->settings.dirOnlyMode = _dirsOnly;
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

void KCoreDirLister::Private::emitChanges()
{
    if (!hasPendingChanges)
        return;

    // reset 'hasPendingChanges' now, in case of recursion
    // (testcase: enabling recursive scan in ktorrent, #174920)
    hasPendingChanges = false;

    const Private::FilterSettings newSettings = settings;
    settings = oldSettings; // temporarily

    // Mark all items that are currently visible
    Q_FOREACH(const QUrl& dir, lstDirs) {
        KFileItemList* itemList = kDirListerCache()->itemsForDir(dir);
        if (!itemList) {
            continue;
        }

        KFileItemList::iterator kit = itemList->begin();
        const KFileItemList::iterator kend = itemList->end();
        for (; kit != kend; ++kit) {
            if (isItemVisible(*kit) && m_parent->matchesMimeFilter(*kit))
                (*kit).mark();
            else
                (*kit).unmark();
        }
    }

    settings = newSettings;

    Q_FOREACH(const QUrl& dir, lstDirs) {
        KFileItemList deletedItems;

        KFileItemList* itemList = kDirListerCache()->itemsForDir(dir);
        if (!itemList) {
            continue;
        }

        KFileItemList::iterator kit = itemList->begin();
        const KFileItemList::iterator kend = itemList->end();
        for (; kit != kend; ++kit) {
            KFileItem& item = *kit;
            const QString text = item.text();
            if (text == "." || text == "..")
                continue;
            const bool nowVisible = isItemVisible(item) && m_parent->matchesMimeFilter(item);
            if (nowVisible && !item.isMarked())
                addNewItem(dir, item); // takes care of emitting newItem or itemsFilteredByMime
            else if (!nowVisible && item.isMarked())
                deletedItems.append(*kit);
        }
        if (!deletedItems.isEmpty()) {
            emit m_parent->itemsDeleted(deletedItems);
            // for compat
            Q_FOREACH(const KFileItem& item, deletedItems)
                emit m_parent->deleteItem(item);
        }
        emitItems();
    }
    oldSettings = settings;
}

void KCoreDirLister::updateDirectory( const QUrl& _u )
{
  kDirListerCache()->updateDirectory( _u );
}

bool KCoreDirLister::isFinished() const
{
  return d->complete;
}

KFileItem KCoreDirLister::rootItem() const
{
  return d->rootFileItem;
}

KFileItem KCoreDirLister::findByUrl( const QUrl& _url ) const
{
  KFileItem *item = kDirListerCache()->findByUrl( this, _url );
  if (item) {
      return *item;
  } else {
      return KFileItem();
  }
}

KFileItem KCoreDirLister::findByName( const QString& _name ) const
{
  return kDirListerCache()->findByName( this, _name );
}


// ================ public filter methods ================ //

void KCoreDirLister::setNameFilter( const QString& nameFilter )
{
    if (d->nameFilter == nameFilter)
        return;

    d->prepareForSettingsChange();

    d->settings.lstFilters.clear();
    d->nameFilter = nameFilter;
    // Split on white space
    const QStringList list = nameFilter.split( ' ', QString::SkipEmptyParts );
    for (QStringList::const_iterator it = list.begin(); it != list.end(); ++it)
        d->settings.lstFilters.append(QRegExp(*it, Qt::CaseInsensitive, QRegExp::Wildcard));
}

QString KCoreDirLister::nameFilter() const
{
  return d->nameFilter;
}

void KCoreDirLister::setMimeFilter( const QStringList& mimeFilter )
{
    if (d->settings.mimeFilter == mimeFilter)
        return;

    d->prepareForSettingsChange();
    if (mimeFilter.contains(QLatin1String("application/octet-stream")) || mimeFilter.contains(QLatin1String("all/allfiles"))) // all files
        d->settings.mimeFilter.clear();
    else
        d->settings.mimeFilter = mimeFilter;
}

void KCoreDirLister::setMimeExcludeFilter( const QStringList& mimeExcludeFilter )
{
    if (d->settings.mimeExcludeFilter == mimeExcludeFilter)
        return;

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

bool KCoreDirLister::matchesFilter( const QString& name ) const
{
    return doNameFilter(name, d->settings.lstFilters);
}

bool KCoreDirLister::matchesMimeFilter( const QString& mime ) const
{
    return doMimeFilter(mime, d->settings.mimeFilter) &&
        d->doMimeExcludeFilter(mime, d->settings.mimeExcludeFilter);
}

// ================ protected methods ================ //

bool KCoreDirLister::matchesFilter( const KFileItem& item ) const
{
  Q_ASSERT( !item.isNull() );

  if ( item.text() == ".." )
    return false;

  if ( !d->settings.isShowingDotFiles && item.isHidden() )
    return false;

  if ( item.isDir() || d->settings.lstFilters.isEmpty() )
    return true;

  return matchesFilter( item.text() );
}

bool KCoreDirLister::matchesMimeFilter( const KFileItem& item ) const
{
    Q_ASSERT(!item.isNull());
    // Don't lose time determining the mimetype if there is no filter
    if (d->settings.mimeFilter.isEmpty() && d->settings.mimeExcludeFilter.isEmpty())
        return true;
    return matchesMimeFilter(item.mimetype());
}

bool KCoreDirLister::doNameFilter( const QString& name, const QList<QRegExp>& filters ) const
{
  for ( QList<QRegExp>::const_iterator it = filters.begin(); it != filters.end(); ++it )
    if ( (*it).exactMatch( name ) )
      return true;

  return false;
}

bool KCoreDirLister::doMimeFilter( const QString& mime, const QStringList& filters ) const
{
  if ( filters.isEmpty() )
    return true;

  QMimeDatabase db;
  const QMimeType mimeptr = db.mimeTypeForName(mime);
  if (!mimeptr.isValid())
    return false;

  //qDebug() << "doMimeFilter: investigating: "<<mimeptr->name();
  QStringList::const_iterator it = filters.begin();
  for ( ; it != filters.end(); ++it )
    if (mimeptr.inherits(*it))
      return true;
    //else   //qDebug() << "doMimeFilter: compared without result to  "<<*it;

  return false;
}

bool KCoreDirLister::Private::doMimeExcludeFilter( const QString& mime, const QStringList& filters ) const
{
  if ( filters.isEmpty() )
    return true;

  QStringList::const_iterator it = filters.begin();
  for ( ; it != filters.end(); ++it )
    if ( (*it) == mime )
      return false;

  return true;
}

void KCoreDirLister::handleError( KIO::Job *job )
{
    qWarning() << job->errorString();
}

void KCoreDirLister::handleErrorMessage(const QString &message)
{
    qWarning() << message;
}

// ================= private methods ================= //

void KCoreDirLister::Private::addNewItem(const QUrl& directoryUrl, const KFileItem &item)
{
    if (!isItemVisible(item))
        return; // No reason to continue... bailing out here prevents a mimetype scan.

    //qDebug() << "in" << directoryUrl << "item:" << item.url();

  if ( m_parent->matchesMimeFilter( item ) )
  {
    if ( !lstNewItems )
    {
      lstNewItems = new NewItemsHash;
    }

    Q_ASSERT( !item.isNull() );
    (*lstNewItems)[directoryUrl].append( item );            // items not filtered
  }
  else
  {
    if ( !lstMimeFilteredItems ) {
      lstMimeFilteredItems = new KFileItemList;
    }

    Q_ASSERT( !item.isNull() );
    lstMimeFilteredItems->append( item );   // only filtered by mime
  }
}

void KCoreDirLister::Private::addNewItems(const QUrl& directoryUrl, const KFileItemList& items)
{
  // TODO: make this faster - test if we have a filter at all first
  // DF: was this profiled? The matchesFoo() functions should be fast, w/o filters...
  // Of course if there is no filter and we can do a range-insertion instead of a loop, that might be good.
  KFileItemList::const_iterator kit = items.begin();
  const KFileItemList::const_iterator kend = items.end();
  for ( ; kit != kend; ++kit )
    addNewItem(directoryUrl, *kit);
}

void KCoreDirLister::Private::addRefreshItem(const QUrl& directoryUrl, const KFileItem& oldItem, const KFileItem& item)
{
    const bool refreshItemWasFiltered = !isItemVisible(oldItem) ||
                                        !m_parent->matchesMimeFilter(oldItem);
  if (isItemVisible(item) && m_parent->matchesMimeFilter(item)) {
    if ( refreshItemWasFiltered )
    {
      if ( !lstNewItems ) {
        lstNewItems = new NewItemsHash;
      }

      Q_ASSERT( !item.isNull() );
      (*lstNewItems)[directoryUrl].append( item );
    }
    else
    {
      if ( !lstRefreshItems ) {
        lstRefreshItems = new QList<QPair<KFileItem,KFileItem> >;
      }

      Q_ASSERT( !item.isNull() );
      lstRefreshItems->append( qMakePair(oldItem, item) );
    }
  }
  else if ( !refreshItemWasFiltered )
  {
    if ( !lstRemoveItems ) {
      lstRemoveItems = new KFileItemList;
    }

    // notify the user that the mimetype of a file changed that doesn't match
    // a filter or does match an exclude filter
    // This also happens when renaming foo to .foo and dot files are hidden (#174721)
    Q_ASSERT(!oldItem.isNull());
    lstRemoveItems->append(oldItem);
  }
}

void KCoreDirLister::Private::emitItems()
{
  NewItemsHash *tmpNew = lstNewItems;
  lstNewItems = 0;

  KFileItemList *tmpMime = lstMimeFilteredItems;
  lstMimeFilteredItems = 0;

  QList<QPair<KFileItem, KFileItem> > *tmpRefresh = lstRefreshItems;
  lstRefreshItems = 0;

  KFileItemList *tmpRemove = lstRemoveItems;
  lstRemoveItems = 0;

    if (tmpNew) {
        QHashIterator<QUrl, KFileItemList> it(*tmpNew);
        while (it.hasNext()) {
            it.next();
            emit m_parent->itemsAdded(it.key(), it.value());
            emit m_parent->newItems(it.value()); // compat
        }
        delete tmpNew;
    }

  if ( tmpMime )
  {
    emit m_parent->itemsFilteredByMime( *tmpMime );
    delete tmpMime;
  }

  if ( tmpRefresh )
  {
    emit m_parent->refreshItems( *tmpRefresh );
    delete tmpRefresh;
  }

  if ( tmpRemove )
  {
      emit m_parent->itemsDeleted( *tmpRemove );
      delete tmpRemove;
  }
}

bool KCoreDirLister::Private::isItemVisible(const KFileItem& item) const
{
    // Note that this doesn't include mime filters, because
    // of the itemsFilteredByMime signal. Filtered-by-mime items are
    // considered "visible", they are just visible via a different signal...
    return (!settings.dirOnlyMode || item.isDir())
        && m_parent->matchesFilter(item);
}

void KCoreDirLister::Private::emitItemsDeleted(const KFileItemList &_items)
{
    KFileItemList items = _items;
    QMutableListIterator<KFileItem> it(items);
    while (it.hasNext()) {
        const KFileItem& item = it.next();
        if (isItemVisible(item) && m_parent->matchesMimeFilter(item)) {
            // for compat
            emit m_parent->deleteItem(item);
        } else {
            it.remove();
        }
    }
    if (!items.isEmpty())
        emit m_parent->itemsDeleted(items);
}

// ================ private slots ================ //

void KCoreDirLister::Private::_k_slotInfoMessage( KJob *, const QString& message )
{
  emit m_parent->infoMessage( message );
}

void KCoreDirLister::Private::_k_slotPercent( KJob *job, unsigned long pcnt )
{
  jobData[static_cast<KIO::ListJob *>(job)].percent = pcnt;

  int result = 0;

  KIO::filesize_t size = 0;

  QMap< KIO::ListJob *, Private::JobData >::Iterator dataIt = jobData.begin();
  while ( dataIt != jobData.end() )
  {
    result += (*dataIt).percent * (*dataIt).totalSize;
    size += (*dataIt).totalSize;
    ++dataIt;
  }

  if ( size != 0 )
    result /= size;
  else
    result = 100;
  emit m_parent->percent( result );
}

void KCoreDirLister::Private::_k_slotTotalSize( KJob *job, qulonglong size )
{
  jobData[static_cast<KIO::ListJob *>(job)].totalSize = size;

  KIO::filesize_t result = 0;
  QMap< KIO::ListJob *, Private::JobData >::Iterator dataIt = jobData.begin();
  while ( dataIt != jobData.end() )
  {
    result += (*dataIt).totalSize;
    ++dataIt;
  }

  emit m_parent->totalSize( result );
}

void KCoreDirLister::Private::_k_slotProcessedSize( KJob *job, qulonglong size )
{
  jobData[static_cast<KIO::ListJob *>(job)].processedSize = size;

  KIO::filesize_t result = 0;
  QMap< KIO::ListJob *, Private::JobData >::Iterator dataIt = jobData.begin();
  while ( dataIt != jobData.end() )
  {
    result += (*dataIt).processedSize;
    ++dataIt;
  }

  emit m_parent->processedSize( result );
}

void KCoreDirLister::Private::_k_slotSpeed( KJob *job, unsigned long spd )
{
  jobData[static_cast<KIO::ListJob *>(job)].speed = spd;

  int result = 0;
  QMap< KIO::ListJob *, Private::JobData >::Iterator dataIt = jobData.begin();
  while ( dataIt != jobData.end() )
  {
    result += (*dataIt).speed;
    ++dataIt;
  }

  emit m_parent->speed( result );
}

uint KCoreDirLister::Private::numJobs()
{
#ifdef DEBUG_CACHE
    // This code helps detecting stale entries in the jobData map.
    //qDebug() << m_parent << "numJobs:" << jobData.count();
    QMapIterator<KIO::ListJob *, JobData> it(jobData);
    while (it.hasNext()) {
        it.next();
        //qDebug() << (void*)it.key();
        //qDebug() << it.key();
    }
#endif

  return jobData.count();
}

void KCoreDirLister::Private::jobDone( KIO::ListJob *job )
{
  jobData.remove( job );
}

void KCoreDirLister::jobStarted( KIO::ListJob *job )
{
  Private::JobData data;
  data.speed = 0;
  data.percent = 0;
  data.processedSize = 0;
  data.totalSize = 0;

  d->jobData.insert( job, data );
  d->complete = false;
}

void KCoreDirLister::Private::connectJob( KIO::ListJob *job )
{
  m_parent->connect( job, SIGNAL(infoMessage(KJob*,QString,QString)),
                     m_parent, SLOT(_k_slotInfoMessage(KJob*,QString)) );
  m_parent->connect( job, SIGNAL(percent(KJob*,ulong)),
                     m_parent, SLOT(_k_slotPercent(KJob*,ulong)) );
  m_parent->connect( job, SIGNAL(totalSize(KJob*,qulonglong)),
                     m_parent, SLOT(_k_slotTotalSize(KJob*,qulonglong)) );
  m_parent->connect( job, SIGNAL(processedSize(KJob*,qulonglong)),
                     m_parent, SLOT(_k_slotProcessedSize(KJob*,qulonglong)) );
  m_parent->connect( job, SIGNAL(speed(KJob*,ulong)),
                     m_parent, SLOT(_k_slotSpeed(KJob*,ulong)) );
}

KFileItemList KCoreDirLister::items( WhichItems which ) const
{
    return itemsForDir( url(), which );
}

KFileItemList KCoreDirLister::itemsForDir( const QUrl& dir, WhichItems which ) const
{
    KFileItemList *allItems = kDirListerCache()->itemsForDir( dir );
    if ( !allItems )
        return KFileItemList();

    if ( which == AllItems )
        return *allItems;
    else // only items passing the filters
    {
        KFileItemList result;
        KFileItemList::const_iterator kit = allItems->constBegin();
        const KFileItemList::const_iterator kend = allItems->constEnd();
        for ( ; kit != kend; ++kit )
        {
            const KFileItem& item = *kit;
            if (d->isItemVisible(item) && matchesMimeFilter(item)) {
                result.append(item);
            }
        }
        return result;
    }
}

bool KCoreDirLister::delayedMimeTypes() const
{
    return d->delayedMimeTypes;
}

void KCoreDirLister::setDelayedMimeTypes( bool delayedMimeTypes )
{
    d->delayedMimeTypes = delayedMimeTypes;
}

// called by KCoreDirListerCache::slotRedirection
void KCoreDirLister::Private::redirect(const QUrl& oldUrl, const QUrl& newUrl, bool keepItems)
{
    if (url.matches(oldUrl, QUrl::StripTrailingSlash)) {
        if (!keepItems) {
            rootFileItem = KFileItem();
        } else {
            rootFileItem.setUrl(newUrl);
        }
        url = newUrl;
    }

    const int idx = lstDirs.indexOf( oldUrl );
    if (idx == -1) {
        qWarning() << "Unexpected redirection from" << oldUrl << "to" << newUrl
                       << "but this dirlister is currently listing/holding" << lstDirs;
    } else {
        lstDirs[ idx ] = newUrl;
    }

    if ( lstDirs.count() == 1 ) {
        if (!keepItems)
            emit m_parent->clear();
        emit m_parent->redirection( newUrl );
    } else {
        if (!keepItems)
            emit m_parent->clear( oldUrl );
    }
    emit m_parent->redirection( oldUrl, newUrl );
}

void KCoreDirListerCacheDirectoryData::moveListersWithoutCachedItemsJob(const QUrl& url)
{
    // Move dirlisters from listersCurrentlyListing to listersCurrentlyHolding,
    // but not those that are still waiting on a CachedItemsJob...
    // Unit-testing note:
    // Run kdirmodeltest in valgrind to hit the case where an update
    // is triggered while a lister has a CachedItemsJob (different timing...)
    QMutableListIterator<KCoreDirLister *> lister_it(listersCurrentlyListing);
    while (lister_it.hasNext()) {
        KCoreDirLister* kdl = lister_it.next();
        if (!kdl->d->cachedItemsJobForUrl(url)) {
            // OK, move this lister from "currently listing" to "currently holding".

            // Huh? The KCoreDirLister was present twice in listersCurrentlyListing, or was in both lists?
            Q_ASSERT(!listersCurrentlyHolding.contains(kdl));
            if (!listersCurrentlyHolding.contains(kdl)) {
                listersCurrentlyHolding.append(kdl);
            }
            lister_it.remove();
        } else {
            //qDebug() << "Not moving" << kdl << "to listersCurrentlyHolding because it still has job" << kdl->d->m_cachedItemsJobs;
        }
    }
}

KFileItem KCoreDirLister::cachedItemForUrl(const QUrl& url)
{
    return kDirListerCache()->itemForUrl(url);
}

#include "moc_kcoredirlister.cpp"
#include "moc_kcoredirlister_p.cpp"
