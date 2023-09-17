/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2002, 2004-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KCOREDIRLISTER_H
#define KCOREDIRLISTER_H

#include "kdirnotify.h" // TODO KF6: remove
#include "kfileitem.h"

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
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
    Q_PROPERTY(bool showingDotFiles READ showingDotFiles WRITE setShowingDotFiles)
#endif
    Q_PROPERTY(bool showHiddenFiles READ showHiddenFiles WRITE setShowHiddenFiles)
    Q_PROPERTY(bool dirOnlyMode READ dirOnlyMode WRITE setDirOnlyMode)
    Q_PROPERTY(bool delayedMimeTypes READ delayedMimeTypes WRITE setDelayedMimeTypes)
    Q_PROPERTY(bool requestMimeTypeWhileListing READ requestMimeTypeWhileListing WRITE setRequestMimeTypeWhileListing)
    Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter)
    Q_PROPERTY(QStringList mimeFilter READ mimeFilters WRITE setMimeFilter RESET clearMimeFilter)
    Q_PROPERTY(bool autoErrorHandlingEnabled READ autoErrorHandlingEnabled WRITE setAutoErrorHandlingEnabled)

public:
    /**
     * @see OpenUrlFlags
     */
    enum OpenUrlFlag {
        NoFlags = 0x0, ///< No additional flags specified.

        Keep = 0x1, ///< Previous directories aren't forgotten
        ///< (they are still watched by kdirwatch and their items
        ///< are kept for this KCoreDirLister). This is useful for e.g.
        ///< a treeview.

        Reload = 0x2, ///< Indicates whether to use the cache or to reread
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
    ~KCoreDirLister() override;

    /**
     * Run the directory lister on the given url.
     *
     * This method causes KCoreDirLister to emit @em all the items of @p dirUrl, in any case.
     * Depending on @p flags, either clear() or clearDir(const QUrl &) will be emitted first.
     *
     * The newItems() signal may be emitted more than once to supply you with KFileItems, up
     * until the signal completed() is emitted (and isFinished() returns @c true).
     *
     * @param dirUrl the directory URL.
     * @param flags whether to keep previous directories, and whether to reload, see OpenUrlFlags
     * @return @c true if successful, @c false otherwise (e.g. if @p dirUrl is invalid)
     *
     * @note clearDir(const QUrl &) is emitted since 5.79; before that it was clear(const QUrl &)
     * which has been deprecated.
     */
    virtual bool openUrl(const QUrl &dirUrl, OpenUrlFlags flags = NoFlags); // TODO KF6: remove virtual, change bool to void

    /**
     * Stop listing all directories currently being listed.
     *
     * Emits canceled() if there was at least one job running.
     * Emits listingDirCanceled(const QUrl &) for each stopped job if there is more than one
     * directory being watched by this KCoreDirLister.
     *
     * @note listingDirCanceled(const QUrl &) is emitted since 5.79; before that it was
     * canceled(const QUrl &) which has been deprecated.
     */
    virtual void stop(); // TODO KF6: remove virtual

    /**
     * Stop listing the given directory.
     *
     * Emits canceled() if the killed job was the last running one.
     * Emits listingDirCanceled(const QUrl &) for the killed job if there is more than one
     * directory being watched by this KCoreDirLister.
     *
     * No signal is emitted if there was no job running for @p dirUrl.
     *
     * @param dirUrl the directory URL
     *
     * @note listingDirCanceled(const QUrl &) is emitted since 5.79; before that it was
     * canceled(const QUrl &) which has been deprecated.
     */
    virtual void stop(const QUrl &dirUrl); // TODO KF6: remove virtual

    /**
     * Stop listening for further changes in the given directory.
     * When a new directory is opened with OpenUrlFlag::Keep the caller will keep being notified of file changes for all directories that were kept open.
     * This call selectively removes a directory from sending future notifications to this KCoreDirLister.
     *
     * @param dirUrl the directory URL.
     * @since 5.91
     */
    void forgetDirs(const QUrl &dirUrl);

    /**
     * @return @c true if the "delayed MIME types" feature was enabled
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
     *
     * @return @c true if KDirWatch is used to automatically update directories
     */
    bool autoUpdate() const;

    /**
     * Toggle automatic directory updating, when a directory changes (using KDirWatch).
     *
     * @param enable set to @c true to enable or @c false to disable
     */
    virtual void setAutoUpdate(bool enable); // TODO KF6: remove virtual

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
    /**
     * Checks whether hidden files (files whose name start with '.') will be shown.
     * By default this option is disabled (hidden files are not shown).
     *
     * @return @c true if dot files are shown, @c false otherwise
     *
     * @deprecated since 5.100, use showHiddenFiles instead.
     *
     * @see setShowingDotFiles()
     */
    KIOCORE_DEPRECATED_VERSION(5, 100, "Use showHiddenFiles instead.")
    bool showingDotFiles() const;
#endif

    /**
     * Checks whether hidden files (e.g. files whose name start with '.' on Unix) will be shown.
     * By default this option is disabled (hidden files are not shown).
     *
     * @return @c true if hidden files are shown, @c false otherwise
     *
     * @see setShowHiddenFiles()
     * @since 5.100
     */
    bool showHiddenFiles() const;

    // Not _ENABLED_ because this is a virtual method
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 100)
    /**
     * Toggles whether hidden files (files whose name start with '.') are shown, by default
     * hidden files are not shown.
     *
     * You need to call emitChanges() afterwards.
     *
     * @param showDotFiles set to @c true/false to show/hide hidden files respectively
     *
     * @deprecated since 5.100, use setShowHiddenFiles() instead.
     *
     * @see showingDotFiles()
     */
    KIOCORE_DEPRECATED_VERSION(5, 100, "Use setShowHiddenFiles instead.")
    virtual void setShowingDotFiles(bool showDotFiles);
