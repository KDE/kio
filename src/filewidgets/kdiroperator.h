// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1999 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000, 2001 Carsten Pfeiffer <pfeiffer@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef KDIROPERATOR_H_
#define KDIROPERATOR_H_

#include "kiofilewidgets_export.h"
#include <kfile.h>

#include <QStyleOptionViewItem>
#include <QUrl>
#include <QWidget>

class QAbstractItemView;
class QMenu;
class QModelIndex;
class QProgressBar;

class KActionCollection;
class KActionMenu;
class KConfigGroup;
class KFileItemList;
class KFilePreviewGenerator;
class KPreviewWidgetBase;

#include <QStack> // SIC TODO KF6: Not used, remove
#include <KToggleAction> // SIC TODO KF6: Not used, remove

// SIC TODO KF6: forward-declaration is enough for these three
#include <KCompletion>
#include <KDirLister>
#include <KFileItem>

namespace KIO
{
class CopyJob;
class DeleteJob;
}

class KDirOperatorPrivate;

/**
 * @class KDirOperator kdiroperator.h <KDirOperator>
 *
 * This widget works as a network transparent filebrowser. You specify a URL
 * to display and this url will be loaded via KDirLister. The user can
 * browse through directories, highlight and select files, delete or rename
 * files.
 *
 * It supports different views, e.g. a detailed view (see KFileDetailView),
 * a simple icon view (see KFileIconView), a combination of two views,
 * separating directories and files ( KCombiView).
 *
 * Additionally, a preview view is available (see KFilePreview), which can
 * show either a simple or detailed view and additionally a preview widget
 * (see setPreviewWidget()). KImageFilePreview is one implementation
 * of a preview widget, that displays previews for all supported filetypes
 * utilizing KIO::PreviewJob.
 *
 * Currently, those classes don't support Drag&Drop out of the box -- there
 * you have to use your own view-classes. You can use some DnD-aware views
 * from Björn Sahlström <bjorn@kbear.org> until they will be integrated
 * into this library. See http://devel-home.kde.org/~pfeiffer/DnD-classes.tar.gz
 *
 * This widget is the one used in the KFileWidget.
 *
 * Basic usage is like this:
 * \code
 *   KDirOperator *op = new KDirOperator(QUrl("file:///home/gis"), this);
 *   // some signals you might be interested in
 *   connect(op, &KDirOperator::urlEntered, this, [this](const QUrl &url) { slotUrlEntered(url); });
 *   connect(op, &KDirOperator::fileHighlighted, this, [this](const KFileItem &item) { slotFileHighlighted(item) });
 *   connect(op, &KDirOperator::fileSelected, this, [this](const KFileItem &item) { slotFileSelected(item) });
 *   connect(op, &KDirOperator::finishedLoading, this, [this]() { slotLoadingFinished(); };
 *
 *   KConfigGroup grp(KSharedConfig::openConfig(),"Your KDiroperator ConfigGroup" );
 *   op->readConfig( &grp);
 *   op->setView(KFile::Default);
 * \endcode
 *
 * This will create a childwidget of 'this' showing the directory contents
 * of /home/gis in the default-view. The view is determined by the readConfig()
 * call, which will read the KDirOperator settings, the user left your program
 * with (and which you saved with op->writeConfig()).
 *
 * @short A widget for displaying files and browsing directories.
 * @author Stephan Kulow <coolo@kde.org>, Carsten Pfeiffer <pfeiffer@kde.org>
 */
class KIOFILEWIDGETS_EXPORT KDirOperator : public QWidget
{
    Q_OBJECT

public:
    /**
     * The various action types. These values can be or'd together
     */
    enum ActionType {
        SortActions =  1,
        ViewActions =  2,
        NavActions  =  4,
        FileActions =  8,
        AllActions  = 15,
    };

