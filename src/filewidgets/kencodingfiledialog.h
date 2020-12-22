// -*- c++ -*-
/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2003 Joseph Wenninger <jowenn@kde.org>
    SPDX-FileCopyrightText: 2003 Andras Mantia <amantia@freemail.hu>
    SPDX-FileCopyrightText: 2013 Teo Mrnjavac <teo@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef __KENCODINGFILEDIALOG_H__
#define __KENCODINGFILEDIALOG_H__

#include "kiofilewidgets_export.h"

#include <QFileDialog>

struct KEncodingFileDialogPrivate;

/**
 * @class KEncodingFileDialog kencodingfiledialog.h <KEncodingFileDialog>
 *
 * Provides a user (and developer) friendly way to select files with support for
 * choosing encoding.
 * This class comes with a private constructor, the only way to show a file dialog
 * is through its static methods.
 */
class KEncodingFileDialog : public QDialog
{
    Q_OBJECT

public:
    class KIOFILEWIDGETS_EXPORT Result
    {
    public:
        QStringList fileNames;
        QList<QUrl> URLs;
        QString encoding;
    };

    /**
     * Creates a modal file dialog and return the selected
     * filename or an empty string if none was chosen additionally a chosen
     * encoding value is returned.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getOpenFileNameAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    /**
     * Creates a modal file dialog and returns the selected encoding and the selected
     * filenames or an empty list if none was chosen.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getOpenFileNamesAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    /**
     * Creates a modal file dialog and returns the selected encoding and
     * URL or an empty string if none was chosen.
     *
     * Note that with
     * this method the user must select an existing URL.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getOpenUrlAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    /**
     * Creates a modal file dialog and returns the selected encoding
     * URLs or an empty list if none was chosen.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getOpenUrlsAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    /**
     * Creates a modal file dialog and returns the selected encoding and
     * filename or an empty string if none was chosen.
     *
     * Note that with this
     * method the user need not select an existing filename.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li a relative path or a filename determining the
     *             directory to start in and the file to be selected.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getSaveFileNameAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    /**
     * Creates a modal file dialog and returns the selected encoding and
     * filename or an empty string if none was chosen.
     *
     * Note that with this
     * method the user need not select an existing filename.
     *
     * @param encoding The encoding shown in the encoding combo.
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li a relative path or a filename determining the
     *             directory to start in and the file to be selected.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     * @param parent The widget the dialog will be centered on initially.
     * @param caption The name of the dialog widget.
     */
    static KIOFILEWIDGETS_EXPORT Result getSaveUrlAndEncoding(const QString &encoding = QString(),
            const QUrl &startDir = QUrl(),
            const QString &filter = QString(),
            QWidget *parent = nullptr,
            const QString &caption = QString());

    QSize sizeHint() const override;

protected:
    void hideEvent(QHideEvent *e) override;

private Q_SLOTS:
    void accept() override;

    void slotOk();
    void slotCancel();

private:
    /**
     * Constructs a file dialog for text files with encoding selection possibility.
     *
     * @param startDir This can either be
     *         @li The URL of the directory to start in.
     *         @li QString() to start in the current working
     *          directory, or the last directory where a file has been
     *          selected.
     *         @li ':&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in the same application that specified
     *             the same keyword.
     *         @li '::&lt;keyword&gt;' to start in the directory last used
     *             by a filedialog in any application that specified the
     *             same keyword.
     *
     * @param encoding The encoding shown in the encoding combo. If it's
     *          QString(), the global default encoding will be shown.
     *
     * @param filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilter() for details.
     *
     * @param caption The caption of the dialog
     *
     * @param type This can either be
     *      @li QFileDialog::AcceptOpen (open dialog, the default setting)
     *      @li QFileDialog::AcceptSave
     * @param parent The parent widget of this dialog
     */
    KEncodingFileDialog(const QUrl &startDir = QUrl(),
                        const QString &encoding = QString(),
                        const QString &filter = QString(),
                        const QString &caption = QString(),
                        QFileDialog::AcceptMode type = QFileDialog::AcceptOpen,
                        QWidget *parent = nullptr);
    /**
     * Destructs the file dialog.
     */
    ~KEncodingFileDialog();

    /**
    * @returns The selected encoding if the constructor with the encoding parameter was used, otherwise QString().
    */
    QString selectedEncoding() const;

    KEncodingFileDialogPrivate *const d;
};

#endif
