/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
    SPDX-FileCopyrightText: 1999, 2000 Preston Brown <pbrown@kde.org>
    SPDX-FileCopyrightText: 2000 Simon Hausmann <hausmann@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KPROPERTIESDIALOG_H
#define KPROPERTIESDIALOG_H

#include <QString>
#include <QUrl>

#include "kiowidgets_export.h"
#include <kfileitem.h>

#include <KPageDialog>

#include <memory>

class KPropertiesDialogPrivate;

/*!
 * @class KPropertiesDialog kpropertiesdialog.h <KPropertiesDialog>
 *
 * The main properties dialog class.
 * A Properties Dialog is a dialog which displays various information
 * about a particular file or URL, or several files or URLs.
 * This main class holds various related classes, which are instantiated in
 * the form of tab entries in the tabbed dialog that this class provides.
 * The various tabs themselves will let the user view, and sometimes change,
 * information about the file or URL.
 *
 * \image html kpropertiesdialog.png "Example of KPropertiesDialog"
 *
 * The best way to display the properties dialog is to use showDialog().
 * Otherwise, you should use (void)new KPropertiesDialog(...)
 * It will take care of deleting itself when closed.
 *
 * If you are looking for more flexibility, see KFileMetaInfo and
 * KFileMetaInfoWidget.
 *
 * This respects the "editfiletype", "run_desktop_files" and "shell_access"
 * Kiosk action restrictions (see KAuthorized::authorize()).
 */
class KIOWIDGETS_EXPORT KPropertiesDialog : public KPageDialog
{
    Q_OBJECT

public:
    /*!
     * Determine whether there are any property pages available for the
     * given file items.
     * @param _items the list of items to check.
     * @return true if there are any property pages, otherwise false.
     */
    static bool canDisplay(const KFileItemList &_items);

    /*!
     * Brings up a Properties dialog, as shown above.
     * This is the normal constructor for
     * file-manager type applications, where you have a KFileItem instance
     * to work with.  Normally you will use this
     * method rather than the one below.
     *
     * @param item file item whose properties should be displayed.
     * @param parent is the parent of the dialog widget.
     */
    explicit KPropertiesDialog(const KFileItem &item, QWidget *parent = nullptr);

    /*!
     * \overload
     *
     * You use this constructor for cases where you have a number of items,
     * rather than a single item. Be careful which methods you use
     * when passing a list of files or URLs, since some of them will only
     * work on the first item in a list.
     *
     * @param _items list of file items whose properties should be displayed.
     * @param parent is the parent of the dialog widget.
     */
    explicit KPropertiesDialog(const KFileItemList &_items, QWidget *parent = nullptr);

    /*!
     * Brings up a Properties dialog. Convenience constructor for
     * non-file-manager applications, where you have a QUrl rather than a
     * KFileItem or KFileItemList.
     *
     * @param url the URL whose properties should be displayed
     * @param parent is the parent of the dialog widget.
     *
     * For local files with a known MIME type, simply create a KFileItem
     * and pass it to the other constructor.
     */
    explicit KPropertiesDialog(const QUrl &url, QWidget *parent = nullptr);

    /*!
     * Brings up a Properties dialog. Convenience constructor for
     * non-file-manager applications, where you have a list of QUrls rather
     * than a KFileItemList.
     *
     * @param urls list of URLs whose properties should be displayed (must
     *             contain at least one non-empty URL)
     * @param parent is the parent of the dialog widget.
     *
     * For local files with a known MIME type, simply create a KFileItemList
     * and pass it to the other constructor.
     *
     * @since 5.10
     */
    explicit KPropertiesDialog(const QList<QUrl> &urls, QWidget *parent = nullptr);

    /*!
     * Creates a properties dialog for a new .desktop file (whose name
     * is not known yet), based on a template. Special constructor for
     * "File / New" in file-manager type applications.
     *
     * @param _tempUrl template used for reading only
     * @param _currentDir directory where the file will be written to
     * @param _defaultName something to put in the name field,
     * like mimetype.desktop
     * @param parent is the parent of the dialog widget.
     */
    KPropertiesDialog(const QUrl &_tempUrl, const QUrl &_currentDir, const QString &_defaultName, QWidget *parent = nullptr);

    /*!
     * Creates an empty properties dialog (for applications that want use
     * a standard dialog, but for things not doable via the plugin-mechanism).
     *
     * @param title is the string display as the "filename" in the title of the dialog.
     * @param parent is the parent of the dialog widget.
     */
    explicit KPropertiesDialog(const QString &title, QWidget *parent = nullptr);

    /*!
     * Cleans up the properties dialog and frees any associated resources,
     * including the dialog itself. Note that when a properties dialog is
     * closed it cleans up and deletes itself.
     */
    ~KPropertiesDialog() override;

    /*!
     * Immediately displays a Properties dialog using constructor with
     * the same parameters.
     * On MS Windows, if @p item points to a local file, native (non modal) property
     * dialog is displayed (@p parent and @p modal are ignored in this case).
     *
     * @return true on successful dialog displaying (can be false on win32).
     */
    static bool showDialog(const KFileItem &item, QWidget *parent = nullptr, bool modal = true);