    /**
     * Constructs the KDirOperator with no initial view. As the views are
     * configurable, call readConfig() to load the user's configuration
     * and then setView to explicitly set a view.
     *
     * This constructor doesn't start loading the url, setView will do it.
     */
    explicit KDirOperator(const QUrl &urlName  = QUrl{}, QWidget *parent = nullptr);
    /**
     * Destroys the KDirOperator.
     */
    virtual ~KDirOperator();

    /**
     * Enables/disables showing hidden files.
     */
    virtual void setShowHiddenFiles(bool s);

    /**
     * @returns true when hidden files are shown or false otherwise.
     */
    bool showHiddenFiles() const;

    /**
     * Stops loading immediately. You don't need to call this, usually.
     */
    void close();

    /**
     * Sets a filter like "*.cpp *.h *.o". Only files matching that filter
     * will be shown.
     *
     * @see KDirLister::setNameFilter
     * @see nameFilter
     */
    void setNameFilter(const QString &filter);

    /**
     * @returns the current namefilter.
     * @see setNameFilter
     */
    QString nameFilter() const;

    /**
     * Sets a list of MIME types as filter. Only files of those MIME types
     * will be shown.
     *
     * Example:
     * \code
     * QStringList filter;
     * filter << "text/html" << "image/png" << "inode/directory";
     * dirOperator->setMimefilter( filter );
     * \endcode
     *
     * Node: Without the MIME type inode/directory, only files would be shown.
     * Call updateDir() to apply it.
     *
     * @see KDirLister::setMimeFilter
     * @see mimeFilter
     */
    void setMimeFilter(const QStringList &mimetypes);

    /**
     * @returns the current MIME type filter.
     */
    QStringList mimeFilter() const;

    /**
     * Only show the files in a given set of MIME types.
     * This is useful in specialized applications (while file managers, on
     * the other hand, want to show all MIME types). Internally uses
     * KNewFileMenu::setSupportedMimeTypes
     *
     * Example:
     * \code
     * QStringList mimeTypes;
     * mimeTypes << "text/html" << "inode/directory";
     * dirOperator->setNewFileMenuSupportedMimeTypes(mimeTypes);
     * \endcode
     *
     * Note: If the list is empty, all options will be shown. Otherwise,
     * without the MIME type inode/directory, only file options will be shown.
     *
     * @see KNewFileMenu::setSupportedMimeTypes
     * @see newFileMenuSupportedMimeTypes
     * @since 4.5
     */
    void setNewFileMenuSupportedMimeTypes(const QStringList &mime);

    /**
     * @returns the current Supported Mimes Types.
     * @since 4.5
     */
    QStringList newFileMenuSupportedMimeTypes() const;

    /**
     * Setting this to true will make a directory get selected when trying to create a new one that has the same name.
     *
     * @since 5.76
     */
    void setNewFileMenuSelectDirWhenAlreadyExist(bool selectOnDirExists);

    /**
     * Clears both the namefilter and MIME type filter, so that all files and
     * directories will be shown. Call updateDir() to apply it.
     *
     * @see setMimeFilter
     * @see setNameFilter
     */
    void clearFilter();

    /**
     * @returns the current url
     */
    QUrl url() const;

    /**
     * Sets a new url to list.
     * @param clearforward specifies whether the "forward" history should be cleared.
     * @param url the URL to set
     */
    virtual void setUrl(const QUrl &url, bool clearforward);

    /**
     * Clears the current selection and attempts to set @p url
     * the current url file.
     */
    void setCurrentItem(const QUrl &url);

    /**
     * Clears the current selection and attempts to set @p item
     * as the current item.
     */
    void setCurrentItem(const KFileItem &item);

    /**
     * Clears the current selection and attempts to set @p urls
     * the current url files.
     * @since 4.2
     */
    void setCurrentItems(const QList<QUrl> &urls);

    /**
     * Clears the current selection and attempts to set @p items
     * as the current items.
     * @since 4.2
     */
    void setCurrentItems(const KFileItemList &items);

