/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1999 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001, 2002, 2004-2006 Michael Brade <brade@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KCOREDIRLISTER_H
#define KCOREDIRLISTER_H

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

/*!
 * \class KCoreDirLister
 * \inmodule KIOCore
 *
 * \brief Helper class for the kiojob used to list and update a directory.
 *
 * The dir lister deals with the kiojob used to list and update a directory
 * and has signals for the user of this class (e.g. konqueror view or
 * kdesktop) to create/destroy its items when asked.
 *
 * This class is independent from the graphical representation of the dir
 * (icon container, tree view, ...) and it stores the items (as KFileItems).
 *
 * Typical usage:
 * \list
 * \li Create an instance.
 * \li Connect to at least update, clear, itemsAdded, and itemsDeleted.
 * \li Call openUrl - the signals will be called.
 * \li Reuse the instance when opening a new url (openUrl).
 * \li Destroy the instance when not needed anymore (usually destructor).
 * \endlist
 *
 * Advanced usage : call openUrl with OpenUrlFlag::Keep to list directories
 * without forgetting the ones previously read (e.g. for a tree view)
 *
 */
class KIOCORE_EXPORT KCoreDirLister : public QObject
{
    friend class KCoreDirListerCache;
    friend struct KCoreDirListerCacheDirectoryData;

    Q_OBJECT

    /*!
     * \property KCoreDirLister::autoUpdate
     */
    Q_PROPERTY(bool autoUpdate READ autoUpdate WRITE setAutoUpdate)

    /*!
     * \property KCoreDirLister::showHiddenFiles
     */
    Q_PROPERTY(bool showHiddenFiles READ showHiddenFiles WRITE setShowHiddenFiles)

    /*!
     * \property KCoreDirLister::dirOnlyMode
     */
    Q_PROPERTY(bool dirOnlyMode READ dirOnlyMode WRITE setDirOnlyMode)

    /*!
     * \property KCoreDirLister::delayedMimeTypes
     */
    Q_PROPERTY(bool delayedMimeTypes READ delayedMimeTypes WRITE setDelayedMimeTypes)

    /*!
     * \property KCoreDirLister::requestMimeTypeWhileListing
     */
    Q_PROPERTY(bool requestMimeTypeWhileListing READ requestMimeTypeWhileListing WRITE setRequestMimeTypeWhileListing)

    /*!
     * \property KCoreDirLister::nameFilter
     */
    Q_PROPERTY(QString nameFilter READ nameFilter WRITE setNameFilter)

    /*!
     * \property KCoreDirLister::mimeFilter
     */
    Q_PROPERTY(QStringList mimeFilter READ mimeFilters WRITE setMimeFilter RESET clearMimeFilter)

    /*!
     * \property KCoreDirLister::autoErrorHandlingEnabled
     */
    Q_PROPERTY(bool autoErrorHandlingEnabled READ autoErrorHandlingEnabled WRITE setAutoErrorHandlingEnabled)

public:
    /*!
     * \value NoFlags No additional flags specified.
     * \value Keep Previous directories aren't forgotten. (they are still watched by kdirwatch and their items are kept for this KCoreDirLister). This is useful
     * for e.g. a treeview.
     * \value Reload Indicates whether to use the cache or to reread the directory from the disk. Use only when opening a dir not yet listed by this lister
     * without using the cache. Otherwise use updateDirectory.
     */
    enum OpenUrlFlag {
        NoFlags = 0x0,
        Keep = 0x1,
        Reload = 0x2,
    };

    Q_DECLARE_FLAGS(OpenUrlFlags, OpenUrlFlag)

    /*!
     * Create a directory lister.
     */
    KCoreDirLister(QObject *parent = nullptr);

    ~KCoreDirLister() override;

    /*!
     * Run the directory lister on the given url.
     *
     * This method causes KCoreDirLister to emit all the items of \a dirUrl, in any case.
     * Depending on \a flags, either clear() or clearDir(const QUrl &) will be emitted first.
     *
     * The newItems() signal may be emitted more than once to supply you with KFileItems, up
     * until the signal completed() is emitted (and isFinished() returns \c true).
     *
     * \a dirUrl the directory URL.
     *
     * \a flags whether to keep previous directories, and whether to reload, see OpenUrlFlags
     *
     * Returns \c true if successful, \c false otherwise (e.g. if \a dirUrl is invalid)
     */
    bool openUrl(const QUrl &dirUrl, OpenUrlFlags flags = NoFlags); // TODO KF6: change bool to void

