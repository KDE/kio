/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ASKUSERACTIONINTERFACE_H
#define ASKUSERACTIONINTERFACE_H

#include <kio/jobuidelegateextension.h> // RenameDialog_Result, SkipDialog_Result
#include <kiocore_export.h>

#include <QObject>
#include <QUrl>

#include <memory>

class KJob;

namespace KIO
{
class AskUserActionInterfacePrivate;

/**
 * @class KIO::AskUserActionInterface askuseractioninterface.h <KIO/AskUserActionInterface>
 *
 * @brief The AskUserActionInterface class allows a KIO::Job to prompt the user
 * for a decision when e.g.\ copying directories/files and there is a conflict
 * (e.g.\ a file with the same name already exists at the destination).
 *
 * The methods in this interface are similar to their counterparts in
 * KIO::JobUiDelegateExtension, the main difference is that AskUserActionInterface
 * shows the dialogs using show() or open(), rather than exec(), the latter creates
 * a nested event loop which could lead to crashes.
 *
 * @sa KIO::JobUiDelegateExtension
 *
 * @since 5.78
 */
class KIOCORE_EXPORT AskUserActionInterface : public QObject
{
    Q_OBJECT

protected:
    /**
     * Constructor
     */
    explicit AskUserActionInterface(QObject *parent = nullptr);

public:
    /**
     * Destructor
     */
    ~AskUserActionInterface() override;

    /**
     * @relates KIO::RenameDialog
     *
     * Constructs a modal, parent-less "rename" dialog, to prompt the user for a decision
     * in case of conflicts, while copying/moving files. The dialog is shown using open(),
     * rather than exec() (the latter creates a nested eventloop which could lead to crashes).
     * You will need to connect to the askUserRenameResult() signal to get the dialog's result
     * (exit code). The exit code is one of KIO::RenameDialog_Result.
     *
     * @see KIO::RenameDialog_Result enum.
     *
     * @param job the job that called this method
     * @param title the title for the dialog box
     * @param src the URL of the file/dir being copied/moved
     * @param dest the URL of the destination file/dir, i.e. the one that already exists
     * @param options parameters for the dialog (which buttons to show... etc), OR'ed values
     *            from the KIO::RenameDialog_Options enum
     * @param sizeSrc size of the source file
     * @param sizeDest size of the destination file
     * @param ctimeSrc creation time of the source file
     * @param ctimeDest creation time of the destination file
     * @param mtimeSrc modification time of the source file
     * @param mtimeDest modification time of the destination file
     */
    virtual void askUserRename(KJob *job,
                               const QString &title,
                               const QUrl &src,
                               const QUrl &dest,
                               KIO::RenameDialog_Options options,
                               KIO::filesize_t sizeSrc,
                               KIO::filesize_t sizeDest,
                               const QDateTime &ctimeSrc = {},
                               const QDateTime &ctimeDest = {},
                               const QDateTime &mtimeSrc = {},
                               const QDateTime &mtimeDest = {}) = 0;

    /**
     * @relates KIO::SkipDialog
     *
     * You need to connect to the askUserSkipResult signal to get the dialog's
     * result.
     *
     * @param job the job that called this method
     * @param options parameters for the dialog (which buttons to show... etc), OR'ed
     *            values from the KIO::SkipDialog_Options enum
     * @param error_text the error text to show to the user (usually the string returned
     *               by KJob::errorText())
     */
    virtual void askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText) = 0;

    /**
     * The type of deletion.
     *
     * Used by askUserDelete().
     */
    enum DeletionType {
        Delete, /// Delete the files/directories directly, i.e. without moving them to Trash
        Trash, /// Move the files/directories to Trash
        EmptyTrash, /// Empty the Trash
        /**
         * This is the same as Delete, but more text is added to the message to inform
         * the user that moving to Trash was tried but failed due to size constraints.
         * Typical use case is re-asking the user about deleting instead of Trashing.
         * @since 5.100
         */
        DeleteInsteadOfTrash,
    };

    /**
     * Deletion confirmation type.
     *
     * Used by askUserDelete().
     */
    enum ConfirmationType {
        DefaultConfirmation, ///< Do not ask if the user has previously set the "Do not ask again"
                             ///< checkbox (which is is shown in the message dialog invoked by askUserDelete())
        ForceConfirmation, ///< Always ask the user for confirmation
    };

    /**
     * Ask for confirmation before moving @p urls (files/directories) to the Trash,
     * emptying the Trash, or directly deleting files (i.e. without moving to Trash).
     *
     * Note that this method is not called automatically by KIO jobs. It's the
     * application's responsibility to ask the user for confirmation before calling
     * KIO::del() or KIO::trash().
     *
     * You need to connect to the askUserDeleteResult signal to get the dialog's result
     * (exit code).
     *
     * @param urls the urls about to be moved to the Trash or deleted directly
     * @param deletionType the type of deletion (Delete for real deletion, Trash otherwise),
     *                 see the DeletionType enum
     * @param confirmationType The type of deletion confirmation, see the ConfirmationType enum.
     *                     Normally set to DefaultConfirmation
     * @param parent the parent widget of the message box
     */
    virtual void askUserDelete(const QList<QUrl> &urls,
                               DeletionType deletionType,
                               ConfirmationType confirmationType,
                               QWidget *parent = nullptr) = 0; // TODO KF6: replace QWidget* with QObject*