    /**
     * Sets a new view to be used for showing and browsing files.
     * Note: this will read the current url() to fill the view.
     *
     * @see KFileTreeView
     * @see view
     */
    virtual void setView(QAbstractItemView *view);

    /**
     * @returns the currently used view.
     * @see setView
     */
    QAbstractItemView *view() const;

    /**
     * Sets one of the predefined fileviews.
     * @see KFile::FileView
     */
    virtual void setView(KFile::FileView viewKind);

    /**
     * Returns the current view mode.
     * @returns KFile::FileView
     * @see KFile::FileView
     * @since 5.0
     */
    KFile::FileView viewMode() const;

    /**
     * Sets the way to sort files and directories.
     */
    void setSorting(QDir::SortFlags);

    /**
     * @returns the current way of sorting files and directories
     */
    QDir::SortFlags sorting() const;

    /**
     * @returns true if we are displaying the root directory of the current url
     */
    bool isRoot() const;

    /**
     * @returns the object listing the directory
     */
    KDirLister *dirLister() const;

    /**
     * @returns the progress widget, that is shown during directory listing.
     * You can for example reparent() it to put it into a statusbar.
     */
    QProgressBar *progressBar() const;

    /**
     * Sets the listing/selection mode for the views, an OR'ed combination of
     * @li File
     * @li Directory
     * @li Files
     * @li ExistingOnly
     * @li LocalOnly
     *
     * You cannot mix File and Files of course, as the former means
     * single-selection mode, the latter multi-selection.
     */
    virtual void setMode(KFile::Modes m);
    /**
     * @returns the listing/selection mode.
     */
    KFile::Modes mode() const;

    /**
     * Sets a preview-widget to be shown next to the file-view.
     * The ownership of @p w is transferred to KDirOperator, so don't
     * delete it yourself!
     */
    virtual void setPreviewWidget(KPreviewWidgetBase *w);

    /**
     * @returns a list of all currently selected items. If there is no view,
     * or there are no selected items, an empty list is returned.
     */
    KFileItemList selectedItems() const;

    /**
     * @returns true if @p item is currently selected, or false otherwise.
     */
    bool isSelected(const KFileItem &item) const;

    /**
     * @returns the number of directories in the currently listed url.
     * Returns 0 if there is no view.
     */
    int numDirs() const;

    /**
     * @returns the number of files in the currently listed url.
     * Returns 0 if there is no view.
     */
    int numFiles() const;

    /**
     * @returns a KCompletion object, containing all filenames and
     * directories of the current directory/URL.
     * You can use it to insert it into a KLineEdit or KComboBox
     * Note: it will only contain files, after prepareCompletionObjects()
     * has been called. It will be implicitly called from makeCompletion()
     * or makeDirCompletion()
     */
    KCompletion *completionObject() const;

    /**
     * @returns a KCompletion object, containing only all directories of the
     * current directory/URL.
     * You can use it to insert it into a KLineEdit or KComboBox
     * Note: it will only contain directories, after
     * prepareCompletionObjects() has been called. It will be implicitly
     * called from makeCompletion() or makeDirCompletion()
     */
    KCompletion *dirCompletionObject() const;

    /**
     * an accessor to a collection of all available Actions. The actions
     * are static, they will be there all the time (no need to connect to
     * the signals KActionCollection::inserted() or removed().
     *
     * There are the following actions:
     *
     * @li popupMenu : an ActionMenu presenting a popupmenu with all actions
     * @li up : changes to the parent directory
     * @li back : goes back to the previous directory
     * @li forward : goes forward in the history
     * @li home : changes to the user's home directory
     * @li reload : reloads the current directory
     * @li mkdir : opens a dialog box to create a directory
     * @li delete : deletes the selected files/directories
     * @li sorting menu : an ActionMenu containing all sort-options
     * @li by name : sorts by name
     * @li by size : sorts by size
     * @li by date : sorts by date
     * @li by type : sorts by type
     * @li descending : reverses the sort order
     * @li view menu : an ActionMenu containing all actions concerning the view
     * @li short view : shows a simple fileview
     * @li detailed view : shows a detailed fileview (dates, permissions ,...)
     * @li show hidden : shows hidden files
     * @li preview  : shows a preview next to the fileview
     * @li properties : shows a KPropertiesDialog for the selected files
     *
     * The short and detailed view are in an exclusive group. The sort-by
     * actions are in an exclusive group as well. Also the "separate dirs",
     * "preview" and "single" actions are in an exclusive group.
     *
     * You can e.g. use
     * \code
     * actionCollection()->action( "up" )->plug( someToolBar );
     * \endcode
     * to add a button into a toolbar, which makes the dirOperator change to
     * its parent directory.
     *
     * @returns all available Actions
     */
    KActionCollection *actionCollection() const;