    /*!
     * Immediately displays a Properties dialog using constructor with
     * the same parameters.
     * On MS Windows, if @p _url points to a local file, native (non modal) property
     * dialog is displayed (@p parent and @p modal are ignored in this case).
     *
     * @return true on successful dialog displaying (can be false on win32).
     */
    static bool showDialog(const QUrl &_url, QWidget *parent = nullptr, bool modal = true);

    /*!
     * Immediately displays a Properties dialog using constructor with
     * the same parameters.
     * On MS Windows, if @p _items has one element and this element points
     * to a local file, native (non modal) property dialog is displayed
     * (@p parent and @p modal are ignored in this case).
     *
     * @return true on successful dialog displaying (can be false on win32).
     */
    static bool showDialog(const KFileItemList &_items, QWidget *parent = nullptr, bool modal = true);

    /*!
     * Immediately displays a Properties dialog using constructor with
     * the same parameters.
     *
     * On MS Windows, if @p _urls has one element and this element points
     * to a local file, native (non modal) property dialog is displayed
     * (@p parent and @p modal are ignored in this case).
     *
     * @param urls list of URLs whose properties should be displayed (must
     *             contain at least one non-empty URL)
     * @param parent is the parent of the dialog widget.
     * @param modal tells the dialog whether it should be modal.
     *
     * @return true on successful dialog displaying (can be false on win32).
     *
     * @since 5.10
     */
    static bool showDialog(const QList<QUrl> &urls, QWidget *parent = nullptr, bool modal = true);

    /*!
     * The URL of the file that has its properties being displayed.
     * This is only valid if the KPropertiesDialog was created/shown
     * for one file or URL.
     *
     * @return the single URL.
     */
    QUrl url() const;

    /*!
     * @return the file item for which the dialog is shown
     *
     * Warning: this method returns the first item of the list.
     * This means that you should use this only if you are sure the dialog is used
     * for a single item. Otherwise, you probably want items() instead.
     */
    KFileItem &item();

    /*!
     * @return the items for which the dialog is shown
     */
    KFileItemList items() const;

    /*!
     * If the dialog is being built from a template, this method
     * returns the current directory. If no template, it returns QString().
     * See the template form of the constructor.
     *
     * @return the current directory or QString()
     */
    QUrl currentDir() const;

    /*!
     * If the dialog is being built from a template, this method
     * returns the default name. If no template, it returns QString().
     * See the template form of the constructor.
     * @return the default name or QString()
     */
    QString defaultName() const;

    /*!
     * Updates the item URL (either called by rename or because
     * a global apps/mimelnk desktop file is being saved)
     * Can only be called if the dialog applies to a single file or URL.
     * @param newUrl the new URL
     */
    void updateUrl(const QUrl &newUrl);

    /*!
     * Renames the item to the specified name. This can only be called if
     * the dialog applies to a single file or URL.
     * @param _name new filename, encoded.
     * \see FilePropsDialogPlugin::applyChanges
     */
    void rename(const QString &_name);

    /*!
     * To abort applying changes.
     */
    void abortApplying();

    /*!
     * Shows the page that was previously set by
     * setFileSharingPage(), or does nothing if no page
     * was set yet.
     * \see setFileSharingPage
     */
    void showFileSharingPage();

    /*!
     * Sets the file sharing page.
     * This page is shown when calling showFileSharingPage().
     *
     * @param page the page to set
     *
     * \note This should only be called by KPropertiesDialog plugins.
     * \see showFileSharingPage
     */
    void setFileSharingPage(QWidget *page);

    /*!
     * Call this to make the filename lineedit readonly, to prevent the user
     * from renaming the file.
     * \param ro true if the lineedit should be read only
     */
    void setFileNameReadOnly(bool ro);

    using KPageDialog::buttonBox;

public Q_SLOTS:
    /*!
     * Called when the user presses 'Ok'.
     * @since 5.25
     */
    void accept() override;
    /*!
     * Called when the user presses 'Cancel' or Esc.
     * @since 5.25
     */
    void reject() override;

Q_SIGNALS:
    /*!
     * This signal is emitted when the Properties Dialog is closed (for
     * example, with OK or Cancel buttons)
     */
    void propertiesClosed();

    /*!
     * This signal is emitted when the properties changes are applied (for
     * example, with the OK button)
     */
    void applied();

    /*!
     * This signal is emitted when the properties changes are aborted (for
     * example, with the Cancel button)
     */
    void canceled();

    /*!
     * Emitted before changes to @p oldUrl are saved as @p newUrl.
     * The receiver may change @p newUrl to point to an alternative
     * save location.
     */
    void saveAs(const QUrl &oldUrl, QUrl &newUrl);

private:
    std::unique_ptr<KPropertiesDialogPrivate> d;

    Q_DISABLE_COPY(KPropertiesDialog)
};

#endif
