/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2002, 2004-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KCOREDIRLISTER_H
#define KCOREDIRLISTER_H

#include "kfileitem.h"
#include "kdirnotify.h" // TODO SIC: remove

#include <QString>
#include <QStringList>
#include <QUrl>

#include <memory>

class KJob;
namespace KIO
{
class Job;
class ListJob;
}

class KCoreDirListerPrivate;

/**
 * @class KCoreDirLister kcoredirlister.h <KCoreDirLister>
 *
 * @short Helper class for the kiojob used to list and update a directory.
 *
 * The dir lister deals with the kiojob used to list and update a directory
 * and has signals for the user of this class (e.g. konqueror view or
 * kdesktop) to create/destroy its items when asked.
 *
 * This class is independent from the graphical representation of the dir
 * (icon container, tree view, ...) and it stores the items (as KFileItems).
 *
 * Typical usage :
 * @li Create an instance.
 * @li Connect to at least update, clear, itemsAdded, and itemsDeleted.
 * @li Call openUrl - the signals will be called.
 * @li Reuse the instance when opening a new url (openUrl).
 * @li Destroy the instance when not needed anymore (usually destructor).
 *
 * Advanced usage : call openUrl with OpenUrlFlag::Keep to list directories
 * without forgetting the ones previously read (e.g. for a tree view)
 *
 * @author Michael Brade <brade@kde.org>
 */
class KIOCORE_EXPORT KCoreDirLister : public QObject
{
    friend class KCoreDirListerCache;
    friend struct KCoreDirListerCacheDirectoryData;

    Q_OBJECT
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate)
    Q_PROPERTY(bool showingDotFiles READ showingDotFiles WRITE setShowingDotFiles)
    Q_PROPERTY(bool dirOnlyMode READ dirOnlyMode WRITE setDirOnlyMode)
    Q_PROPERTY(bool delayedMimeTypes READ delayedMimeTypes WRITE setDelayedMimeTypes)
    Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter)
    Q_PROPERTY(QStringList mimeFilter READ mimeFilters WRITE setMimeFilter RESET clearMimeFilter)

