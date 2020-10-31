/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef ASKUSERACTIONINTERFACE_H
#define ASKUSERACTIONINTERFACE_H

#include <kiocore_export.h>
#include <jobuidelegateextension.h> // RenameDialog_Result, SkipDialog_Result

#include <QObject>

#include <memory>

class KJob;

namespace KIO {

class AskUserActionInterfacePrivate;

/**
 * @class KIO::AskUserActionInterface askuseractioninterface.h <KIO/AskUserActionInterface>
 *
 * @brief The AskUserActionInterface class allows a KIO::Job to prompt the user
 * for a decision when e.g. copying directories/files and there is a conflict
 * (e.g. a file with the same name already exists at the destination).
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
    AskUserActionInterface();

    /**
     * Destructor
     */
    ~AskUserActionInterface() override;

public:
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
     * @param caption the title for the dialog box
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
                               const QString &caption,
                               const QUrl &src,
                               const QUrl &dest,
                               KIO::RenameDialog_Options options,
                               KIO::filesize_t sizeSrc,
                               KIO::filesize_t sizeDest,
                               const QDateTime &ctimeSrc,
                               const QDateTime &ctimeDest,
                               const QDateTime &mtimeSrc,
                               const QDateTime &mtimeDest) = 0;

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
    virtual void askUserSkip(KJob *job,
                             KIO::SkipDialog_Options options,
                             const QString &errorText) = 0;

Q_SIGNALS:
    /**
     * Implementations of this interface must emit this signal when the rename dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * @param result the exit code from the rename dialog, one of the KIO::RenameDialog_Result
     *           enum
     * @param newUrl the new destination URL set by the user
     * @param parentJob the parent KIO::Job that invoked the dialog
     */
    void askUserRenameResult(KIO::RenameDialog_Result result, const QUrl &newUrl, KJob *parentJob);

    /**
     * Implementations of this interface must emit this signal when the skip dialog
     * finishes, to notify the caller of the dialog's result.
     *
     * @param result the exit code from the skip dialog, one of the KIO::SkipDialog_Result enum
     * @param parentJob the parent KJob that invoked the dialog
     */
    void askUserSkipResult(KIO::SkipDialog_Result result, KJob *parentJob);

private:
    std::unique_ptr<AskUserActionInterfacePrivate> d;
};

} // namespace KIO

#endif // ASKUSERACTIONINTERFACE_H