    enum MessageDialogType {
        QuestionTwoActions = 1, ///< @since 5.100
        QuestionTwoActionsCancel = 2, ///< @since 5.100
        WarningTwoActions = 3, ///< @since 5.100
        WarningTwoActionsCancel = 4, ///< @since 5.100
        WarningContinueCancel = 5,
        SSLMessageBox = 6,
        Information = 7,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 97)
        Sorry ///< @deprecated Since 5.97, use Error.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 97, "Use Error.") = 8,
#endif
        Error = 9,
#if KIOCORE_ENABLE_DEPRECATED_SINCE(5, 100)
        QuestionYesNo ///< @deprecated Since 5.100, use QuestionTwoActions.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use QuestionTwoActions.") = QuestionTwoActions,
        QuestionYesNoCancel ///< @deprecated Since 5.100, use QuestionTwoActionsCancel.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use QuestionTwoActionsCancel.") = QuestionTwoActionsCancel,
        WarningYesNo ///< @deprecated Since 5.100, use WarningTwoActions.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use WarningTwoActions.") = WarningTwoActions,
        WarningYesNoCancel ///< @deprecated Since 5.100, use WarningTwoActionsCancel.
            KIOCORE_ENUMERATOR_DEPRECATED_VERSION(5, 100, "Use WarningTwoActionsCancel.") = WarningTwoActionsCancel,
#endif
    };

    /**
     * This function allows for the delegation of user prompts from the KIO worker.
     *
     * @param type the desired type of message box, see the MessageDialogType enum
     * @param text the message to show to the user
     * @param title the title of the message dialog box
     * @param primaryActionText the text for the primary action
     * @param secondatyActionText the text for the secondary action
     * @param primaryActionIconName the icon to show on the primary action
     * @param secondatyActionIconName the icon to show on the secondary action
     * @param dontAskAgainName the config key name used to store the result from
     *                     'Do not ask again' checkbox
     * @param details more details about the message shown to the user
     * @param sslMetaData SSL information primarily used by the SSLMessageBox dialog
     * @param parent the parent widget of the dialog
     */
    virtual void requestUserMessageBox(MessageDialogType type,
                                       const QString &text,
                                       const QString &title,
                                       const QString &primaryActionText,
                                       const QString &secondatyActionText,
                                       const QString &primaryActionIconName = {},
                                       const QString &secondatyActionIconName = {},
                                       const QString &dontAskAgainName = {},
                                       const QString &details = {},
                                       const KIO::MetaData &sslMetaData = {},
                                       QWidget *parent = nullptr) = 0; // TODO KF6: replace QWidget* with QObject*, document "widget or window"

Q_SIGNALS:
    /**
     * Implementations of this interface must emit this signal when the rename dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * @param result the exit code from the rename dialog, one of the KIO::RenameDialog_Result
     *           enum
     * @param newUrl the new destination URL set by the user
     * @param parentJob the job that invoked the dialog
     */
    void askUserRenameResult(KIO::RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob);

    /**
     * Implementations of this interface must emit this signal when the skip dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * @param result the exit code from the skip dialog, one of the KIO::SkipDialog_Result enum
     * @param parentJob the job that invoked the dialog
     */
    void askUserSkipResult(KIO::SkipDialog_Result result, KJob *parentJob);

    /**
     * Implementations of this interface must emit this signal when the dialog invoked
     * by askUserDelete() finishes, to notify the caller of the user's decision.
     *
     * @param allowDelete set to true if the user confirmed the delete operation, otherwise
     * set to false
     * @param urls a list of urls to delete/trash
     * @param deletionType the deletion type to use, one of KIO::AskUserActionInterface::DeletionType
     * @param parent the parent widget passed to askUserDelete(), for request identification
     */
    void askUserDeleteResult(bool allowDelete,
                             const QList<QUrl> &urls,
                             KIO::AskUserActionInterface::DeletionType deletionType,
                             QWidget *parent); // TODO KF6: replace QWidget* with QObject*

    /**
     * Implementations of this interface must emit this signal when the dialog invoked
     * by requestUserMessageBox() finishes, to notify the caller of the dialog's result
     * (exit code).
     *
     * @param result the exit code of the dialog, one of KIO::WorkerBase::ButtonCode enum
     */
    void messageBoxResult(int result); // TODO KF6: add a QObject* to identify requests? Or return an int from the request method and pass it back here?

private:
    std::unique_ptr<AskUserActionInterfacePrivate> d;
};

} // namespace KIO

#endif // ASKUSERACTIONINTERFACE_H
