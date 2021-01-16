/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000-2013 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOBUIDELEGATEEXTENSION_H
#define KIO_JOBUIDELEGATEEXTENSION_H

#include "kiocore_export.h"
#include <kio/global.h>
#include <QDateTime>

class KJob;
namespace KIO
{
class Job;
class ClipboardUpdater;

/**
 * @see RenameDialog_Options
 * @since 5.0
 */
enum RenameDialog_Option {
    RenameDialog_Overwrite = 1, ///< We have an existing destination, show details about it and offer to overwrite it.
    RenameDialog_OverwriteItself = 2, ///< Warn that the current operation would overwrite a file with itself, which is not allowed.
    RenameDialog_Skip = 4, ///< Offer a "Skip" button, to skip other files too. Requires RenameDialog_MultipleItems.
    RenameDialog_MultipleItems = 8, ///< Set if the current operation concerns multiple files, so it makes sense to offer buttons that apply the user's choice to all files/folders.
    RenameDialog_Resume = 16, ///< Offer a "Resume" button (plus "Resume All" if RenameDialog_MultipleItems).
    RenameDialog_NoRename = 64, ///< Don't offer a "Rename" button.
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 78)
    RenameDialog_IsDirectory = 128, ///< @deprecated since 5.78, use RenameDialog_DestIsDirectory instead.
#endif
    RenameDialog_DestIsDirectory = 128, ///< @since 5.78. The destination is a directory, the dialog updates labels and tooltips accordingly.
    RenameDialog_SourceIsDirectory = 256, ///< @since 5.78. The source is a directory, the dialog updates labels and tooltips accordingly.
};
/**
 * Stores a combination of #RenameDialog_Option values.
 */
Q_DECLARE_FLAGS(RenameDialog_Options, RenameDialog_Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(RenameDialog_Options)

// For compat
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
/**
 * @deprecated since 5.0, use the RenameDialog_Option enum values
 */
enum {
    M_OVERWRITE = RenameDialog_Overwrite,
    M_OVERWRITE_ITSELF = RenameDialog_OverwriteItself,
    M_SKIP = RenameDialog_Skip,
    M_MULTI = RenameDialog_MultipleItems,
    M_RESUME = RenameDialog_Resume,
    M_NORENAME = RenameDialog_NoRename,
    M_ISDIR = RenameDialog_IsDirectory,
};
/**
 * @deprecated since 5.0, use RenameDialog_Options
 */
KIOCORE_DEPRECATED_VERSION(5, 0, "Use KIO::RenameDialog_Options")
typedef RenameDialog_Options RenameDialog_Mode;
#endif

/**
 * SkipDialog_MultipleItems: Set if the current operation concerns multiple files, so it makes sense
 *  to offer buttons that apply the user's choice to all files/folders.
 * @see SkipDialog_Options
 * @since 5.0
 */
enum SkipDialog_Option {
    SkipDialog_MultipleItems = 8,
};
/**
 * Stores a combination of #SkipDialog_Option values.
 */
Q_DECLARE_FLAGS(SkipDialog_Options, SkipDialog_Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(SkipDialog_Options)


/**
 * The result of a rename or skip dialog
 */
enum RenameDialog_Result {
    Result_Cancel = 0,
    Result_Rename = 1,
    Result_Skip = 2,
    Result_AutoSkip = 3,
    Result_Overwrite = 4,
    Result_OverwriteAll = 5,
    Result_Resume = 6,
    Result_ResumeAll = 7,
    Result_AutoRename = 8,
    Result_Retry = 9,
    /*
     * Can be returned only when multiple files are passed, Option overwrite is passed
     * And files modification times are valid
     * @since 5.77
     */
    Result_OverwriteWhenOlder = 10,

    // @deprecated since 5.0, use the undeprecated enum values
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 0)
    R_CANCEL = Result_Cancel,
    R_RENAME = Result_Rename,
    R_SKIP = Result_Skip,
    R_AUTO_SKIP = Result_AutoSkip,
    R_OVERWRITE = Result_Overwrite,
    R_OVERWRITE_ALL = Result_OverwriteAll,
    R_RESUME = Result_Resume,
    R_RESUME_ALL = Result_ResumeAll,
    R_AUTO_RENAME = Result_AutoRename,
    R_RETRY = Result_Retry,

    S_CANCEL = Result_Cancel,
    S_SKIP = Result_Skip,
    S_AUTO_SKIP = Result_AutoSkip,
    S_RETRY = Result_Retry,
#endif
};
typedef RenameDialog_Result SkipDialog_Result;

/**
 * @class KIO::JobUiDelegateExtension jobuidelegateextension.h <KIO/JobUiDelegateExtension>
 *
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
     * @param options parameters for the dialog (which buttons to show...)
     * @param newDest the new destination path, valid if R_RENAME was returned.
     * @param sizeSrc size of source file
     * @param sizeDest size of destination file
     * @param ctimeSrc creation time of source file
     * @param ctimeDest creation time of destination file
     * @param mtimeSrc modification time of source file
     * @param mtimeDest modification time of destination file
     * @return the result
     */
    virtual KIO::RenameDialog_Result askFileRename(KJob *job,
            const QString &caption,
            const QUrl &src,
            const QUrl &dest,
            KIO::RenameDialog_Options options,
            QString &newDest,
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
    virtual KIO::SkipDialog_Result askSkip(KJob *job,
                                      KIO::SkipDialog_Options options,
                                      const QString &error_text) = 0;

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
     * @param deletionType the type of deletion (Delete for real deletion, Trash otherwise)
     * @param confirmationType see ConfirmationType. Normally set to DefaultConfirmation.
     * Note: the window passed to setWindow is used as the parent for the message box.
     * @return true if confirmed
     */
    virtual bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType,
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
        SSLMessageBox = 6,
        //In KMessageBox::DialogType; Sorry = 7, Error = 8, QuestionYesNoCancel = 9
        WarningContinueCancelDetailed = 10,
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
        RemoveContent,
    };

    /**
     * Creates a clipboard updater as a child of the given job.
     */
    virtual ClipboardUpdater *createClipboardUpdater(Job *job, ClipboardUpdaterMode mode);
    /**
     * Update URL in clipboard, if present
     */
    virtual void updateUrlInClipboard(const QUrl &src, const QUrl &dest);

    // TODO KF6: add virtual_hook

private:
    class Private;
    Private *const d;
};

/**
 * Returns the default job UI delegate extension to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return nullptr, if no kio GUI library is loaded.
 * @since 5.0
 */
KIOCORE_EXPORT JobUiDelegateExtension *defaultJobUiDelegateExtension();

/**
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate extension
 * automatically.
 * @since 5.0
 */
KIOCORE_EXPORT void setDefaultJobUiDelegateExtension(JobUiDelegateExtension *extension);

} // namespace KIO

#endif
