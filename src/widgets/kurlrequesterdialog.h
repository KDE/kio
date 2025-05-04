/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Wilco Greven <greven@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KURLREQUESTERDIALOG_H
#define KURLREQUESTERDIALOG_H

#include "kiowidgets_export.h"
#include <QDialog>
#include <QUrl>

#include <memory>

class KUrlRequester;
class QFileDialog;
class KUrlRequesterDialogPrivate;

/*!
 * \class KUrlRequesterDialog
 * \inmodule KIOWidgets
 *
 * \brief Simple dialog to enter a filename/url.
 *
 * Dialog in which a user can enter a filename or url. It is a dialog
 * encapsulating KUrlRequester.
 */
class KIOWIDGETS_EXPORT KUrlRequesterDialog : public QDialog
{
    Q_OBJECT

public:
    /*!
     * Constructs a KUrlRequesterDialog.
     *
     * \a url    The url of the directory to start in. Use QString()
     *               to start in the current working directory, or the last
     *               directory where a file has been selected.
     *
     * \a parent The parent object of this widget.
     */
    explicit KUrlRequesterDialog(const QUrl &url, QWidget *parent = nullptr);

    /*!
     * Constructs a KUrlRequesterDialog.
     *
     * \a url    The url of the directory to start in. Use QString()
     *               to start in the current working directory, or the last
     *               directory where a file has been selected.
     *
     * \a text   Text of the label
     *
     * \a parent The parent object of this widget.
     */
    KUrlRequesterDialog(const QUrl &url, const QString &text, QWidget *parent);

    ~KUrlRequesterDialog() override;

    /*!
     * Returns the fully qualified filename.
     */
    QUrl selectedUrl() const;

    /*!
     * Creates a modal dialog, executes it and returns the selected URL.
     *
     * \a url This specifies the initial path of the input line.
     *
     * \a parent The widget the dialog will be centered on initially.
     *
     * \a title The title to use for the dialog.
     */
    static QUrl getUrl(const QUrl &url = QUrl(), QWidget *parent = nullptr, const QString &title = QString());

    /*!
     * Returns a pointer to the KUrlRequester.
     */
    KUrlRequester *urlRequester();

private:
    friend class KUrlRequesterDialogPrivate;
    std::unique_ptr<KUrlRequesterDialogPrivate> const d;

    Q_DISABLE_COPY(KUrlRequesterDialog)
};

#endif // KURLREQUESTERDIALOG_H
