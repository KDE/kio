// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 1997, 1998 Richard Moore <rich@kde.org>
    SPDX-FileCopyrightText: 1998 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1998 Daniel Grana <grana@ie.iwi.unibe.ch>
    SPDX-FileCopyrightText: 2000, 2001 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2001 Frerich Raabe <raabe@kde.org>
    SPDX-FileCopyrightText: 2007 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2008 Rafael Fernández López <ereslibre@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KFILEWIDGET_H
#define KFILEWIDGET_H

#include "kiofilewidgets_export.h"
#include "kfile.h"
#include <QWidget>

#include <memory>

class QUrl;
class QPushButton;
class KActionCollection;
class KToolBar;
class KFileWidgetPrivate;
class KUrlComboBox;
class KFileFilterCombo;

class KPreviewWidgetBase;
class QMimeType;
class KConfigGroup;
class KJob;
class KFileItem;
class KDirOperator;

/**
 * @class KFileWidget kfilewidget.h <KFileWidget>
 *
 * File selector widget.
 *
 * This is the contents of the KDE file dialog, without the actual QDialog around it.
 * It can be embedded directly into applications.
 */
class KIOFILEWIDGETS_EXPORT KFileWidget : public QWidget
{
    Q_OBJECT
public:
    /**
      * Constructs a file selector widget.
      *
      * @param startDir This can either be:
      *         @li An empty URL (QUrl()) to start in the current working directory,
      *             or the last directory where a file has been selected.
      *         @li The path or URL of a starting directory.
      *         @li An initial file name to select, with the starting directory being
      *             the current working directory or the last directory where a file
      *             has been selected.
      *         @li The path or URL of a file, specifying both the starting directory and
      *             an initially selected file name.
      *         @li A URL of the form @c kfiledialog:///&lt;keyword&gt; to start in the
      *             directory last used by a filedialog in the same application that
      *             specified the same keyword.
      *         @li A URL of the form @c kfiledialog:///&lt;keyword&gt;/&lt;filename&gt;
      *             to start in the directory last used by a filedialog in the same
      *             application that specified the same keyword, and to initially
      *             select the specified filename.
      *         @li A URL of the form @c kfiledialog:///&lt;keyword&gt;?global to start
      *             in the directory last used by a filedialog in any application that
      *             specified the same keyword.
      *         @li A URL of the form @c kfiledialog:///&lt;keyword&gt;/&lt;filename&gt;?global
      *             to start in the directory last used by a filedialog in any
      *             application that specified the same keyword, and to initially
      *             select the specified filename.
      *
      * @param parent The parent widget of this widget
      *
      */
    explicit KFileWidget(const QUrl &startDir, QWidget *parent = nullptr);

    /**
     * Destructor
     */
    ~KFileWidget() override;

    /**
     * Defines some default behavior of the filedialog.
     * E.g. in mode @p Opening and @p Saving, the selected files/urls will
     * be added to the "recent documents" list. The Saving mode also implies
     * setKeepLocation() being set.
     *
     * @p Other means that no default actions are performed.
     *
     * @see setOperationMode
     * @see operationMode
     */
    enum OperationMode { Other = 0, Opening, Saving };

    /**
     * @returns The selected fully qualified filename.
     */
    QUrl selectedUrl() const;

    /**
     * @returns The list of selected URLs.
     */
    QList<QUrl> selectedUrls() const;

    /**
     * @returns the currently shown directory.
     */
    QUrl baseUrl() const;

    /**
     * Returns the full path of the selected file in the local filesystem.
     * (Local files only)
     */
    QString selectedFile() const;

    /**
     * Returns a list of all selected local files.
     */
    QStringList selectedFiles() const;

