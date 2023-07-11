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
#include <QDateTime>
#include <kio/global.h>

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
    RenameDialog_MultipleItems =
        8, ///< Set if the current operation concerns multiple files, so it makes sense to offer buttons that apply the user's choice to all files/folders.
    RenameDialog_Resume = 16, ///< Offer a "Resume" button (plus "Resume All" if RenameDialog_MultipleItems).
    RenameDialog_NoRename = 64, ///< Don't offer a "Rename" button.
    RenameDialog_DestIsDirectory = 128, ///< The destination is a directory, the dialog updates labels and tooltips accordingly. @since 5.78
    RenameDialog_SourceIsDirectory = 256, ///< The source is a directory, the dialog updates labels and tooltips accordingly. @since 5.78
};
/**
 * Stores a combination of #RenameDialog_Option values.
 */
Q_DECLARE_FLAGS(RenameDialog_Options, RenameDialog_Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(RenameDialog_Options)

/**
 * @see SkipDialog_Options
 * @since 5.0
 */
enum SkipDialog_Option {
    /**
     * Set if the current operation concerns multiple files, so it makes sense
     * to offer buttons that apply the user's choice to all files/folders.
     */
    SkipDialog_MultipleItems = 8,
    /**
     * Set if the current operation involves copying files/folders with certain
     * characters in their names that are not supported by the destination
     * filesystem (e.g.\ VFAT and NTFS disallow "*" in file/folder names).
     *
     * This will make the SkipDialog show a "Replace" button that can be used
     * to instruct the underlying job to replace any problematic character with
     * an underscore "_".
     *
     * @since 5.86
     */
    SkipDialog_Replace_Invalid_Chars = 16,

    /**
     * Set if the current operation @e cannot be retried.
     *
     * For example if there is an issue that involves the destination filesystem
     * support, e.g. VFAT and ExFat don't support symlinks, then retrying doesn't
     * make sense.
     *
     * @since 5.88
     */
    SkipDialog_Hide_Retry = 32,
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
    /**
     * Can be returned only when multiple files are passed, Option overwrite is passed
     * And files modification times are valid
     * @since 5.77
     */
    Result_OverwriteWhenOlder = 10,
    /**
     * Can be returned if the user selects to replace any character disallowed
     * by the destination filesystem with an underscore "_".
     *
     * See @ref SkipDialog_Option::SkipDialog_Replace_Invalid_Chars
     *
     * @since 5.86
     */
    Result_ReplaceInvalidChars = 11,
    /**
     * The same as @c Result_ReplaceInvalidChars, but the user selected to
     * automatically replace any invalid character, without being asked about
     * every file/folder.
     *
     * @since 5.86
     */
    Result_ReplaceAllInvalidChars = 12,
};
typedef RenameDialog_Result SkipDialog_Result;

/**
 * @class KIO::JobUiDelegateExtension jobuidelegateextension.h <KIO/JobUiDelegateExtension>
 *
 * An abstract class defining interaction with users from KIO jobs:
 * \li asking for confirmation before deleting files or directories
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
    virtual bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType) = 0;

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
