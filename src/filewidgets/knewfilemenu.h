/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Sven Leiber <s.leiber@web.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

#ifndef KNEWFILEMENU_H
#define KNEWFILEMENU_H

#include "kiofilewidgets_export.h"

#include <KActionMenu>
#include <QUrl>

#include <memory>

class KJob;

class KActionCollection;
class KNewFileMenuPrivate;

/**
 * @class KNewFileMenu knewfilemenu.h <KNewFileMenu>
 *
 * The 'Create New' submenu, for creating files using templates
 * (e.g.\ "new HTML file") and directories.
 *
 * The same instance can be used by both for the File menu and the RMB popup menu,
 * in a file manager. This is also used in the file dialog's RMB menu.
 *
 * To use this class, you need to connect aboutToShow() of the File menu
 * with slotCheckUpToDate() and to call slotCheckUpToDate() before showing
 * the RMB popupmenu.
 *
 * KNewFileMenu automatically updates the list of templates shown if installed templates
 * are added/updated/deleted.
 *
 * @author Björn Ruberg <bjoern@ruberg-wegener.de>
 * Made dialogs working asynchronously
 * @author David Faure <faure@kde.org>
 * Ideas and code for the new template handling mechanism ('link' desktop files)
 * from Christoph Pickart <pickart@iam.uni-bonn.de>
 * @since 4.5
 */
class KIOFILEWIDGETS_EXPORT KNewFileMenu : public KActionMenu
{
    Q_OBJECT
public:
#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 100)
    /**
     * Constructor.
     * @param collection the KActionCollection the QAction with name @p name should be added to.
     * @param name action name, when adding the action to @p collection
     * @param parent the parent object, for ownership.
     * If the parent object is a widget, it will also used as parent widget
     * for any dialogs that this class might show. Otherwise, call setParentWidget.
     * @note If you want the "Create directory..." action shortcut to show up next to its text,
     *       make sure to have an action with name "create_dir" (and shortcut set) in @p collection.
     *       This will only work with KIO >= 5.27.
     *       From KIO >= 5.53, an action named "create_file" (and shortcut set) in @p collection
     *       will be linked to the creation of the first file template (either from XDG_TEMPLATES_DIR
     *       or from :/kio5/newfile-templates)
     * @deprecated since 5.100, use KNewFileMenu(QObject *) instead. To associate the action shortcuts use
     * setNewFileShortcutAction() and setNewFolderShortcutAction()
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 100, "Use KNewFileMenu(QObject *)")
    KNewFileMenu(KActionCollection *collection, const QString &name, QObject *parent);
#endif

    /**
     * Constructor.
     *
     * @param parent the parent object, for ownership.
     * If the parent object is a widget, it will also be used as the parent widget
     * for any dialogs that this class might show. Otherwise, call setParentWidget.
     *
     * @since 5.100
     */
    KNewFileMenu(QObject *parent);

    /**
     * Destructor.
     * KNewMenu uses internally a globally shared cache, so that multiple instances
     * of it don't need to parse the installed templates multiple times. Therefore
     * you can safely create and delete KNewMenu instances without a performance issue.
     */
    ~KNewFileMenu() override;

    /**
     * Returns the modality of dialogs
     */
    bool isModal() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 97)
    /**
     * Returns the files that the popup is shown for
     * @deprecated since 5.97, use KNewFileMenu::workingDirectory().
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 97, "Use KNewFileMenu::workingDirectory()")
    QList<QUrl> popupFiles() const;
#endif

    /**
     * Sets the modality of dialogs created by KNewFile. Set to false if you do not want to block
     * your application window when entering a new directory name i.e.
     */
    void setModal(bool modality);

    /**
     * Sets a parent widget for the dialogs shown by KNewFileMenu.
     * This is strongly recommended, for apps with a main window.
     */
    void setParentWidget(QWidget *parentWidget);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 97)
    /**
     * Set the files the popup is shown for
     * Call this before showing up the menu
     * @deprecated since 5.97, use KNewFileMenu::setWorkingDirectory(const QUrl &).
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 97, "Use KNewFileMenu::setWorkingDirectory(const QUrl &)")
    void setPopupFiles(const QList<QUrl> &files);
#endif

    /**
     * Set the working directory.
     * Files will be created relative to this directory.
     * @since 5.97.
     */
    void setWorkingDirectory(const QUrl &directory);

    /**
     * Returns the working directory.
     * Files will be created relative to this directory.
     * @since 5.97.
     */
    QUrl workingDirectory() const;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 0)
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 0, "Use KNewFileMenu::setWorkingDirectory(const QUrl &)")
    void setPopupFiles(const QUrl &file)
    {
        setWorkingDirectory(file);
    }
