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

#include "kiowidgets_export.h"
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

/*!
 * \class KIO::JobUiDelegate
 * \inmodule KIOWidgets
 * \inheaderfile KIO/JobUiDelegate
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

    /*!
     * Constructs a new KIO Job UI delegate.
     *
     * \a flags allows to enable automatic error/warning handling
     *
     * \a window the window associated with this delegate, see setWindow.
     *
     * \a ifaces Interface instances such as OpenWithHandlerInterface to replace the default interfaces
     *
     * \since 5.98
     */
    explicit JobUiDelegate(KJobUiDelegate::Flags flags = AutoHandlingDisabled, QWidget *window = nullptr, const QList<QObject *> &ifaces = {});

public:
    ~JobUiDelegate() override;

public:
    /*!
     * Associate this job with a window given by \a window.
     *
     * \a window the window to associate to
     *
     * \sa window()
     */
    void setWindow(QWidget *window) override;

    /*!
     * Unregister the given window from kded.
     * This is normally done automatically when the window is destroyed.
     *
     * This method is useful for instance when keeping a hidden window
     * around to make it faster to reuse later.
     * \since 5.2
     */
    static void unregisterWindow(QWidget *window);

#if KIOWIDGETS_ENABLE_DEPRECATED_SINCE(6, 15)
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
     * \note The window passed to setWindow is used as the parent for the message box.
     *
     * Returns true if confirmed
     *
     * \deprecated[6.15] Use AskUserActionInterface::askUserDelete
     */
    bool askDeleteConfirmation(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType) override;
#endif

    /*!
     * Creates a clipboard updater
     */
    ClipboardUpdater *createClipboardUpdater(Job *job, ClipboardUpdaterMode mode) override;
    /*!
     * Update URL in clipboard, if present
     */
    void updateUrlInClipboard(const QUrl &src, const QUrl &dest) override;

private:
    std::unique_ptr<JobUiDelegatePrivate> const d;
};
}

#endif
