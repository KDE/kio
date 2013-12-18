/* This file is part of the KDE libraries
    Copyright (C) 2000 Stephan Kulow <coolo@kde.org>
    Copyright (C) 2000-2013 David Faure <faure@kde.org>
    Copyright (C) 2006 Kevin Ottens <ervin@kde.org>

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

#ifndef KIO_JOBUIDELEGATEEXTENSION_H
#define KIO_JOBUIDELEGATEEXTENSION_H

#include <kio/kiocore_export.h>
#include <kio/global.h>
#include <QDateTime>

class KJob;
namespace KIO
{
class Job;
class ClipboardUpdater;

// KDE5: get rid of M_OVERWRITE_ITSELF, trigger it internally if src==dest
// KDE5: get rid of M_SINGLE. If not multi, then single ;)
// KDE5: use QFlags to get rid of all the casting!
/**
 * M_OVERWRITE: We have an existing dest, show details about it and offer to overwrite it.
 * M_OVERWRITE_ITSELF: Warn that the current operation would overwrite a file with itself,
 *                     which is not allowed.
 * M_SKIP: Offer a "Skip" button, to skip other files too. Requires M_MULTI.
 * M_SINGLE: Deprecated and unused, please ignore.
 * M_MULTI: Set if the current operation concerns multiple files, so it makes sense
 *  to offer buttons that apply the user's choice to all files/folders.
 * M_RESUME: Offer a "Resume" button (plus "Resume All" if M_MULTI)
 * M_NORENAME: Don't offer a "Rename" button
 * M_ISDIR: The dest is a directory, so label the "overwrite" button something like "merge" instead.
 */
enum RenameDialog_Mode { M_OVERWRITE = 1, M_OVERWRITE_ITSELF = 2, M_SKIP = 4, M_SINGLE = 8, M_MULTI = 16, M_RESUME = 32, M_NORENAME = 64, M_ISDIR = 128 };

/**
 * The result of open_RenameDialog().
 */
enum RenameDialog_Result {R_RESUME = 6, R_RESUME_ALL = 7, R_OVERWRITE = 4, R_OVERWRITE_ALL = 5, R_SKIP = 2, R_AUTO_SKIP = 3, R_RENAME = 1, R_AUTO_RENAME = 8, R_RETRY = 9, R_CANCEL = 0};


enum SkipDialog_Result { S_SKIP = 1, S_AUTO_SKIP = 2, S_RETRY = 3, S_CANCEL = 0 };


/**
 * An abstract class defining interaction with users from KIO jobs:
 * \li asking what to do in case of a conflict while copying/moving files or directories
 * \li asking what to do in case of an error while copying/moving files or directories
 * \li asking for confirmation before deleting files or directories
 * \li popping up message boxes when the slave requests it
 * @since 5.0
 */
class KIOCORE_EXPORT JobUiDelegateExtension
{
protected:
    /**
     * Constructor
     */
    JobUiDelegateExtension();

    /**
     * Destructor
     */
    virtual ~JobUiDelegateExtension();

public:

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
    virtual RenameDialog_Result askFileRename(KJob * job,
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
                                              const QDateTime &mtimeDest = QDateTime()) = 0;

    /**
     * @internal
     * See skipdialog.h
     */
    virtual SkipDialog_Result askSkip(KJob * job,
                                      bool multi,
                                      const QString & error_text) = 0;

    /**
     * The type of deletion: real deletion, moving the files to the trash
     * or emptying the trash
     * Used by askDeleteConfirmation.
     */
    enum DeletionType { Delete, Trash, EmptyTrash };
    /**
     * ForceConfirmation: always ask the user for confirmation
     * DefaultConfirmation: don't ask the user if he/she said "don't ask again".
     *
     * Used by askDeleteConfirmation.
     */
    enum ConfirmationType { DefaultConfirmation, ForceConfirmation };
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
    virtual bool askDeleteConfirmation(const QList<QUrl>& urls, DeletionType deletionType,
                                       ConfirmationType confirmationType) = 0;

    /**
     * Message box types.
     *
     * Should be kept in sync with SlaveBase::MessageBoxType.
     *
     * @since 4.11
     */
    enum MessageBoxType {
        QuestionYesNo = 1,
        WarningYesNo = 2,
        WarningContinueCancel = 3,
        WarningYesNoCancel = 4,
        Information = 5,
        SSLMessageBox = 6
    };

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
     */
    virtual int requestMessageBox(MessageBoxType type, const QString &text,
                                  const QString &caption,
                                  const QString &buttonYes,
                                  const QString &buttonNo,
                                  const QString &iconYes = QString(),
                                  const QString &iconNo = QString(),
                                  const QString &dontAskAgainName = QString(),
                                  const KIO::MetaData &sslMetaData = KIO::MetaData()) = 0;

    enum ClipboardUpdaterMode {
        UpdateContent,
        OverwriteContent,
        RemoveContent
    };

    /**
     * Creates a clipboard updater as a child of the given job.
     */
    virtual ClipboardUpdater* createClipboardUpdater(Job* job, ClipboardUpdaterMode mode);
    /**
     * Update URL in clipboard, if present
     */
    virtual void updateUrlInClipboard(const QUrl &src, const QUrl &dest);

private:
    class Private;
    Private * const d;
};

/**
 * Returns the default job UI delegate extension to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return NULL, if no kio GUI library is loaded.
 * @since 5.0
 */
KIOCORE_EXPORT JobUiDelegateExtension *defaultJobUiDelegateExtension();

/**
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate extension
 * automatically.
 * @since 5.0
 */
KIOCORE_EXPORT void setDefaultJobUiDelegateExtension(JobUiDelegateExtension* extension);

} // namespace KIO

#endif