public:
    /**
     * @see OpenUrlFlags
     */
    enum OpenUrlFlag {
        NoFlags = 0x0,   ///< No additional flags specified.

        Keep = 0x1,      ///< Previous directories aren't forgotten
        ///< (they are still watched by kdirwatch and their items
        ///< are kept for this KCoreDirLister). This is useful for e.g.
        ///< a treeview.

        Reload = 0x2,     ///< Indicates whether to use the cache or to reread
                 ///< the directory from the disk.
                 ///< Use only when opening a dir not yet listed by this lister
                 ///< without using the cache. Otherwise use updateDirectory.
    };

    /**
     * Stores a combination of #OpenUrlFlag values.
     */
    Q_DECLARE_FLAGS(OpenUrlFlags, OpenUrlFlag)

    /**
     * Create a directory lister.
     */
    KCoreDirLister(QObject *parent = nullptr);

    /**
     * Destroy the directory lister.
     */
    virtual ~KCoreDirLister();

    /**
     * Run the directory lister on the given url.
     *
     * This method causes KCoreDirLister to emit _all_ the items of @p _url, in any case.
     * Depending on _flags, either clear() or clear(const QUrl &) will be
     * emitted first.
     *
     * The newItems() signal may be emitted more than once to supply you
     * with KFileItems, up until the signal completed() is emitted
     * (and isFinished() returns true).
     *
     * @param _url     the directory URL.
     * @param _flags   whether to keep previous directories, and whether to reload, see OpenUrlFlags
     * @return true    if successful,
     *         false   otherwise (e.g. invalid @p _url)
     */
    virtual bool openUrl(const QUrl &_url, OpenUrlFlags _flags = NoFlags);

    /**
     * Stop listing all directories currently being listed.
     *
     * Emits canceled() if there was at least one job running.
     * Emits canceled( const QUrl& ) for each stopped job if
     * there are at least two directories being watched by KCoreDirLister.
     */
    virtual void stop();

    /**
     * Stop listing the given directory.
     *
     * Emits canceled() if the killed job was the last running one.
     * Emits canceled( const QUrl& ) for the killed job if
     * there are at least two directories being watched by KCoreDirLister.
     * No signal is emitted if there was no job running for @p _url.
     * @param _url the directory URL
     */
    virtual void stop(const QUrl &_url);

    /**
     * @return true if the "delayed MIME types" feature was enabled
     * @see setDelayedMimeTypes
     */
    bool delayedMimeTypes() const;

    /**
     * Delayed MIME types feature:
     * If enabled, MIME types will be fetched on demand, which leads to a
     * faster initial directory listing, where icons get progressively replaced
     * with the correct one while KMimeTypeResolver is going through the items
     * with unknown or imprecise MIME type (e.g. files with no extension or an
     * unknown extension).
     */
    void setDelayedMimeTypes(bool delayedMimeTypes);

    /**
     * Checks whether KDirWatch will automatically update directories. This is
     * enabled by default.
     * @return true if KDirWatch is used to automatically update directories.
     */
    bool autoUpdate() const;

    /**
     * Enable/disable automatic directory updating, when a directory changes
     * (using KDirWatch).
     * @param enable true to enable, false to disable
     */
    virtual void setAutoUpdate(bool enable);

    /**
     * Checks whether hidden files (files beginning with a dot) will be
     * shown.
     * By default this option is disabled (hidden files will be not shown).
     * @return true if dot files are shown, false otherwise
     * @see setShowingDotFiles()
     */
    bool showingDotFiles() const;

    /**
     * Changes the "is viewing dot files" setting.
     * You need to call emitChanges() afterwards.
     * By default this option is disabled (hidden files will not be shown).
     * @param _showDotFiles true to enable showing hidden files, false to
     *        disable
     * @see showingDotFiles()
     */
    virtual void setShowingDotFiles(bool _showDotFiles);

    /**
     * Checks whether the KCoreDirLister only lists directories or all
     * files.
     * By default this option is disabled (all files will be shown).
     * @return true if setDirOnlyMode(true) was called
     */
    bool dirOnlyMode() const;

    /**
     * Call this to list only directories.
     * You need to call emitChanges() afterwards.
     * By default this option is disabled (all files will be shown).
     * @param dirsOnly true to list only directories
     */
    virtual void setDirOnlyMode(bool dirsOnly);

    /**
     * Returns the top level URL that is listed by this KCoreDirLister.
     * It might be different from the one given with openUrl() if there was a
     * redirection. If you called openUrl() with OpenUrlFlag::Keep this is the
     * first url opened (e.g. in a treeview this is the root).
     *
     * @return the url used by this instance to list the files.
     */
    QUrl url() const;

    /**
     * Returns all URLs that are listed by this KCoreDirLister. This is only
     * useful if you called openUrl() with OpenUrlFlag::Keep, as it happens in a
     * treeview, for example. (Note that the base url is included in the list
     * as well, of course.)
     *
     * @return the list of all listed URLs
     */
    QList<QUrl> directories() const;

    /**
     * Actually emit the changes made with setShowingDotFiles, setDirOnlyMode,
     * setNameFilter and setMimeFilter.
     */
    virtual void emitChanges();

    /**
     * Update the directory @p _dir. This method causes KCoreDirLister to _only_ emit
     * the items of @p _dir that actually changed compared to the current state in the
     * cache and updates the cache.
     *
     * The current implementation calls updateDirectory automatically for
     * local files, using KDirWatch (if autoUpdate() is true), but it might be
     * useful to force an update manually.
     *
     * @param _dir the directory URL
     */
    virtual void updateDirectory(const QUrl &_dir);

    /**
     * Returns true if no io operation is currently in progress.
     * @return true if finished, false otherwise
     */
    bool isFinished() const;

    /**
     * Returns the file item of the URL.
     *
     * Can return an empty KFileItem.
     * @return the file item for url() itself (".")
     */
    KFileItem rootItem() const;

    /**
     * Find an item by its URL.
     * @param _url the item URL
     * @return the KFileItem
     */
    virtual KFileItem findByUrl(const QUrl &_url) const;

    /**
     * Find an item by its name.
     * @param name the item name
     * @return the KFileItem
     */
    virtual KFileItem findByName(const QString &name) const;

    /**
     * Set a name filter to only list items matching this name, e.g. "*.cpp".
     *
     * You can set more than one filter by separating them with whitespace, e.g
     * "*.cpp *.h".
     * Note: the directory is not automatically reloaded.
     * You need to call emitChanges() afterwards.
     *
     * @param filter the new filter, QString() to disable filtering
     * @see matchesFilter
     */
    virtual void setNameFilter(const QString &filter);

    /**
     * Returns the current name filter, as set via setNameFilter()
     * @return the current name filter, can be QString() if filtering
     *         is turned off
     */
    QString nameFilter() const;

    /**
     * Set MIME type based filter to only list items matching the given MIME types.
     *
     * NOTE: setting the filter does not automatically reload directory.
     * Also calling this function will not affect any named filter already set.
     *
     * You need to call emitChanges() afterwards.
     *
     * @param mimeList a list of MIME types
     *
     * @see clearMimeFilter
     * @see matchesMimeFilter
     */
    virtual void setMimeFilter(const QStringList &mimeList);

    /**
     * Filtering should be done with KFileFilter. This will be implemented in a later
     * revision of KCoreDirLister. This method may be removed then.
     *
     * Set MIME type based exclude filter to only list items not matching the given MIME types
     *
     * NOTE: setting the filter does not automatically reload directory.
     * Also calling this function will not affect any named filter already set.
     *
     * @param mimeList a list of MIME types
     * @see clearMimeFilter
     * @see matchesMimeFilter
     * @internal
     */
    void setMimeExcludeFilter(const QStringList &mimeList);

    /**
     * Clears the MIME type based filter.
     *
     * You need to call emitChanges() afterwards.
     *
     * @see setMimeFilter
     */
    virtual void clearMimeFilter();

    /**
     * Returns the list of MIME type based filters, as set via setMimeFilter().
     * @return the list of MIME type based filters. Empty, when no MIME type filter is set.
     */
    QStringList mimeFilters() const;

    /**
     * Checks whether @p name matches a filter in the list of name filters.
     * @return true if @p name matches a filter in the list,
     * otherwise false.
     * @see setNameFilter
     */
    bool matchesFilter(const QString &name) const;

    /**
     * Checks whether @p mimeType matches a filter in the list of MIME types
     * @param mimeType the MIME type to find in the filter list.
     * @return true if @p mimeType matches a filter in the list,
     * otherwise false.
     * @see setMimeFilter.
     */
    bool matchesMimeFilter(const QString &mimeType) const;

    /**
     * Used by items() and itemsForDir() to specify whether you want
     * all items for a directory or just the filtered ones.
     */
    enum WhichItems {
        AllItems = 0,
        FilteredItems = 1,
    };

    /**
     * Returns the items listed for the current url().
     * This method will NOT start listing a directory, you should only call
     * this when receiving the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used
     * by KCoreDirLister.
     *
     * @param which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter(), mimeFilter(),
     *              etc. Note that the latter causes iteration over all the
     *              items, filtering them. If this is too slow for you, use the
     *              newItems() signal, sending out filtered items in chunks.
     * @return the items listed for the current url().
     */
    KFileItemList items(WhichItems which = FilteredItems) const;

    /**
     * Returns the items listed for the given @p dir.
     * This method will NOT start listing @p dir, you should only call
     * this when receiving the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used
     * by KCoreDirLister.
     *
     * @param dir specifies the url for which the items should be returned. This
     *            is only useful if you use KCoreDirLister with multiple URLs
     *            i.e. using bool OpenUrlFlag::Keep in openUrl().
     * @param which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter, mimeFilter, etc.
     *              Note that the latter causes iteration over all the items,
     *              filtering them. If this is too slow for you, use the
     * newItems() signal, sending out filtered items in chunks.
     * @return the items listed for @p dir.
     */
    KFileItemList itemsForDir(const QUrl &dir,
                              WhichItems which = FilteredItems) const;

    /**
     * Return the KFileItem for the given URL, if we listed it recently
     * and it's still in the cache - which is always the case if a directory
     * view is currently showing this item. If not, then it might be in the
     * cache, or it might not, in which case you get a null KFileItem.
     * If you really need a KFileItem for this URL in all cases, then use
     * KIO::stat() instead.
     *
     * @since 4.2
     */
    static KFileItem cachedItemForUrl(const QUrl &url);