    /**
     * Sets the config object and the to be used group in KDirOperator. This
     * will be used to store the view's configuration.
     * If you don't set this, the views cannot save and restore their
     * configuration.
     *
     * Usually you call this right after KDirOperator creation so that the view
     * instantiation can make use of it already.
     *
     * Note that KDirOperator does NOT take ownership of that object (typically
     * it's KSharedConfig::openConfig() anyway.
     *
     * You must not delete the KConfig or KConfigGroup object (and master config object) before
     * either deleting the KDirOperator or  calling setViewConfig(0); or something like that
     *
     * @see viewConfig
     * @see viewConfigGroup
     */
    virtual void setViewConfig(KConfigGroup &configGroup);

    /**
     * @returns the group set by setViewConfig configuration.
     */
    KConfigGroup *viewConfigGroup() const;

    /**
     * Reads the default settings for a view, i.e. the default KFile::FileView.
     * Also reads the sorting and whether hidden files should be shown.
     * Note: the default view will not be set - you have to call
     * \code
     * setView( KFile::Default )
     * \endcode
     * to apply it.
     *
     * @see setView
     * @see setViewConfig
     * @see writeConfig
     */
    virtual void readConfig(const KConfigGroup &configGroup);

    /**
     * Saves the current settings like sorting, simple or detailed view.
     *
     * @see readConfig
     * @see setViewConfig
     */
    virtual void writeConfig(KConfigGroup &configGroup);

    /**
     * This toggles between double/single click file and directory selection mode.
     * When argument is true, files and directories are highlighted with single click and
     * selected (executed) with double click.
     *
     * NOTE: this currently has no effect.
     *
     * The default follows the single/double click system setting.
     */
    void setOnlyDoubleClickSelectsFiles(bool enable);

    /**
     * @returns whether files (not directories) should only be select()ed by
     * double-clicks.
     * @see setOnlyDoubleClickSelectsFiles
     */
    bool onlyDoubleClickSelectsFiles() const;

    /**
     * Toggles whether setUrl is called on newly created directories.
     * @since 5.62
     */
    void setFollowNewDirectories(bool enable);

    /**
     * @returns true if setUrl is called on newly created directories, false 
     * otherwise. Enabled by default.
     * @since 5.62
     * @see setFollowNewDirectories
     */
    bool followNewDirectories() const;

    /**
     * Toggles whether setUrl is called on selected directories when a tree view
     * is used.
     * @since 5.62
     */
    void setFollowSelectedDirectories(bool enable);

    /**
     * @returns whether setUrl is called on selected directories when a tree
     * view is used. Enabled by default.
     * @since 5.62
     */
    bool followSelectedDirectories() const;

#if KIOFILEWIDGETS_BUILD_DEPRECATED_SINCE(5, 78)
    /**
     * Creates the given directory/url. If it is a relative path,
     * it will be completed with the current directory.
     * If enterDirectory is true, the directory will be entered after a
     * successful operation. If unsuccessful, a messagebox will be presented
     * to the user.
     * @returns true if the directory could be created.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 78, "Deprecated for lack of usage; use the other"
                                             " KDirOperator::mkdir() method instead.")
    virtual bool mkdir(const QString &directory, bool enterDirectory = true);
#endif

    /**
     * Starts and returns a KIO::DeleteJob to delete the given @p items.
     *
     * @param items the list of items to be deleted
     * @param parent the parent widget used for the confirmation dialog
     * @param ask specifies whether a confirmation dialog should be shown
     * @param showProgress passed to the DeleteJob to show a progress dialog
     */
    virtual KIO::DeleteJob *del(const KFileItemList &items, QWidget *parent = nullptr,
                                bool ask = true, bool showProgress = true);

