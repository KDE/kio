/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 1999-2008 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2001 Holger Freyther <freyther@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_RENAMEDIALOG_H
#define KIO_RENAMEDIALOG_H

#include <QDateTime>
#include <QDialog>
#include <QString>
#include <kio/jobuidelegateextension.h>

#include "kiowidgets_export.h"
#include <kio/global.h>

#include <memory>

class QScrollArea;
class QLabel;
class QPixmap;
class KFileItem;
class KSqueezedTextLabel;

namespace KIO
{
/*!
 * \class KIO::RenameDialog
 * \inheaderfile KIO/RenameDialog
 * \inmodule KIOWidgets
 *
 * The dialog shown when a CopyJob realizes that a destination file already exists,
 * and wants to offer the user with the choice to either Rename, Overwrite, Skip;
 * this dialog is also used when a .part file exists and the user can choose to
 * Resume a previous download.
 */
class KIOWIDGETS_EXPORT RenameDialog : public QDialog
{
    Q_OBJECT
public:
    /*!
     * Construct a "rename" dialog to let the user know that \a src is about to overwrite \a dest.
     *
     * \a parent parent widget (often 0)
     *
     * \a title the title for the dialog box
     *
     * \a src the url to the file/dir we're trying to copy, as it's part of the text message
     *
     * \a dest the path to destination file/dir, i.e. the one that already exists
     *
     * \a options parameters for the dialog (which buttons to show...),
     *
     * \a sizeSrc size of source file
     *
     * \a sizeDest size of destination file
     *
     * \a ctimeSrc creation time of source file
     *
     * \a ctimeDest creation time of destination file
     *
     * \a mtimeSrc modification time of source file
     *
     * \a mtimeDest modification time of destination file
     */
    RenameDialog(QWidget *parent,
                 const QString &title,
                 const QUrl &src,
                 const QUrl &dest,
                 RenameDialog_Options options,
                 KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                 KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                 const QDateTime &ctimeSrc = QDateTime(),
                 const QDateTime &ctimeDest = QDateTime(),
                 const QDateTime &mtimeSrc = QDateTime(),
                 const QDateTime &mtimeDest = QDateTime());
    ~RenameDialog() override;

    /*!
     * Returns the new destination
     * valid only if RENAME was chosen
     */
    QUrl newDestUrl();

    /*!
     * Returns an automatically renamed destination
     * valid always
     */
    QUrl autoDestUrl() const;

public Q_SLOTS:
    /*!
     *
     */
    void cancelPressed();

    /*!
     *
     */
    void renamePressed();

    /*!
     *
     */
    void skipPressed();

    /*!
     *
     */
    void overwritePressed();

    /*!
     *
     */
    void overwriteAllPressed();

    /*!
     *
     */
    void overwriteWhenOlderPressed();

    /*!
     *
     */
    void resumePressed();

    /*!
     *
     */
    void resumeAllPressed();

    /*!
     *
     */
    void suggestNewNamePressed();

protected Q_SLOTS:
    void enableRenameButton(const QString &);
private Q_SLOTS:
    KIOWIDGETS_NO_EXPORT void applyAllPressed();
    KIOWIDGETS_NO_EXPORT void showSrcIcon(const KFileItem &);
    KIOWIDGETS_NO_EXPORT void showDestIcon(const KFileItem &);
    KIOWIDGETS_NO_EXPORT void showSrcPreview(const KFileItem &, const QPixmap &);
    KIOWIDGETS_NO_EXPORT void showDestPreview(const KFileItem &, const QPixmap &);
    KIOWIDGETS_NO_EXPORT void resizePanels();

private:
    KIOWIDGETS_NO_EXPORT QWidget *createContainerWidget(QLabel *preview, QLabel *SizeLabel, QLabel *DateLabel);

    class RenameDialogPrivate;
    std::unique_ptr<RenameDialogPrivate> const d;
};

}

#endif
