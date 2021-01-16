/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOBUIDELEGATE_H
#define KIO_JOBUIDELEGATE_H

#include <KDialogJobUiDelegate>
#include <kio/jobuidelegateextension.h>
#include <kio/askuseractioninterface.h>
#include <kio/skipdialog.h>
#include <kio/renamedialog.h>
#include <kio/global.h>

class KJob;
namespace KIO
{
class JobUiDelegatePrivate;

class Job;

/**
 * @class KIO::JobUiDelegate jobuidelegate.h <KIO/JobUiDelegate>
 *
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
     * Constructs a new KIO Job UI Delegate.
     * @param flags allows to enable automatic error/warning handling
     * @param window the window associated with this delegate, see setWindow.
     * @since 5.70
     */
    explicit JobUiDelegate(KJobUiDelegate::Flags flags, QWidget *window); // KF6 TODO: merge with previous ctor using AutoHandlingDisabled and nullptr default values (same in KDialogJobUiDelegate)

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
    void setWindow(QWidget *window) override;

    /**
     * Unregister the given window from kded.
     * This is normally done automatically when the window is destroyed.
     *
     * This method is useful for instance when keeping a hidden window
     * around to make it faster to reuse later.
     * @since 5.2
     */
    static void unregisterWindow(QWidget *window);

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
     * @param newDestPath the new destination path, valid if R_RENAME was returned.
     * @param sizeSrc size of source file
     * @param sizeDest size of destination file
     * @param ctimeSrc creation time of source file
     * @param ctimeDest creation time of destination file
     * @param mtimeSrc modification time of source file
     * @param mtimeDest modification time of destination file
     * @return the result
     */
    RenameDialog_Result askFileRename(KJob *job,
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
                                      const QDateTime &mtimeDest = QDateTime()) override;

    /**
     * @internal
     * See skipdialog.h
     */
    SkipDialog_Result askSkip(KJob *job,
                              KIO::SkipDialog_Options options,
                              const QString &error_text) override;

    /**
     * Ask for confirmation before deleting/trashing @p urls.
     *
     * Note that this method is not called automatically by KIO jobs. It's the application's
     * responsibility to ask the user for confirmation before calling KIO::del() or KIO::trash().
     *
     * @param urls the urls about to be deleted/trashed
     * @param deletionType the type of deletion (Delete for real deletion, Trash otherwise)
     * @param confirmation see ConfirmationType. Normally set to DefaultConfirmation.
     * Note: the window passed to setWindow is used as the parent for the message box.
     * @return true if confirmed
     */
    bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType,
                               ConfirmationType confirmationType) override;

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
    * @param metaData SSL information used by the SSLMessageBox. Since 5.66 this is also used for privilege operation details.
    *
    * @since 4.11
    *
    * @internal
    */
    // KF6 TODO Add a QString parameter for "details" and keep in sync with API in SlaveBase, SlaveInterface, and JobUiDelegateExtension.
    int requestMessageBox(MessageBoxType type, const QString &text,
                          const QString &caption,
                          const QString &buttonYes,
                          const QString &buttonNo,
                          const QString &iconYes = QString(),
                          const QString &iconNo = QString(),
                          const QString &dontAskAgainName = QString(),
                          const KIO::MetaData &metaData = KIO::MetaData()) override;

    /**
     * Creates a clipboard updater
     */
    ClipboardUpdater *createClipboardUpdater(Job *job, ClipboardUpdaterMode mode) override;
    /**
     * Update URL in clipboard, if present
     */
    void updateUrlInClipboard(const QUrl &src, const QUrl &dest) override;

private:
    std::unique_ptr<JobUiDelegatePrivate> const d;
};
}

#endif