    /**
     * Clears the forward and backward history.
     */
    void clearHistory();

    /**
     * When using the up or back actions to navigate the directory hierarchy, KDirOperator
     * can highlight the directory that was just left.
     *
     * For example:
     *  - starting in /a/b/c/, going up to /a/b, "c" will be highlighted
     *  - starting in /a/b/c, going up (twice) to /a, "b" will be highlighted;
     *    using the back action to go to /a/b/, "c" will be highlighted
     *  - starting in /a, going to "b", then going to "c", using the back action
     *    to go to /a/b/, "c" will be highlighted; using the back action again to go
     *    to /a/, "b" will be highlighted
     *
     * @see dirHighlighting. The default is to highlight directories when going back/up.
     */
    virtual void setEnableDirHighlighting(bool enable);

    /**
     * @returns whether the last directory will be made the current item
     * (and hence highlighted) when going up or back in the directory hierarchy
     *
     * Directories are highlighted by default.
     */
    bool dirHighlighting() const;

    /**
     * @returns true if we are in directory-only mode, that is, no files are
     * shown.
     */
    bool dirOnlyMode() const;

    static bool dirOnlyMode(uint mode);

    /**
     * Sets up the action menu.
     * @param whichActions is an value of OR'd ActionTypes that controls which actions to show in the action menu
     */
    void setupMenu(int whichActions);

    /**
     * Reimplemented - allow dropping of files if @p b is true, defaults to true since 5.59
     * @param b true if the widget should allow dropping of files
     */
    virtual void setAcceptDrops(bool b);

    /**
     * Sets the options for dropping files.
     * CURRENTLY NOT IMPLEMENTED
     */
    virtual void setDropOptions(int options);

    /**
     * Starts and returns a KIO::CopyJob to trash the given @p items.
     *
     * @param items the list of items to be trashed
     * @param parent the parent widget used for the confirmation dialog
     * @param ask specifies whether a confirmation dialog should be shown
     * @param showProgress passed to the CopyJob to show a progress dialog
     */
    virtual KIO::CopyJob *trash(const KFileItemList &items, QWidget *parent,
                                bool ask = true, bool showProgress = true);

    /**
      * Returns the preview generator for the current view.
      * @since 4.2
      */
    KFilePreviewGenerator *previewGenerator() const;

    /**
     * Forces the inline previews to be shown or hidden, depending on @p show.
     *
     * @param show Whether to show inline previews or not.
     * @since 4.2
     */
    void setInlinePreviewShown(bool show);

    /**
     * Returns the position where icons are shown relative to the labels
     * of file items in the icon view.
     * @since 4.2.3
     */
    QStyleOptionViewItem::Position decorationPosition() const;

    /**
     * Sets the position where icons shall be shown relative to the labels
     * of file items in the icon view.
     * @since 4.2.3
     */
    void setDecorationPosition(QStyleOptionViewItem::Position position);

    /**
     * Returns whether the inline previews are shown or not.
     * @since 4.2
     */
    bool isInlinePreviewShown() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 76)
    /**
     * Returns the icon zoom.
     * @since 4.2
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 76, "Use KDirOperator::iconSize()")
    int iconsZoom() const;
#endif

    /**
     * Returns the icon size in pixels, ranged from KIconLoader::SizeSmall (16) to
     * KIconLoader::SizeEnormous (128).
     *
     * @since 5.76
     */
    int iconSize() const;

    /**
     * If the system is set up to trigger items on single click, if @p isSaving
     * is true, we will force to double click to accept.
     * @note this is false by default
     * @since 4.2
     */
    void setIsSaving(bool isSaving);

    /**
     * Returns whether KDirOperator will force a double click to accept.
     * @note this is false by default
     * @since 4.2
     */
    bool isSaving() const;

    /**
     * Returns the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported.
     *
     * @sa QFileDialog::supportedSchemes
     * @since 5.43
     */
    QStringList supportedSchemes() const;