    /*!
     * Stop listing all directories currently being listed.
     *
     * Emits canceled() if there was at least one job running.
     * Emits listingDirCanceled(const QUrl &) for each stopped job if there is more than one
     * directory being watched by this KCoreDirLister.
     */
    void stop();

    /*!
     * Stop listing the given directory.
     *
     * Emits canceled() if the killed job was the last running one.
     * Emits listingDirCanceled(const QUrl &) for the killed job if there is more than one
     * directory being watched by this KCoreDirLister.
     *
     * No signal is emitted if there was no job running for \a dirUrl.
     *
     * \a dirUrl the directory URL
     */
    void stop(const QUrl &dirUrl);

    /*!
     * Stop listening for further changes in the given directory.
     * When a new directory is opened with OpenUrlFlag::Keep the caller will keep being notified of file changes for all directories that were kept open.
     * This call selectively removes a directory from sending future notifications to this KCoreDirLister.
     *
     * \a dirUrl the directory URL.
     *
     * \since 5.91
     */
    void forgetDirs(const QUrl &dirUrl);

    /*!
     * Returns \c true if the "delayed MIME types" feature was enabled
     * \sa setDelayedMimeTypes
     */
    bool delayedMimeTypes() const;

    /*!
     * Delayed MIME types feature:
     * If enabled, MIME types will be fetched on demand, which leads to a
     * faster initial directory listing, where icons get progressively replaced
     * with the correct one while you run KFileItem::determineMimeType() through
     * the items with unknown or imprecise MIME type (e.g. files with no extension
     * or an unknown extension).
     */
    void setDelayedMimeTypes(bool delayedMimeTypes);

    /*!
     * Checks whether KDirWatch will automatically update directories. This is
     * enabled by default.
     *
     * Returns \c true if KDirWatch is used to automatically update directories
     */
    bool autoUpdate() const;

    /*!
     * Toggle automatic directory updating, when a directory changes (using KDirWatch).
     *
     * \a enable set to \c true to enable or \c false to disable
     */
    void setAutoUpdate(bool enable);

    /*!
     * Checks whether hidden files (e.g. files whose name start with '.' on Unix) will be shown.
     * By default this option is disabled (hidden files are not shown).
     *
     * Returns \c true if hidden files are shown, \c false otherwise
     *
     * \sa setShowHiddenFiles()
     * \since 5.100
     */
    bool showHiddenFiles() const;

    /*!
     * Toggles whether hidden files (e.g. files whose name start with '.' on Unix) are shown/
     * By default hidden files are not shown.
     *
     * You need to call emitChanges() afterwards.
     *
     * \a showHiddenFiles set to true/false to show/hide hidden files respectively
     *
     * \sa showHiddenFiles()
     * \since 5.100
     */
    void setShowHiddenFiles(bool showHiddenFiles);

    /*!
     * Checks whether this KCoreDirLister only lists directories or all items (directories and
     * files), by default all items are listed.
     *
     * Returns \c true if only directories are listed, \c false otherwise
     *
     * \sa setDirOnlyMode()
     */
    bool dirOnlyMode() const;

    /*!
     * Call this to list only directories (by default all items (directories and files)
     * are listed).
     *
     * You need to call emitChanges() afterwards.
     *
     * \a dirsOnly set to \c true to list only directories
     */
    void setDirOnlyMode(bool dirsOnly);

    /*!
     * Returns true if quickfiltering is enabled.
     *
     * If true, list only files that match the filter text.
     *
     * Returns \c true if any files and folders matching filter are listed, \c false otherwise.
     *
     * \sa setQuickFilterMode(bool)
     * \since 6.14
     */
    bool quickFilterMode() const;