#endif

    /**
     * Toggles whether hidden files (e.g. files whose name start with '.' on Unix) are shown/
     * By default hidden files are not shown.
     *
     * You need to call emitChanges() afterwards.
     *
     * @param showHiddenFiles set to @c true/false to show/hide hidden files respectively
     *
     * @see showHiddenFiles()
     * @since 5.100
     */
    void setShowHiddenFiles(bool showHiddenFiles);

    /**
     * Checks whether this KCoreDirLister only lists directories or all items (directories and
     * files), by default all items are listed.
     *
     * @return @c true if only directories are listed, @c false otherwise
     *
     * @see setDirOnlyMode(bool)
     */
    bool dirOnlyMode() const;

    /**
     * Call this to list only directories (by default all items (directories and files)
     * are listed).
     *
     * You need to call emitChanges() afterwards.
     *
     * @param dirsOnly set to @c true to list only directories
     */
    virtual void setDirOnlyMode(bool dirsOnly); // TODO KF6: remove virtual

    /**
     * Checks whether this KCoreDirLister requests the MIME type of files from the worker.
     *
     * Enabling this will tell the worker used for listing that it should try to
     * determine the mime type of entries while listing them. This potentially
     * reduces the speed at which entries are listed but ensures mime types are
     * immediately available when an entry is added, greatly speeding up things
     * like mime type filtering.
     *
     * By default this is disabled.
     *
     * @return @c true if the worker is asked for MIME types, @c false otherwise.
     *
     * @see setRequestMimeTypeWhileListing(bool)
     *
     * @since 5.82
     */
    bool requestMimeTypeWhileListing() const;

    /**
     * Toggles whether to request MIME types from the worker or in-process.
     *
     * @param request set to @c true to request MIME types from the worker.
     *
     * @note If this is changed while the lister is already listing a directory,
     * it will only have an effect the next time openUrl() is called.
     *
     * @see requestMimeTypeWhileListing()
     *
     * @since 5.82
     */
    void setRequestMimeTypeWhileListing(bool request);

    /**
     * Returns the top level URL that is listed by this KCoreDirLister.
     *
     * It might be different from the one given with openUrl() if there was a
     * redirection. If you called openUrl() with OpenUrlFlag::Keep this is the
     * first url opened (e.g. in a treeview this is the root).
     *
     * @return the url used by this instance to list the files
     */
    QUrl url() const;

    /**
     * Returns all URLs that are listed by this KCoreDirLister. This is only
     * useful if you called openUrl() with OpenUrlFlag::Keep, as it happens in a
     * treeview, for example. (Note that the base url is included in the list
     * as well, of course.)
     *
     * @return a list of all listed URLs
     */
    QList<QUrl> directories() const;

    /**
     * Actually emit the changes made with setShowHiddenFiles, setDirOnlyMode,
     * setNameFilter and setMimeFilter.
     */
    virtual void emitChanges(); // TODO KF6: remove virtual

    /**
     * Update the directory @p dirUrl. This method causes KCoreDirLister to @em only emit
     * the items of @p dirUrl that actually changed compared to the current state in the
     * cache, and updates the cache.
     *
     * The current implementation calls updateDirectory automatically for local files, using
     * KDirWatch (if autoUpdate() is @c true), but it might be useful to force an update manually.
     *
     * @param dirUrl the directory URL
     */
    virtual void updateDirectory(const QUrl &dirUrl); // TODO KF6: remove virtual

    /**
     * Returns @c true if no I/O operation is currently in progress.
     *
     * @return @c true if finished, @c false otherwise
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
     * @param url the item URL
     * @return the KFileItem
     */
    virtual KFileItem findByUrl(const QUrl &url) const; // TODO KF6: remove virtual

    /**
     * Find an item by its name.
     * @param name the item name
     * @return the KFileItem
     */
    virtual KFileItem findByName(const QString &name) const; // TODO KF6: remove virtual

    /**
     * Set a name filter to only list items matching this name, e.g.\ "*.cpp".
     *
     * You can set more than one filter by separating them with whitespace, e.g
     * "*.cpp *.h".
     * Note: the directory is not automatically reloaded.
     * You need to call emitChanges() afterwards.
     *
     * @param filter the new filter, QString() to disable filtering
     * @see matchesFilter
     */
    virtual void setNameFilter(const QString &filter); // TODO KF6: remove virtual

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
    virtual void setMimeFilter(const QStringList &mimeList); // TODO KF6: remove virtual

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
     */
    void setMimeExcludeFilter(const QStringList &mimeList);

    /**
     * Clears the MIME type based filter.
     *
     * You need to call emitChanges() afterwards.
     *
     * @see setMimeFilter
     */
    virtual void clearMimeFilter(); // TODO KF6: remove virtual

    /**
     * Returns the list of MIME type based filters, as set via setMimeFilter().
     * @return the list of MIME type based filters. Empty, when no MIME type filter is set.
     */
    QStringList mimeFilters() const;

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 94)
    /**
     * Checks whether @p name matches a filter in the list of name filters.
     *
     * @return @c true if @p name matches a filter in the list, otherwise @c false.
     *
     * @deprecated since 5.94, no known users.
     *
     * @see setNameFilter()
     */
    KIOCORE_DEPRECATED_VERSION(5, 94, "No known users.")
    bool matchesFilter(const QString &name) const;