protected:
    /**
     * A view factory for creating predefined fileviews. Called internally by setView,
     * but you can also call it directly. Reimplement this if you depend on self defined fileviews.
     * @param parent   is the QWidget to be set as parent
     * @param viewKind is the predefined view to be set, note: this can be several ones OR:ed together
     * @returns the created view
     * @see KFile::FileView
     * @see setView
     */
    virtual QAbstractItemView *createView(QWidget *parent, KFile::FileView viewKind);

    /**
     * Sets a custom KDirLister to list directories.
     * The KDirOperator takes ownership of the given KDirLister.
     */
    virtual void setDirLister(KDirLister *lister);

    void resizeEvent(QResizeEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override; // TODO KF6 REMOVE

    /**
     * Sets up all the actions. Called from the constructor, you usually
     * better not call this.
     */
    void setupActions();

    /**
     * Updates the sorting-related actions to comply with the current sorting
     * @see sorting
     */
    void updateSortActions();

    /**
     * Updates the view-related actions to comply with the current
     * KFile::FileView
     */
    void updateViewActions();

    /**
     * Sets up the context-menu with all the necessary actions. Called from the
     * constructor, you usually don't need to call this.
     */
    void setupMenu();

    /**
     * Synchronizes the completion objects with the entries of the
     * currently listed url.
     *
     * Automatically called from makeCompletion() and
     * makeDirCompletion()
     */
    void prepareCompletionObjects();

    /**
     * Checks if there support from KIO::PreviewJob for the currently
     * shown files, taking mimeFilter() and nameFilter() into account
     * Enables/disables the preview-action accordingly.
     */
    bool checkPreviewSupport();

    /**
     * Called upon right-click to activate the popupmenu.
     */
    virtual void activatedMenu(const KFileItem &item, const QPoint &pos);

    void changeEvent(QEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

public Q_SLOTS:
    /**
     * Goes one step back in the history and opens that url.
     */
    virtual void back();

    /**
     * Goes one step forward in the history and opens that url.
     */
    virtual void forward();

    /**
     * Enters the home directory.
     */
    virtual void home();

    /**
     * Goes one directory up from the current url.
     */
    virtual void cdUp();

    /**
     * to update the view after changing the settings
     */
    void updateDir();

    /**
     * Re-reads the current url.
     */
    virtual void rereadDir();

    /**
     * Opens a dialog to create a new directory.
     */
    virtual void mkdir();

    /**
     * Deletes the currently selected files/directories.
     */
    virtual void deleteSelected();

    /**
     * Enables/disables actions that are selection dependent. Call this e.g.
     * when you are about to show a popup menu using some of KDirOperators
     * actions.
     */
    void updateSelectionDependentActions();

    /**
     * Tries to complete the given string (only completes files).
     */
    QString makeCompletion(const QString &);

    /**
     * Tries to complete the given string (only completes directories).
     */
    QString makeDirCompletion(const QString &);

    /**
     * Initiates a rename operation on the currently selected files/directories,
     * prompting the user to choose a new name(s) for the currently selected items
     * @since 5.67
     */
    void renameSelected();

    /**
     * Trashes the currently selected files/directories.
     *
     * This function used to take activation reason and keyboard modifiers,
     * in order to call deleteSelected() if the user wanted to delete.
     * Instead, call deleteSelected().
     *
     * FIXME KAction Port: link deleteSelected() up correctly
     */
    virtual void trashSelected();

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 76)
    /**
     * Notifies that the icons size should change. @p value is an int ranged from 0 to 100.
     * 100 means KIconLoader::SizeEnormous.
     * @since 4.2
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 76, "Use KDirOperator::setIconSize(int)")
    void setIconsZoom(int value);
#endif

    /**
     * Notifies that the icons size should change. @p value is the icon size in pixels, ranged
     * from KIconLoader::SizeSmall (16) to KIconLoader::SizeEnormous (128).
     *
     * @since 5.76
     */
    void setIconSize(int value);

    /**
     * Set the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported. Examples for
     * schemes are @c "file" or @c "ftp".
     *
     * @sa QFileDialog::setSupportedSchemes
     * @since 5.43
     */
    void setSupportedSchemes(const QStringList &schemes);

protected Q_SLOTS:
    /**
     * Restores the normal cursor after showing the busy-cursor. Also hides
     * the progressbar.
     */
    void resetCursor();

    /**
     * Called after setUrl() to load the directory, update the history,
     * etc.
     */
    void pathChanged();

    /**
     * Enters the directory specified by the given @p item.
     */
    virtual void selectDir(const KFileItem &item);

    /**
     * Emits fileSelected( item )
     */
    void selectFile(const KFileItem &item);

    /**
     * Emits fileHighlighted(item)
     */
    void highlightFile(const KFileItem &item);

    /**
     * Changes sorting to sort by name
     */
    void sortByName();

    /**
     * Changes sorting to sort by size
     */
    void sortBySize();

    /**
     * Changes sorting to sort by date
     */
    void sortByDate();

    /**
     * Changes sorting to sort by date
     */
    void sortByType();

    /**
     * Changes sorting to reverse sorting
     */
    void sortReversed();

    /**
     * Toggles showing directories first / having them sorted like files.
     */
    void toggleDirsFirst();

    /**
     * Toggles case sensitive / case insensitive sorting
     */
    void toggleIgnoreCase();

    /**
     * Tries to make the given @p match as current item in the view and emits
     * completion( match )
     */
    void slotCompletionMatch(const QString &match);

Q_SIGNALS:
    void urlEntered(const QUrl &);
    void updateInformation(int files, int dirs);
    void completion(const QString &);
    void finishedLoading();

    /**
     * Emitted whenever the current fileview is changed, either by an explicit
     * call to setView() or by the user selecting a different view thru
     * the GUI.
     */
    void viewChanged(QAbstractItemView *newView);

    /**
     * Emitted when a file is highlighted or generally the selection changes in
     * multiselection mode. In the latter case, @p item is a null KFileItem.
     * You can access the selected items with selectedItems().
     */
    void fileHighlighted(const KFileItem &item);
    void dirActivated(const KFileItem &item);
    void fileSelected(const KFileItem &item);
    /**
     * Emitted when files are dropped. Dropping files is disabled by
     * default. You need to enable it with setAcceptDrops()
     * @param item the item on which the drop occurred or 0.
     * @param event the drop event itself.
     * @param urls the urls that where dropped.
     */
    void dropped(const KFileItem &item, QDropEvent *event, const QList<QUrl> &urls);

    /**
     * Emitted just before the context menu is shown, allows users to
     * extend the menu with custom actions.
     *
     * @param item the file on which the context menu was invoked
     * @param menu the context menu, pre-populated with the file-management actions
     * @since 4.2
     */
    void contextMenuAboutToShow(const KFileItem &item, QMenu *menu);

    /**
     * Will notify that the icon size has changed. Since we save the icon size depending
     * on the view type (list view or a different kind of view), a call to setView() can
     * trigger this signal to be emitted.
     * @since 4.2
     */
    void currentIconSizeChanged(int size);

    /**
     * Triggered when the user hit Enter/Return
     * @since 5.57
     */
    void keyEnterReturnPressed();

private:
    friend class KDirOperatorPrivate;
    std::unique_ptr<KDirOperatorPrivate> d;
};

#endif
