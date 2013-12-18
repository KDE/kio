/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
                       David Faure <faure@kde.org>
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>
    Copyright (C) 2013 Dawit Alemayehu <adawit@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KIO_JOBUIDELEGATE_H
#define KIO_JOBUIDELEGATE_H

#include <kdialogjobuidelegate.h>
#include <kio/jobuidelegateextension.h>
#include <kio/skipdialog.h>
#include <kio/renamedialog.h>
#include <kio/global.h>

class KJob;
namespace KIO
{
class Job;

/**
 * A UI delegate tuned to be used with KIO Jobs.
 */
class KIOWIDGETS_EXPORT JobUiDelegate : public KDialogJobUiDelegate, public JobUiDelegateExtension
{
    Q_OBJECT

public:
    /**
     * Constructs a new KIO Job UI delegate.
     */
    JobUiDelegate();

    /**
     * Destroys the KIO Job UI delegate.
     */
    virtual ~JobUiDelegate();

public:

    /**
     * Associate this job with a window given by @p window.
     * @param window the window to associate to
     * @see window()
     */
    void setWindow(QWidget *window) Q_DECL_OVERRIDE;

    /**
     * \relates KIO::RenameDialog
     * Construct a modal, parent-less "rename" dialog, and return
     * a result code, as well as the new dest. Much easier to use than the
     * class RenameDialog directly.
     *
     * @param caption the caption for the dialog box
     * @param src the URL of the file/dir we're trying to copy, as it's part of the text message
     * @param dest the URL of the destination file/dir, i.e. the one that already exists
     * @param mode parameters for the dialog (which buttons to show...),
     *             see RenameDialog_Mode
     * @param newDestPath the new destination path, valid if R_RENAME was returned.
     * @param sizeSrc size of source file
     * @param sizeDest size of destination file
     * @param ctimeSrc creation time of source file
     * @param ctimeDest creation time of destination file
     * @param mtimeSrc modification time of source file
     * @param mtimeDest modification time of destination file
     * @return the result
     */
    RenameDialog_Result askFileRename(KJob * job,
                                      const QString & caption,
                                      const QUrl & src,
                                      const QUrl & dest,
                                      KIO::RenameDialog_Mode mode,
                                      QString& newDest,
                                      KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                                      KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                                      const QDateTime &ctimeSrc = QDateTime(),
                                      const QDateTime &ctimeDest = QDateTime(),
                                      const QDateTime &mtimeSrc = QDateTime(),
                                      const QDateTime &mtimeDest = QDateTime()) Q_DECL_OVERRIDE;

    /**
     * @internal
     * See skipdialog.h
     */
    SkipDialog_Result askSkip(KJob * job,
                              bool multi,
                              const QString & error_text) Q_DECL_OVERRIDE;

    /**
     * Ask for confirmation before deleting/trashing @p urls.
     *
     * Note that this method is not called automatically by KIO jobs. It's the application's
     * responsibility to ask the user for confirmation before calling KIO::del() or KIO::trash().
     *
     * @param urls the urls about to be deleted/trashed
     * @param method the type of deletion (Delete for real deletion, Trash otherwise)
     * @param confirmation see ConfirmationType. Normally set to DefaultConfirmation.
     * Note: the window passed to setWindow is used as the parent for the message box.
     * @return true if confirmed
     */
    bool askDeleteConfirmation(const QList<QUrl>& urls, DeletionType deletionType,
                               ConfirmationType confirmationType) Q_DECL_OVERRIDE;

    /**
    * This function allows for the delegation user prompts from the ioslaves.
    *
    * @param type the desired type of message box.
    * @param text the message shown to the user.
    * @param caption the caption of the message dialog box.
    * @param buttonYes the text for the YES button.
    * @param buttonNo the text for the NO button.
    * @param iconYes the icon shown on the YES button.
    * @param iconNo the icon shown on the NO button.
    * @param dontAskAgainName the name used to store result from 'Do not ask again' checkbox.
    * @param sslMetaData SSL information used by the SSLMessageBox.
    *
    * @since 4.11
    *
    * @internal
    */
    int requestMessageBox(MessageBoxType type, const QString& text,
                          const QString& caption,
                          const QString& buttonYes,
                          const QString& buttonNo,
                          const QString& iconYes = QString(),
                          const QString& iconNo = QString(),
                          const QString& dontAskAgainName = QString(),
                          const KIO::MetaData& sslMetaData = KIO::MetaData()) Q_DECL_OVERRIDE;

    /**
     * Creates a clipboard updater
     */
    ClipboardUpdater* createClipboardUpdater(Job* job, ClipboardUpdaterMode mode) Q_DECL_OVERRIDE;
    /**
     * Update URL in clipboard, if present
     */
    void updateUrlInClipboard(const QUrl &src, const QUrl &dest) Q_DECL_OVERRIDE;

private:
    class Private;
    Private * const d;
};
}

#endif