#endif

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 94)
    /**
     * Checks whether @p mimeType matches a filter in the list of MIME types.
     *
     * @param mimeType the MIME type to find in the filter list
     *
     * @return @c true if @p mimeType matches a filter in the list, otherwise @c false
     *
     * @deprecated since 5.94, no known users.
     *
     * @see setMimeFilter.
     */
    KIOCORE_DEPRECATED_VERSION(5, 94, "No known users.")
    bool matchesMimeFilter(const QString &mimeType) const;
#endif

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
     *
     * This method will @em not start listing a directory, you should only call
     * this in a slot connected to the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used by KCoreDirLister.
     *
     * @param which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter(), mimeFilter(),
     *              etc. Note that the latter causes iteration over all the
     *              items, filtering them. If this is too slow for you, use the
     *              newItems() signal, sending out filtered items in chunks
     * @return the items listed for the current url()
     */
    KFileItemList items(WhichItems which = FilteredItems) const;

    /**
     * Returns the items listed for the given @p dirUrl.
     * This method will @em not start listing @p dirUrl, you should only call
     * this in a slot connected to the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used by KCoreDirLister.
     *
     * @param dirUrl specifies the url for which the items should be returned. This
     *            is only useful if you use KCoreDirLister with multiple URLs
     *            i.e. using bool OpenUrlFlag::Keep in openUrl()
     * @param which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter, mimeFilter, etc.
     *              Note that the latter causes iteration over all the items,
     *              filtering them. If this is too slow for you, use the
     * newItems() signal, sending out filtered items in chunks
     *
     * @return the items listed for @p dirUrl
     */
    KFileItemList itemsForDir(const QUrl &dirUrl, WhichItems which = FilteredItems) const;

    /**
     * Return the KFileItem for the given URL, if it was listed recently and it's
     * still in the cache, which is always the case if a directory view is currently
     * showing this item. If not, then it might be in the cache; if not in the cache a
     * a null KFileItem will be returned.
     *
     * If you really need a KFileItem for this URL in all cases, then use KIO::stat() instead.
     *
     * @since 4.2
     */
    static KFileItem cachedItemForUrl(const QUrl &url);

    /**
     * Checks whether auto error handling is enabled.
     * If enabled, it will show an error dialog to the user when an
     * error occurs (assuming the application links to KIOWidgets).
     * It is turned on by default.
     * @return @c true if auto error handling is enabled, @c false otherwise
     * @see setAutoErrorHandlingEnabled()
     * @since 5.82
     */
    bool autoErrorHandlingEnabled() const;

    /**
     * Enable or disable auto error handling.
     * If enabled, it will show an error dialog to the user when an
     * error occurs. It is turned on by default.
     * @param enable true to enable auto error handling, false to disable
     * @param parent the parent widget for the error dialogs, can be @c nullptr for
     *               top-level
     * @see autoErrorHandlingEnabled()
     * @since 5.82
     */
    void setAutoErrorHandlingEnabled(bool enable);