    /**
     * Sets the directory to view.
     *
     * @param url URL to show.
     * @param clearforward Indicates whether the forward queue
     * should be cleared.
     */
    void setUrl(const QUrl &url, bool clearforward = true);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 33)
    /**
     * Sets the file to preselect to @p pathOrUrl
     *
     * This method handles absolute paths (on Unix, but probably not correctly on Windows)
     * and absolute URLs as strings (but for those you should use setSelectedUrl instead).
     *
     * This method does not work with relative paths (filenames)
     * (it would misinterpret a ':' or a '#' in the filename).
     *
     * @deprecated since 5.33, use setSelectedUrl instead, after ensuring that
     * construct the QUrl correctly (e.g. use fromLocalFile for local paths).
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 33, "Use KFileWidget::setSelectedUrl(const QUrl &)")
    void setSelection(const QString &pathOrUrl);
#endif

    /**
     * Sets the URL to preselect to @p url
     *
     * This method handles absolute URLs (remember to use fromLocalFile for local paths).
     * It also handles relative URLs, which you should construct like this:
     * QUrl relativeUrl; relativeUrl.setPath(fileName);
     *
     * @since 5.33
     */
    void setSelectedUrl(const QUrl &url);

    /**
     * Sets a list of URLs as preselected
     * 
     * @see setSelectedUrl
     * @since 5.75
     */
    void setSelectedUrls(const QList<QUrl> &urls);

    /**
     * Sets the operational mode of the filedialog to @p Saving, @p Opening
     * or @p Other. This will set some flags that are specific to loading
     * or saving files. E.g. setKeepLocation() makes mostly sense for
     * a save-as dialog. So setOperationMode( KFileWidget::Saving ); sets
     * setKeepLocation for example.
     *
     * The mode @p Saving, together with a default filter set via
     * setMimeFilter() will make the filter combobox read-only.
     *
     * The default mode is @p Opening.
     *
     * Call this method right after instantiating KFileWidget.
     *
     * @see operationMode
     * @see KFileWidget::OperationMode
     */
    void setOperationMode(OperationMode);

    /**
     * @returns the current operation mode, Opening, Saving or Other. Default
     * is Other.
     *
     * @see operationMode
     * @see KFileWidget::OperationMode
     */
    OperationMode operationMode() const;

    /**
     * Sets whether the filename/url should be kept when changing directories.
     * This is for example useful when having a predefined filename where
     * the full path for that file is searched.
     *
     * This is implicitly set when operationMode() is KFileWidget::Saving
     *
     * getSaveFileName() and getSaveUrl() set this to true by default, so that
     * you can type in the filename and change the directory without having
     * to type the name again.
     */
    void setKeepLocation(bool keep);

    /**
     * @returns whether the contents of the location edit are kept when
     * changing directories.
     */
    bool keepsLocation() const;

    /**
     * Sets the filter to be used to @p filter.
     *
     * You can set more
     * filters for the user to select separated by '\n'. Every
     * filter entry is defined through namefilter|text to display.
     * If no | is found in the expression, just the namefilter is
     * shown. Examples:
     *
     * \code
     * kfile->setFilter("*.cpp|C++ Source Files\n*.h|Header files");
     * kfile->setFilter("*.cpp");
     * kfile->setFilter("*.cpp|Sources (*.cpp)");
     * kfile->setFilter("*.cpp|" + i18n("Sources (*.cpp)"));
     * kfile->setFilter("*.cpp *.cc *.C|C++ Source Files\n*.h *.H|Header files");
     * \endcode
     *
     * Note: The text to display is not parsed in any way. So, if you
     * want to show the suffix to select by a specific filter, you must
     * repeat it.
     *
     * If the filter contains an unescaped '/', a MIME type filter is assumed.
     * If you would like a '/' visible in your filter it can be escaped with
     * a '\'. You can specify multiple MIME types like this (separated with
     * space):
     *
     * \code
     * kfile->setFilter( "image/png text/html text/plain" );
     * kfile->setFilter( "*.cue|CUE\\/BIN Files (*.cue)" );
     * \endcode
     *
     * @see filterChanged
     * @see setMimeFilter
     */
    void setFilter(const QString &filter);

    /**
     * Returns the current filter as entered by the user or one of the
     * predefined set via setFilter().
     *
     * @see setFilter()
     * @see filterChanged()
     */
    QString currentFilter() const;

    /**
     * Returns the MIME type for the desired output format.
     *
     * This is only valid if setFilterMimeType() has been called
     * previously.
     *
     * @see setFilterMimeType()
     */
    QMimeType currentFilterMimeType();

    /**
     * Sets the filter up to specify the output type.
     *
     * @param types a list of MIME types that can be used as output format
     * @param defaultType the default MIME type to use as output format, if any.
     * If @p defaultType is set, it will be set as the current item.
     * Otherwise, a first item showing all the MIME types will be created.
     * Typically, @p defaultType should be empty for loading and set for saving.
     *
     * Do not use in conjunction with setFilter()
     */
    void setMimeFilter(const QStringList &types,
                       const QString &defaultType = QString());

    /**
     * The MIME type for the desired output format.
     *
     * This is only valid if setMimeFilter() has been called
     * previously.
     *
     * @see setMimeFilter()
     */
    QString currentMimeFilter() const;

    /**
     *  Clears any MIME type or name filter. Does not reload the directory.
     */
    void clearFilter();

    /**
     * Adds a preview widget and enters the preview mode.
     *
     * In this mode the dialog is split and the right part contains your
     * preview widget.
     *
     * Ownership is transferred to KFileWidget. You need to create the
     * preview-widget with "new", i.e. on the heap.
     *
     * @param w The widget to be used for the preview.
     */
    void setPreviewWidget(KPreviewWidgetBase *w);

    /**
     * Sets the mode of the dialog.
     *
     * The mode is defined as (in kfile.h):
     * \code
     *    enum Mode {
     *         File         = 1,
     *         Directory    = 2,
     *         Files        = 4,
     *         ExistingOnly = 8,
     *         LocalOnly    = 16,
     *    };
     * \endcode
     * You can OR the values, e.g.
     * \code
     * KFile::Modes mode = KFile::Files |
     *                     KFile::ExistingOnly |
     *                     KFile::LocalOnly );
     * setMode( mode );
     * \endcode
     */
    void setMode(KFile::Modes m);

    /**
     * Returns the mode of the filedialog.
     * @see setMode()
     */
    KFile::Modes mode() const;

    /**
     * Sets the text to be displayed in front of the selection.
     *
     * The default is "Location".
     * Most useful if you want to make clear what
     * the location is used for.
     */
    void setLocationLabel(const QString &text);

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(5, 66)
    /**
     * Returns a pointer to the toolbar.
     * @deprecated since 5.66 due to no known users and leaking KXMLGui into the API.
     *
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(5, 66, "No known user")
    KToolBar *toolBar() const;
#endif

    /**
     * @returns a pointer to the OK-Button in the filedialog.
     * Note that the button is hidden and unconnected when using KFileWidget alone;
     * KFileDialog shows it and connects to it.
     */
    QPushButton *okButton() const;

    /**
     * @returns a pointer to the Cancel-Button in the filedialog.
     * Note that the button is hidden and unconnected when using KFileWidget alone;
     * KFileDialog shows it and connects to it.
     */
    QPushButton *cancelButton() const;

    /**
     * @returns the combobox used to type the filename or full location of the file.
     */
    KUrlComboBox *locationEdit() const;

    /**
     * @returns the combobox that contains the filters
     */
    KFileFilterCombo *filterWidget() const;

    /**
     * @returns a pointer to the action collection, holding all the used
     * KActions.
     */
    KActionCollection *actionCollection() const;

    /**
     * This method implements the logic to determine the user's default directory
     * to be listed. E.g. the documents directory, home directory or a recently
     * used directory.
     * @param startDir A URL specifying the initial directory, or using the
     *                 @c kfiledialog:/// syntax to specify a last used
     *                 directory.  If this URL specifies a file name, it is
     *                 ignored.  Refer to the KFileWidget::KFileWidget()
     *                 documentation for the @c kfiledialog:/// URL syntax.
     * @param recentDirClass If the @c kfiledialog:/// syntax is used, this
     *        will return the string to be passed to KRecentDirs::dir() and
     *        KRecentDirs::add().
     * @return The URL that should be listed by default (e.g. by KFileDialog or
     *         KDirSelectDialog).
     * @see KFileWidget::KFileWidget()
     */
    static QUrl getStartUrl(const QUrl &startDir, QString &recentDirClass);

    /**
     * Similar to getStartUrl(const QUrl& startDir,QString& recentDirClass),
     * but allows both the recent start directory keyword and a suggested file name
     * to be returned.
     * @param startDir A URL specifying the initial directory and/or filename,
     *                 or using the @c kfiledialog:/// syntax to specify a
     *                 last used location.
     *                 Refer to the KFileWidget::KFileWidget()
     *                 documentation for the @c kfiledialog:/// URL syntax.
     * @param recentDirClass If the @c kfiledialog:/// syntax is used, this
     *        will return the string to be passed to KRecentDirs::dir() and
     *        KRecentDirs::add().
     * @param fileName The suggested file name, if specified as part of the
     *        @p StartDir URL.
     * @return The URL that should be listed by default (e.g. by KFileDialog or
     *         KDirSelectDialog).
     *
     * @see KFileWidget::KFileWidget()
     * @since 4.3
     */
    static QUrl getStartUrl(const QUrl &startDir, QString &recentDirClass, QString &fileName);

    /**
     * @internal
     * Used by KDirSelectDialog to share the dialog's start directory.
     */
    static void setStartDir(const QUrl &directory);

    /**
     * Set a custom widget that should be added to the file dialog.
     * @param widget A widget, or a widget of widgets, for displaying custom
     *               data in the file widget. This can be used, for example, to
     *               display a check box with the caption "Open as read-only".
     *               When creating this widget, you don't need to specify a parent,
     *               since the widget's parent will be set automatically by KFileWidget.
     */
    void setCustomWidget(QWidget *widget);

    /**
     * Sets a custom widget that should be added below the location and the filter
     * editors.
     * @param text     Label of the custom widget, which is displayed below the labels
     *                 "Location:" and "Filter:".
     * @param widget   Any kind of widget, but preferable a combo box or a line editor
     *                 to be compliant with the location and filter layout.
     *                 When creating this widget, you don't need to specify a parent,
     *                 since the widget's parent will be set automatically by KFileWidget.
     */
    void setCustomWidget(const QString &text, QWidget *widget);

    /**
     * Sets whether the user should be asked for confirmation
     * when an overwrite might occur.
     *
     * @param enable Set this to true to enable checking.
     * @since 4.2
     */
    void setConfirmOverwrite(bool enable);

    /**
     * Forces the inline previews to be shown or hidden, depending on @p show.
     *
     * @param show Whether to show inline previews or not.
     * @since 4.2
     */
    void setInlinePreviewShown(bool show);

    /**
     * Provides a size hint, useful for dialogs that embed the widget.
     *
     * @return a QSize, calculated to be optimal for a dialog.
     * @since 5.0
     */
    QSize dialogSizeHint() const;

    /**
     * Sets how the view should be displayed.
     *
     * @see KFile::FileView
     * @since 5.0
     */
    void setViewMode(KFile::FileView mode);

    /**
     * Reimplemented
     */
    QSize sizeHint() const override;

    /**
     * Set the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported.
     *
     * @sa QFileDialog::setSupportedSchemes
     * @since 5.43
     */
    void setSupportedSchemes(const QStringList &schemes);

    /**
     * Returns the URL schemes that the file widget should allow navigating to.
     *
     * If the returned list is empty, all schemes are supported. Examples for
     * schemes are @c "file" or @c "ftp".
     *
     * @sa QFileDialog::supportedSchemes
     * @since 5.43
     */
    QStringList supportedSchemes() const;

