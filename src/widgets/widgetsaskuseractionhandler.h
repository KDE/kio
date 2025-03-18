/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2020 Ahmad Samir <a.samirh78@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#ifndef WIDGETSASKUSERACTIONHANDLER_H
#define WIDGETSASKUSERACTIONHANDLER_H

#include "askignoresslerrorsjob.h"
#include "kiowidgets_export.h"
#include "ksslerroruidata.h"
#include <kio/askuseractioninterface.h>
#include <kio/global.h>
#include <kio/jobuidelegateextension.h>
#include <kio/renamedialog.h>
#include <kio/skipdialog.h>

namespace KIO
{
// TODO KF6: Handle this the same way we end up handling WidgetsUntrustedProgramHandler.

/**
 * @class KIO::WidgetsAskUserActionHandler widgetsaskuseractionhandler.h <KIO/WidgetsAskUserActionHandler>
 *
 * This implements KIO::AskUserActionInterface.
 * @see KIO::AskUserActionInterface()
 *
 * @sa KIO::JobUiDelegateExtension()
 *
 * @since 5.78
 * @note This header wasn't installed until 5.98
 */

class WidgetsAskUserActionHandlerPrivate;

class KIOWIDGETS_EXPORT WidgetsAskUserActionHandler : public AskUserActionInterface
{
    Q_OBJECT
public:
    explicit WidgetsAskUserActionHandler(QObject *parent = nullptr);

    ~WidgetsAskUserActionHandler() override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserRename()
     */
    void askUserRename(KJob *job,
                       const QString &title,
                       const QUrl &src,
                       const QUrl &dest,
                       KIO::RenameDialog_Options options,
                       KIO::filesize_t sizeSrc = KIO::filesize_t(-1),
                       KIO::filesize_t sizeDest = KIO::filesize_t(-1),
                       const QDateTime &ctimeSrc = {},
                       const QDateTime &ctimeDest = {},
                       const QDateTime &mtimeSrc = {},
                       const QDateTime &mtimeDest = {}) override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserSkip()
     */
    void askUserSkip(KJob *job, KIO::SkipDialog_Options options, const QString &error_text) override;

    /**
     * @copydoc KIO::AskUserActionInterface::askUserDelete()
     */
    void askUserDelete(const QList<QUrl> &urls, DeletionType deletionType, ConfirmationType confirmationType, QWidget *parent = nullptr) override;

    /**
     * @copydoc KIO::AskUserActionInterface::requestUserMessageBox()
     */
    void requestUserMessageBox(MessageDialogType type,
                               const QString &text,
                               const QString &title,
                               const QString &primaryActionText,
                               const QString &secondaryActionText,
                               const QString &primaryActionIconName = {},
                               const QString &secondaryActionIconName = {},
                               const QString &dontAskAgainName = {},
                               const QString &details = {},
                               QWidget *parent = nullptr) override;

    void askIgnoreSslErrors(const QVariantMap &sslErrorData, QWidget *parent) override;

    void askIgnoreSslErrors(const KSslErrorUiData &uiData, KIO::AskIgnoreSslErrorsJob::RulesStorage storedRules, QObject *parent) override;

    void setWindow(QWidget *window);

private:
    void showSslDetails(const QVariantMap &sslErrorData, QWidget *parentWidget);
    KIOWIDGETS_NO_EXPORT void showSslRememberDialog(const KSslErrorUiData &uiData, KIO::AskIgnoreSslErrorsJob::RulesStorage storedRules, QObject *parent);
    std::unique_ptr<WidgetsAskUserActionHandlerPrivate> d;
};

} // namespace KIO

#endif // WIDGETSASKUSERACTIONHANDLER_H