    /*!
     * Call this to set the quick filtering mode on, which will filter through all the
     * items based by their name, ignoring them if the name does not fit.
     *
     * This is equal to the Dolphin quick filter mode.
     *
     * You need to call emitChanges() afterwards.
     *
     * \a quickFilterMode set to \c true to list only files and folders that match the filter text.
     * \since 6.14
     */
    void setQuickFilterMode(bool quickFilterMode);

    /*!
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
     * Returns \c true if the worker is asked for MIME types, \c false otherwise.
     *
     * \sa setRequestMimeTypeWhileListing()
     *
     * \since 5.82
     */
    bool requestMimeTypeWhileListing() const;

    /*!
     * Toggles whether to request MIME types from the worker or in-process.
     *
     * \a request set to \c true to request MIME types from the worker.
     *
     * \note If this is changed while the lister is already listing a directory,
     * it will only have an effect the next time openUrl() is called.
     *
     * \sa requestMimeTypeWhileListing()
     *
     * \since 5.82
     */
    void setRequestMimeTypeWhileListing(bool request);

    /*!
     * Returns the top level URL that is listed by this KCoreDirLister.
     *
     * It might be different from the one given with openUrl() if there was a
     * redirection. If you called openUrl() with OpenUrlFlag::Keep this is the
     * first url opened (e.g. in a treeview this is the root).
     */
    QUrl url() const;

    /*!
     * Returns all URLs that are listed by this KCoreDirLister. This is only
     * useful if you called openUrl() with OpenUrlFlag::Keep, as it happens in a
     * treeview, for example. (Note that the base url is included in the list
     * as well, of course.)
     */
    QList<QUrl> directories() const;

    /*!
     * Actually emit the changes made with setShowHiddenFiles, setDirOnlyMode,
     * setNameFilter and setMimeFilter.
     */
    void emitChanges();

    /*!
     * Update the directory \a dirUrl. This method causes KCoreDirLister to only emit
     * the items of \a dirUrl that actually changed compared to the current state in the
     * cache, and updates the cache.
     *
     * The current implementation calls updateDirectory automatically for local files, using
     * KDirWatch (if autoUpdate() is \c true), but it might be useful to force an update manually.
     *
     * \a dirUrl the directory URL
     */
    void updateDirectory(const QUrl &dirUrl);

    /*!
     * Returns \c true if no I/O operation is currently in progress.
     *
     * Returns \c true if finished, \c false otherwise
     */
    bool isFinished() const;

    /*!
     * Returns the file item of the URL.
     *
     * Can return an empty KFileItem.
     *
     * Returns the file item for url() itself (".")
     */
    KFileItem rootItem() const;

    /*!
     * Find an item by its URL.
     *
     * \a url the item URL
     */
    KFileItem findByUrl(const QUrl &url) const;

    /*!
     * Find an item by its name.
     * \a name the item name
     */
    KFileItem findByName(const QString &name) const;

    /*!
     * Set a name filter to only list items matching this name, e.g. "*.cpp".
     *
     * You can set more than one filter by separating them with whitespace, e.g
     * "*.cpp *.h".
     * \note the directory is not automatically reloaded.
     * You need to call emitChanges() afterwards.
     *
     * \a filter the new filter, QString() to disable filtering
     */
    void setNameFilter(const QString &filter);

    /*!
     * Returns the current name filter, as set via setNameFilter().
     *
     * Can be QString() if filtering is turned off
     */
    QString nameFilter() const;

    /*!
     * Set MIME type based filter to only list items matching the given MIME types.
     *
     * \note Setting the filter does not automatically reload directory.
     * Also calling this function will not affect any named filter already set.
     *
     * You need to call emitChanges() afterwards.
     *
     * \a mimeList a list of MIME types
     *
     * \sa clearMimeFilter
     */
    void setMimeFilter(const QStringList &mimeList);

    /*!
     * Filtering should be done with KFileFilter. This will be implemented in a later
     * revision of KCoreDirLister. This method may be removed then.
     *
     * Set MIME type based exclude filter to only list items not matching the given MIME types
     *
     * \note setting the filter does not automatically reload directory.
     * Also calling this function will not affect any named filter already set.
     *
     * \a mimeList a list of MIME types
     *
     * \sa clearMimeFilter
     */
    void setMimeExcludeFilter(const QStringList &mimeList);

