/*
    SPDX-FileCopyrightText: 1998, 2008, 2009 David Faure <faure@kde.org>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef KNAMEANDURLINPUTDIALOG_H
#define KNAMEANDURLINPUTDIALOG_H

#include "kiofilewidgets_export.h"
#include <QDialog>

#include <memory>

class KUrlRequester;
class KNameAndUrlInputDialogPrivate;
class QUrl;

/*!
 * \class KNameAndUrlInputDialog
 * \inmodule KIOFileWidgets
 *
 * \brief Dialog to ask for a name (e.g.\ filename) and a URL.
 *
 * Basically a merge of KLineEditDlg and KUrlRequesterDlg ;)
 */
class KIOFILEWIDGETS_EXPORT KNameAndUrlInputDialog : public QDialog
{
    Q_OBJECT
public:
    /*!
     * \a nameLabel label for the name field
     *
     * \a urlLabel label for the URL requester
     *
     * \a startDir start directory for the URL requester (optional)
     *
     * \a parent parent widget
     */
    KNameAndUrlInputDialog(const QString &nameLabel, const QString &urlLabel, const QUrl &startDir, QWidget *parent);

    ~KNameAndUrlInputDialog() override;

    /*!
     * Pre-fill the name lineedit.
     */
    void setSuggestedName(const QString &name);
    /*!
     * Pre-fill the URL requester.
     */
    void setSuggestedUrl(const QUrl &url);

    /*!
     * Returns the name the user entered
     */
    QString name() const;
    /*!
     * Returns the URL the user entered
     */
    QUrl url() const;
    /*!
     * Returns the URL the user entered, as plain text.
     * This is only useful for creating relative symlinks.
     * \since 5.25
     */
    QString urlText() const;

private:
    std::unique_ptr<KNameAndUrlInputDialogPrivate> const d;
};

#endif