public Q_SLOTS:
    /**
     * Called when clicking ok (when this widget is used in KFileDialog)
     * Might or might not call accept().
     */
    void slotOk();
    void accept();
    void slotCancel();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

Q_SIGNALS:
    /**
      * Emitted when the user selects a file. It is only emitted in single-
      * selection mode. The best way to get notified about selected file(s)
      * is to connect to the okClicked() signal inherited from KDialog
      * and call selectedFile(), selectedFiles(),
      * selectedUrl() or selectedUrls().
      *
      * \since 4.4
      */
    void fileSelected(const QUrl &);

    /**
     * Emitted when the user highlights a file.
     * \since 4.4
     */
    void fileHighlighted(const QUrl &);

    /**
     * Emitted when the user highlights one or more files in multiselection mode.
     *
     * Note: fileHighlighted() or fileSelected() are @em not
     * emitted in multiselection mode. You may use selectedItems() to
     * ask for the current highlighted items.
     * @see fileSelected
     */
    void selectionChanged();

    /**
     * Emitted when the filter changed, i.e. the user entered an own filter
     * or chose one of the predefined set via setFilter().
     *
     * @param filter contains the new filter (only the extension part,
     * not the explanation), i.e. "*.cpp" or "*.cpp *.cc".
     *
     * @see setFilter()
     * @see currentFilter()
     */
    void filterChanged(const QString &filter);

    /**
     * Emitted by slotOk() (directly or asynchronously) once everything has
     * been done. Should be used by the caller to call accept().
     */
    void accepted();

public:
    /**
     * @returns the KDirOperator used to navigate the filesystem
     * @since 4.3
     */
    KDirOperator *dirOperator();

    /**
     * reads the configuration for this widget from the given config group
     * @param group the KConfigGroup to read from
     * @since 4.4
     */
    void readConfig(KConfigGroup &group);

private:
    friend class KFileWidgetPrivate;
    std::unique_ptr<KFileWidgetPrivate> const d;
};

#endif