    /*!
     * Clears the MIME type based filter.
     *
     * You need to call emitChanges() afterwards.
     *
     * \sa setMimeFilter
     */
    void clearMimeFilter();

    /*!
     * Returns the list of MIME type based filters, as set via setMimeFilter().
     *
     * Empty, when no MIME type filter is set.
     */
    QStringList mimeFilters() const;

    /*!
     * Used by items() and itemsForDir() to specify whether you want
     * all items for a directory or just the filtered ones.
     *
     * \value AllItems
     * \value FilteredItems
     */
    enum WhichItems {
        AllItems = 0,
        FilteredItems = 1,
    };

    /*!
     * Returns the items listed for the current url().
     *
     * This method will not start listing a directory, you should only call
     * this in a slot connected to the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used by KCoreDirLister.
     *
     * \a which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter(), mimeFilter(),
     *              etc. Note that the latter causes iteration over all the
     *              items, filtering them. If this is too slow for you, use the
     *              newItems() signal, sending out filtered items in chunks
     */
    KFileItemList items(WhichItems which = FilteredItems) const;

    /*!
     * Returns the items listed for the given \a dirUrl.
     *
     * This method will not start listing \a dirUrl, you should only call
     * this in a slot connected to the finished() signal.
     *
     * The items in the KFileItemList are copies of the items used by KCoreDirLister.
     *
     * \a dirUrl specifies the url for which the items should be returned. This
     *            is only useful if you use KCoreDirLister with multiple URLs
     *            i.e. using bool OpenUrlFlag::Keep in openUrl()
     *
     * \a which specifies whether the returned list will contain all entries
     *              or only the ones that passed the nameFilter, mimeFilter, etc.
     *              Note that the latter causes iteration over all the items,
     *              filtering them. If this is too slow for you, use the
     * newItems() signal, sending out filtered items in chunks
     *
     * Returns the items listed for \a dirUrl
     */
    KFileItemList itemsForDir(const QUrl &dirUrl, WhichItems which = FilteredItems) const;

    /*!
     * Return the KFileItem for the given URL, if it was listed recently and it's
     * still in the cache, which is always the case if a directory view is currently
     * showing this item. If not, then it might be in the cache; if not in the cache a
     * a null KFileItem will be returned.
     *
     * If you really need a KFileItem for this URL in all cases, then use KIO::stat() instead.
     *
     */
    static KFileItem cachedItemForUrl(const QUrl &url);

    /*!
     * Checks whether auto error handling is enabled.
     *
     * If enabled, it will show an error dialog to the user when an
     * error occurs (assuming the application links to KIOWidgets).
     * It is turned on by default.
     *
     * Returns \c true if auto error handling is enabled, \c false otherwise
     * \sa setAutoErrorHandlingEnabled()
     * \since 5.82
     */
    bool autoErrorHandlingEnabled() const;

    /*!
     * Enable or disable auto error handling.
     *
     * If enabled, it will show an error dialog to the user when an
     * error occurs. It is turned on by default.
     *
     * \a enable true to enable auto error handling, false to disable
     *
     * \a parent the parent widget for the error dialogs, can be \c nullptr for
     *               top-level
     * \sa autoErrorHandlingEnabled()
     * \since 5.82
     */
    void setAutoErrorHandlingEnabled(bool enable);

Q_SIGNALS:
    /*!
     * Tell the view that this KCoreDirLister has started to list \a dirUrl. Note that this
     * does not imply that there is really a job running! I.e. KCoreDirLister::jobs()
     * may return an empty list, in which case the items are taken from the cache.
     *
     * The view knows that openUrl should start it, so this might seem useless, but the view
     * also needs to know when an automatic update happens.
     *
     * \a dirUrl the URL to list
     */
    void started(const QUrl &dirUrl);

    /*!
     * Tell the view that listing is finished. There are no jobs running anymore.
     */
    void completed();

    /*!
     * Tell the view that the listing of the directory \a dirUrl is finished.
     * There might be other running jobs left.
     *
     * \a dirUrl the directory URL
     *
     * \since 5.79
     */
    void listingDirCompleted(const QUrl &dirUrl);

