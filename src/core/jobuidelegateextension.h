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

/*!
 * \since 5.0
 *
 * \value RenameDialog_Overwrite We have an existing destination, show details about it and offer to overwrite it
 * \value RenameDialog_OverwriteItself Warn that the current operation would overwrite a file with itself, which is not allowed
 * \value RenameDialog_Skip Offer a "Skip" button, to skip other files too. Requires RenameDialog_MultipleItems
 * \value RenameDialog_MultipleItems Set if the current operation concerns multiple files, so it makes sense to offer buttons that apply the user's choice to
 * all files/folders
 * \value RenameDialog_Resume Offer a "Resume" button (plus "Resume All" if RenameDialog_MultipleItems)
 * \value RenameDialog_NoRename Don't offer a "Rename" button
 * \value [since 5.78] RenameDialog_DestIsDirectory The destination is a directory, the dialog updates labels and tooltips accordingly
 * \value [since 5.78] RenameDialog_SourceIsDirectory The source is a directory, the dialog updates labels and tooltips accordingly
 */
enum RenameDialog_Option {
    RenameDialog_Overwrite = 1,
    RenameDialog_OverwriteItself = 2,
    RenameDialog_Skip = 4,
    RenameDialog_MultipleItems = 8,
    RenameDialog_Resume = 16,
    RenameDialog_NoRename = 64,
    RenameDialog_DestIsDirectory = 128,
    RenameDialog_SourceIsDirectory = 256,
};

Q_DECLARE_FLAGS(RenameDialog_Options, RenameDialog_Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(RenameDialog_Options)

/*!
 * \since 5.0
 *
 * \value SkipDialog_MultipleItems Set if the current operation concerns multiple files, so it makes sense to offer buttons that apply the user's choice to all
files/folders.
 * \value [since 5.86] SkipDialog_Replace_Invalid_Chars Set if the current operation involves copying files/folders with certain characters in their names that
are not supported by the destination filesystem (e.g.\ VFAT and NTFS disallow "*" in file/folder names). This will make the SkipDialog show a "Replace" button
that can be used to instruct the underlying job to replace any problematic character with an underscore "_"
 * \value [since 5.88] SkipDialog_Hide_Retry Set if the current operation cannot be retried. For example if there is an issue that involves the destination
filesystem support, e.g. VFAT and ExFat don't support symlinks, then retrying doesn't make sense.
*/
enum SkipDialog_Option {
    SkipDialog_MultipleItems = 8,
    SkipDialog_Replace_Invalid_Chars = 16,
    SkipDialog_Hide_Retry = 32,
};
Q_DECLARE_FLAGS(SkipDialog_Options, SkipDialog_Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(SkipDialog_Options)

/*!
 * The result of a rename or skip dialog
 *
 * \value Result_Cancel
 * \value Result_Rename
 * \value Result_Skip
 * \value Result_AutoSkip
 * \value Result_Overwrite
 * \value Result_OverwriteAll
 * \value Result_Resume
 * \value Result_Rename
 * \value Result_Retry
 * \value Result_ResumeAll
 * \value Result_AutoRename
 * \value [since 5.77] Result_OverwriteWhenOlder Can be returned only when multiple files are passed, Option overwrite is passed and files modification times
 * are valid
 * \value [since 5.86] Result_ReplaceInvalidChars Can be returned if the user selects to replace any character disallowed by the destination filesystem with an
 * underscore "_". See SkipDialog_Option::SkipDialog_Replace_Invalid_Chars
 * \value [since 5.86] Result_ReplaceAllInvalidChars The same as Result_ReplaceInvalidChars, but the user selected to automatically replace any invalid
 * character, without being asked about every file/folder
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
    Result_OverwriteWhenOlder = 10,
    Result_ReplaceInvalidChars = 11,
    Result_ReplaceAllInvalidChars = 12,
};
typedef RenameDialog_Result SkipDialog_Result;

/*!
 * \class KIO::JobUiDelegateExtension
 * \inheaderfile KIO/JobUiDelegateExtension
 * \inmodule KIOCore
 *
 * An abstract class defining interaction with users from KIO jobs:
 * \list
 * \li asking for confirmation before deleting files or directories
 * \endlist
 * \since 5.0
 */
class KIOCORE_EXPORT JobUiDelegateExtension
{
protected:
    /*!
     * Constructor
     */
    JobUiDelegateExtension();

    virtual ~JobUiDelegateExtension();

public:
#if KIOCORE_BUILD_DEPRECATED_SINCE(6, 15)
    /*!
     * The type of deletion: real deletion, moving the files to the trash
     * or emptying the trash
     * Used by askDeleteConfirmation.
     *
     * \value Delete
     * \value Trash
     * \value EmptyTrash
     */
    enum DeletionType {
        Delete,
        Trash,
        EmptyTrash
    };

    /*!
     * ForceConfirmation: always ask the user for confirmation
     * DefaultConfirmation: don't ask the user if he/she said "don't ask again".
     *
     * Used by askDeleteConfirmation.
     *
     * \value DefaultConfirmation
     * \value ForceConfirmation
     */
    enum ConfirmationType {
        DefaultConfirmation,
        ForceConfirmation
    };
    /*!
     * Ask for confirmation before deleting/trashing \a urls.
     *
     * Note that this method is not called automatically by KIO jobs. It's the application's
     * responsibility to ask the user for confirmation before calling KIO::del() or KIO::trash().
     *
     * \a urls the urls about to be deleted/trashed
     *
     * \a deletionType the type of deletion (Delete for real deletion, Trash otherwise)
     *
     * \a confirmationType see ConfirmationType. Normally set to DefaultConfirmation.
     *
     * \note the window passed to setWindow is used as the parent for the message box.
     *
     * Returns \c true if confirmed
     */
    KIOCORE_DEPRECATED_VERSION(6, 15, "Use AskUserActionInterface::askUserDelete instead")
    virtual bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType) = 0;
#endif

    /*!
     * \value UpdateContent
     * \value OverwriteContent
     * \value RemoveContent
     *
     */
    enum ClipboardUpdaterMode {
        UpdateContent,
        OverwriteContent,
        RemoveContent,
    };

    /*!
     * Creates a clipboard updater as a child of the given job.
     */
    virtual ClipboardUpdater *createClipboardUpdater(Job *job, ClipboardUpdaterMode mode);
    /*!
     * Update URL in clipboard, if present
     */
    virtual void updateUrlInClipboard(const QUrl &src, const QUrl &dest);

    // TODO KF6: add virtual_hook

private:
    class Private;
    Private *const d;
};

/*!
 * \relates KIO::JobUiDelegateExtension
 *
 * Returns the default job UI delegate extension to be used by all KIO jobs (in which HideProgressInfo is not set)
 * Can return nullptr, if no kio GUI library is loaded.
 * \since 5.0
 */
KIOCORE_EXPORT JobUiDelegateExtension *defaultJobUiDelegateExtension();

/*!
 * \relates KIO::JobUiDelegateExtension
 *
 * Internal. Allows the KIO widgets library to register its widget-based job UI delegate extension
 * automatically.
 * \since 5.0
 */
KIOCORE_EXPORT void setDefaultJobUiDelegateExtension(JobUiDelegateExtension *extension);

} // namespace KIO

#endif
