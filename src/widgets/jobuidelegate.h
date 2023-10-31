/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2000 Stephan Kulow <coolo@kde.org>
    SPDX-FileCopyrightText: 2000 David Faure <faure@kde.org>
    SPDX-FileCopyrightText: 2006 Kevin Ottens <ervin@kde.org>
    SPDX-FileCopyrightText: 2013 Dawit Alemayehu <adawit@kde.org>
    SPDX-FileCopyrightText: 2022 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KIO_JOBUIDELEGATE_H
#define KIO_JOBUIDELEGATE_H

#include <KDialogJobUiDelegate>
#include <kio/askuseractioninterface.h>
#include <kio/global.h>
#include <kio/jobuidelegateextension.h>
#include <kio/renamedialog.h>
#include <kio/skipdialog.h>

class KJob;
class KDirOperator;
class KIOWidgetJobUiDelegateFactory;

namespace KIO
{
class JobUiDelegatePrivate;

class FileUndoManager;

class Job;

/**
 * @class KIO::JobUiDelegate jobuidelegate.h <KIO/JobUiDelegate>
 *
 * A UI delegate tuned to be used with KIO Jobs.
 */
class KIOWIDGETS_EXPORT JobUiDelegate : public KDialogJobUiDelegate, public JobUiDelegateExtension
{
    Q_OBJECT
    // Allow the factory to construct. Everyone else needs to go through the factory or derive!
    friend class ::KIOWidgetJobUiDelegateFactory;
    // KIO internals don't need to derive either
    friend class KIO::FileUndoManager;

protected:
    friend class ::KDirOperator;

    /**
     * Constructs a new KIO Job UI delegate.
     * @param flags allows to enable automatic error/warning handling
     * @param window the window associated with this delegate, see setWindow.
     * @param ifaces Interface instances such as OpenWithHandlerInterface to replace the default interfaces
     * @since 5.98
     */
    explicit JobUiDelegate(KJobUiDelegate::Flags flags = AutoHandlingDisabled, QWidget *window = nullptr, const QList<QObject *> &ifaces = {});

public:
    /**
     * Destroys the KIO Job UI delegate.
     */
    ~JobUiDelegate() override;

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
    bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType) override;

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