    /*!
     * Tell the view that the user canceled the listing. No running jobs are left.
     */
    void canceled();

    /*!
     * Tell the view that the listing of the directory \a dirUrl was canceled.
     * There might be other running jobs left.
     *
     * \a dirUrl the directory URL
     *
     * \since 5.79
     */
    void listingDirCanceled(const QUrl &dirUrl);

    /*!
     * Signals a redirection.
     *
     * \a oldUrl the original URL
     *
     * \a newUrl the new URL
     */
    void redirection(const QUrl &oldUrl, const QUrl &newUrl);

    /*!
     * Signals to the view to remove all items (when e.g. going from dirA to dirB).
     * Make sure to connect to this signal to avoid having duplicate items in the view.
     */
    void clear();

    /*!
     * Signals to the view to clear all items from directory \a dirUrl.
     *
     * This is only emitted if the lister is holding more than one directory.
     *
     * \a dirUrl the directory that the view should clear all items from
     *
     * \since 5.79
     */
    void clearDir(const QUrl &dirUrl);

    /*!
     * Signal new items.
     *
     * \a items a list of new items
     */
    void newItems(const KFileItemList &items);

    /*!
     * Signal that new items were found during directory listing.
     * Alternative signal emitted at the same time as newItems(),
     * but itemsAdded also passes the url of the parent directory.
     *
     * \a items a list of new items
     */
    void itemsAdded(const QUrl &directoryUrl, const KFileItemList &items);

    /*!
     * Send a list of items filtered-out by MIME type.
     *
     * \a items the list of filtered items
     */
    void itemsFilteredByMime(const KFileItemList &items);

    /*!
     * Signal that items have been deleted
     *
     * \a items the list of deleted items
     * \since 4.1.2
     */
    void itemsDeleted(const KFileItemList &items);

    /*!
     * Signal an item to refresh (its MIME-type/icon/name has changed).
     *
     * \note KFileItem::refresh has already been called on those items.
     *
     * \a items the items to refresh. This is a list of pairs, where
     * the first item in the pair is the OLD item, and the second item is the
     * NEW item. This allows to track which item has changed, especially after
     * a renaming.
     */
    void refreshItems(const QList<QPair<KFileItem, KFileItem>> &items);

    /*!
     * Emitted to display information about running jobs.
     *
     * Examples of message are "Resolving host", "Connecting to host...", etc.
     *
     * \a msg the info message
     */
    void infoMessage(const QString &msg);

    /*!
     * Progress signal showing the overall progress of the KCoreDirLister.
     *
     * This allows using a progress bar very easily. (see QProgressBar)
     *
     * \a percent the progress in percent
     */
    void percent(int percent);

    /*!
     * Emitted when we know the size of the jobs.
     *
     * \a size the total size in bytes
     */
    void totalSize(KIO::filesize_t size);

    /*!
     * Regularly emitted to show the progress of this KCoreDirLister.
     *
     * \a size the processed size in bytes
     */
    void processedSize(KIO::filesize_t size);

    /*!
     * Emitted to display information about the speed of the jobs.
     *
     * \a bytes_per_second the speed in bytes/s
     */
    void speed(int bytes_per_second);

    /*!
     * Emitted if listing a directory fails with an error.
     *
     * A typical implementation in a widgets-based application
     * would show a message box by calling this in a slot connected to this signal:
     *
     * \code
     * job->uiDelegate()->showErrorMessage()
     * \endcode
     *
     * Many applications might prefer to embed the error message though
     * (e.g. by using the KMessageWidget class, from the KWidgetsAddons Framework).
     *
     * \a the job with an error
     * \since 5.82
     */
    void jobError(KIO::Job *job);

protected:
    /*!
     * Reimplemented by KDirLister to associate windows with jobs
     * \since 5.0
     */
    virtual void jobStarted(KIO::ListJob *);

private:
    friend class KCoreDirListerPrivate;
    std::unique_ptr<KCoreDirListerPrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(KCoreDirLister::OpenUrlFlags)

#endif
