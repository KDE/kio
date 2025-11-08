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

#include <QSsl>

#include <memory>

class KJob;

namespace KIO
{
class AskUserActionInterfacePrivate;

/*!
 * \class KIO::AskUserActionInterface
 * \inheaderfile KIO/AskUserActionInterface
 * \inmodule KIOCore
 *
 * \brief The AskUserActionInterface class allows a KIO::Job to prompt the user
 * for a decision when e.g.\ copying directories/files and there is a conflict
 * (e.g.\ a file with the same name already exists at the destination).
 *
 * The methods in this interface are similar to their counterparts in
 * KIO::JobUiDelegateExtension, the main difference is that AskUserActionInterface
 * shows the dialogs using show() or open(), rather than exec(), the latter creates
 * a nested event loop which could lead to crashes.
 *
 * \sa KIO::JobUiDelegateExtension
 *
 * \since 5.78
 */
class KIOCORE_EXPORT AskUserActionInterface : public QObject
{
    Q_OBJECT

protected:
    /*!
     * Constructor
     */
    explicit AskUserActionInterface(QObject *parent = nullptr);

public:
    ~AskUserActionInterface() override;

    /*!
     * \sa KIO::RenameDialog
     *
     * Constructs a modal, parent-less "rename" dialog, to prompt the user for a decision
     * in case of conflicts, while copying/moving files. The dialog is shown using open(),
     * rather than exec() (the latter creates a nested eventloop which could lead to crashes).
     * You will need to connect to the askUserRenameResult() signal to get the dialog's result
     * (exit code). The exit code is one of KIO::RenameDialog_Result.
     *
     * \sa KIO::RenameDialog_Result
     *
     * \a job the job that called this method
     *
     * \a title the title for the dialog box
     *
     * \a src the URL of the file/dir being copied/moved
     *
     * \a dest the URL of the destination file/dir, i.e. the one that already exists
     *
     * \a options parameters for the dialog (which buttons to show... etc), OR'ed values
     *
     * \a from the KIO::RenameDialog_Options enum
     *
     * \a sizeSrc size of the source file
     *
     * \a sizeDest size of the destination file
     *
     * \a ctimeSrc creation time of the source file
     *
     * \a ctimeDest creation time of the destination file
     *
     * \a mtimeSrc modification time of the source file
     *
     * \a mtimeDest modification time of the destination file
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

    /*!
     * \sa KIO::SkipDialog
     *
     * You need to connect to the askUserSkipResult signal to get the dialog's
     * result.
     *
     * \a job the job that called this method
     *
     * \a options parameters for the dialog (which buttons to show... etc), OR'ed
     *            values from the KIO::SkipDialog_Options enum
     *
     * \a error_text the error text to show to the user (usually the string returned
     *               by KJob::errorText())
     */
    virtual void askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &errorText) = 0;

    /*!
     * The type of deletion.
     *
     * Used by askUserDelete().
     *
     * \value Delete Delete the files/directories directly, i.e. without moving them to Trash
     * \value Trash Move the files/directories to Trash
     * \value EmptyTrash Empty the Trash
     * \value [since 5.100] DeleteInsteadOfTrash This is the same as Delete, but more text is added to the message to inform the user that moving to Trash was
     * tried but failed due to size constraints. Typical use case is re-asking the user about deleting instead of Trashing.
     * \value [since 6.21] DeleteNoTrashAvailable This is the same as DeleteInsteadOfTrash, but used when trashing failed because no trash directory
     * was available.
     */
    enum DeletionType {
        Delete,
        Trash,
        EmptyTrash,
        DeleteInsteadOfTrash, // TODO KF7 rename to DeleteTrashToSmall or something like that to prevent confusion
        DeleteNoTrashAvailable,
    };

    /*!
     * Deletion confirmation type.
     *
     * Used by askUserDelete().
     *
     * \value DefaultConfirmation Do not ask if the user has previously set the "Do not ask again" checkbox (which is is shown in the message dialog invoked by
     * askUserDelete())
     * \value ForceConfirmation Always ask the user for confirmation
     *
     */
    enum ConfirmationType {
        DefaultConfirmation,
        ForceConfirmation,
    };