Q_SIGNALS:
    /**
     * Tell the view that this KCoreDirLister has started to list @p dirUrl. Note that this
     * does @em not imply that there is really a job running! I.e. KCoreDirLister::jobs()
     * may return an empty list, in which case the items are taken from the cache.
     *
     * The view knows that openUrl should start it, so this might seem useless, but the view
     * also needs to know when an automatic update happens.
     * @param dirUrl the URL to list
     */
    void started(const QUrl &dirUrl);

    /**
     * Tell the view that listing is finished. There are no jobs running anymore.
     */
    void completed(); // clazy:exclude=overloaded-signal

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Tell the view that the listing of the directory @p dirUrl is finished.
     * There might be other running jobs left.
     *
     * @param dirUrl the directory URL
     *
     * @deprecated since 5.79, use KCoreDirLister::listingDirCompleted(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use KCoreDirLister::listingDirCompleted(const QUrl &)")
    void completed(const QUrl &dirUrl); // clazy:exclude=overloaded-signal
#endif

    /**
     * Tell the view that the listing of the directory @p dirUrl is finished.
     * There might be other running jobs left.
     *
     * @param dirUrl the directory URL
     *
     * @since 5.79
     */
    void listingDirCompleted(const QUrl &dirUrl);

    /**
     * Tell the view that the user canceled the listing. No running jobs are left.
     */
    void canceled(); // clazy:exclude=overloaded-signal

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Tell the view that the listing of the directory @p dirUrl was canceled.
     * There might be other running jobs left.
     *
     * @param dirUrl the directory URL
     *
     * @deprecated since 5.79, use KCoreDirLister::listingDirCanceled(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "use KCoreDirLister::listingDirCanceled(const QUrl &)")
    void canceled(const QUrl &dirUrl); // clazy:exclude=overloaded-signal
#endif

    /**
     * Tell the view that the listing of the directory @p dirUrl was canceled.
     * There might be other running jobs left.
     *
     * @param dirUrl the directory URL
     *
     * @since 5.79
     */
    void listingDirCanceled(const QUrl &dirUrl);

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 80)
    /**
     * Signal a redirection.
     * Only emitted if there's just one directory to list, i.e. most
     * probably openUrl() has been called without OpenUrlFlag::Keep.
     *
     * @param dirUrl the new URL
     *
     * @deprecated since 5.80, use redirection(const QUrl &, const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 80, "Use redirection(const QUrl &, const QUrl &)")
    void redirection(const QUrl &_url); // clazy:exclude=overloaded-signal
#endif

    /**
     * Signals a redirection.
     *
     * @param oldUrl the original URL
     * @param newUrl the new URL
     */
    void redirection(const QUrl &oldUrl, const QUrl &newUrl); // clazy:exclude=overloaded-signal

    /**
     * Signals to the view to remove all items (when e.g.\ going from dirA to dirB).
     * Make sure to connect to this signal to avoid having duplicate items in the view.
     */
    void clear(); // clazy:exclude=overloaded-signal

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 79)
    /**
     * Signals to the view to clear all items from directory @p dirUrl.
     *
     * This is only emitted if the lister is holding more than one directory.
     *
     * @param dirUrl the directory that the view should clear all items from
     *
     * @deprecated since 5.79, use clearDir(const QUrl &)
     */
    KIOCORE_DEPRECATED_VERSION(5, 79, "Use clearDir(const QUrl &)")
    void clear(const QUrl &dirUrl); // clazy:exclude=overloaded-signal