#endif

    /**
     * Only show the files in a given set of MIME types.
     * This is useful in specialized applications (while file managers, on
     * the other hand, want to show all MIME types).
     */
    void setSupportedMimeTypes(const QStringList &mime);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 97)
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 97, "Calling this has no effect")
    /**
     * Set if the directory view currently shows dot files.
     * @deprecated since 5.97. Calling this has no effect.
     */
    void setViewShowsHiddenFiles(bool b);
#endif

    /**
     * Returns the MIME types set in supportedMimeTypes()
     */
    QStringList supportedMimeTypes() const;

    /**
     * Whether on not the dialog should emit `selectExistingDir` when trying to create an exist directory
     *
     * default: false
     *
     * @since 5.76
     */
    void setSelectDirWhenAlreadyExist(bool b);

    /**
     * Use this to set a shortcut for the "New Folder" action.
     *
     * The shortcut is copied from @param action.
     *
     * @since 5.100
     */
    void setNewFolderShortcutAction(QAction *action);

    /**
     * Use this to set a shortcut for the new file action.
     *
     * The shortcut is copied from @param action.
     *
     * @since 5.100
     */
    void setNewFileShortcutAction(QAction *action);

public Q_SLOTS:
    /**
     * Checks if updating the list is necessary
     * IMPORTANT : Call this in the slot for aboutToShow.
     */
    void checkUpToDate();

    /**
     * Call this to create a new directory as if the user had done it using
     * a popupmenu. This is useful to make sure that creating a directory with
     * a key shortcut (e.g. F10) triggers the exact same code as when using
     * the New menu.
     * Requirements: since 5.97 call setWorkingDirectory first (for older releases call setPopupFiles first), and keep this KNewFileMenu instance
     * alive (the mkdir is async).
     */
    void createDirectory();

    /**
     * Call this to create a new file as if the user had done it using
     * a popupmenu. This is useful to make sure that creating a directory with
     * a key shortcut (e.g. Shift-F10) triggers the exact same code as when using
     * the New menu.
     * Requirements: since 5.97 call setWorkingDirectory first (for older releases call setPopupFiles first), and keep this KNewFileMenu instance
     * alive (the copy is async).
     * @since 5.53
     */
    void createFile();

Q_SIGNALS:
    /**
     * Emitted once the file (or symlink) @p url has been successfully created
     */
    void fileCreated(const QUrl &url);

    /**
     * Emitted once the directory @p url has been successfully created
     */
    void directoryCreated(const QUrl &url);

    /**
     * Emitted when trying to create a new directory that has the same name as
     * an existing one, so that KDirOperator can select the existing item in
     * the view (in case the user wants to use that directory instead of creating
     * a new one).
     *
     * @since 5.76
     */
    void selectExistingDir(const QUrl &url);

protected Q_SLOTS:

    /**
     * Called when the job that copied the template has finished.
     * This method is virtual so that error handling can be reimplemented.
     * Make sure to call the base class slotResult when !job->error() though.
     */
    virtual void slotResult(KJob *job);

private:
    friend class KNewFileMenuPrivate;
    std::unique_ptr<KNewFileMenuPrivate> const d;
};

#endif
