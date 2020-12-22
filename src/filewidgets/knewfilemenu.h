/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998-2009 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2003 Sven Leiber <s.leiber@web.de>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only
*/

#ifndef KNEWFILEMENU_H
#define KNEWFILEMENU_H

#include <QUrl>
#include <KActionMenu>
#include "kiofilewidgets_export.h"

class KJob;

class KActionCollection;
class KNewFileMenuPrivate;

/**
 * @class KNewFileMenu knewfilemenu.h <KNewFileMenu>
 *
 * The 'Create New' submenu, for creating files using templates
 * (e.g. "new HTML file") and directories.
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
     */
    KNewFileMenu(KActionCollection *collection, const QString &name, QObject *parent);

    /**
     * Destructor.
     * KNewMenu uses internally a globally shared cache, so that multiple instances
     * of it don't need to parse the installed templates multiple times. Therefore
     * you can safely create and delete KNewMenu instances without a performance issue.
     */
    virtual ~KNewFileMenu();

    /**
     * Returns the modality of dialogs
     */
    bool isModal() const;

    /**
     * Returns the files that the popup is shown for
     */
    QList<QUrl> popupFiles() const;

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

    /**
     * Set the files the popup is shown for
     * Call this before showing up the menu
     */
    void setPopupFiles(const QList<QUrl> &files);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 0)
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 0, "Use KNewFileMenu::setPopupFiles(const QList<QUrl> &)")
    void setPopupFiles(const QUrl &file)
    {
        setPopupFiles(QList<QUrl>{file});
    }
#endif

    /**
     * Only show the files in a given set of MIME types.
     * This is useful in specialized applications (while file managers, on
     * the other hand, want to show all MIME types).
     */
    void setSupportedMimeTypes(const QStringList &mime);

    /**
     * Set if the directory view currently shows dot files.
     */
    void setViewShowsHiddenFiles(bool b);

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

public Q_SLOTS:
    /**
     * Checks if updating the list is necessary
     * IMPORTANT : Call this in the slot for aboutToShow.
     * And while you're there, you probably want to call setViewShowsHiddenFiles ;)
     */
    void checkUpToDate();

    /**
     * Call this to create a new directory as if the user had done it using
     * a popupmenu. This is useful to make sure that creating a directory with
     * a key shortcut (e.g. F10) triggers the exact same code as when using
     * the New menu.
     * Requirements: call setPopupFiles first, and keep this KNewFileMenu instance
     * alive (the mkdir is async).
     */
    void createDirectory();

    /**
     * Call this to create a new file as if the user had done it using
     * a popupmenu. This is useful to make sure that creating a directory with
     * a key shortcut (e.g. Shift-F10) triggers the exact same code as when using
     * the New menu.
     * Requirements: call setPopupFiles first, and keep this KNewFileMenu instance
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
     * an existing one, so that KDirOperator can select the exisiting item in
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
    Q_PRIVATE_SLOT(d, void _k_slotAbortDialog())
    Q_PRIVATE_SLOT(d, void _k_slotActionTriggered(QAction *))
    Q_PRIVATE_SLOT(d, void _k_slotCreateDirectory())
    Q_PRIVATE_SLOT(d, void _k_slotCreateHiddenDirectory())
    Q_PRIVATE_SLOT(d, void _k_slotFillTemplates())
    Q_PRIVATE_SLOT(d, void _k_slotOtherDesktopFile())
    Q_PRIVATE_SLOT(d, void _k_slotOtherDesktopFileClosed())
    Q_PRIVATE_SLOT(d, void _k_slotRealFileOrDir())
    Q_PRIVATE_SLOT(d, void _k_slotSymLink())
    Q_PRIVATE_SLOT(d, void _k_slotUrlDesktopFile())

    friend class KNewFileMenuPrivate;
    KNewFileMenuPrivate *const d;

};

#endif