#endif

    /**
     * Signals to the view to clear all items from directory @p dirUrl.
     *
     * This is only emitted if the lister is holding more than one directory.
     *
     * @param dirUrl the directory that the view should clear all items from
     *
     * @since 5.79
     */
    void clearDir(const QUrl &dirUrl);

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

#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
    /**
     * Send a list of items filtered-out by MIME type.
     * @param items the list of filtered items
     *
     * @deprecated since 5.100, no known users.
     */
    KIOCORE_DEPRECATED_VERSION(5, 100, "No known users.")
    void itemsFilteredByMime(const KFileItemList &items);
#endif

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
    void refreshItems(const QList<QPair<KFileItem, KFileItem>> &items);

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

    /**
     * Emitted if listing a directory fails with an error.
     * A typical implementation in a widgets-based application
     * would show a message box by calling this in a slot connected to this signal:
     * <tt>job->uiDelegate()->showErrorMessage()</tt>
     * Many applications might prefer to embed the error message though
     * (e.g. by using the KMessageWidget class, from the KWidgetsAddons Framework).
     * @param the job with an error
     * @since 5.82
     */
    void jobError(KIO::Job *job);

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
     * @return @c true if the item is "ok".
     *         @c false if the item shall not be shown in a view, e.g.
     * files not matching a pattern *.cpp ( KFileItem::isHidden())
     * @see matchesFilter
     * @see setNameFilter
     */
    virtual bool matchesFilter(const KFileItem &) const;

    /**
     * Called for every new item before emitting newItems().
     * You may reimplement this method in a subclass to implement your own
     * filtering.
     * The default implementation filters out everything not matching
     * the mime filter(s)
     * @return @c true if the item is "ok".
     *         @c false if the item shall not be shown in a view, e.g.
     * files not matching the mime filter
     * @see matchesMimeFilter
     * @see setMimeFilter
     */
    virtual bool matchesMimeFilter(const KFileItem &) const;

#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 90)
    /**
     * Called by the public matchesFilter() to do the
     * actual filtering. Those methods may be reimplemented to customize
     * filtering.
     * @param name the name to filter
     * @param filters a list of regular expressions for filtering
     *
     * @deprecated Since 5.90, removed from the public API as it has no users.
     */
    // TODO KF6 remove
    KIOCORE_DEPRECATED_VERSION(5, 90, "Removed from the public API as it has no users.")
    virtual bool doNameFilter(const QString &name, const QList<QRegExp> &filters) const;
#endif

    /**
     * Called by the public matchesMimeFilter() to do the
     * actual filtering. Those methods may be reimplemented to customize
     * filtering.
     * @param mimeType the MIME type to filter
     * @param filters the list of MIME types to filter
     */
    // TODO KF6 remove
    virtual bool doMimeFilter(const QString &mimeType, const QStringList &filters) const;

    // Not _ENABLED_ because this is a virtual method
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 82)
    /**
     * Reimplement to customize error handling
     * @deprecated since 5.82, connect to the jobError() signal instead
     */
    KIOCORE_DEPRECATED_VERSION(5, 82, "Connect to the jobError() signal instead")
    virtual void handleError(KIO::Job *);
#endif

    // Not _ENABLED_ because this is a virtual method
#if KIOCORE_BUILD_DEPRECATED_SINCE(5, 81)
    /**
     * Reimplement to customize error handling
     * @deprecated since 5.81, use handleError. Since 5.82, jobError() is emitted instead for the two
     * cases where handleErrorMessage was emitted (invalid URL, protocol not supporting listing).
     */
    KIOCORE_DEPRECATED_VERSION(5, 81, "For 5.81 and older releases use handleError(); since 5.82 connect to the jobError() signal instead")
    virtual void handleErrorMessage(const QString &message);
#endif

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
