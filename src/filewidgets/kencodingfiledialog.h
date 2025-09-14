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

#include <memory>

struct KEncodingFileDialogPrivate;

#if KIOFILEWIDGETS_ENABLE_DEPRECATED_SINCE(6, 19)

/*!
 * \class KEncodingFileDialog
 * \inmodule KIOFileWidgets
 *
 * \brief Provides a user (and developer) friendly way to select files with support for
 * choosing encoding.
 *
 * This class comes with a private constructor, the only way to show a file dialog
 * is through its static methods.
 *
 * \deprecated[6.19] Use QFileDialog instead.
 */
class KEncodingFileDialog : public QDialog
{
    Q_OBJECT

public:
    class KIOFILEWIDGETS_EXPORT Result
    {
    public:
        /*!
         *
         */
        QStringList fileNames;

        /*!
         *
         */
        QList<QUrl> URLs;

        /*!
         *
         */
        QString encoding;
    };

    /*!
     * Creates a modal file dialog and return the selected
     * filename or an empty string if none was chosen additionally a chosen
     * encoding value is returned.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * \a encoding The encoding shown in the encoding combo.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getOpenFileNameAndEncoding(const QString &encoding = QString(),
                                                                   const QUrl &startDir = QUrl(),
                                                                   const QString &filter = QString(),
                                                                   QWidget *parent = nullptr,
                                                                   const QString &title = QString());

    /*!
     * Creates a modal file dialog and returns the selected encoding and the selected
     * filenames or an empty list if none was chosen.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * \a encoding The encoding shown in the encoding combo.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getOpenFileNamesAndEncoding(const QString &encoding = QString(),
                                                                    const QUrl &startDir = QUrl(),
                                                                    const QString &filter = QString(),
                                                                    QWidget *parent = nullptr,
                                                                    const QString &title = QString());

    /*!
     * Creates a modal file dialog and returns the selected encoding and
     * URL or an empty string if none was chosen.
     *
     * Note that with
     * this method the user must select an existing URL.
     *
     * \a encoding The encoding shown in the encoding combo.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getOpenUrlAndEncoding(const QString &encoding = QString(),
                                                              const QUrl &startDir = QUrl(),
                                                              const QString &filter = QString(),
                                                              QWidget *parent = nullptr,
                                                              const QString &title = QString());

    /*!
     * Creates a modal file dialog and returns the selected encoding
     * URLs or an empty list if none was chosen.
     *
     * Note that with
     * this method the user must select an existing filename.
     *
     * \a encoding The encoding shown in the encoding combo.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getOpenUrlsAndEncoding(const QString &encoding = QString(),
                                                               const QUrl &startDir = QUrl(),
                                                               const QString &filter = QString(),
                                                               QWidget *parent = nullptr,
                                                               const QString &title = QString());

    /*!
     * Creates a modal file dialog and returns the selected encoding and
     * filename or an empty string if none was chosen.
     *
     * Note that with this
     * method the user need not select an existing filename.
     *
     * \a encoding The encoding shown in the encoding combo.
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li a relative path or a filename determining the
     *     directory to start in and the file to be selected.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>;' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getSaveFileNameAndEncoding(const QString &encoding = QString(),
                                                                   const QUrl &startDir = QUrl(),
                                                                   const QString &filter = QString(),
                                                                   QWidget *parent = nullptr,
                                                                   const QString &title = QString());

    /*!
     * Creates a modal file dialog and returns the selected encoding and
     * filename or an empty string if none was chosen.
     *
     * Note that with this
     * method the user need not select an existing filename.
     *
     * \a encoding The encoding shown in the encoding combo.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li a relative path or a filename determining the
     *     directory to start in and the file to be selected.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    see KFileFilter::KFileFilter(const QString &label, const QStringList &filePatterns, const QStringList &mimePatterns)
     *    for examples of patterns.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The name of the dialog widget.
     */
    KIOFILEWIDGETS_DEPRECATED_VERSION(6, 19, "Use QFileDialog")
    static KIOFILEWIDGETS_EXPORT Result getSaveUrlAndEncoding(const QString &encoding = QString(),
                                                              const QUrl &startDir = QUrl(),
                                                              const QString &filter = QString(),
                                                              QWidget *parent = nullptr,
                                                              const QString &title = QString());

    QSize sizeHint() const override;

protected:
    void hideEvent(QHideEvent *e) override;

private Q_SLOTS:
    void accept() override;

    void slotOk();
    void slotCancel();

private:
    /*!
     * Constructs a file dialog for text files with encoding selection possibility.
     *
     * \a startDir This can either be
     * \list
     * \li The URL of the directory to start in.
     * \li QString() to start in the current working
     *  directory, or the last directory where a file has been
     *  selected.
     * \li ':<keyword>' to start in the directory last used
     *     by a filedialog in the same application that specified
     *     the same keyword.
     * \li '::<keyword>' to start in the directory last used
     *     by a filedialog in any application that specified the
     *     same keyword.
     * \endlist
     *
     * \a encoding The encoding shown in the encoding combo. If it's
     *          QString(), the global default encoding will be shown.
     *
     * \a filter A shell glob or a MIME type filter that specifies which files to display.
     *    The preferred option is to set a list of MIME type names, see setMimeFilter() for details.
     *    Otherwise you can set the text to be displayed for the each glob, and
     *    provide multiple globs, see setFilters() for details.
     *
     * \a title The title of the dialog
     *
     * \a type This can either be
     * \list
     * \li QFileDialog::AcceptOpen (open dialog, the default setting)
     * \li QFileDialog::AcceptSave
     * \endlist
     * \a parent The parent widget of this dialog
     */
    KEncodingFileDialog(const QUrl &startDir = QUrl(),
                        const QString &encoding = QString(),
                        const QString &filter = QString(),
                        const QString &title = QString(),
                        QFileDialog::AcceptMode type = QFileDialog::AcceptOpen,
                        QWidget *parent = nullptr);
    ~KEncodingFileDialog() override;

    /*!
     * Returns The selected encoding if the constructor with the encoding parameter was used, otherwise QString().
     */
    QString selectedEncoding() const;

    std::unique_ptr<KEncodingFileDialogPrivate> const d;
};

#endif
#endif