Q_SIGNALS:

    /**
     * Tell the view that we started to list @p _url. NOTE: this does _not_ imply that there
     * is really a job running! I.e. KCoreDirLister::jobs() may return an empty list. In this case
     * the items are taken from the cache.
     *
     * The view knows that openUrl should start it, so it might seem useless,
     * but the view also needs to know when an automatic update happens.
     * @param _url the URL to list
     */
    void started(const QUrl &_url);

    /**
     * Tell the view that listing is finished. There are no jobs running anymore.
     */
    void completed();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Tell the view that the listing of the directory @p _url is finished.
     * There might be other running jobs left.
     *
     * @param _url the directory URL
     *
     * @deprecated since 5.79, use KCoreDirLister::listingDirCompleted(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use KCoreDirLister::listingDirCompleted(const QUrl &)")
    void completed(const QUrl &_url);
#endif

    /**
     * Tell the view that the listing of the directory @p _url is finished.
     * There might be other running jobs left.
     *
     * @param _url the directory URL
     *
     * @since 5.79
     */
    void listingDirCompleted(const QUrl &url);

    /**
     * Tell the view that the user canceled the listing. No running jobs are left.
     */
    void canceled();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Tell the view that the listing of the directory @p _url was canceled.
     * There might be other running jobs left.
     *
     * @param _url the directory URL
     *
     * @deprecated since 5.79, use KCoreDirLister::listingDirCanceled(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "use KCoreDirLister::listingDirCanceled(const QUrl &)")
    void canceled(const QUrl &_url);
#endif

    /**
     * Tell the view that the listing of the directory @p _url was canceled.
     * There might be other running jobs left.
     *
     * @param _url the directory URL
     *
     * @since 5.79
     */
    void listingDirCanceled(const QUrl &_url);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Signal a redirection.
     * Only emitted if there's just one directory to list, i.e. most
     * probably openUrl() has been called without OpenUrlFlag::Keep.
     *
     * @param _url the new URL
     *
     * @deprecated since 5.79, use singleUrlRedirection(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use singleUrlRedirection(const QUrl &)")
    void redirection(const QUrl &_url);
#endif

    /**
     * Signal a redirection.
     * Only emitted if there's just one directory to list, i.e. most
     * probably openUrl() has been called without OpenUrlFlag::Keep.
     * @param _url the new URL
     *
     * @since 5.79
     */
    void singleUrlRedirection(const QUrl &_url);

    /**
     * Signal a redirection.
     * @param oldUrl the original URL
     * @param newUrl the new URL
     */
    void redirection(const QUrl &oldUrl, const QUrl &newUrl);

    /**
     * Emitted when all the directories are removed from the cache.
     * Make sure to connect to this signal to avoid having duplicate items.
     */
    void clear();

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Emitted when directory @p _url is removed from the cache.
     * It is only emitted if the lister is holding more than one directory.
     *
     * @param _url the directory that was removed from the cache
     *
     * @deprecated since 5.79, use dirRemovedFromCache(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use dirRemovedFromCache(const QUrl &)")
    void clear(const QUrl &_url);
#endif

    /**
     * Emitted when directory @p _url is removed from the cache.
     * It is only emitted if the lister is holding more than one directory.
     *
     * @param _url the directory that was removed from the cache
     *
     * @since 5.79
     */
    void dirRemovedFromCache(const QUrl &_url);

    /**
     * Signal new items.
     *
     * @param items a list of new items
     */
    void newItems(const KFileItemList &items);

    /**
     * Signal that new items were found during directory listing.
     * Alternative signal emitted at the same time as newItems(),
     * but itemsAdded also passes the url of the parent directory.
     *
     * @param items a list of new items
     * @since 4.2
     */
    void itemsAdded(const QUrl &directoryUrl, const KFileItemList &items);

    /**
     * Send a list of items filtered-out by MIME type.
     * @param items the list of filtered items
     */
    void itemsFilteredByMime(const KFileItemList &items);

    /**
     * Signal that items have been deleted
     *
     * @since 4.1.2
     * @param items the list of deleted items
     */
    void itemsDeleted(const KFileItemList &items);

    /**
     * Signal an item to refresh (its MIME-type/icon/name has changed).
     * Note: KFileItem::refresh has already been called on those items.
     * @param items the items to refresh. This is a list of pairs, where
     * the first item in the pair is the OLD item, and the second item is the
     * NEW item. This allows to track which item has changed, especially after
     * a renaming.
     */
    void refreshItems(const QList<QPair<KFileItem, KFileItem> > &items);

    /**
     * Emitted to display information about running jobs.
     * Examples of message are "Resolving host", "Connecting to host...", etc.
     * @param msg the info message
     */
    void infoMessage(const QString &msg);

    /**
     * Progress signal showing the overall progress of the KCoreDirLister.
     * This allows using a progress bar very easily. (see QProgressBar)
     * @param percent the progress in percent
     */
    void percent(int percent);

    /**
     * Emitted when we know the size of the jobs.
     * @param size the total size in bytes
     */
    void totalSize(KIO::filesize_t size);

    /**
     * Regularly emitted to show the progress of this KCoreDirLister.
     * @param size the processed size in bytes
     */
    void processedSize(KIO::filesize_t size);

    /**
     * Emitted to display information about the speed of the jobs.
     * @param bytes_per_second the speed in bytes/s
     */
    void speed(int bytes_per_second);

protected:
#if KIOCORE_ENABLE_DEPRECATED_SINCE(4, 3)
    /// @deprecated Since 4.3, and unused, ignore this
    enum Changes { NONE = 0, NAME_FILTER = 1, MIME_FILTER = 2, DOT_FILES = 4, DIR_ONLY_MODE = 8 };
#endif

    /**
     * Called for every new item before emitting newItems().
     * You may reimplement this method in a subclass to implement your own
     * filtering.
     * The default implementation filters out ".." and everything not matching
     * the name filter(s)
     * @return true if the item is "ok".
     *         false if the item shall not be shown in a view, e.g.
     * files not matching a pattern *.cpp ( KFileItem::isHidden())
     * @see matchesFilter
     * @see setNameFilter
     */
    virtual bool matchesFilter(const KFileItem &) const;

    /**
     * Called for every new item before emitting newItems().
     * You may reimplement this method in a subclass to implement your own
     * filtering.
     * The default implementation filters out ".." and everything not matching
     * the name filter(s)
     * @return true if the item is "ok".
     *         false if the item shall not be shown in a view, e.g.
     * files not matching a pattern *.cpp ( KFileItem::isHidden())
     * @see matchesMimeFilter
     * @see setMimeFilter
     */
    virtual bool matchesMimeFilter(const KFileItem &) const;

    /**
     * Called by the public matchesFilter() to do the
     * actual filtering. Those methods may be reimplemented to customize
     * filtering.
     * @param name the name to filter
     * @param filters a list of regular expressions for filtering
     */
    // TODO KF6 remove
    virtual bool doNameFilter(const QString &name, const QList<QRegExp> &filters) const;

    /**
     * Called by the public matchesMimeFilter() to do the
     * actual filtering. Those methods may be reimplemented to customize
     * filtering.
     * @param mimeType the MIME type to filter
     * @param filters the list of MIME types to filter
     */
    // TODO KF6 remove
    virtual bool doMimeFilter(const QString &mimeType, const QStringList &filters) const;

    /**
     * Reimplement to customize error handling
     */
    virtual void handleError(KIO::Job *);
    /**
     * Reimplement to customize error handling
     * @since 5.0
     */
    virtual void handleErrorMessage(const QString &message);
    /**
     * Reimplemented by KDirLister to associate windows with jobs
     * @since 5.0
     */
    virtual void jobStarted(KIO::ListJob *);

private:
    friend class KCoreDirListerPrivate;
    std::unique_ptr<KCoreDirListerPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KCoreDirLister::OpenUrlFlags)

#endif