    /*!
     * Ask for confirmation before moving \a urls (files/directories) to the Trash,
     * emptying the Trash, or directly deleting files (i.e. without moving to Trash).
     *
     * Note that this method is not called automatically by KIO jobs. It's the
     * application's responsibility to ask the user for confirmation before calling
     * KIO::del() or KIO::trash().
     *
     * You need to connect to the askUserDeleteResult signal to get the dialog's result
     * (exit code).
     *
     * \a urls the urls about to be moved to the Trash or deleted directly
     *
     * \a deletionType the type of deletion (Delete for real deletion, Trash otherwise),
     *                 see the DeletionType enum
     *
     * \a confirmationType The type of deletion confirmation, see the ConfirmationType enum.
     *                     Normally set to DefaultConfirmation
     *
     * \a parent the parent widget of the message box
     */
    virtual void askUserDelete(const QList<QUrl> &urls,
                               DeletionType deletionType,
                               ConfirmationType confirmationType,
                               QWidget *parent = nullptr) = 0; // TODO KF6: replace QWidget* with QObject*

    /*!
     * \value [since 5.100] QuestionTwoActions
     * \value [since 5.100] QuestionTwoActionsCancel
     * \value [since 5.100] WarningTwoActions
     * \value [since 5.100] WarningTwoActionsCancel
     * \value WarningContinueCancel
     * \value Information
     * \value Error
     */
    enum MessageDialogType {
        QuestionTwoActions = 1,
        QuestionTwoActionsCancel = 2,
        WarningTwoActions = 3,
        WarningTwoActionsCancel = 4,
        WarningContinueCancel = 5,
        Information = 7,
        Error = 9,
    };

    /*!
     * This function allows for the delegation of user prompts from the KIO worker.
     *
     * \a type the desired type of message box, see the MessageDialogType enum
     *
     * \a text the message to show to the user
     *
     * \a title the title of the message dialog box
     *
     * \a primaryActionText the text for the primary action
     *
     * \a secondatyActionText the text for the secondary action
     *
     * \a primaryActionIconName the icon to show on the primary action
     *
     * \a secondatyActionIconName the icon to show on the secondary action
     *
     * \a dontAskAgainName the config key name used to store the result from
     *               'Do not ask again' checkbox
     *
     * \a details more details about the message shown to the user
     *
     * \a parent the parent widget of the dialog
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
                                       QWidget *parent = nullptr) = 0; // TODO KF6: replace QWidget* with QObject*, document "widget or window"

    /*!
     * \since 6.0
     */
    virtual void askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *parent) = 0;

Q_SIGNALS:
    /*!
     * Implementations of this interface must emit this signal when the rename dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * \a result the exit code from the rename dialog, one of the KIO::RenameDialog_Result
     *           enum
     *
     * \a newUrl the new destination URL set by the user
     *
     * \a parentJob the job that invoked the dialog
     */
    void askUserRenameResult(KIO::RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob);

    /*!
     * Implementations of this interface must emit this signal when the skip dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * \a result the exit code from the skip dialog, one of the KIO::SkipDialog_Result enum
     *
     * \a parentJob the job that invoked the dialog
     */
    void askUserSkipResult(KIO::SkipDialog_Result result, KJob *parentJob);

    /*!
     * Implementations of this interface must emit this signal when the dialog invoked
     * by askUserDelete() finishes, to notify the caller of the user's decision.
     *
     * \a allowDelete set to true if the user confirmed the delete operation, otherwise
     * set to false
     *
     * \a urls a list of urls to delete/trash
     *
     * \a deletionType the deletion type to use, one of KIO::AskUserActionInterface::DeletionType
     *
     * \a parent the parent widget passed to askUserDelete(), for request identification
     */
    void askUserDeleteResult(bool allowDelete,
                             const QList<QUrl> &urls,
                             KIO::AskUserActionInterface::DeletionType deletionType,
                             QWidget *parent); // TODO KF6: replace QWidget* with QObject*

    /*!
     * Implementations of this interface must emit this signal when the dialog invoked
     * by requestUserMessageBox() finishes, to notify the caller of the dialog's result
     * (exit code).
     *
     * \a result the exit code of the dialog, one of KIO::WorkerBase::ButtonCode enum
     */
    void messageBoxResult(int result); // TODO KF6: add a QObject* to identify requests? Or return an int from the request method and pass it back here?

    /*!
     *
     */
    void askIgnoreSslErrorsResult(int result);

private:
    std::unique_ptr<AskUserActionInterfacePrivate> d;
};

} // namespace KIO

#endif // ASKUSERACTIONINTERFACE_H
